#include "dbc_decoder.h"

#include "signal_codec.h"

size_t dbc_decode_frame_to_signal_cache(const DbcDatabase *db,
                                        const CanFrame *frame,
                                        SignalCache *cache,
                                        uint32_t now_ms) {
  if (db == NULL || frame == NULL || cache == NULL) {
    return 0u;
  }

  const DbcMessage *message = dbc_find_message(db, frame->id);
  if (message == NULL) {
    return 0u;
  }

  size_t updated = 0u;
  const size_t end = message->signal_start + message->signal_count;
  for (size_t i = message->signal_start; i < end; ++i) {
    const DbcSignal *signal = &db->signals[i];
    int64_t raw_value = 0;
    double physical_value = 0.0;
    if (!signal_extract_raw(frame->data, frame->dlc, &signal->spec, &raw_value) ||
        !dbc_decode_signal_value(signal, frame, &physical_value)) {
      continue;
    }
    if (signal_cache_upsert(cache,
                            message->name,
                            signal->name,
                            signal->unit,
                            physical_value,
                            raw_value,
                            now_ms)) {
      ++updated;
    }
  }
  return updated;
}

static const DbcSelectedRuntimeMessage *find_selected_message(
  const DbcSelectedRuntime *runtime,
  const CanFrame *frame,
  uint16_t *message_index) {
  if (runtime == NULL || frame == NULL || message_index == NULL ||
      (frame->ide != CAN_ID_STANDARD && frame->ide != CAN_ID_EXTENDED) ||
      (frame->ide == CAN_ID_STANDARD && frame->id > 0x7ffu) ||
      (frame->ide == CAN_ID_EXTENDED && frame->id > 0x1fffffffu) ||
      frame->dlc > CAN_FRAME_MAX_DATA_LEN) {
    return NULL;
  }
  const uint8_t frame_ide =
    frame->ide == CAN_ID_EXTENDED ? LARGE_DBC_SIGNAL_FLAG_IDE : 0u;
  for (uint16_t i = 0u; i < runtime->message_count; ++i) {
    const DbcSelectedRuntimeMessage *message = &runtime->messages[i];
    const uint8_t message_ide =
      (uint8_t)(message->flags & LARGE_DBC_SIGNAL_FLAG_IDE);
    const bool fd_required =
      (message->flags & LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED) != 0u;
    if (message->normalized_id == frame->id && message_ide == frame_ide &&
        (!fd_required || frame->fd) &&
        message->declared_payload_length == frame->dlc) {
      *message_index = i;
      return message;
    }
  }
  return NULL;
}

static bool publish_selected_value(SignalValueState *state,
                                   double value,
                                   int64_t raw,
                                   uint32_t now_ms) {
  if (state == NULL) {
    return false;
  }
  uint32_t sequence = state->update_seq;
  if ((sequence & 1u) != 0u) {
    ++sequence;
  }
  state->update_seq = sequence + 1u;
  state->value = value;
  state->raw = raw;
  state->updated_ms = now_ms;
  state->quality = SIGNAL_VALUE_QUALITY_GOOD;
  state->update_seq = sequence + 2u;
  return true;
}

__attribute__((noinline)) size_t dbc_decode_frame_to_selected_values(
  DbcSelectedRuntimeSnapshot *snapshot,
  const CanFrame *frame,
  uint32_t now_ms) {
  const DbcSelectedRuntime *runtime =
    dbc_selected_runtime_active(snapshot);
  SignalValueState *value_slots =
    dbc_selected_runtime_active_value_slots(snapshot);
  if (runtime == NULL || value_slots == NULL || frame == NULL) {
    return 0u;
  }

  uint16_t message_index = 0u;
  const DbcSelectedRuntimeMessage *message =
    find_selected_message(runtime, frame, &message_index);
  if (message == NULL ||
      message->first_signal_index > runtime->signal_count ||
      message->signal_count >
        runtime->signal_count - message->first_signal_index) {
    return 0u;
  }

  size_t updated = 0u;
  const uint16_t end =
    (uint16_t)(message->first_signal_index + message->signal_count);
  for (uint16_t i = message->first_signal_index; i < end; ++i) {
    const DbcSelectedRuntimeSignal *signal = &runtime->signals[i];
    const uint8_t identity_flags =
      (uint8_t)(signal->flags &
                (LARGE_DBC_SIGNAL_FLAG_IDE |
                 LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED));
    const uint8_t message_identity_flags =
      (uint8_t)(message->flags &
                (LARGE_DBC_SIGNAL_FLAG_IDE |
                 LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED));
    if (signal->runtime_message_index != message_index ||
        signal->normalized_id != message->normalized_id ||
        identity_flags != message_identity_flags ||
        signal->declared_payload_length != message->declared_payload_length ||
        signal->value_state_index >= runtime->signal_count) {
      continue;
    }
    const SignalSpec spec = {
      .start_bit = signal->start_bit,
      .bit_length = signal->bit_length,
      .byte_order =
        (signal->flags & LARGE_DBC_SIGNAL_FLAG_MOTOROLA) != 0u ?
          SIGNAL_ENDIAN_MOTOROLA : SIGNAL_ENDIAN_INTEL,
      .is_signed =
        (signal->flags & LARGE_DBC_SIGNAL_FLAG_SIGNED) != 0u,
      .factor = signal->factor,
      .offset = signal->offset,
      .minimum = signal->minimum,
      .maximum = signal->maximum
    };
    int64_t raw = 0;
    if (!signal_extract_raw(frame->data, frame->dlc, &spec, &raw)) {
      continue;
    }
    const double value = (double)raw * signal->factor + signal->offset;
    if (publish_selected_value(&value_slots[signal->value_state_index],
                               value, raw, now_ms)) {
      ++updated;
    }
  }
  return updated;
}
