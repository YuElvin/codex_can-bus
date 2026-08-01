#include "dbc_candidate_format.h"

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

static void put_u16(uint8_t *data, uint16_t value) {
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8u);
}

static void put_u32(uint8_t *data, uint32_t value) {
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8u);
  data[2] = (uint8_t)(value >> 16u);
  data[3] = (uint8_t)(value >> 24u);
}

static void put_u64(uint8_t *data, uint64_t value) {
  put_u32(data, (uint32_t)value);
  put_u32(data + 4u, (uint32_t)(value >> 32u));
}

static void repair_manifest_crc(uint8_t *bytes) {
  put_u32(bytes + LARGE_DBC_MANIFEST_CRC32_OFFSET,
          dbc_candidate_crc32(bytes, LARGE_DBC_MANIFEST_CRC32_COVERED_BYTES));
}

static void repair_selection_header_crc(uint8_t *bytes) {
  put_u32(bytes + LARGE_DBC_SELECTION_HEADER_CRC32_OFFSET,
          dbc_candidate_crc32(bytes,
                              LARGE_DBC_SELECTION_HEADER_CRC32_COVERED_BYTES));
}

static uint32_t catalog_index_size(uint16_t messages, uint16_t signals) {
  return LARGE_DBC_INDEX_HEADER_SIZE +
         (uint32_t)messages * LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE +
         (uint32_t)signals * LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE;
}

static DbcManifestV1 candidate_manifest_zero(void) {
  DbcManifestV1 manifest = {
    .object_kind = LARGE_DBC_MANIFEST_OBJECT_CANDIDATE,
    .generation = UINT64_C(0x11),
    .source_size = 100071u,
    .source_crc32 = UINT32_C(0x4B88D9CE),
    .index_size = 145232u,
    .index_crc32 = UINT32_C(0x12345678),
    .selection_size = LARGE_DBC_SELECTION_TOTAL_SIZE,
    .selection_crc32 = UINT32_C(0x89ABCDEF),
    .selected_count = 0u,
    .selected_message_count = 0u,
    .catalog_message_count = 112u,
    .catalog_signal_count = 896u
  };
  return manifest;
}

static DbcManifestReferenceFacts facts_from_manifest(const DbcManifestV1 *manifest) {
  DbcManifestReferenceFacts facts = {
    .generation = manifest->generation,
    .source_size = manifest->source_size,
    .source_crc32 = manifest->source_crc32,
    .index_size = manifest->index_size,
    .index_crc32 = manifest->index_crc32,
    .selection_size = manifest->selection_size,
    .selection_crc32 = manifest->selection_crc32,
    .selected_count = manifest->selected_count,
    .selected_message_count = manifest->selected_message_count,
    .catalog_message_count = manifest->catalog_message_count,
    .catalog_signal_count = manifest->catalog_signal_count
  };
  return facts;
}

static bool test_crc_and_manifest_golden(void) {
  static const uint8_t check[] = "123456789";
  ASSERT_TRUE(dbc_candidate_crc32(check, sizeof(check) - 1u) ==
              UINT32_C(0xCBF43926));
  DbcManifestV1 manifest = candidate_manifest_zero();
  uint8_t bytes[LARGE_DBC_MANIFEST_SIZE];
  ASSERT_STATUS(dbc_manifest_v1_encode(&manifest, bytes),
                DBC_CANDIDATE_FORMAT_OK);
  static const uint8_t golden_prefix[20] = {
    0x44, 0x42, 0x43, 0x4D, 0x01, 0x00, 0x40, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
  };
  ASSERT_TRUE(memcmp(bytes, golden_prefix, sizeof(golden_prefix)) == 0);
  ASSERT_TRUE(dbc_candidate_crc32(bytes, sizeof(bytes)) == UINT32_C(0x2144DF1C));
  DbcManifestV1 decoded;
  ASSERT_STATUS(dbc_manifest_v1_decode(bytes, sizeof(bytes), &decoded),
                DBC_CANDIDATE_FORMAT_OK);
  const DbcManifestReferenceFacts facts = facts_from_manifest(&manifest);
  ASSERT_TRUE(decoded.object_kind == manifest.object_kind);
  ASSERT_STATUS(dbc_manifest_v1_verify_references(&decoded, &facts),
                DBC_CANDIDATE_FORMAT_OK);
  return true;
}

static bool test_manifest_damage_and_limits(void) {
  DbcManifestV1 manifest = candidate_manifest_zero();
  uint8_t good[LARGE_DBC_MANIFEST_SIZE];
  uint8_t bytes[LARGE_DBC_MANIFEST_SIZE];
  ASSERT_STATUS(dbc_manifest_v1_encode(&manifest, good), DBC_CANDIDATE_FORMAT_OK);
  DbcManifestV1 decoded;
  ASSERT_STATUS(dbc_manifest_v1_decode(good, sizeof(good) - 1u, &decoded),
                DBC_CANDIDATE_FORMAT_INVALID_FORMAT);
  memcpy(bytes, good, sizeof(bytes));
  bytes[LARGE_DBC_MANIFEST_SOURCE_CRC32_OFFSET] ^= 1u;
  ASSERT_STATUS(dbc_manifest_v1_decode(bytes, sizeof(bytes), &decoded),
                DBC_CANDIDATE_FORMAT_CRC_MISMATCH);

  static const size_t corrupt_offsets[] = {
    LARGE_DBC_MANIFEST_MAGIC_OFFSET,
    LARGE_DBC_MANIFEST_VERSION_OFFSET,
    LARGE_DBC_MANIFEST_HEADER_SIZE_OFFSET,
    LARGE_DBC_MANIFEST_FLAGS_OFFSET,
    10u,
    52u
  };
  for (size_t i = 0u; i < sizeof(corrupt_offsets) / sizeof(corrupt_offsets[0]); ++i) {
    memcpy(bytes, good, sizeof(bytes));
    bytes[corrupt_offsets[i]] ^= 1u;
    repair_manifest_crc(bytes);
    ASSERT_STATUS(dbc_manifest_v1_decode(bytes, sizeof(bytes), &decoded),
                  DBC_CANDIDATE_FORMAT_INVALID_FORMAT);
  }

  memcpy(bytes, good, sizeof(bytes));
  put_u64(bytes + LARGE_DBC_MANIFEST_GENERATION_OFFSET, 0u);
  repair_manifest_crc(bytes);
  ASSERT_STATUS(dbc_manifest_v1_decode(bytes, sizeof(bytes), &decoded),
                DBC_CANDIDATE_FORMAT_INVALID_FORMAT);
  memcpy(bytes, good, sizeof(bytes));
  put_u32(bytes + LARGE_DBC_MANIFEST_INDEX_SIZE_OFFSET, manifest.index_size + 1u);
  repair_manifest_crc(bytes);
  ASSERT_STATUS(dbc_manifest_v1_decode(bytes, sizeof(bytes), &decoded),
                DBC_CANDIDATE_FORMAT_INVALID_FORMAT);
  memcpy(bytes, good, sizeof(bytes));
  put_u32(bytes + LARGE_DBC_MANIFEST_SELECTION_SIZE_OFFSET,
          LARGE_DBC_SELECTION_TOTAL_SIZE - 1u);
  repair_manifest_crc(bytes);
  ASSERT_STATUS(dbc_manifest_v1_decode(bytes, sizeof(bytes), &decoded),
                DBC_CANDIDATE_FORMAT_INVALID_FORMAT);

  manifest.selected_count = 129u;
  manifest.selected_message_count = 1u;
  ASSERT_STATUS(dbc_manifest_v1_encode(&manifest, bytes),
                DBC_CANDIDATE_FORMAT_LIMIT_EXCEEDED);
  manifest = candidate_manifest_zero();
  manifest.selected_count = 1u;
  ASSERT_STATUS(dbc_manifest_v1_encode(&manifest, bytes),
                DBC_CANDIDATE_FORMAT_INVALID_FORMAT);
  manifest.selected_message_count = 2u;
  ASSERT_STATUS(dbc_manifest_v1_encode(&manifest, bytes),
                DBC_CANDIDATE_FORMAT_LIMIT_EXCEEDED);
  manifest.selected_message_count = 0u;
  manifest.object_kind = LARGE_DBC_MANIFEST_OBJECT_ACTIVE;
  ASSERT_STATUS(dbc_manifest_v1_encode(&manifest, bytes),
                DBC_CANDIDATE_FORMAT_INVALID_FORMAT);
  manifest.selected_count = 0u;
  ASSERT_STATUS(dbc_manifest_v1_encode(&manifest, bytes),
                DBC_CANDIDATE_FORMAT_EMPTY_SELECTION);
  manifest.selected_count = 128u;
  manifest.selected_message_count = 64u;
  ASSERT_STATUS(dbc_manifest_v1_encode(&manifest, bytes),
                DBC_CANDIDATE_FORMAT_OK);
  return true;
}

static bool test_manifest_references_and_recovery(void) {
  DbcManifestV1 current = candidate_manifest_zero();
  DbcManifestV1 previous = current;
  previous.generation = 16u;
  uint8_t current_bytes[LARGE_DBC_MANIFEST_SIZE];
  uint8_t previous_bytes[LARGE_DBC_MANIFEST_SIZE];
  ASSERT_STATUS(dbc_manifest_v1_encode(&current, current_bytes),
                DBC_CANDIDATE_FORMAT_OK);
  ASSERT_STATUS(dbc_manifest_v1_encode(&previous, previous_bytes),
                DBC_CANDIDATE_FORMAT_OK);
  DbcManifestReferenceFacts current_facts = facts_from_manifest(&current);
  DbcManifestReferenceFacts previous_facts = facts_from_manifest(&previous);
  ASSERT_STATUS(dbc_manifest_v1_verify_references(&current, &current_facts),
                DBC_CANDIDATE_FORMAT_OK);
  DbcManifestV1 selected;
  ASSERT_TRUE(dbc_manifest_v1_select_current_or_previous(
                current_bytes, sizeof(current_bytes), &current_facts,
                previous_bytes, sizeof(previous_bytes), &previous_facts,
                LARGE_DBC_MANIFEST_OBJECT_CANDIDATE, &selected) ==
              DBC_MANIFEST_RECOVERY_CURRENT);
  ASSERT_TRUE(selected.generation == current.generation);

  current_facts.index_crc32 ^= 1u;
  ASSERT_STATUS(dbc_manifest_v1_verify_references(&current, &current_facts),
                DBC_CANDIDATE_FORMAT_REFERENCE_MISMATCH);
  ASSERT_TRUE(dbc_manifest_v1_select_current_or_previous(
                current_bytes, sizeof(current_bytes), &current_facts,
                previous_bytes, sizeof(previous_bytes), &previous_facts,
                LARGE_DBC_MANIFEST_OBJECT_CANDIDATE, &selected) ==
              DBC_MANIFEST_RECOVERY_PREVIOUS);
  ASSERT_TRUE(selected.generation == previous.generation);

  previous_bytes[0] ^= 1u;
  ASSERT_TRUE(dbc_manifest_v1_select_current_or_previous(
                current_bytes, sizeof(current_bytes), &current_facts,
                previous_bytes, sizeof(previous_bytes), &previous_facts,
                LARGE_DBC_MANIFEST_OBJECT_CANDIDATE, &selected) ==
              DBC_MANIFEST_RECOVERY_NONE);
  ASSERT_TRUE(selected.generation == 0u);
  return true;
}

static bool test_default_selection_boundaries(void) {
  static const uint16_t select_all_counts[] = {1u, 127u, 128u};
  for (size_t i = 0u; i < sizeof(select_all_counts) / sizeof(select_all_counts[0]); ++i) {
    DbcSelectionV1 selection;
    const uint16_t count = select_all_counts[i];
    ASSERT_STATUS(dbc_selection_v1_init_default(&selection,
                                                17u,
                                                1u,
                                                100071u,
                                                UINT32_C(0x4B88D9CE),
                                                count),
                  DBC_CANDIDATE_FORMAT_OK);
    ASSERT_TRUE(selection.selected_count == count);
    for (uint16_t ordinal = 0u; ordinal < count; ++ordinal) {
      ASSERT_TRUE(dbc_selection_v1_is_selected(&selection, ordinal));
    }
    ASSERT_TRUE(!dbc_selection_v1_is_selected(&selection, count));
  }
  static const uint16_t select_none_counts[] = {129u, 896u, 2048u};
  for (size_t i = 0u; i < sizeof(select_none_counts) / sizeof(select_none_counts[0]); ++i) {
    DbcSelectionV1 selection;
    ASSERT_STATUS(dbc_selection_v1_init_default(&selection,
                                                17u,
                                                1u,
                                                100071u,
                                                UINT32_C(0x4B88D9CE),
                                                select_none_counts[i]),
                  DBC_CANDIDATE_FORMAT_OK);
    ASSERT_TRUE(selection.selected_count == 0u);
    ASSERT_TRUE(!dbc_selection_v1_is_selected(&selection, 0u));
  }
  DbcSelectionV1 selection;
  ASSERT_STATUS(dbc_selection_v1_init_default(&selection,
                                              17u,
                                              1u,
                                              100071u,
                                              UINT32_C(0x4B88D9CE),
                                              0u),
                DBC_CANDIDATE_FORMAT_INVALID_FORMAT);
  ASSERT_STATUS(dbc_selection_v1_init_default(&selection,
                                              17u,
                                              1u,
                                              100071u,
                                              UINT32_C(0x4B88D9CE),
                                              2049u),
                DBC_CANDIDATE_FORMAT_INVALID_FORMAT);
  return true;
}

static bool test_selection_golden_damage_activation(void) {
  DbcSelectionV1 selection;
  ASSERT_STATUS(dbc_selection_v1_init_default(&selection,
                                              17u,
                                              3u,
                                              100071u,
                                              UINT32_C(0x4B88D9CE),
                                              128u),
                DBC_CANDIDATE_FORMAT_OK);
  uint8_t good[LARGE_DBC_SELECTION_TOTAL_SIZE];
  uint8_t bytes[LARGE_DBC_SELECTION_TOTAL_SIZE];
  ASSERT_STATUS(dbc_selection_v1_encode(&selection, good),
                DBC_CANDIDATE_FORMAT_OK);
  static const uint8_t golden_prefix[16] = {
    0x44, 0x42, 0x43, 0x53, 0x01, 0x00, 0x40, 0x00,
    0x40, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00
  };
  ASSERT_TRUE(memcmp(good, golden_prefix, sizeof(golden_prefix)) == 0);
  ASSERT_TRUE(dbc_candidate_crc32(good, sizeof(good)) == UINT32_C(0x62BA0D66));
  DbcSelectionV1 decoded;
  ASSERT_STATUS(dbc_selection_v1_decode(good, sizeof(good), &decoded),
                DBC_CANDIDATE_FORMAT_OK);
  ASSERT_TRUE(decoded.selected_count == 128u);
  ASSERT_STATUS(dbc_selection_v1_verify_activation(&decoded, 64u),
                DBC_CANDIDATE_FORMAT_OK);
  ASSERT_STATUS(dbc_selection_v1_verify_activation(&decoded, 65u),
                DBC_CANDIDATE_FORMAT_LIMIT_EXCEEDED);

  ASSERT_STATUS(dbc_selection_v1_decode(good, sizeof(good) - 1u, &decoded),
                DBC_CANDIDATE_FORMAT_INVALID_FORMAT);
  memcpy(bytes, good, sizeof(bytes));
  bytes[LARGE_DBC_SELECTION_HEADER_SIZE] ^= 1u;
  ASSERT_STATUS(dbc_selection_v1_decode(bytes, sizeof(bytes), &decoded),
                DBC_CANDIDATE_FORMAT_CRC_MISMATCH);
  memcpy(bytes, good, sizeof(bytes));
  bytes[LARGE_DBC_SELECTION_SOURCE_CRC32_OFFSET] ^= 1u;
  ASSERT_STATUS(dbc_selection_v1_decode(bytes, sizeof(bytes), &decoded),
                DBC_CANDIDATE_FORMAT_CRC_MISMATCH);

  static const size_t corrupt_offsets[] = {
    LARGE_DBC_SELECTION_MAGIC_OFFSET,
    LARGE_DBC_SELECTION_VERSION_OFFSET,
    LARGE_DBC_SELECTION_HEADER_SIZE_OFFSET,
    LARGE_DBC_SELECTION_TOTAL_SIZE_OFFSET,
    LARGE_DBC_SELECTION_BITMAP_SIZE_OFFSET,
    14u,
    48u
  };
  for (size_t i = 0u; i < sizeof(corrupt_offsets) / sizeof(corrupt_offsets[0]); ++i) {
    memcpy(bytes, good, sizeof(bytes));
    bytes[corrupt_offsets[i]] ^= 1u;
    repair_selection_header_crc(bytes);
    ASSERT_STATUS(dbc_selection_v1_decode(bytes, sizeof(bytes), &decoded),
                  DBC_CANDIDATE_FORMAT_INVALID_FORMAT);
  }

  memcpy(bytes, good, sizeof(bytes));
  put_u16(bytes + LARGE_DBC_SELECTION_SELECTED_COUNT_OFFSET, 127u);
  repair_selection_header_crc(bytes);
  ASSERT_STATUS(dbc_selection_v1_decode(bytes, sizeof(bytes), &decoded),
                DBC_CANDIDATE_FORMAT_INVALID_FORMAT);

  memcpy(bytes, good, sizeof(bytes));
  put_u16(bytes + LARGE_DBC_SELECTION_CATALOG_SIGNAL_COUNT_OFFSET, 127u);
  repair_selection_header_crc(bytes);
  ASSERT_STATUS(dbc_selection_v1_decode(bytes, sizeof(bytes), &decoded),
                DBC_CANDIDATE_FORMAT_INVALID_FORMAT);

  selection.selected_count = 129u;
  memset(selection.bitmap, 0xff, 17u);
  ASSERT_STATUS(dbc_selection_v1_encode(&selection, bytes),
                DBC_CANDIDATE_FORMAT_LIMIT_EXCEEDED);

  ASSERT_STATUS(dbc_selection_v1_init_default(&selection,
                                              17u,
                                              3u,
                                              100071u,
                                              UINT32_C(0x4B88D9CE),
                                              896u),
                DBC_CANDIDATE_FORMAT_OK);
  ASSERT_STATUS(dbc_selection_v1_verify_activation(&selection, 0u),
                DBC_CANDIDATE_FORMAT_EMPTY_SELECTION);
  return true;
}

static bool test_candidate_set_cross_checks(void) {
  DbcSelectionV1 selection;
  ASSERT_STATUS(dbc_selection_v1_init_default(&selection,
                                              17u,
                                              1u,
                                              100071u,
                                              UINT32_C(0x4B88D9CE),
                                              128u),
                DBC_CANDIDATE_FORMAT_OK);
  uint8_t selection_bytes[LARGE_DBC_SELECTION_TOTAL_SIZE];
  ASSERT_STATUS(dbc_selection_v1_encode(&selection, selection_bytes),
                DBC_CANDIDATE_FORMAT_OK);
  DbcCandidateIndexFacts index = {
    .source_size = 100071u,
    .source_crc32 = UINT32_C(0x4B88D9CE),
    .index_size = catalog_index_size(64u, 128u),
    .index_crc32 = UINT32_C(0x10203040),
    .catalog_message_count = 64u,
    .catalog_signal_count = 128u
  };
  DbcManifestV1 manifest = {
    .object_kind = LARGE_DBC_MANIFEST_OBJECT_CANDIDATE,
    .generation = 17u,
    .source_size = index.source_size,
    .source_crc32 = index.source_crc32,
    .index_size = index.index_size,
    .index_crc32 = index.index_crc32,
    .selection_size = sizeof(selection_bytes),
    .selection_crc32 = dbc_candidate_crc32(selection_bytes, sizeof(selection_bytes)),
    .selected_count = 128u,
    .selected_message_count = 64u,
    .catalog_message_count = index.catalog_message_count,
    .catalog_signal_count = index.catalog_signal_count
  };
  DbcSelectionV1 decoded;
  ASSERT_STATUS(dbc_candidate_v1_verify_set(&manifest,
                                            &index,
                                            selection_bytes,
                                            sizeof(selection_bytes),
                                            64u,
                                            &decoded),
                DBC_CANDIDATE_FORMAT_OK);
  ASSERT_TRUE(decoded.candidate_generation == 17u);

  DbcManifestV1 bad_manifest = manifest;
  bad_manifest.generation = 18u;
  ASSERT_STATUS(dbc_candidate_v1_verify_set(&bad_manifest,
                                            &index,
                                            selection_bytes,
                                            sizeof(selection_bytes),
                                            64u,
                                            NULL),
                DBC_CANDIDATE_FORMAT_REFERENCE_MISMATCH);
  DbcCandidateIndexFacts bad_index = index;
  bad_index.source_size ^= 1u;
  ASSERT_STATUS(dbc_candidate_v1_verify_set(&manifest,
                                            &bad_index,
                                            selection_bytes,
                                            sizeof(selection_bytes),
                                            64u,
                                            NULL),
                DBC_CANDIDATE_FORMAT_REFERENCE_MISMATCH);
  bad_index = index;
  bad_index.source_crc32 ^= 1u;
  ASSERT_STATUS(dbc_candidate_v1_verify_set(&manifest,
                                            &bad_index,
                                            selection_bytes,
                                            sizeof(selection_bytes),
                                            64u,
                                            NULL),
                DBC_CANDIDATE_FORMAT_REFERENCE_MISMATCH);
  bad_index = index;
  bad_index.index_size += 1u;
  ASSERT_STATUS(dbc_candidate_v1_verify_set(&manifest,
                                            &bad_index,
                                            selection_bytes,
                                            sizeof(selection_bytes),
                                            64u,
                                            NULL),
                DBC_CANDIDATE_FORMAT_REFERENCE_MISMATCH);
  bad_index = index;
  bad_index.index_crc32 ^= 1u;
  ASSERT_STATUS(dbc_candidate_v1_verify_set(&manifest,
                                            &bad_index,
                                            selection_bytes,
                                            sizeof(selection_bytes),
                                            64u,
                                            NULL),
                DBC_CANDIDATE_FORMAT_REFERENCE_MISMATCH);
  bad_index = index;
  --bad_index.catalog_message_count;
  ASSERT_STATUS(dbc_candidate_v1_verify_set(&manifest,
                                            &bad_index,
                                            selection_bytes,
                                            sizeof(selection_bytes),
                                            64u,
                                            NULL),
                DBC_CANDIDATE_FORMAT_REFERENCE_MISMATCH);
  bad_index = index;
  --bad_index.catalog_signal_count;
  ASSERT_STATUS(dbc_candidate_v1_verify_set(&manifest,
                                            &bad_index,
                                            selection_bytes,
                                            sizeof(selection_bytes),
                                            64u,
                                            NULL),
                DBC_CANDIDATE_FORMAT_REFERENCE_MISMATCH);
  ASSERT_STATUS(dbc_candidate_v1_verify_set(&manifest,
                                            &index,
                                            selection_bytes,
                                            sizeof(selection_bytes),
                                            63u,
                                            NULL),
                DBC_CANDIDATE_FORMAT_REFERENCE_MISMATCH);
  bad_manifest = manifest;
  bad_manifest.selection_crc32 ^= 1u;
  ASSERT_STATUS(dbc_candidate_v1_verify_set(&bad_manifest,
                                            &index,
                                            selection_bytes,
                                            sizeof(selection_bytes),
                                            64u,
                                            NULL),
                DBC_CANDIDATE_FORMAT_REFERENCE_MISMATCH);
  selection_bytes[LARGE_DBC_SELECTION_HEADER_SIZE] ^= 1u;
  ASSERT_STATUS(dbc_candidate_v1_verify_set(&manifest,
                                            &index,
                                            selection_bytes,
                                            sizeof(selection_bytes),
                                            64u,
                                            NULL),
                DBC_CANDIDATE_FORMAT_CRC_MISMATCH);
  return true;
}

static bool test_token_and_generation(void) {
  char token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES];
  ASSERT_STATUS(dbc_candidate_token_format(17u,
                                           100071u,
                                           UINT32_C(0x4B88D9CE),
                                           token),
                DBC_CANDIDATE_FORMAT_OK);
  ASSERT_TRUE(strcmp(token, "0000000000000011-000186E7-4B88D9CE") == 0);
  uint64_t generation = 0u;
  uint32_t source_size = 0u;
  uint32_t source_crc = 0u;
  ASSERT_STATUS(dbc_candidate_token_parse(token,
                                          &generation,
                                          &source_size,
                                          &source_crc),
                DBC_CANDIDATE_FORMAT_OK);
  ASSERT_TRUE(generation == 17u && source_size == 100071u &&
              source_crc == UINT32_C(0x4B88D9CE));
  DbcManifestV1 manifest = candidate_manifest_zero();
  ASSERT_STATUS(dbc_candidate_token_verify_manifest(token, &manifest),
                DBC_CANDIDATE_FORMAT_OK);
  token[33] = 'F';
  ASSERT_STATUS(dbc_candidate_token_verify_manifest(token, &manifest),
                DBC_CANDIDATE_FORMAT_TOKEN_MISMATCH);
  token[33] = 'e';
  ASSERT_STATUS(dbc_candidate_token_parse(token,
                                          &generation,
                                          &source_size,
                                          &source_crc),
                DBC_CANDIDATE_FORMAT_INVALID_FORMAT);

  uint64_t next = 0u;
  ASSERT_STATUS(dbc_candidate_next_generation(0u, &next),
                DBC_CANDIDATE_FORMAT_OK);
  ASSERT_TRUE(next == 1u);
  ASSERT_STATUS(dbc_candidate_next_generation(UINT64_MAX, &next),
                DBC_CANDIDATE_FORMAT_GENERATION_EXHAUSTED);
  return true;
}

int main(void) {
  ASSERT_TRUE(test_crc_and_manifest_golden());
  ASSERT_TRUE(test_manifest_damage_and_limits());
  ASSERT_TRUE(test_manifest_references_and_recovery());
  ASSERT_TRUE(test_default_selection_boundaries());
  ASSERT_TRUE(test_selection_golden_damage_activation());
  ASSERT_TRUE(test_candidate_set_cross_checks());
  ASSERT_TRUE(test_token_and_generation());
  puts("DBC candidate manifest/selection v1 tests passed");
  return 0;
}
