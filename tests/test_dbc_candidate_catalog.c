#include "dbc_candidate_catalog.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(condition) do { if (!(condition)) { \
  fprintf(stderr, "ASSERT failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
  return false; } } while (0)

#define MESSAGE_CAPACITY \
  (LARGE_DBC_CATALOG_MAX_MESSAGES * LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE)
#define SIGNAL_CAPACITY \
  (LARGE_DBC_CATALOG_MAX_SIGNALS * LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE)
#define INDEX_CAPACITY \
  (LARGE_DBC_INDEX_HEADER_SIZE + MESSAGE_CAPACITY + SIGNAL_CAPACITY)

typedef struct {
  uint8_t *data;
  uint32_t capacity;
  uint32_t size;
  uint32_t read_count;
  uint32_t fail_read_call;
} MemoryIo;

static uint8_t g_message_bytes[MESSAGE_CAPACITY];
static uint8_t g_signal_bytes[SIGNAL_CAPACITY];
static uint8_t g_index_bytes[INDEX_CAPACITY];
static MemoryIo g_message_memory;
static MemoryIo g_signal_memory;
static MemoryIo g_index_memory;
static DbcCatalogIndexBuilder g_builder;
static DbcCatalogIndexVerifyWorkspace g_verify_workspace;
static DbcCandidateCatalogWorkspace g_workspace;

static bool memory_read(void *context, uint32_t offset,
                        uint8_t *data, uint32_t size) {
  MemoryIo *memory = context;
  ++memory->read_count;
  if (memory->fail_read_call == memory->read_count || offset > memory->size ||
      size > memory->size - offset) {
    return false;
  }
  memcpy(data, memory->data + offset, size);
  return true;
}

static bool memory_write(void *context, uint32_t offset,
                         const uint8_t *data, uint32_t size) {
  MemoryIo *memory = context;
  if (offset > memory->capacity || size > memory->capacity - offset) {
    return false;
  }
  memcpy(memory->data + offset, data, size);
  if (offset + size > memory->size) {
    memory->size = offset + size;
  }
  return true;
}

static bool memory_resize(void *context, uint32_t size) {
  MemoryIo *memory = context;
  if (size > memory->capacity) {
    return false;
  }
  memory->size = size;
  return true;
}

static bool memory_size(void *context, uint32_t *size) {
  const MemoryIo *memory = context;
  *size = memory->size;
  return true;
}

static DbcCatalogIndexIo memory_io(MemoryIo *memory) {
  const DbcCatalogIndexIo io = {
    .context = memory,
    .read_at = memory_read,
    .write_at = memory_write,
    .resize = memory_resize,
    .get_size = memory_size
  };
  return io;
}

static void reset_memory(void) {
  g_message_memory = (MemoryIo){g_message_bytes, sizeof(g_message_bytes), 0u, 0u, 0u};
  g_signal_memory = (MemoryIo){g_signal_bytes, sizeof(g_signal_bytes), 0u, 0u, 0u};
  g_index_memory = (MemoryIo){g_index_bytes, sizeof(g_index_bytes), 0u, 0u, 0u};
}

static bool build_index(uint16_t message_count, uint16_t signals_per_message,
                        DbcCatalogIndexSummary *summary) {
  reset_memory();
  const DbcCatalogIndexIo message_io = memory_io(&g_message_memory);
  const DbcCatalogIndexIo signal_io = memory_io(&g_signal_memory);
  const DbcCatalogIndexIo index_io = memory_io(&g_index_memory);
  ASSERT_TRUE(dbc_catalog_index_builder_init(&g_builder, &message_io, &signal_io,
                                             1000u, UINT32_C(0x12345678)) ==
              DBC_CATALOG_INDEX_OK);
  uint16_t global_signal = 0u;
  for (uint16_t message_ordinal = 0u; message_ordinal < message_count;
       ++message_ordinal) {
    const DbcCatalogIndexMessageInput message = {
      .normalized_id = (uint32_t)(256u + message_ordinal),
      .source_line = (uint32_t)message_ordinal + 1u,
      .declared_payload_length = 8u,
      .ide = false
    };
    ASSERT_TRUE(dbc_catalog_index_builder_begin_message(&g_builder, &message) ==
                DBC_CATALOG_INDEX_OK);
    for (uint16_t local = 0u; local < signals_per_message; ++local) {
      char key[LARGE_DBC_SIGNAL_RECORD_KEY_BYTES];
      const int key_length = snprintf(key, sizeof(key), "Msg%03u.Signal%03u",
                                      (unsigned)message_ordinal,
                                      (unsigned)global_signal);
      ASSERT_TRUE(key_length > 0 && (size_t)key_length < sizeof(key));
      DbcCatalogIndexSignalInput signal = {
        .start_bit = (uint16_t)(local * 8u),
        .bit_length = 8u,
        .motorola = false,
        .is_signed = false,
        .factor = 1.0,
        .offset = 0.0,
        .minimum = 0.0,
        .maximum = 255.0,
        .source_line = (uint32_t)global_signal + 100u,
        .source_offset = (uint32_t)global_signal * 50u,
        .key = key,
        .key_length = (uint8_t)key_length,
        .unit = "u",
        .unit_length = 1u
      };
      signal.definition_hash = dbc_catalog_definition_hash_v1(&message, &signal);
      ASSERT_TRUE(dbc_catalog_index_builder_add_signal(&g_builder, &signal) ==
                  DBC_CATALOG_INDEX_OK);
      ++global_signal;
    }
  }
  ASSERT_TRUE(dbc_catalog_index_builder_finalize(&g_builder, &index_io, summary) ==
              DBC_CATALOG_INDEX_OK);
  const DbcCatalogIndexExpectedSource expected = {
    .enabled = true,
    .source_size = 1000u,
    .source_crc32 = UINT32_C(0x12345678)
  };
  DbcCatalogIndexSummary verified;
  ASSERT_TRUE(dbc_catalog_index_verify(&index_io, &expected,
                                        &g_verify_workspace, &verified) ==
              DBC_CATALOG_INDEX_OK);
  ASSERT_TRUE(memcmp(summary, &verified, sizeof(verified)) == 0);
  g_index_memory.read_count = 0u;
  return true;
}

static void make_selection(DbcSelectionV1 *selection,
                           const DbcCatalogIndexSummary *summary,
                           uint64_t generation,
                           const uint16_t *ordinals,
                           size_t ordinal_count) {
  memset(selection, 0, sizeof(*selection));
  selection->candidate_generation = generation;
  selection->selection_generation = generation;
  selection->source_size = summary->source_size;
  selection->source_crc32 = summary->source_crc32;
  selection->catalog_signal_count = summary->signal_count;
  selection->selected_count = (uint16_t)ordinal_count;
  for (size_t i = 0u; i < ordinal_count; ++i) {
    selection->bitmap[ordinals[i] >> 3u] |=
      (uint8_t)(1u << (ordinals[i] & 7u));
  }
}

static bool make_manifest(const DbcCatalogIndexSummary *summary,
                          const DbcSelectionV1 *selection,
                          uint16_t selected_messages,
                          DbcManifestV1 *manifest) {
  uint8_t selection_bytes[LARGE_DBC_SELECTION_TOTAL_SIZE];
  ASSERT_TRUE(dbc_selection_v1_encode(selection, selection_bytes) ==
              DBC_CANDIDATE_FORMAT_OK);
  *manifest = (DbcManifestV1){
    .object_kind = LARGE_DBC_MANIFEST_OBJECT_CANDIDATE,
    .generation = selection->candidate_generation,
    .source_size = summary->source_size,
    .source_crc32 = summary->source_crc32,
    .index_size = summary->total_size,
    .index_crc32 = dbc_candidate_crc32(g_index_bytes, g_index_memory.size),
    .selection_size = LARGE_DBC_SELECTION_TOTAL_SIZE,
    .selection_crc32 = dbc_candidate_crc32(selection_bytes,
                                            sizeof(selection_bytes)),
    .selected_count = selection->selected_count,
    .selected_message_count = selected_messages,
    .catalog_message_count = summary->message_count,
    .catalog_signal_count = summary->signal_count
  };
  return true;
}

static bool test_query_search_filter_and_pages(void) {
  DbcCatalogIndexSummary summary;
  ASSERT_TRUE(build_index(4u, 3u, &summary));
  static const uint16_t selected_ordinals[] = {0u, 2u, 5u, 9u};
  DbcSelectionV1 selection;
  make_selection(&selection, &summary, 17u, selected_ordinals,
                 sizeof(selected_ordinals) / sizeof(selected_ordinals[0]));
  const DbcCatalogIndexIo index_io = memory_io(&g_index_memory);
  DbcCandidateCatalogPage page;
  DbcCandidateCatalogQuery query = {
    .page = 0u,
    .page_size = 2u,
    .query = "mSG001",
    .query_length = 6u,
    .selected_filter = DBC_CANDIDATE_FILTER_ALL
  };
  ASSERT_TRUE(dbc_candidate_catalog_query(&index_io, &summary, &selection,
                                           &query, &g_workspace, &page) ==
              DBC_CANDIDATE_CATALOG_OK);
  ASSERT_TRUE(page.catalog_total == 12u && page.matched_total == 3u &&
              page.item_count == 2u && page.has_more);
  ASSERT_TRUE(page.items[0].ordinal == 3u && page.items[1].ordinal == 4u);
  ASSERT_TRUE(strcmp(page.items[0].key, "Msg001.Signal003") == 0);
  ASSERT_TRUE(g_index_memory.read_count == summary.signal_count);

  g_index_memory.read_count = 0u;
  query.page = 1u;
  ASSERT_TRUE(dbc_candidate_catalog_query(&index_io, &summary, &selection,
                                           &query, &g_workspace, &page) ==
              DBC_CANDIDATE_CATALOG_OK);
  ASSERT_TRUE(page.item_count == 1u && page.items[0].ordinal == 5u &&
              !page.has_more);

  g_index_memory.read_count = 0u;
  query = (DbcCandidateCatalogQuery){
    .page = 0u,
    .page_size = 8u,
    .query = NULL,
    .query_length = 0u,
    .selected_filter = DBC_CANDIDATE_FILTER_SELECTED
  };
  ASSERT_TRUE(dbc_candidate_catalog_query(&index_io, &summary, &selection,
                                           &query, &g_workspace, &page) ==
              DBC_CANDIDATE_CATALOG_OK);
  ASSERT_TRUE(page.matched_total == 4u && page.item_count == 4u);
  ASSERT_TRUE(page.items[0].ordinal == 0u && page.items[3].ordinal == 9u);
  ASSERT_TRUE(g_index_memory.read_count == 4u);

  g_index_memory.read_count = 0u;
  query = (DbcCandidateCatalogQuery){
    .page = 1u,
    .page_size = 2u,
    .query = NULL,
    .query_length = 0u,
    .selected_filter = DBC_CANDIDATE_FILTER_ALL
  };
  ASSERT_TRUE(dbc_candidate_catalog_query(&index_io, &summary, &selection,
                                           &query, &g_workspace, &page) ==
              DBC_CANDIDATE_CATALOG_OK);
  ASSERT_TRUE(page.matched_total == summary.signal_count &&
              page.item_count == 2u && page.items[0].ordinal == 2u &&
              page.items[1].ordinal == 3u && page.has_more);
  ASSERT_TRUE(g_index_memory.read_count == 2u);

  g_index_memory.read_count = 0u;
  query = (DbcCandidateCatalogQuery){
    .page = 0u,
    .page_size = 8u,
    .query = NULL,
    .query_length = 0u,
    .selected_filter = DBC_CANDIDATE_FILTER_SELECTED
  };
  ASSERT_TRUE(dbc_candidate_catalog_query(&index_io, &summary, &selection,
                                           &query, &g_workspace, &page) ==
              DBC_CANDIDATE_CATALOG_OK);
  ASSERT_TRUE(page.matched_total == 4u && page.item_count == 4u);
  ASSERT_TRUE(g_index_memory.read_count == 4u);

  query.page = 99u;
  ASSERT_TRUE(dbc_candidate_catalog_query(&index_io, &summary, &selection,
                                           &query, &g_workspace, &page) ==
              DBC_CANDIDATE_CATALOG_OK);
  ASSERT_TRUE(page.matched_total == 4u && page.item_count == 0u &&
              !page.has_more);
  return true;
}

static bool test_query_bounds_and_io_failure(void) {
  DbcCatalogIndexSummary summary;
  ASSERT_TRUE(build_index(2u, 2u, &summary));
  static const uint16_t selected[] = {0u};
  DbcSelectionV1 selection;
  make_selection(&selection, &summary, 1u, selected, 1u);
  const DbcCatalogIndexIo index_io = memory_io(&g_index_memory);
  DbcCandidateCatalogPage page;
  DbcCandidateCatalogQuery query = {
    .page_size = 9u,
    .selected_filter = DBC_CANDIDATE_FILTER_ALL
  };
  ASSERT_TRUE(dbc_candidate_catalog_query(&index_io, &summary, &selection,
                                           &query, &g_workspace, &page) ==
              DBC_CANDIDATE_CATALOG_INVALID_ARGUMENT);
  query.page_size = 8u;
  g_index_memory.read_count = 0u;
  g_index_memory.fail_read_call = 2u;
  ASSERT_TRUE(dbc_candidate_catalog_query(&index_io, &summary, &selection,
                                           &query, &g_workspace, &page) ==
              DBC_CANDIDATE_CATALOG_IO_FAILED);
  g_index_memory.fail_read_call = 0u;
  return true;
}

static bool prepare_context(uint16_t messages, uint16_t signals_per_message,
                            const uint16_t *selected, size_t selected_count,
                            uint16_t selected_messages,
                            uint64_t generation,
                            DbcCatalogIndexSummary *summary,
                            DbcSelectionV1 *selection,
                            DbcManifestV1 *manifest,
                            char token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES]) {
  ASSERT_TRUE(build_index(messages, signals_per_message, summary));
  make_selection(selection, summary, generation, selected, selected_count);
  ASSERT_TRUE(make_manifest(summary, selection, selected_messages, manifest));
  ASSERT_TRUE(dbc_candidate_token_format(generation, manifest->source_size,
                                          manifest->source_crc32, token) ==
              DBC_CANDIDATE_FORMAT_OK);
  return true;
}

static bool test_selection_update_success_and_token(void) {
  DbcCatalogIndexSummary summary;
  DbcSelectionV1 selection;
  DbcManifestV1 manifest;
  char token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES];
  static const uint16_t selected[] = {0u, 1u};
  ASSERT_TRUE(prepare_context(4u, 3u, selected, 2u, 1u, 17u,
                              &summary, &selection, &manifest, token));
  static const uint16_t set[] = {2u};
  static const uint16_t clear[] = {0u};
  const DbcCandidateSelectionMutation mutation = {
    .candidate_token = token,
    .set_ordinals = set,
    .set_count = 1u,
    .clear_ordinals = clear,
    .clear_count = 1u,
    .writes_blocked = false
  };
  const DbcCatalogIndexIo index_io = memory_io(&g_index_memory);
  DbcCandidateSelectionUpdate update;
  ASSERT_TRUE(dbc_candidate_selection_prepare_update(
                &index_io, &summary, &manifest, &selection, &mutation,
                &g_workspace, &update) == DBC_CANDIDATE_CATALOG_OK);
  ASSERT_TRUE(update.manifest.generation == 18u &&
              update.selection.candidate_generation == 18u &&
              update.selection.selection_generation == 18u);
  ASSERT_TRUE(update.selection.selected_count == 2u &&
              !dbc_selection_v1_is_selected(&update.selection, 0u) &&
              dbc_selection_v1_is_selected(&update.selection, 1u) &&
              dbc_selection_v1_is_selected(&update.selection, 2u));
  ASSERT_TRUE(update.manifest.selected_message_count == 1u);
  ASSERT_TRUE(dbc_candidate_token_verify_manifest(update.candidate_token,
                                                   &update.manifest) ==
              DBC_CANDIDATE_FORMAT_OK);
  ASSERT_TRUE(selection.candidate_generation == 17u && selection.selected_count == 2u &&
              dbc_selection_v1_is_selected(&selection, 0u));

  char wrong_token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES];
  ASSERT_TRUE(dbc_candidate_token_format(17u, manifest.source_size + 1u,
                                          manifest.source_crc32, wrong_token) ==
              DBC_CANDIDATE_FORMAT_OK);
  DbcCandidateSelectionMutation wrong = mutation;
  wrong.candidate_token = wrong_token;
  ASSERT_TRUE(dbc_candidate_selection_prepare_update(
                &index_io, &summary, &manifest, &selection, &wrong,
                &g_workspace, &update) ==
              DBC_CANDIDATE_CATALOG_TOKEN_MISMATCH);
  wrong = mutation;
  wrong.writes_blocked = true;
  ASSERT_TRUE(dbc_candidate_selection_prepare_update(
                &index_io, &summary, &manifest, &selection, &wrong,
                &g_workspace, &update) ==
              DBC_CANDIDATE_CATALOG_WRITE_BLOCKED);
  return true;
}

static bool test_selection_mutation_rejections(void) {
  DbcCatalogIndexSummary summary;
  DbcSelectionV1 selection;
  DbcManifestV1 manifest;
  char token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES];
  static const uint16_t selected[] = {0u, 1u};
  ASSERT_TRUE(prepare_context(4u, 3u, selected, 2u, 1u, 17u,
                              &summary, &selection, &manifest, token));
  const DbcCatalogIndexIo index_io = memory_io(&g_index_memory);
  DbcCandidateSelectionUpdate update;
  update.manifest.generation = UINT64_C(0xAAAAAAAAAAAAAAAA);

  DbcCandidateSelectionMutation mutation = {
    .candidate_token = token
  };
  ASSERT_TRUE(dbc_candidate_selection_prepare_update(
                &index_io, &summary, &manifest, &selection, &mutation,
                &g_workspace, &update) ==
              DBC_CANDIDATE_CATALOG_INVALID_ARGUMENT);

  static const uint16_t duplicate[] = {2u, 2u};
  mutation = (DbcCandidateSelectionMutation){
    .candidate_token = token,
    .set_ordinals = duplicate,
    .set_count = 2u
  };
  ASSERT_TRUE(dbc_candidate_selection_prepare_update(
                &index_io, &summary, &manifest, &selection, &mutation,
                &g_workspace, &update) ==
              DBC_CANDIDATE_CATALOG_MUTATION_CONFLICT);
  ASSERT_TRUE(update.manifest.generation == UINT64_C(0xAAAAAAAAAAAAAAAA));

  static const uint16_t overlap[] = {1u};
  mutation = (DbcCandidateSelectionMutation){token, overlap, 1u, overlap, 1u, false};
  ASSERT_TRUE(dbc_candidate_selection_prepare_update(
                &index_io, &summary, &manifest, &selection, &mutation,
                &g_workspace, &update) ==
              DBC_CANDIDATE_CATALOG_MUTATION_CONFLICT);
  static const uint16_t out_of_range[] = {12u};
  mutation = (DbcCandidateSelectionMutation){token, out_of_range, 1u,
                                              NULL, 0u, false};
  ASSERT_TRUE(dbc_candidate_selection_prepare_update(
                &index_io, &summary, &manifest, &selection, &mutation,
                &g_workspace, &update) ==
              DBC_CANDIDATE_CATALOG_ORDINAL_OUT_OF_RANGE);
  uint16_t too_many[LARGE_DBC_SELECTION_UPDATE_MAX_ORDINALS + 1u];
  for (size_t i = 0u; i < sizeof(too_many) / sizeof(too_many[0]); ++i) {
    too_many[i] = (uint16_t)i;
  }
  mutation = (DbcCandidateSelectionMutation){token, too_many,
    sizeof(too_many) / sizeof(too_many[0]), NULL, 0u, false};
  ASSERT_TRUE(dbc_candidate_selection_prepare_update(
                &index_io, &summary, &manifest, &selection, &mutation,
                &g_workspace, &update) ==
              DBC_CANDIDATE_CATALOG_INVALID_ARGUMENT);
  return true;
}

static bool test_signal_and_message_limits(void) {
  DbcCatalogIndexSummary summary;
  DbcSelectionV1 selection;
  DbcManifestV1 manifest;
  char token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES];
  uint16_t selected[128];
  for (uint16_t i = 0u; i < 128u; ++i) {
    selected[i] = i;
  }
  ASSERT_TRUE(prepare_context(65u, 2u, selected, 128u, 64u, 33u,
                              &summary, &selection, &manifest, token));
  const DbcCatalogIndexIo index_io = memory_io(&g_index_memory);
  DbcCandidateSelectionUpdate update;
  static const uint16_t keep_128[] = {0u};
  DbcCandidateSelectionMutation mutation = {
    token, keep_128, 1u, NULL, 0u, false
  };
  ASSERT_TRUE(dbc_candidate_selection_prepare_update(
                &index_io, &summary, &manifest, &selection, &mutation,
                &g_workspace, &update) == DBC_CANDIDATE_CATALOG_OK);
  ASSERT_TRUE(update.selection.selected_count == 128u &&
              update.manifest.selected_message_count == 64u);
  static const uint16_t add_129[] = {128u};
  mutation = (DbcCandidateSelectionMutation){token, add_129, 1u,
                                              NULL, 0u, false};
  ASSERT_TRUE(dbc_candidate_selection_prepare_update(
                &index_io, &summary, &manifest, &selection, &mutation,
                &g_workspace, &update) ==
              DBC_CANDIDATE_CATALOG_SELECTION_LIMIT);

  uint16_t one_per_message[64];
  for (uint16_t i = 0u; i < 64u; ++i) {
    one_per_message[i] = (uint16_t)(i * 2u);
  }
  make_selection(&selection, &summary, 34u, one_per_message, 64u);
  ASSERT_TRUE(make_manifest(&summary, &selection, 64u, &manifest));
  ASSERT_TRUE(dbc_candidate_token_format(34u, manifest.source_size,
                                          manifest.source_crc32, token) ==
              DBC_CANDIDATE_FORMAT_OK);
  mutation = (DbcCandidateSelectionMutation){token, add_129, 1u, NULL, 0u, false};
  ASSERT_TRUE(dbc_candidate_selection_prepare_update(
                &index_io, &summary, &manifest, &selection, &mutation,
                &g_workspace, &update) ==
              DBC_CANDIDATE_CATALOG_MESSAGE_LIMIT);
  return true;
}

static bool test_selection_clear_last_to_zero(void) {
  DbcCatalogIndexSummary summary;
  DbcSelectionV1 selection;
  DbcManifestV1 manifest;
  char token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES];
  static const uint16_t selected[] = {0u};
  ASSERT_TRUE(prepare_context(2u, 2u, selected, 1u, 1u, 17u,
                              &summary, &selection, &manifest, token));
  const DbcCatalogIndexIo index_io = memory_io(&g_index_memory);
  const DbcCandidateSelectionMutation keep_one = {
    token, selected, 1u, NULL, 0u, false
  };
  DbcCandidateSelectionUpdate update;
  ASSERT_TRUE(dbc_candidate_selection_prepare_update(
                &index_io, &summary, &manifest, &selection, &keep_one,
                &g_workspace, &update) == DBC_CANDIDATE_CATALOG_OK);
  ASSERT_TRUE(update.selection.selected_count == 1u &&
              update.manifest.selected_message_count == 1u);
  const DbcCandidateSelectionMutation mutation = {
    token, NULL, 0u, selected, 1u, false
  };
  ASSERT_TRUE(dbc_candidate_selection_prepare_update(
                &index_io, &summary, &manifest, &selection, &mutation,
                &g_workspace, &update) == DBC_CANDIDATE_CATALOG_OK);
  ASSERT_TRUE(update.manifest.generation == 18u &&
              update.manifest.selected_count == 0u &&
              update.manifest.selected_message_count == 0u &&
              update.selection.candidate_generation == 18u &&
              update.selection.selection_generation == 18u &&
              update.selection.selected_count == 0u &&
              !dbc_selection_v1_is_selected(&update.selection, 0u));
  ASSERT_TRUE(dbc_candidate_token_verify_manifest(update.candidate_token,
                                                   &update.manifest) ==
              DBC_CANDIDATE_FORMAT_OK);
  uint8_t selection_bytes[LARGE_DBC_SELECTION_TOTAL_SIZE];
  ASSERT_TRUE(dbc_selection_v1_encode(&update.selection, selection_bytes) ==
              DBC_CANDIDATE_FORMAT_OK);
  ASSERT_TRUE(dbc_candidate_crc32(selection_bytes, sizeof(selection_bytes)) ==
              update.manifest.selection_crc32);
  const DbcCandidateIndexFacts index_facts = {
    .source_size = update.manifest.source_size,
    .source_crc32 = update.manifest.source_crc32,
    .index_size = update.manifest.index_size,
    .index_crc32 = update.manifest.index_crc32,
    .catalog_message_count = update.manifest.catalog_message_count,
    .catalog_signal_count = update.manifest.catalog_signal_count
  };
  ASSERT_TRUE(dbc_candidate_v1_verify_set(
                &update.manifest, &index_facts, selection_bytes,
                sizeof(selection_bytes), 0u, NULL) == DBC_CANDIDATE_FORMAT_OK);
  ASSERT_TRUE(selection.candidate_generation == 17u &&
              selection.selected_count == 1u &&
              dbc_selection_v1_is_selected(&selection, 0u));
  return true;
}

static bool test_generation_exhausted_and_io_failure(void) {
  DbcCatalogIndexSummary summary;
  DbcSelectionV1 selection;
  DbcManifestV1 manifest;
  char token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES];
  static const uint16_t selected[] = {0u};
  ASSERT_TRUE(prepare_context(2u, 2u, selected, 1u, 1u, UINT64_MAX,
                              &summary, &selection, &manifest, token));
  const DbcCatalogIndexIo index_io = memory_io(&g_index_memory);
  DbcCandidateSelectionUpdate update;
  static const uint16_t set_existing[] = {0u};
  const DbcCandidateSelectionMutation mutation = {
    token, set_existing, 1u, NULL, 0u, false
  };
  ASSERT_TRUE(dbc_candidate_selection_prepare_update(
                &index_io, &summary, &manifest, &selection, &mutation,
                &g_workspace, &update) ==
              DBC_CANDIDATE_CATALOG_GENERATION_EXHAUSTED);

  ASSERT_TRUE(dbc_candidate_token_format(manifest.generation,
                                          manifest.source_size,
                                          manifest.source_crc32, token) ==
              DBC_CANDIDATE_FORMAT_OK);
  g_index_memory.read_count = 0u;
  g_index_memory.fail_read_call = 1u;
  ASSERT_TRUE(dbc_candidate_selection_prepare_update(
                &index_io, &summary, &manifest, &selection, &mutation,
                &g_workspace, &update) == DBC_CANDIDATE_CATALOG_IO_FAILED);
  g_index_memory.fail_read_call = 0u;
  return true;
}

int main(void) {
  if (!test_query_search_filter_and_pages() ||
      !test_query_bounds_and_io_failure() ||
      !test_selection_update_success_and_token() ||
      !test_selection_mutation_rejections() ||
      !test_selection_clear_last_to_zero() ||
      !test_signal_and_message_limits() ||
      !test_generation_exhausted_and_io_failure()) {
    return 1;
  }
  puts("dbc candidate catalog tests passed");
  return 0;
}
