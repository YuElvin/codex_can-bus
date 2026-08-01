#include "dbc_selected_runtime.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(condition)                                                   \
  do {                                                                           \
    if (!(condition)) {                                                          \
      fprintf(stderr, "assertion failed: %s:%d: %s\n", __FILE__, __LINE__,     \
              #condition);                                                       \
      return false;                                                              \
    }                                                                            \
  } while (0)

#define ASSERT_STATUS(actual, expected) ASSERT_TRUE((actual) == (expected))

#define TEST_MAX_MESSAGES 128u
#define TEST_MAX_SIGNALS 256u
#define TEST_MESSAGE_BYTES \
  (TEST_MAX_MESSAGES * LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE)
#define TEST_SIGNAL_BYTES \
  (TEST_MAX_SIGNALS * LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE)
#define TEST_INDEX_BYTES \
  (LARGE_DBC_INDEX_HEADER_SIZE + TEST_MESSAGE_BYTES + TEST_SIGNAL_BYTES)

typedef struct {
  uint8_t *data;
  uint32_t size;
  uint32_t capacity;
  bool fail_reads;
} MemoryFile;

static uint8_t g_message_bytes[TEST_MESSAGE_BYTES];
static uint8_t g_signal_bytes[TEST_SIGNAL_BYTES];
static uint8_t g_index_bytes[TEST_INDEX_BYTES];
static MemoryFile g_message_file = {
  g_message_bytes, 0u, sizeof(g_message_bytes), false
};
static MemoryFile g_signal_file = {
  g_signal_bytes, 0u, sizeof(g_signal_bytes), false
};
static MemoryFile g_index_file = {
  g_index_bytes, 0u, sizeof(g_index_bytes), false
};
static DbcCatalogIndexBuilder g_index_builder;
static DbcSelectedRuntimeSnapshot g_snapshot;
static DbcSelectedRuntime g_saved_active;

static bool memory_read(void *context,
                        uint32_t offset,
                        uint8_t *data,
                        uint32_t size) {
  MemoryFile *file = context;
  if (file->fail_reads || offset > file->size || size > file->size - offset) {
    return false;
  }
  memcpy(data, file->data + offset, size);
  return true;
}

static bool memory_write(void *context,
                         uint32_t offset,
                         const uint8_t *data,
                         uint32_t size) {
  MemoryFile *file = context;
  if (offset > file->capacity || size > file->capacity - offset) {
    return false;
  }
  memcpy(file->data + offset, data, size);
  if (offset + size > file->size) {
    file->size = offset + size;
  }
  return true;
}

static bool memory_resize(void *context, uint32_t size) {
  MemoryFile *file = context;
  if (size > file->capacity) {
    return false;
  }
  if (size > file->size) {
    memset(file->data + file->size, 0, size - file->size);
  }
  file->size = size;
  return true;
}

static bool memory_size(void *context, uint32_t *size) {
  MemoryFile *file = context;
  *size = file->size;
  return true;
}

static DbcCatalogIndexIo memory_io(MemoryFile *file) {
  DbcCatalogIndexIo io = {
    .context = file,
    .read_at = memory_read,
    .write_at = memory_write,
    .resize = memory_resize,
    .get_size = memory_size
  };
  return io;
}

static bool begin_index(void) {
  g_message_file.size = 0u;
  g_signal_file.size = 0u;
  g_index_file.size = 0u;
  g_index_file.fail_reads = false;
  const DbcCatalogIndexIo messages = memory_io(&g_message_file);
  const DbcCatalogIndexIo signals = memory_io(&g_signal_file);
  ASSERT_STATUS(dbc_catalog_index_builder_init(&g_index_builder,
                                               &messages,
                                               &signals,
                                               100071u,
                                               UINT32_C(0x4B88D9CE)),
                DBC_CATALOG_INDEX_OK);
  return true;
}

static DbcCatalogIndexMessageInput message_input(uint16_t ordinal) {
  const DbcCatalogIndexMessageInput message = {
    .normalized_id = 0x100u + ordinal,
    .source_line = 2u + ordinal * 4u,
    .declared_payload_length = 8u,
    .ide = false
  };
  return message;
}

static DbcCatalogIndexSignalInput signal_input(
  const DbcCatalogIndexMessageInput *message,
  const char *key,
  uint16_t start_bit,
  double factor) {
  DbcCatalogIndexSignalInput signal = {
    .start_bit = start_bit,
    .bit_length = 8u,
    .motorola = false,
    .is_signed = false,
    .factor = factor,
    .offset = 0.0,
    .minimum = 0.0,
    .maximum = 255.0 * factor,
    .source_line = message->source_line + 1u,
    .source_offset = 20u,
    .key = key,
    .key_length = (uint8_t)strlen(key),
    .unit = "V",
    .unit_length = 1u
  };
  signal.definition_hash = dbc_catalog_definition_hash_v1(message, &signal);
  return signal;
}

static bool add_message_signals(uint16_t message_ordinal,
                                uint16_t signals_per_message,
                                double factor) {
  const DbcCatalogIndexMessageInput message = message_input(message_ordinal);
  ASSERT_STATUS(dbc_catalog_index_builder_begin_message(&g_index_builder, &message),
                DBC_CATALOG_INDEX_OK);
  char key[32];
  for (uint16_t signal_ordinal = 0u;
       signal_ordinal < signals_per_message;
       ++signal_ordinal) {
    const int length = snprintf(key,
                                sizeof(key),
                                "M%03u.S%u",
                                (unsigned)message_ordinal,
                                (unsigned)signal_ordinal);
    ASSERT_TRUE(length > 0 && (size_t)length < sizeof(key));
    DbcCatalogIndexSignalInput signal =
      signal_input(&message, key, (uint16_t)(signal_ordinal * 8u), factor);
    ASSERT_STATUS(dbc_catalog_index_builder_add_signal(&g_index_builder, &signal),
                  DBC_CATALOG_INDEX_OK);
  }
  return true;
}

static bool finish_index(DbcCatalogIndexSummary *summary) {
  const DbcCatalogIndexIo output = memory_io(&g_index_file);
  ASSERT_STATUS(dbc_catalog_index_builder_finalize(&g_index_builder,
                                                   &output,
                                                   summary),
                DBC_CATALOG_INDEX_OK);
  return true;
}

static bool selection_from_ordinals(DbcSelectionV1 *selection,
                                    uint16_t catalog_count,
                                    const uint16_t *ordinals,
                                    uint16_t count,
                                    uint64_t generation) {
  ASSERT_STATUS(dbc_selection_v1_init_default(selection,
                                              generation,
                                              generation,
                                              100071u,
                                              UINT32_C(0x4B88D9CE),
                                              catalog_count),
                DBC_CANDIDATE_FORMAT_OK);
  memset(selection->bitmap, 0, sizeof(selection->bitmap));
  selection->selected_count = count;
  for (uint16_t i = 0u; i < count; ++i) {
    ASSERT_TRUE(ordinals[i] < catalog_count);
    selection->bitmap[ordinals[i] >> 3u] |=
      (uint8_t)(1u << (ordinals[i] & 7u));
  }
  uint8_t encoded[LARGE_DBC_SELECTION_TOTAL_SIZE];
  ASSERT_STATUS(dbc_selection_v1_encode(selection, encoded),
                DBC_CANDIDATE_FORMAT_OK);
  return true;
}

static uint32_t selection_file_crc(const DbcSelectionV1 *selection) {
  uint8_t encoded[LARGE_DBC_SELECTION_TOTAL_SIZE];
  if (dbc_selection_v1_encode(selection, encoded) != DBC_CANDIDATE_FORMAT_OK) {
    return 0u;
  }
  return dbc_candidate_crc32(encoded, sizeof(encoded));
}

static DbcSelectedRuntimeBuildRequest build_request(
  const DbcCatalogIndexSummary *summary,
  const DbcSelectionV1 *selection,
  uint64_t runtime_generation,
  const DbcSelectedRuleRequirement *rules,
  size_t rule_count) {
  static DbcCatalogIndexIo index_io;
  index_io = memory_io(&g_index_file);
  const DbcSelectedRuntimeBuildRequest request = {
    .index = &index_io,
    .index_summary = summary,
    .selection = selection,
    .runtime_generation = runtime_generation,
    .selection_crc32 = selection_file_crc(selection),
    .rules = rules,
    .rule_count = rule_count
  };
  return request;
}

static bool test_selected_only_and_publish(void) {
  ASSERT_TRUE(begin_index());
  for (uint16_t message = 0u; message < 3u; ++message) {
    ASSERT_TRUE(add_message_signals(message, 2u, 1.0));
  }
  DbcCatalogIndexSummary summary;
  ASSERT_TRUE(finish_index(&summary));
  const uint16_t ordinals[] = {1u, 4u};
  DbcSelectionV1 selection;
  ASSERT_TRUE(selection_from_ordinals(&selection, 6u, ordinals, 2u, 17u));
  dbc_selected_runtime_snapshot_init(&g_snapshot);
  DbcSelectedRuntimeBuildRequest request =
    build_request(&summary, &selection, 100u, NULL, 0u);
  ASSERT_STATUS(dbc_selected_runtime_build_inactive(&g_snapshot, &request),
                DBC_SELECTED_RUNTIME_OK);
  ASSERT_TRUE(dbc_selected_runtime_active(&g_snapshot) == NULL);
  const DbcSelectedRuntime *prepared =
    dbc_selected_runtime_prepared(&g_snapshot);
  ASSERT_TRUE(prepared != NULL && prepared->signal_count == 2u &&
              prepared->message_count == 2u);
  ASSERT_TRUE(prepared->signals[0].catalog_ordinal == 1u &&
              prepared->signals[0].value_state_index == 0u &&
              strcmp(prepared->signals[0].key, "M000.S1") == 0);
  ASSERT_TRUE(prepared->signals[1].catalog_ordinal == 4u &&
              prepared->signals[1].value_state_index == 1u &&
              strcmp(prepared->signals[1].key, "M002.S0") == 0);
  ASSERT_TRUE(dbc_selected_runtime_find_signal(prepared, "M000.S0") == NULL);
  ASSERT_STATUS(dbc_selected_runtime_publish_prepared(&g_snapshot),
                DBC_SELECTED_RUNTIME_OK);
  const DbcSelectedRuntime *active = dbc_selected_runtime_active(&g_snapshot);
  ASSERT_TRUE(active != NULL && active->runtime_generation == 100u);
  const DbcSelectedRuleRequirement matching_rule = {
    .enabled = true,
    .key = "M000.S1",
    .definition_hash = active->signals[0].definition_hash
  };
  request.runtime_generation = 101u;
  request.rules = &matching_rule;
  request.rule_count = 1u;
  ASSERT_STATUS(dbc_selected_runtime_build_inactive(&g_snapshot, &request),
                DBC_SELECTED_RUNTIME_OK);
  ASSERT_TRUE(dbc_selected_runtime_active(&g_snapshot)->runtime_generation == 100u);
  ASSERT_TRUE(dbc_selected_runtime_prepared(&g_snapshot)->runtime_generation == 101u);
  ASSERT_STATUS(dbc_selected_runtime_publish_prepared(&g_snapshot),
                DBC_SELECTED_RUNTIME_OK);
  ASSERT_TRUE(dbc_selected_runtime_active(&g_snapshot)->runtime_generation == 101u);
  request.selection_crc32 ^= 1u;
  ASSERT_STATUS(dbc_selected_runtime_build_inactive(&g_snapshot, &request),
                DBC_SELECTED_RUNTIME_INVALID_SELECTION);
  ASSERT_TRUE(dbc_selected_runtime_active(&g_snapshot)->runtime_generation == 101u);
  ASSERT_STATUS(dbc_selected_runtime_publish_prepared(&g_snapshot),
                DBC_SELECTED_RUNTIME_NOT_PREPARED);
  return true;
}

static bool build_single_signal_catalog(double factor,
                                        DbcCatalogIndexSummary *summary,
                                        DbcSelectionV1 *selection) {
  ASSERT_TRUE(begin_index());
  ASSERT_TRUE(add_message_signals(0u, 1u, factor));
  ASSERT_TRUE(finish_index(summary));
  const uint16_t ordinal = 0u;
  ASSERT_TRUE(selection_from_ordinals(selection, 1u, &ordinal, 1u, 18u));
  return true;
}

static bool test_rule_conflict_preserves_active(void) {
  DbcCatalogIndexSummary summary;
  DbcSelectionV1 selection;
  ASSERT_TRUE(build_single_signal_catalog(1.0, &summary, &selection));
  DbcSelectedRuntimeBuildRequest request =
    build_request(&summary, &selection, 200u, NULL, 0u);
  dbc_selected_runtime_snapshot_init(&g_snapshot);
  ASSERT_STATUS(dbc_selected_runtime_build_inactive(&g_snapshot, &request),
                DBC_SELECTED_RUNTIME_OK);
  ASSERT_STATUS(dbc_selected_runtime_publish_prepared(&g_snapshot),
                DBC_SELECTED_RUNTIME_OK);
  const DbcSelectedRuntime *active = dbc_selected_runtime_active(&g_snapshot);
  ASSERT_TRUE(active != NULL);
  g_saved_active = *active;
  const uint64_t old_hash = active->signals[0].definition_hash;

  ASSERT_TRUE(build_single_signal_catalog(2.0, &summary, &selection));
  const DbcSelectedRuleRequirement rule = {
    .enabled = true,
    .key = "M000.S0",
    .definition_hash = old_hash
  };
  request = build_request(&summary, &selection, 201u, &rule, 1u);
  ASSERT_STATUS(dbc_selected_runtime_build_inactive(&g_snapshot, &request),
                DBC_SELECTED_RUNTIME_RULE_DEFINITION_CONFLICT);
  ASSERT_TRUE(dbc_selected_runtime_prepared(&g_snapshot) == NULL);
  active = dbc_selected_runtime_active(&g_snapshot);
  ASSERT_TRUE(active != NULL &&
              memcmp(active, &g_saved_active, sizeof(*active)) == 0);

  const DbcSelectedRuleRequirement missing = {
    .enabled = true,
    .key = "Missing.Signal",
    .definition_hash = old_hash
  };
  request = build_request(&summary, &selection, 202u, &missing, 1u);
  ASSERT_STATUS(dbc_selected_runtime_build_inactive(&g_snapshot, &request),
                DBC_SELECTED_RUNTIME_RULE_KEY_MISSING);
  active = dbc_selected_runtime_active(&g_snapshot);
  ASSERT_TRUE(active != NULL &&
              memcmp(active, &g_saved_active, sizeof(*active)) == 0);
  return true;
}

static bool test_128_signals_64_messages(void) {
  ASSERT_TRUE(begin_index());
  for (uint16_t message = 0u; message < 64u; ++message) {
    ASSERT_TRUE(add_message_signals(message, 2u, 1.0));
  }
  DbcCatalogIndexSummary summary;
  ASSERT_TRUE(finish_index(&summary));
  uint16_t ordinals[128];
  for (uint16_t i = 0u; i < 128u; ++i) {
    ordinals[i] = i;
  }
  DbcSelectionV1 selection;
  ASSERT_TRUE(selection_from_ordinals(&selection, 128u, ordinals, 128u, 20u));
  dbc_selected_runtime_snapshot_init(&g_snapshot);
  DbcSelectedRuntimeBuildRequest request =
    build_request(&summary, &selection, 300u, NULL, 0u);
  ASSERT_STATUS(dbc_selected_runtime_build_inactive(&g_snapshot, &request),
                DBC_SELECTED_RUNTIME_OK);
  const DbcSelectedRuntime *runtime =
    dbc_selected_runtime_prepared(&g_snapshot);
  ASSERT_TRUE(runtime != NULL && runtime->signal_count == 128u &&
              runtime->message_count == 64u);
  ASSERT_TRUE(runtime->signals[127].value_state_index == 127u &&
              runtime->signals[127].definition_hash != 0u);
  selection.selected_count = 129u;
  selection.bitmap[16] |= 1u;
  request = build_request(&summary, &selection, 301u, NULL, 0u);
  ASSERT_STATUS(dbc_selected_runtime_build_inactive(&g_snapshot, &request),
                DBC_SELECTED_RUNTIME_INVALID_SELECTION);
  return true;
}

static bool test_65_message_and_empty_rejection(void) {
  ASSERT_TRUE(begin_index());
  for (uint16_t message = 0u; message < 65u; ++message) {
    ASSERT_TRUE(add_message_signals(message, 1u, 1.0));
  }
  DbcCatalogIndexSummary summary;
  ASSERT_TRUE(finish_index(&summary));
  uint16_t ordinals[65];
  for (uint16_t i = 0u; i < 65u; ++i) {
    ordinals[i] = i;
  }
  DbcSelectionV1 selection;
  ASSERT_TRUE(selection_from_ordinals(&selection, 65u, ordinals, 65u, 21u));
  dbc_selected_runtime_snapshot_init(&g_snapshot);
  DbcSelectedRuntimeBuildRequest request =
    build_request(&summary, &selection, 301u, NULL, 0u);
  ASSERT_STATUS(dbc_selected_runtime_build_inactive(&g_snapshot, &request),
                DBC_SELECTED_RUNTIME_MESSAGE_LIMIT);

  ASSERT_STATUS(dbc_selection_v1_init_default(&selection,
                                              21u,
                                              21u,
                                              100071u,
                                              UINT32_C(0x4B88D9CE),
                                              129u),
                DBC_CANDIDATE_FORMAT_OK);
  request = build_request(&summary, &selection, 302u, NULL, 0u);
  ASSERT_STATUS(dbc_selected_runtime_build_inactive(&g_snapshot, &request),
                DBC_SELECTED_RUNTIME_INDEX_MISMATCH);

  ASSERT_TRUE(begin_index());
  ASSERT_TRUE(add_message_signals(0u, 2u, 1.0));
  ASSERT_TRUE(finish_index(&summary));
  ASSERT_STATUS(dbc_selection_v1_init_default(&selection,
                                              21u,
                                              21u,
                                              100071u,
                                              UINT32_C(0x4B88D9CE),
                                              2u),
                DBC_CANDIDATE_FORMAT_OK);
  memset(selection.bitmap, 0, sizeof(selection.bitmap));
  selection.selected_count = 0u;
  request = build_request(&summary, &selection, 303u, NULL, 0u);
  ASSERT_STATUS(dbc_selected_runtime_build_inactive(&g_snapshot, &request),
                DBC_SELECTED_RUNTIME_EMPTY_SELECTION);
  return true;
}

static bool test_index_io_failure_preserves_active(void) {
  DbcCatalogIndexSummary summary;
  DbcSelectionV1 selection;
  ASSERT_TRUE(build_single_signal_catalog(1.0, &summary, &selection));
  dbc_selected_runtime_snapshot_init(&g_snapshot);
  DbcSelectedRuntimeBuildRequest request =
    build_request(&summary, &selection, 400u, NULL, 0u);
  ASSERT_STATUS(dbc_selected_runtime_build_inactive(&g_snapshot, &request),
                DBC_SELECTED_RUNTIME_OK);
  ASSERT_STATUS(dbc_selected_runtime_publish_prepared(&g_snapshot),
                DBC_SELECTED_RUNTIME_OK);
  g_saved_active = *dbc_selected_runtime_active(&g_snapshot);
  g_index_file.fail_reads = true;
  request.runtime_generation = 401u;
  ASSERT_STATUS(dbc_selected_runtime_build_inactive(&g_snapshot, &request),
                DBC_SELECTED_RUNTIME_INDEX_ERROR);
  ASSERT_TRUE(memcmp(dbc_selected_runtime_active(&g_snapshot),
                     &g_saved_active,
                     sizeof(g_saved_active)) == 0);
  g_index_file.fail_reads = false;
  return true;
}

int main(void) {
  ASSERT_TRUE(sizeof(DbcSelectedRuntimeSnapshot) <= 48u * 1024u);
  ASSERT_TRUE(test_selected_only_and_publish());
  ASSERT_TRUE(test_rule_conflict_preserves_active());
  ASSERT_TRUE(test_128_signals_64_messages());
  ASSERT_TRUE(test_65_message_and_empty_rejection());
  ASSERT_TRUE(test_index_io_failure_preserves_active());
  puts("DBC selected-only runtime tests passed");
  return 0;
}
