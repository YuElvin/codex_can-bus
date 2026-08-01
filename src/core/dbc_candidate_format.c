#include "dbc_candidate_format.h"

#include <stdio.h>
#include <string.h>

static uint16_t get_u16(const uint8_t *data) {
  return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8u));
}

static uint32_t get_u32(const uint8_t *data) {
  return (uint32_t)data[0] |
         ((uint32_t)data[1] << 8u) |
         ((uint32_t)data[2] << 16u) |
         ((uint32_t)data[3] << 24u);
}

static uint64_t get_u64(const uint8_t *data) {
  return (uint64_t)get_u32(data) | ((uint64_t)get_u32(data + 4u) << 32u);
}

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

uint32_t dbc_candidate_crc32(const uint8_t *data, size_t size) {
  uint32_t crc = LARGE_DBC_CRC32_INITIAL_VALUE;
  if (data == NULL && size != 0u) {
    return 0u;
  }
  while (size-- > 0u) {
    crc ^= *data++;
    for (unsigned bit = 0u; bit < 8u; ++bit) {
      crc = (crc >> 1u) ^ ((crc & 1u) != 0u ?
        LARGE_DBC_CRC32_REFLECTED_POLYNOMIAL : 0u);
    }
  }
  return crc ^ LARGE_DBC_CRC32_FINAL_XOR;
}

static uint32_t expected_index_size(uint16_t messages, uint16_t signals) {
  return LARGE_DBC_INDEX_HEADER_SIZE +
         (uint32_t)messages * LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE +
         (uint32_t)signals * LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE;
}

static DbcCandidateFormatStatus validate_manifest(const DbcManifestV1 *manifest) {
  if (manifest == NULL) {
    return DBC_CANDIDATE_FORMAT_INVALID_ARGUMENT;
  }
  if (manifest->object_kind != LARGE_DBC_MANIFEST_OBJECT_CANDIDATE &&
      manifest->object_kind != LARGE_DBC_MANIFEST_OBJECT_ACTIVE) {
    return DBC_CANDIDATE_FORMAT_INVALID_FORMAT;
  }
  if (manifest->generation == 0u || manifest->source_size == 0u ||
      manifest->source_size > LARGE_DBC_SOURCE_MAX_BYTES ||
      manifest->catalog_message_count == 0u ||
      manifest->catalog_message_count > LARGE_DBC_CATALOG_MAX_MESSAGES ||
      manifest->catalog_signal_count == 0u ||
      manifest->catalog_signal_count > LARGE_DBC_CATALOG_MAX_SIGNALS ||
      manifest->index_size != expected_index_size(manifest->catalog_message_count,
                                                  manifest->catalog_signal_count) ||
      manifest->selection_size != LARGE_DBC_SELECTION_TOTAL_SIZE) {
    return DBC_CANDIDATE_FORMAT_INVALID_FORMAT;
  }
  if (manifest->selected_count > LARGE_DBC_ACTIVE_MAX_SIGNALS ||
      manifest->selected_count > manifest->catalog_signal_count ||
      manifest->selected_message_count > LARGE_DBC_ACTIVE_MAX_MESSAGES ||
      manifest->selected_message_count > manifest->catalog_message_count ||
      manifest->selected_message_count > manifest->selected_count) {
    return DBC_CANDIDATE_FORMAT_LIMIT_EXCEEDED;
  }
  if ((manifest->selected_count == 0u) !=
      (manifest->selected_message_count == 0u)) {
    return DBC_CANDIDATE_FORMAT_INVALID_FORMAT;
  }
  if (manifest->object_kind == LARGE_DBC_MANIFEST_OBJECT_ACTIVE &&
      manifest->selected_count == 0u) {
    return DBC_CANDIDATE_FORMAT_EMPTY_SELECTION;
  }
  return DBC_CANDIDATE_FORMAT_OK;
}

DbcCandidateFormatStatus dbc_manifest_v1_encode(
  const DbcManifestV1 *manifest,
  uint8_t output[LARGE_DBC_MANIFEST_SIZE]) {
  if (output == NULL) {
    return DBC_CANDIDATE_FORMAT_INVALID_ARGUMENT;
  }
  DbcCandidateFormatStatus status = validate_manifest(manifest);
  if (status != DBC_CANDIDATE_FORMAT_OK) {
    return status;
  }
  memset(output, 0, LARGE_DBC_MANIFEST_SIZE);
  put_u32(output + LARGE_DBC_MANIFEST_MAGIC_OFFSET, LARGE_DBC_MANIFEST_MAGIC);
  put_u16(output + LARGE_DBC_MANIFEST_VERSION_OFFSET, LARGE_DBC_CONTRACT_VERSION);
  put_u16(output + LARGE_DBC_MANIFEST_HEADER_SIZE_OFFSET, LARGE_DBC_MANIFEST_SIZE);
  output[LARGE_DBC_MANIFEST_OBJECT_KIND_OFFSET] = manifest->object_kind;
  put_u64(output + LARGE_DBC_MANIFEST_GENERATION_OFFSET, manifest->generation);
  put_u32(output + LARGE_DBC_MANIFEST_SOURCE_SIZE_OFFSET, manifest->source_size);
  put_u32(output + LARGE_DBC_MANIFEST_SOURCE_CRC32_OFFSET, manifest->source_crc32);
  put_u32(output + LARGE_DBC_MANIFEST_INDEX_SIZE_OFFSET, manifest->index_size);
  put_u32(output + LARGE_DBC_MANIFEST_INDEX_CRC32_OFFSET, manifest->index_crc32);
  put_u32(output + LARGE_DBC_MANIFEST_SELECTION_SIZE_OFFSET, manifest->selection_size);
  put_u32(output + LARGE_DBC_MANIFEST_SELECTION_CRC32_OFFSET,
          manifest->selection_crc32);
  put_u16(output + LARGE_DBC_MANIFEST_SELECTED_COUNT_OFFSET,
          manifest->selected_count);
  put_u16(output + LARGE_DBC_MANIFEST_SELECTED_MESSAGE_COUNT_OFFSET,
          manifest->selected_message_count);
  put_u16(output + LARGE_DBC_MANIFEST_CATALOG_MESSAGE_COUNT_OFFSET,
          manifest->catalog_message_count);
  put_u16(output + LARGE_DBC_MANIFEST_CATALOG_SIGNAL_COUNT_OFFSET,
          manifest->catalog_signal_count);
  put_u32(output + LARGE_DBC_MANIFEST_CRC32_OFFSET,
          dbc_candidate_crc32(output, LARGE_DBC_MANIFEST_CRC32_COVERED_BYTES));
  return DBC_CANDIDATE_FORMAT_OK;
}

DbcCandidateFormatStatus dbc_manifest_v1_decode(
  const uint8_t *bytes,
  size_t size,
  DbcManifestV1 *manifest) {
  if (bytes == NULL || manifest == NULL) {
    return DBC_CANDIDATE_FORMAT_INVALID_ARGUMENT;
  }
  if (size != LARGE_DBC_MANIFEST_SIZE ||
      get_u32(bytes + LARGE_DBC_MANIFEST_MAGIC_OFFSET) != LARGE_DBC_MANIFEST_MAGIC ||
      get_u16(bytes + LARGE_DBC_MANIFEST_VERSION_OFFSET) !=
        LARGE_DBC_CONTRACT_VERSION ||
      get_u16(bytes + LARGE_DBC_MANIFEST_HEADER_SIZE_OFFSET) !=
        LARGE_DBC_MANIFEST_SIZE ||
      bytes[LARGE_DBC_MANIFEST_FLAGS_OFFSET] != 0u || bytes[10] != 0u ||
      bytes[11] != 0u) {
    return DBC_CANDIDATE_FORMAT_INVALID_FORMAT;
  }
  for (size_t i = 52u; i < LARGE_DBC_MANIFEST_CRC32_OFFSET; ++i) {
    if (bytes[i] != 0u) {
      return DBC_CANDIDATE_FORMAT_INVALID_FORMAT;
    }
  }
  if (dbc_candidate_crc32(bytes, LARGE_DBC_MANIFEST_CRC32_COVERED_BYTES) !=
      get_u32(bytes + LARGE_DBC_MANIFEST_CRC32_OFFSET)) {
    return DBC_CANDIDATE_FORMAT_CRC_MISMATCH;
  }
  memset(manifest, 0, sizeof(*manifest));
  manifest->object_kind = bytes[LARGE_DBC_MANIFEST_OBJECT_KIND_OFFSET];
  manifest->generation = get_u64(bytes + LARGE_DBC_MANIFEST_GENERATION_OFFSET);
  manifest->source_size = get_u32(bytes + LARGE_DBC_MANIFEST_SOURCE_SIZE_OFFSET);
  manifest->source_crc32 = get_u32(bytes + LARGE_DBC_MANIFEST_SOURCE_CRC32_OFFSET);
  manifest->index_size = get_u32(bytes + LARGE_DBC_MANIFEST_INDEX_SIZE_OFFSET);
  manifest->index_crc32 = get_u32(bytes + LARGE_DBC_MANIFEST_INDEX_CRC32_OFFSET);
  manifest->selection_size = get_u32(bytes + LARGE_DBC_MANIFEST_SELECTION_SIZE_OFFSET);
  manifest->selection_crc32 = get_u32(bytes + LARGE_DBC_MANIFEST_SELECTION_CRC32_OFFSET);
  manifest->selected_count = get_u16(bytes + LARGE_DBC_MANIFEST_SELECTED_COUNT_OFFSET);
  manifest->selected_message_count =
    get_u16(bytes + LARGE_DBC_MANIFEST_SELECTED_MESSAGE_COUNT_OFFSET);
  manifest->catalog_message_count =
    get_u16(bytes + LARGE_DBC_MANIFEST_CATALOG_MESSAGE_COUNT_OFFSET);
  manifest->catalog_signal_count =
    get_u16(bytes + LARGE_DBC_MANIFEST_CATALOG_SIGNAL_COUNT_OFFSET);
  return validate_manifest(manifest);
}

DbcCandidateFormatStatus dbc_manifest_v1_verify_references(
  const DbcManifestV1 *manifest,
  const DbcManifestReferenceFacts *facts) {
  DbcCandidateFormatStatus status = validate_manifest(manifest);
  if (facts == NULL) {
    return DBC_CANDIDATE_FORMAT_INVALID_ARGUMENT;
  }
  if (status != DBC_CANDIDATE_FORMAT_OK) {
    return status;
  }
  if (manifest->generation != facts->generation ||
      manifest->source_size != facts->source_size ||
      manifest->source_crc32 != facts->source_crc32 ||
      manifest->index_size != facts->index_size ||
      manifest->index_crc32 != facts->index_crc32 ||
      manifest->selection_size != facts->selection_size ||
      manifest->selection_crc32 != facts->selection_crc32 ||
      manifest->selected_count != facts->selected_count ||
      manifest->selected_message_count != facts->selected_message_count ||
      manifest->catalog_message_count != facts->catalog_message_count ||
      manifest->catalog_signal_count != facts->catalog_signal_count) {
    return DBC_CANDIDATE_FORMAT_REFERENCE_MISMATCH;
  }
  return DBC_CANDIDATE_FORMAT_OK;
}

static bool recover_manifest(const uint8_t *bytes,
                             size_t size,
                             const DbcManifestReferenceFacts *facts,
                             uint8_t expected_kind,
                             DbcManifestV1 *manifest) {
  return bytes != NULL && facts != NULL &&
         dbc_manifest_v1_decode(bytes, size, manifest) == DBC_CANDIDATE_FORMAT_OK &&
         manifest->object_kind == expected_kind &&
         dbc_manifest_v1_verify_references(manifest, facts) ==
           DBC_CANDIDATE_FORMAT_OK;
}

DbcManifestRecoverySlot dbc_manifest_v1_select_current_or_previous(
  const uint8_t *current_bytes,
  size_t current_size,
  const DbcManifestReferenceFacts *current_facts,
  const uint8_t *previous_bytes,
  size_t previous_size,
  const DbcManifestReferenceFacts *previous_facts,
  uint8_t expected_object_kind,
  DbcManifestV1 *selected_manifest) {
  if (selected_manifest == NULL ||
      (expected_object_kind != LARGE_DBC_MANIFEST_OBJECT_CANDIDATE &&
       expected_object_kind != LARGE_DBC_MANIFEST_OBJECT_ACTIVE)) {
    return DBC_MANIFEST_RECOVERY_NONE;
  }
  if (recover_manifest(current_bytes,
                       current_size,
                       current_facts,
                       expected_object_kind,
                       selected_manifest)) {
    return DBC_MANIFEST_RECOVERY_CURRENT;
  }
  if (recover_manifest(previous_bytes,
                       previous_size,
                       previous_facts,
                       expected_object_kind,
                       selected_manifest)) {
    return DBC_MANIFEST_RECOVERY_PREVIOUS;
  }
  memset(selected_manifest, 0, sizeof(*selected_manifest));
  return DBC_MANIFEST_RECOVERY_NONE;
}

static uint16_t popcount_bitmap(const uint8_t *bitmap) {
  uint16_t count = 0u;
  for (size_t i = 0u; i < LARGE_DBC_SELECTION_BITMAP_BYTES; ++i) {
    uint8_t value = bitmap[i];
    while (value != 0u) {
      value &= (uint8_t)(value - 1u);
      ++count;
    }
  }
  return count;
}

static bool tail_bits_clear(const DbcSelectionV1 *selection) {
  for (uint16_t ordinal = selection->catalog_signal_count;
       ordinal < LARGE_DBC_CATALOG_MAX_SIGNALS;
       ++ordinal) {
    if ((selection->bitmap[ordinal >> 3u] &
         (uint8_t)(1u << (ordinal & 7u))) != 0u) {
      return false;
    }
  }
  return true;
}

static DbcCandidateFormatStatus validate_selection(const DbcSelectionV1 *selection) {
  if (selection == NULL) {
    return DBC_CANDIDATE_FORMAT_INVALID_ARGUMENT;
  }
  if (selection->candidate_generation == 0u ||
      selection->selection_generation == 0u ||
      selection->source_size == 0u ||
      selection->source_size > LARGE_DBC_SOURCE_MAX_BYTES ||
      selection->catalog_signal_count == 0u ||
      selection->catalog_signal_count > LARGE_DBC_CATALOG_MAX_SIGNALS) {
    return DBC_CANDIDATE_FORMAT_INVALID_FORMAT;
  }
  if (selection->selected_count > LARGE_DBC_ACTIVE_MAX_SIGNALS) {
    return DBC_CANDIDATE_FORMAT_LIMIT_EXCEEDED;
  }
  if (!tail_bits_clear(selection) ||
      selection->selected_count != popcount_bitmap(selection->bitmap)) {
    return DBC_CANDIDATE_FORMAT_INVALID_FORMAT;
  }
  return DBC_CANDIDATE_FORMAT_OK;
}

DbcCandidateFormatStatus dbc_selection_v1_init_default(
  DbcSelectionV1 *selection,
  uint64_t candidate_generation,
  uint64_t selection_generation,
  uint32_t source_size,
  uint32_t source_crc32,
  uint16_t catalog_signal_count) {
  if (selection == NULL) {
    return DBC_CANDIDATE_FORMAT_INVALID_ARGUMENT;
  }
  memset(selection, 0, sizeof(*selection));
  selection->candidate_generation = candidate_generation;
  selection->selection_generation = selection_generation;
  selection->source_size = source_size;
  selection->source_crc32 = source_crc32;
  selection->catalog_signal_count = catalog_signal_count;
  if (catalog_signal_count <= LARGE_DBC_ACTIVE_MAX_SIGNALS) {
    selection->selected_count = catalog_signal_count;
    for (uint16_t ordinal = 0u; ordinal < catalog_signal_count; ++ordinal) {
      selection->bitmap[ordinal >> 3u] |= (uint8_t)(1u << (ordinal & 7u));
    }
  }
  return validate_selection(selection);
}

bool dbc_selection_v1_is_selected(const DbcSelectionV1 *selection,
                                  uint16_t ordinal) {
  return selection != NULL && ordinal < selection->catalog_signal_count &&
         (selection->bitmap[ordinal >> 3u] &
          (uint8_t)(1u << (ordinal & 7u))) != 0u;
}

DbcCandidateFormatStatus dbc_selection_v1_encode(
  const DbcSelectionV1 *selection,
  uint8_t output[LARGE_DBC_SELECTION_TOTAL_SIZE]) {
  if (output == NULL) {
    return DBC_CANDIDATE_FORMAT_INVALID_ARGUMENT;
  }
  DbcCandidateFormatStatus status = validate_selection(selection);
  if (status != DBC_CANDIDATE_FORMAT_OK) {
    return status;
  }
  memset(output, 0, LARGE_DBC_SELECTION_TOTAL_SIZE);
  put_u32(output + LARGE_DBC_SELECTION_MAGIC_OFFSET, LARGE_DBC_SELECTION_MAGIC);
  put_u16(output + LARGE_DBC_SELECTION_VERSION_OFFSET, LARGE_DBC_CONTRACT_VERSION);
  put_u16(output + LARGE_DBC_SELECTION_HEADER_SIZE_OFFSET,
          LARGE_DBC_SELECTION_HEADER_SIZE);
  put_u32(output + LARGE_DBC_SELECTION_TOTAL_SIZE_OFFSET,
          LARGE_DBC_SELECTION_TOTAL_SIZE);
  put_u32(output + LARGE_DBC_SELECTION_BITMAP_SIZE_OFFSET,
          LARGE_DBC_SELECTION_BITMAP_BYTES);
  put_u64(output + LARGE_DBC_SELECTION_CANDIDATE_GENERATION_OFFSET,
          selection->candidate_generation);
  put_u32(output + LARGE_DBC_SELECTION_SOURCE_SIZE_OFFSET, selection->source_size);
  put_u32(output + LARGE_DBC_SELECTION_SOURCE_CRC32_OFFSET,
          selection->source_crc32);
  put_u16(output + LARGE_DBC_SELECTION_CATALOG_SIGNAL_COUNT_OFFSET,
          selection->catalog_signal_count);
  put_u16(output + LARGE_DBC_SELECTION_SELECTED_COUNT_OFFSET,
          selection->selected_count);
  memcpy(output + LARGE_DBC_SELECTION_HEADER_SIZE,
         selection->bitmap,
         LARGE_DBC_SELECTION_BITMAP_BYTES);
  put_u32(output + LARGE_DBC_SELECTION_BITMAP_CRC32_OFFSET,
          dbc_candidate_crc32(output + LARGE_DBC_SELECTION_HEADER_SIZE,
                              LARGE_DBC_SELECTION_BITMAP_BYTES));
  put_u64(output + LARGE_DBC_SELECTION_GENERATION_OFFSET,
          selection->selection_generation);
  put_u32(output + LARGE_DBC_SELECTION_HEADER_CRC32_OFFSET,
          dbc_candidate_crc32(output,
                              LARGE_DBC_SELECTION_HEADER_CRC32_COVERED_BYTES));
  return DBC_CANDIDATE_FORMAT_OK;
}

DbcCandidateFormatStatus dbc_selection_v1_decode(
  const uint8_t *bytes,
  size_t size,
  DbcSelectionV1 *selection) {
  if (bytes == NULL || selection == NULL) {
    return DBC_CANDIDATE_FORMAT_INVALID_ARGUMENT;
  }
  if (size != LARGE_DBC_SELECTION_TOTAL_SIZE ||
      get_u32(bytes + LARGE_DBC_SELECTION_MAGIC_OFFSET) != LARGE_DBC_SELECTION_MAGIC ||
      get_u16(bytes + LARGE_DBC_SELECTION_VERSION_OFFSET) !=
        LARGE_DBC_CONTRACT_VERSION ||
      get_u16(bytes + LARGE_DBC_SELECTION_HEADER_SIZE_OFFSET) !=
        LARGE_DBC_SELECTION_HEADER_SIZE ||
      get_u32(bytes + LARGE_DBC_SELECTION_TOTAL_SIZE_OFFSET) !=
        LARGE_DBC_SELECTION_TOTAL_SIZE ||
      get_u32(bytes + LARGE_DBC_SELECTION_BITMAP_SIZE_OFFSET) !=
        LARGE_DBC_SELECTION_BITMAP_BYTES || bytes[14] != 0u || bytes[15] != 0u) {
    return DBC_CANDIDATE_FORMAT_INVALID_FORMAT;
  }
  for (size_t i = 48u; i < LARGE_DBC_SELECTION_HEADER_CRC32_OFFSET; ++i) {
    if (bytes[i] != 0u) {
      return DBC_CANDIDATE_FORMAT_INVALID_FORMAT;
    }
  }
  if (dbc_candidate_crc32(bytes, LARGE_DBC_SELECTION_HEADER_CRC32_COVERED_BYTES) !=
      get_u32(bytes + LARGE_DBC_SELECTION_HEADER_CRC32_OFFSET) ||
      dbc_candidate_crc32(bytes + LARGE_DBC_SELECTION_HEADER_SIZE,
                          LARGE_DBC_SELECTION_BITMAP_BYTES) !=
        get_u32(bytes + LARGE_DBC_SELECTION_BITMAP_CRC32_OFFSET)) {
    return DBC_CANDIDATE_FORMAT_CRC_MISMATCH;
  }
  memset(selection, 0, sizeof(*selection));
  selection->candidate_generation =
    get_u64(bytes + LARGE_DBC_SELECTION_CANDIDATE_GENERATION_OFFSET);
  selection->selection_generation =
    get_u64(bytes + LARGE_DBC_SELECTION_GENERATION_OFFSET);
  selection->source_size = get_u32(bytes + LARGE_DBC_SELECTION_SOURCE_SIZE_OFFSET);
  selection->source_crc32 = get_u32(bytes + LARGE_DBC_SELECTION_SOURCE_CRC32_OFFSET);
  selection->catalog_signal_count =
    get_u16(bytes + LARGE_DBC_SELECTION_CATALOG_SIGNAL_COUNT_OFFSET);
  selection->selected_count =
    get_u16(bytes + LARGE_DBC_SELECTION_SELECTED_COUNT_OFFSET);
  memcpy(selection->bitmap,
         bytes + LARGE_DBC_SELECTION_HEADER_SIZE,
         LARGE_DBC_SELECTION_BITMAP_BYTES);
  return validate_selection(selection);
}

DbcCandidateFormatStatus dbc_selection_v1_verify_activation(
  const DbcSelectionV1 *selection,
  uint16_t selected_message_count) {
  DbcCandidateFormatStatus status = validate_selection(selection);
  if (status != DBC_CANDIDATE_FORMAT_OK) {
    return status;
  }
  if (selection->selected_count == 0u) {
    return DBC_CANDIDATE_FORMAT_EMPTY_SELECTION;
  }
  if (selected_message_count == 0u ||
      selected_message_count > LARGE_DBC_ACTIVE_MAX_MESSAGES) {
    return DBC_CANDIDATE_FORMAT_LIMIT_EXCEEDED;
  }
  return DBC_CANDIDATE_FORMAT_OK;
}

DbcCandidateFormatStatus dbc_candidate_v1_verify_set(
  const DbcManifestV1 *manifest,
  const DbcCandidateIndexFacts *index,
  const uint8_t *selection_bytes,
  size_t selection_size,
  uint16_t selected_message_count,
  DbcSelectionV1 *decoded_selection) {
  if (manifest == NULL || index == NULL || selection_bytes == NULL) {
    return DBC_CANDIDATE_FORMAT_INVALID_ARGUMENT;
  }
  DbcCandidateFormatStatus status = validate_manifest(manifest);
  if (status != DBC_CANDIDATE_FORMAT_OK) {
    return status;
  }
  if (manifest->object_kind != LARGE_DBC_MANIFEST_OBJECT_CANDIDATE) {
    return DBC_CANDIDATE_FORMAT_INVALID_FORMAT;
  }
  DbcSelectionV1 local_selection;
  status = dbc_selection_v1_decode(selection_bytes, selection_size, &local_selection);
  if (status != DBC_CANDIDATE_FORMAT_OK) {
    return status;
  }
  if (manifest->generation != local_selection.candidate_generation ||
      manifest->source_size != index->source_size ||
      manifest->source_size != local_selection.source_size ||
      manifest->source_crc32 != index->source_crc32 ||
      manifest->source_crc32 != local_selection.source_crc32 ||
      manifest->index_size != index->index_size ||
      manifest->index_crc32 != index->index_crc32 ||
      manifest->selection_size != selection_size ||
      manifest->selection_crc32 !=
        dbc_candidate_crc32(selection_bytes, selection_size) ||
      manifest->catalog_message_count != index->catalog_message_count ||
      manifest->catalog_signal_count != index->catalog_signal_count ||
      manifest->catalog_signal_count != local_selection.catalog_signal_count ||
      manifest->selected_count != local_selection.selected_count ||
      manifest->selected_message_count != selected_message_count) {
    return DBC_CANDIDATE_FORMAT_REFERENCE_MISMATCH;
  }
  if ((local_selection.selected_count == 0u) != (selected_message_count == 0u) ||
      selected_message_count > LARGE_DBC_ACTIVE_MAX_MESSAGES ||
      selected_message_count > index->catalog_message_count ||
      selected_message_count > local_selection.selected_count) {
    return selected_message_count > LARGE_DBC_ACTIVE_MAX_MESSAGES ?
      DBC_CANDIDATE_FORMAT_LIMIT_EXCEEDED :
      DBC_CANDIDATE_FORMAT_REFERENCE_MISMATCH;
  }
  if (decoded_selection != NULL) {
    *decoded_selection = local_selection;
  }
  return DBC_CANDIDATE_FORMAT_OK;
}

static int hex_value(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }
  return -1;
}

static bool parse_hex(const char *text, size_t count, uint64_t *value) {
  uint64_t result = 0u;
  for (size_t i = 0u; i < count; ++i) {
    const int digit = hex_value(text[i]);
    if (digit < 0) {
      return false;
    }
    result = (result << 4u) | (uint64_t)(unsigned)digit;
  }
  *value = result;
  return true;
}

DbcCandidateFormatStatus dbc_candidate_token_format(
  uint64_t generation,
  uint32_t source_size,
  uint32_t source_crc32,
  char output[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES]) {
  if (output == NULL || generation == 0u || source_size == 0u ||
      source_size > LARGE_DBC_SOURCE_MAX_BYTES) {
    return DBC_CANDIDATE_FORMAT_INVALID_ARGUMENT;
  }
  const int length = snprintf(output,
                              LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES,
                              "%08lX%08lX-%08lX-%08lX",
                              (unsigned long)(generation >> 32u),
                              (unsigned long)(uint32_t)generation,
                              (unsigned long)source_size,
                              (unsigned long)source_crc32);
  return length == (int)LARGE_DBC_CANDIDATE_TOKEN_CHARS ?
    DBC_CANDIDATE_FORMAT_OK : DBC_CANDIDATE_FORMAT_INVALID_FORMAT;
}

DbcCandidateFormatStatus dbc_candidate_token_parse(
  const char *token,
  uint64_t *generation,
  uint32_t *source_size,
  uint32_t *source_crc32) {
  if (token == NULL || generation == NULL || source_size == NULL ||
      source_crc32 == NULL) {
    return DBC_CANDIDATE_FORMAT_INVALID_ARGUMENT;
  }
  if (strlen(token) != LARGE_DBC_CANDIDATE_TOKEN_CHARS || token[16] != '-' ||
      token[25] != '-') {
    return DBC_CANDIDATE_FORMAT_INVALID_FORMAT;
  }
  uint64_t parsed_generation = 0u;
  uint64_t parsed_size = 0u;
  uint64_t parsed_crc = 0u;
  if (!parse_hex(token, 16u, &parsed_generation) ||
      !parse_hex(token + 17u, 8u, &parsed_size) ||
      !parse_hex(token + 26u, 8u, &parsed_crc) || parsed_generation == 0u ||
      parsed_size == 0u || parsed_size > LARGE_DBC_SOURCE_MAX_BYTES) {
    return DBC_CANDIDATE_FORMAT_INVALID_FORMAT;
  }
  *generation = parsed_generation;
  *source_size = (uint32_t)parsed_size;
  *source_crc32 = (uint32_t)parsed_crc;
  return DBC_CANDIDATE_FORMAT_OK;
}

DbcCandidateFormatStatus dbc_candidate_token_verify_manifest(
  const char *token,
  const DbcManifestV1 *manifest) {
  if (manifest == NULL) {
    return DBC_CANDIDATE_FORMAT_INVALID_ARGUMENT;
  }
  uint64_t generation = 0u;
  uint32_t source_size = 0u;
  uint32_t source_crc = 0u;
  DbcCandidateFormatStatus status =
    dbc_candidate_token_parse(token, &generation, &source_size, &source_crc);
  if (status != DBC_CANDIDATE_FORMAT_OK) {
    return status;
  }
  return generation == manifest->generation && source_size == manifest->source_size &&
         source_crc == manifest->source_crc32 ?
    DBC_CANDIDATE_FORMAT_OK : DBC_CANDIDATE_FORMAT_TOKEN_MISMATCH;
}

DbcCandidateFormatStatus dbc_candidate_next_generation(uint64_t current,
                                                        uint64_t *next) {
  if (next == NULL) {
    return DBC_CANDIDATE_FORMAT_INVALID_ARGUMENT;
  }
  if (current == UINT64_MAX) {
    return DBC_CANDIDATE_FORMAT_GENERATION_EXHAUSTED;
  }
  *next = current + 1u;
  return DBC_CANDIDATE_FORMAT_OK;
}

const char *dbc_candidate_format_status_string(DbcCandidateFormatStatus status) {
  switch (status) {
    case DBC_CANDIDATE_FORMAT_OK: return "ok";
    case DBC_CANDIDATE_FORMAT_INVALID_ARGUMENT: return "invalid_argument";
    case DBC_CANDIDATE_FORMAT_INVALID_FORMAT: return "invalid_format";
    case DBC_CANDIDATE_FORMAT_CRC_MISMATCH: return "crc_mismatch";
    case DBC_CANDIDATE_FORMAT_LIMIT_EXCEEDED: return "limit_exceeded";
    case DBC_CANDIDATE_FORMAT_EMPTY_SELECTION: return "empty_selection";
    case DBC_CANDIDATE_FORMAT_REFERENCE_MISMATCH: return "reference_mismatch";
    case DBC_CANDIDATE_FORMAT_TOKEN_MISMATCH: return "token_mismatch";
    case DBC_CANDIDATE_FORMAT_GENERATION_EXHAUSTED: return "generation_exhausted";
    default: return "unknown";
  }
}
