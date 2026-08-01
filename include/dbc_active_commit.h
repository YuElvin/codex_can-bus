#ifndef DBC_ACTIVE_COMMIT_H
#define DBC_ACTIVE_COMMIT_H

#include <stdbool.h>
#include <stdint.h>

#include "dbc_candidate_commit.h"

typedef struct {
  uint64_t active_generation;
  uint64_t candidate_generation;
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
} DbcActiveDescriptor;

typedef enum {
  DBC_ACTIVE_COMMIT_OK = 0,
  DBC_ACTIVE_COMMIT_INVALID_ARGUMENT,
  DBC_ACTIVE_COMMIT_EMPTY_SELECTION,
  DBC_ACTIVE_COMMIT_SELECTION_LIMIT,
  DBC_ACTIVE_COMMIT_CANDIDATE_VERIFY_FAILED,
  DBC_ACTIVE_COMMIT_LOGGING_ACTIVE,
  DBC_ACTIVE_COMMIT_RULE_KEY_MISSING,
  DBC_ACTIVE_COMMIT_RULE_DEFINITION_CONFLICT,
  DBC_ACTIVE_COMMIT_RUNTIME_BUILD_FAILED,
  DBC_ACTIVE_COMMIT_RUNTIME_VERIFY_FAILED,
  DBC_ACTIVE_COMMIT_SOURCE_WRITE_FAILED,
  DBC_ACTIVE_COMMIT_INDEX_WRITE_FAILED,
  DBC_ACTIVE_COMMIT_SELECTION_WRITE_FAILED,
  DBC_ACTIVE_COMMIT_SOURCE_READBACK_FAILED,
  DBC_ACTIVE_COMMIT_INDEX_READBACK_FAILED,
  DBC_ACTIVE_COMMIT_SELECTION_READBACK_FAILED,
  DBC_ACTIVE_COMMIT_CROSS_VERIFY_FAILED,
  DBC_ACTIVE_COMMIT_MANIFEST_PUBLISH_FAILED
} DbcActiveCommitStatus;

typedef enum {
  DBC_ACTIVE_RULES_OK = 0,
  DBC_ACTIVE_RULES_KEY_MISSING,
  DBC_ACTIVE_RULES_DEFINITION_CONFLICT,
  DBC_ACTIVE_RULES_INVALID
} DbcActiveRulesStatus;

/*
 * All callbacks before publish_current_manifest operate on non-active state.
 * publish_current_manifest is the persistent commit point.  publish_runtime
 * must be a non-failing, short critical-section pointer/generation flip; it
 * must not perform filesystem I/O.
 */
typedef struct {
  void *context;
  bool (*verify_candidate)(void *context,
                           const DbcCandidateDescriptor *candidate);
  bool (*logging_is_active)(void *context);
  DbcActiveRulesStatus (*check_rules)(
    void *context, const DbcCandidateDescriptor *candidate);
  bool (*build_inactive_runtime)(
    void *context, const DbcActiveDescriptor *active);
  bool (*verify_inactive_runtime)(
    void *context, const DbcActiveDescriptor *active);
  bool (*write_generation_object)(
    void *context, DbcCandidateObjectKind kind,
    const DbcActiveDescriptor *active);
  bool (*verify_generation_object)(
    void *context, DbcCandidateObjectKind kind,
    const DbcActiveDescriptor *active);
  bool (*cross_verify_generation)(
    void *context, const DbcActiveDescriptor *active);
  bool (*publish_current_manifest)(
    void *context, const DbcActiveDescriptor *active);
  void (*publish_runtime)(void *context, const DbcActiveDescriptor *active);
} DbcActiveCommitCallbacks;

DbcActiveCommitStatus dbc_active_commit(
  uint64_t active_generation,
  const DbcCandidateDescriptor *candidate,
  const DbcActiveCommitCallbacks *callbacks,
  DbcActiveDescriptor *published_active);

const char *dbc_active_commit_status_string(DbcActiveCommitStatus status);

#endif
