#include "dbc_decoder.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr)                                                       \
  do {                                                                          \
    if (!(expr)) {                                                              \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
      return false;                                                             \
    }                                                                           \
  } while (0)

static bool test_legacy_decoder_api(void) {
  const char dbc_text[] =
    "BO_ 801 Can2Data: 8 Vector__XXX\n"
    " SG_ marker : 0|16@1+ (1,0) [0|65535] \"count\" Vector__XXX\n"
    " SG_ sequence : 16|16@1+ (1,0) [0|65535] \"count\" Vector__XXX\n";
  DbcDatabase db;
  SignalCache cache;
  size_t lines = 0u;
  ASSERT_TRUE(dbc_parse_text(&db, dbc_text, strlen(dbc_text), &lines));
  signal_cache_init(&cache);

  const CanFrame frame = {
    .id = 801u,
    .ide = CAN_ID_STANDARD,
    .dlc = 8u,
    .data = {0xc2u, 0xa5u, 0x34u, 0x12u},
  };
  ASSERT_TRUE(dbc_decode_frame_to_signal_cache(&db, &frame, &cache, 42u) == 2u);

  const SignalCacheEntry *marker = signal_cache_find(&cache, "Can2Data.marker");
  const SignalCacheEntry *sequence = signal_cache_find(&cache, "Can2Data.sequence");
  ASSERT_TRUE(marker != NULL && marker->raw_value == 0xa5c2 &&
              marker->physical_value == 42434.0);
  ASSERT_TRUE(sequence != NULL && sequence->raw_value == 0x1234 &&
              sequence->updated_ms == 42u);

  CanFrame unmatched = frame;
  unmatched.id = 0x123u;
  ASSERT_TRUE(dbc_decode_frame_to_signal_cache(&db, &unmatched, &cache, 43u) == 0u);
  ASSERT_TRUE(cache.count == 2u);
  return true;
}

static void set_signal(DbcSelectedRuntimeSignal *signal,
                       uint16_t runtime_message_index,
                       uint16_t value_state_index,
                       uint32_t normalized_id,
                       uint8_t flags,
                       uint8_t declared_payload_length,
                       double factor,
                       double offset) {
  memset(signal, 0, sizeof(*signal));
  signal->runtime_message_index = runtime_message_index;
  signal->value_state_index = value_state_index;
  signal->normalized_id = normalized_id;
  signal->start_bit = 0u;
  signal->bit_length = 8u;
  signal->flags = flags;
  signal->declared_payload_length = declared_payload_length;
  signal->factor = factor;
  signal->offset = offset;
  signal->minimum = 0.0;
  signal->maximum = 255.0 * factor + offset;
}

static void init_identity_runtime(DbcSelectedRuntimeSnapshot *snapshot) {
  dbc_selected_runtime_snapshot_init(snapshot);
  snapshot->active_slot = 0u;
  snapshot->has_active = true;
  DbcSelectedRuntime *runtime = &snapshot->slots[0];
  runtime->runtime_generation = 7u;
  runtime->message_count = 2u;
  runtime->signal_count = 2u;
  runtime->messages[0] = (DbcSelectedRuntimeMessage){
    .normalized_id = 0x321u,
    .first_signal_index = 0u,
    .signal_count = 1u,
    .flags = 0u,
    .declared_payload_length = 8u
  };
  runtime->messages[1] = (DbcSelectedRuntimeMessage){
    .normalized_id = 0x321u,
    .first_signal_index = 1u,
    .signal_count = 1u,
    .flags = LARGE_DBC_SIGNAL_FLAG_IDE,
    .declared_payload_length = 8u
  };
  set_signal(&runtime->signals[0], 0u, 0u, 0x321u, 0u, 8u, 2.0, 1.0);
  set_signal(&runtime->signals[1], 1u, 1u, 0x321u,
             LARGE_DBC_SIGNAL_FLAG_IDE, 8u, 1.0, 0.0);
}

static bool snapshot_is(const DbcSelectedRuntimeSnapshot *runtime,
                        uint16_t index,
                        SignalValueQuality quality,
                        int64_t raw,
                        double value,
                        uint32_t updated_ms,
                        uint32_t update_seq) {
  SignalValueSnapshot snapshot;
  return dbc_selected_runtime_copy_active_value(runtime, index, &snapshot) &&
         snapshot.quality == quality && snapshot.raw == raw &&
         snapshot.value == value && snapshot.updated_ms == updated_ms &&
         snapshot.update_seq == update_seq;
}

static bool test_selected_identity_and_classic_decode(void) {
  DbcSelectedRuntimeSnapshot runtime;
  init_identity_runtime(&runtime);
  ASSERT_TRUE(snapshot_is(&runtime, 0u, SIGNAL_VALUE_QUALITY_MISSING,
                          0, 0.0, 0u, 0u));
  ASSERT_TRUE(snapshot_is(&runtime, 1u, SIGNAL_VALUE_QUALITY_MISSING,
                          0, 0.0, 0u, 0u));

  CanFrame frame = {
    .id = 0x321u,
    .ide = CAN_ID_STANDARD,
    .fd = false,
    .dlc = 8u,
    .data = {42u}
  };
  ASSERT_TRUE(dbc_decode_frame_to_selected_values(&runtime, &frame, 100u) == 1u);
  ASSERT_TRUE(snapshot_is(&runtime, 0u, SIGNAL_VALUE_QUALITY_GOOD,
                          42, 85.0, 100u, 2u));
  ASSERT_TRUE(snapshot_is(&runtime, 1u, SIGNAL_VALUE_QUALITY_MISSING,
                          0, 0.0, 0u, 0u));

  frame.ide = (CanIdType)2u;
  ASSERT_TRUE(dbc_decode_frame_to_selected_values(&runtime, &frame, 101u) == 0u);
  ASSERT_TRUE(snapshot_is(&runtime, 0u, SIGNAL_VALUE_QUALITY_GOOD,
                          42, 85.0, 100u, 2u));

  frame.ide = CAN_ID_EXTENDED;
  frame.data[0] = 7u;
  ASSERT_TRUE(dbc_decode_frame_to_selected_values(&runtime, &frame, 101u) == 1u);
  ASSERT_TRUE(snapshot_is(&runtime, 0u, SIGNAL_VALUE_QUALITY_GOOD,
                          42, 85.0, 100u, 2u));
  ASSERT_TRUE(snapshot_is(&runtime, 1u, SIGNAL_VALUE_QUALITY_GOOD,
                          7, 7.0, 101u, 2u));

  frame.ide = CAN_ID_STANDARD;
  frame.fd = true;
  frame.data[0] = 8u;
  ASSERT_TRUE(dbc_decode_frame_to_selected_values(&runtime, &frame, 102u) == 1u);
  ASSERT_TRUE(snapshot_is(&runtime, 0u, SIGNAL_VALUE_QUALITY_GOOD,
                          8, 17.0, 102u, 4u));
  return true;
}

static bool test_length_and_fd_requirement_reject_without_update(void) {
  DbcSelectedRuntimeSnapshot runtime;
  dbc_selected_runtime_snapshot_init(&runtime);
  runtime.active_slot = 0u;
  runtime.has_active = true;
  DbcSelectedRuntime *active = &runtime.slots[0];
  active->runtime_generation = 8u;
  active->message_count = 1u;
  active->signal_count = 1u;
  active->messages[0] = (DbcSelectedRuntimeMessage){
    .normalized_id = 0x456u,
    .first_signal_index = 0u,
    .signal_count = 1u,
    .flags = LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED,
    .declared_payload_length = 12u
  };
  set_signal(&active->signals[0], 0u, 0u, 0x456u,
             LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED, 12u, 1.0, 0.0);

  CanFrame frame = {
    .id = 0x456u,
    .ide = CAN_ID_STANDARD,
    .fd = false,
    .dlc = 12u,
    .data = {9u}
  };
  ASSERT_TRUE(dbc_decode_frame_to_selected_values(&runtime, &frame, 200u) == 0u);
  ASSERT_TRUE(snapshot_is(&runtime, 0u, SIGNAL_VALUE_QUALITY_MISSING,
                          0, 0.0, 0u, 0u));

  frame.fd = true;
  frame.dlc = 8u;
  ASSERT_TRUE(dbc_decode_frame_to_selected_values(&runtime, &frame, 201u) == 0u);
  ASSERT_TRUE(snapshot_is(&runtime, 0u, SIGNAL_VALUE_QUALITY_MISSING,
                          0, 0.0, 0u, 0u));

  frame.dlc = 12u;
  ASSERT_TRUE(dbc_decode_frame_to_selected_values(&runtime, &frame, 202u) == 1u);
  ASSERT_TRUE(snapshot_is(&runtime, 0u, SIGNAL_VALUE_QUALITY_GOOD,
                          9, 9.0, 202u, 2u));

  frame.dlc = 13u;
  frame.data[0] = 10u;
  ASSERT_TRUE(dbc_decode_frame_to_selected_values(&runtime, &frame, 203u) == 0u);
  ASSERT_TRUE(snapshot_is(&runtime, 0u, SIGNAL_VALUE_QUALITY_GOOD,
                          9, 9.0, 202u, 2u));
  return true;
}

int main(void) {
  if (!test_legacy_decoder_api() ||
      !test_selected_identity_and_classic_decode() ||
      !test_length_and_fd_requirement_reject_without_update()) {
    return 1;
  }
  puts("DBC decoder tests passed");
  return 0;
}
