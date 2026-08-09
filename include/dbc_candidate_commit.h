#ifndef DBC_CANDIDATE_COMMIT_H
#define DBC_CANDIDATE_COMMIT_H

#include <stdbool.h>
#include <stdint.h>

#include "large_dbc_contract.h"

typedef struct {
  uint64_t generation;
  uint32_t source_size;
  uint32_t source_crc32;
} DbcCandidateUpload;

/* Semantic manifest fields only; no serialized manifest/selection layout. */
typedef struct {
  uint64_t generation;
  uint64_t selection_generation;
  uint32_t source_size;
  uint32_t source_crc32;
  uint32_t index_size;
  uint32_t index_crc32;
  uint32_t selection_size;
  uint32_t selection_crc32;
  uint16_t catalog_message_count;
  uint16_t catalog_signal_count;
  uint16_t selected_count;
  uint16_t selected_message_count;
} DbcCandidateDescriptor;

typedef enum {
  DBC_CANDIDATE_OBJECT_SOURCE = 0,
  DBC_CANDIDATE_OBJECT_INDEX,
  DBC_CANDIDATE_OBJECT_SELECTION
} DbcCandidateObjectKind;

typedef enum {
  DBC_CANDIDATE_INDEX_BUILD_OK = 0,
  DBC_CANDIDATE_INDEX_SOURCE_READ_FAILED,
  DBC_CANDIDATE_INDEX_PARSE_FAILED,
  DBC_CANDIDATE_INDEX_WRITE_OR_VERIFY_FAILED
} DbcCandidateIndexBuildResult;

typedef enum {
  DBC_CANDIDATE_COMMIT_OK = 0,
  DBC_CANDIDATE_COMMIT_INVALID_ARGUMENT,
  DBC_CANDIDATE_COMMIT_SOURCE_READ_FAILED,
  DBC_CANDIDATE_COMMIT_PARSE_FAILED,
  DBC_CANDIDATE_COMMIT_INDEX_FAILED,
  DBC_CANDIDATE_COMMIT_DESCRIPTOR_INVALID,
  DBC_CANDIDATE_COMMIT_SELECTION_FAILED,
  DBC_CANDIDATE_COMMIT_SOURCE_WRITE_FAILED,
  DBC_CANDIDATE_COMMIT_INDEX_WRITE_FAILED,
  DBC_CANDIDATE_COMMIT_SELECTION_WRITE_FAILED,
  DBC_CANDIDATE_COMMIT_SOURCE_READBACK_FAILED,
  DBC_CANDIDATE_COMMIT_INDEX_READBACK_FAILED,
  DBC_CANDIDATE_COMMIT_SELECTION_READBACK_FAILED,
  DBC_CANDIDATE_COMMIT_CROSS_VERIFY_FAILED,
  DBC_CANDIDATE_COMMIT_MANIFEST_PUBLISH_FAILED
} DbcCandidateCommitStatus;

/*
 * build_index_from_upload must sequentially reread upload.<generation>.tmp,
 * verify the input fingerprint, and drive DbcStreamParser through the existing
 * DbcStreamIndexAdapter/DbcCatalogIndexBuilder.  It returns only after the
 * index tmp is finalized and verified.
 *
 * publish_current_manifest is the sole commit point.  Its implementation must
 * write/readback current.tmp before the recoverable current/previous update;
 * returning false must leave the previously committed current readable.
 */
typedef struct {
  void *context;
  DbcCandidateIndexBuildResult (*build_index_from_upload)(
    void *context,
    const DbcCandidateUpload *upload,
    DbcCandidateDescriptor *candidate);
  bool (*build_default_selection)(void *context,
                                  DbcCandidateDescriptor *candidate);
  bool (*write_generation_object)(void *context,
                                  DbcCandidateObjectKind kind,
                                  const DbcCandidateDescriptor *candidate);
  bool (*verify_generation_object)(void *context,
                                   DbcCandidateObjectKind kind,
                                   const DbcCandidateDescriptor *candidate);
  bool (*cross_verify_generation)(void *context,
                                  const DbcCandidateDescriptor *candidate);
  bool (*publish_current_manifest)(void *context,
                                   const DbcCandidateDescriptor *candidate);
} DbcCandidateCommitCallbacks;

DbcCandidateCommitStatus dbc_candidate_commit(
  const DbcCandidateUpload *upload,
  const DbcCandidateCommitCallbacks *callbacks,
  DbcCandidateDescriptor *published_candidate);

typedef enum {
  DBC_CANDIDATE_SLOT_ABSENT = 0,
  DBC_CANDIDATE_SLOT_INVALID,
  DBC_CANDIDATE_SLOT_VALID
} DbcCandidateSlotState;

typedef struct {
  DbcCandidateSlotState state;
  DbcCandidateDescriptor descriptor;
} DbcCandidateRecoverySlot;

typedef enum {
  DBC_CANDIDATE_RECOVERY_NONE = 0,
  DBC_CANDIDATE_RECOVERY_CURRENT,
  DBC_CANDIDATE_RECOVERY_PREVIOUS
} DbcCandidateRecoveryDecision;

/* VALID means the manifest and every referenced generation object verified. */
DbcCandidateRecoveryDecision dbc_candidate_recovery_choose(
  const DbcCandidateRecoverySlot *current,
  const DbcCandidateRecoverySlot *previous,
  DbcCandidateDescriptor *recovered_candidate);

const char *dbc_candidate_commit_status_string(DbcCandidateCommitStatus status);

#endif
