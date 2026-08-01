#ifndef DBC_CANDIDATE_FORMAT_H
#define DBC_CANDIDATE_FORMAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "large_dbc_contract.h"

typedef enum {
  DBC_CANDIDATE_FORMAT_OK = 0,
  DBC_CANDIDATE_FORMAT_INVALID_ARGUMENT,
  DBC_CANDIDATE_FORMAT_INVALID_FORMAT,
  DBC_CANDIDATE_FORMAT_CRC_MISMATCH,
  DBC_CANDIDATE_FORMAT_LIMIT_EXCEEDED,
  DBC_CANDIDATE_FORMAT_EMPTY_SELECTION,
  DBC_CANDIDATE_FORMAT_REFERENCE_MISMATCH,
  DBC_CANDIDATE_FORMAT_TOKEN_MISMATCH,
  DBC_CANDIDATE_FORMAT_GENERATION_EXHAUSTED
} DbcCandidateFormatStatus;

typedef struct {
  uint8_t object_kind;
  uint64_t generation;
  uint32_t source_size;
  uint32_t source_crc32;
  uint32_t index_size;
  uint32_t index_crc32;
  uint32_t selection_size;
  uint32_t selection_crc32;
  uint16_t selected_count;
  uint16_t selected_message_count;
  uint16_t catalog_message_count;
  uint16_t catalog_signal_count;
} DbcManifestV1;

/* Facts obtained only after the manifest's referenced files were verified. */
typedef struct {
  uint64_t generation;
  uint32_t source_size;
  uint32_t source_crc32;
  uint32_t index_size;
  uint32_t index_crc32;
  uint32_t selection_size;
  uint32_t selection_crc32;
  uint16_t selected_count;
  uint16_t selected_message_count;
  uint16_t catalog_message_count;
  uint16_t catalog_signal_count;
} DbcManifestReferenceFacts;

typedef enum {
  DBC_MANIFEST_RECOVERY_NONE = 0,
  DBC_MANIFEST_RECOVERY_CURRENT,
  DBC_MANIFEST_RECOVERY_PREVIOUS
} DbcManifestRecoverySlot;

typedef struct {
  uint64_t candidate_generation;
  uint64_t selection_generation;
  uint32_t source_size;
  uint32_t source_crc32;
  uint16_t catalog_signal_count;
  uint16_t selected_count;
  uint8_t bitmap[LARGE_DBC_SELECTION_BITMAP_BYTES];
} DbcSelectionV1;

typedef struct {
  uint32_t source_size;
  uint32_t source_crc32;
  uint32_t index_size;
  uint32_t index_crc32;
  uint16_t catalog_message_count;
  uint16_t catalog_signal_count;
} DbcCandidateIndexFacts;

uint32_t dbc_candidate_crc32(const uint8_t *data, size_t size);

DbcCandidateFormatStatus dbc_manifest_v1_encode(
  const DbcManifestV1 *manifest,
  uint8_t output[LARGE_DBC_MANIFEST_SIZE]);
DbcCandidateFormatStatus dbc_manifest_v1_decode(
  const uint8_t *bytes,
  size_t size,
  DbcManifestV1 *manifest);
DbcCandidateFormatStatus dbc_manifest_v1_verify_references(
  const DbcManifestV1 *manifest,
  const DbcManifestReferenceFacts *facts);
DbcManifestRecoverySlot dbc_manifest_v1_select_current_or_previous(
  const uint8_t *current_bytes,
  size_t current_size,
  const DbcManifestReferenceFacts *current_facts,
  const uint8_t *previous_bytes,
  size_t previous_size,
  const DbcManifestReferenceFacts *previous_facts,
  uint8_t expected_object_kind,
  DbcManifestV1 *selected_manifest);

DbcCandidateFormatStatus dbc_selection_v1_init_default(
  DbcSelectionV1 *selection,
  uint64_t candidate_generation,
  uint64_t selection_generation,
  uint32_t source_size,
  uint32_t source_crc32,
  uint16_t catalog_signal_count);
bool dbc_selection_v1_is_selected(const DbcSelectionV1 *selection,
                                  uint16_t ordinal);
DbcCandidateFormatStatus dbc_selection_v1_encode(
  const DbcSelectionV1 *selection,
  uint8_t output[LARGE_DBC_SELECTION_TOTAL_SIZE]);
DbcCandidateFormatStatus dbc_selection_v1_decode(
  const uint8_t *bytes,
  size_t size,
  DbcSelectionV1 *selection);
DbcCandidateFormatStatus dbc_selection_v1_verify_activation(
  const DbcSelectionV1 *selection,
  uint16_t selected_message_count);

DbcCandidateFormatStatus dbc_candidate_v1_verify_set(
  const DbcManifestV1 *manifest,
  const DbcCandidateIndexFacts *index,
  const uint8_t *selection_bytes,
  size_t selection_size,
  uint16_t selected_message_count,
  DbcSelectionV1 *decoded_selection);

/* Active generation is independent of the candidate/selection generation. */
DbcCandidateFormatStatus dbc_active_v1_verify_set(
  const DbcManifestV1 *manifest,
  const DbcCandidateIndexFacts *index,
  const uint8_t *selection_bytes,
  size_t selection_size,
  uint16_t selected_message_count,
  DbcSelectionV1 *decoded_selection);

DbcCandidateFormatStatus dbc_candidate_token_format(
  uint64_t generation,
  uint32_t source_size,
  uint32_t source_crc32,
  char output[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES]);
DbcCandidateFormatStatus dbc_candidate_token_parse(
  const char *token,
  uint64_t *generation,
  uint32_t *source_size,
  uint32_t *source_crc32);
DbcCandidateFormatStatus dbc_candidate_token_verify_manifest(
  const char *token,
  const DbcManifestV1 *manifest);

DbcCandidateFormatStatus dbc_candidate_next_generation(uint64_t current,
                                                        uint64_t *next);
const char *dbc_candidate_format_status_string(DbcCandidateFormatStatus status);

#endif
