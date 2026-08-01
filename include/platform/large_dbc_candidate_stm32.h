#ifndef LARGE_DBC_CANDIDATE_STM32_H
#define LARGE_DBC_CANDIDATE_STM32_H

#include <stdbool.h>
#include <stdint.h>

#include "dbc_candidate_catalog.h"
#include "dbc_candidate_commit.h"
#include "dbc_active_commit.h"
#include "dbc_selected_runtime.h"

typedef enum {
  LARGE_DBC_CANDIDATE_IO_OPEN = 0,
  LARGE_DBC_CANDIDATE_IO_CLOSE,
  LARGE_DBC_CANDIDATE_IO_READ,
  LARGE_DBC_CANDIDATE_IO_WRITE,
  LARGE_DBC_CANDIDATE_IO_SEEK,
  LARGE_DBC_CANDIDATE_IO_SYNC,
  LARGE_DBC_CANDIDATE_IO_TRUNCATE,
  LARGE_DBC_CANDIDATE_IO_STAT,
  LARGE_DBC_CANDIDATE_IO_UNLINK,
  LARGE_DBC_CANDIDATE_IO_RENAME
} LargeDbcCandidateIoOperation;

typedef void (*LargeDbcCandidateProgressCallback)(
  void *context,
  LargeDbcCandidateIoOperation operation,
  uint32_t transferred_bytes,
  int io_result);

typedef enum {
  LARGE_DBC_CANDIDATE_STM32_OK = 0,
  LARGE_DBC_CANDIDATE_STM32_INVALID_ARGUMENT,
  LARGE_DBC_CANDIDATE_STM32_LOCK_FAILED,
  LARGE_DBC_CANDIDATE_STM32_PATH_FAILED,
  LARGE_DBC_CANDIDATE_STM32_IO_FAILED,
  LARGE_DBC_CANDIDATE_STM32_COLLISION,
  LARGE_DBC_CANDIDATE_STM32_SOURCE_MISMATCH,
  LARGE_DBC_CANDIDATE_STM32_PARSE_FAILED,
  LARGE_DBC_CANDIDATE_STM32_INDEX_FAILED,
  LARGE_DBC_CANDIDATE_STM32_FORMAT_FAILED,
  LARGE_DBC_CANDIDATE_STM32_VERIFY_FAILED,
  LARGE_DBC_CANDIDATE_STM32_MANIFEST_FAILED,
  LARGE_DBC_CANDIDATE_STM32_GENERATION_EXHAUSTED,
  LARGE_DBC_CANDIDATE_STM32_NOT_FOUND,
  LARGE_DBC_CANDIDATE_STM32_QUERY_FAILED,
  LARGE_DBC_CANDIDATE_STM32_TOKEN_MISMATCH,
  LARGE_DBC_CANDIDATE_STM32_WRITE_BLOCKED,
  LARGE_DBC_CANDIDATE_STM32_SELECTION_FAILED
} LargeDbcCandidateStm32Status;

typedef struct {
  DbcCandidateDescriptor candidate;
  char candidate_token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES];
} LargeDbcCandidateSnapshot;

typedef struct {
  LargeDbcCandidateSnapshot snapshot;
  DbcCandidateCatalogPage page;
} LargeDbcCandidateCatalogResult;

typedef enum {
  LARGE_DBC_ACTIVE_STM32_OK = 0,
  LARGE_DBC_ACTIVE_STM32_INVALID_ARGUMENT,
  LARGE_DBC_ACTIVE_STM32_LOCK_FAILED,
  LARGE_DBC_ACTIVE_STM32_NOT_FOUND,
  LARGE_DBC_ACTIVE_STM32_VERIFY_FAILED,
  LARGE_DBC_ACTIVE_STM32_GENERATION_EXHAUSTED,
  LARGE_DBC_ACTIVE_STM32_LOGGING_ACTIVE,
  LARGE_DBC_ACTIVE_STM32_RULE_KEY_MISSING,
  LARGE_DBC_ACTIVE_STM32_RULE_DEFINITION_CONFLICT,
  LARGE_DBC_ACTIVE_STM32_RUNTIME_FAILED,
  LARGE_DBC_ACTIVE_STM32_COLLISION,
  LARGE_DBC_ACTIVE_STM32_IO_FAILED,
  LARGE_DBC_ACTIVE_STM32_MANIFEST_FAILED
} LargeDbcActiveStm32Status;

typedef DbcSelectedRuntimeStatus (*LargeDbcActivePublishCallback)(
  void *context,
  DbcSelectedRuntimeSnapshot *runtime_snapshot,
  const DbcActiveDescriptor *active,
  uint8_t prepared_slot);

typedef struct {
  uint64_t active_generation;
  bool writes_blocked;
  const DbcSelectedRuleRequirement *rules;
  size_t rule_count;
  DbcSelectedRuntimeSnapshot *runtime_snapshot;
  LargeDbcActivePublishCallback publish;
  void *publish_context;
} LargeDbcActiveRequest;

typedef struct {
  DbcActiveDescriptor active;
  uint8_t runtime_slot;
} LargeDbcActiveResult;

/*
 * Synchronous DbcTask backend.  The progress callback runs after every FatFs
 * call and must not perform filesystem I/O.  Completion publishes only the
 * candidate manifest; active/runtime state is outside this module.
 */
LargeDbcCandidateStm32Status stm32h750_large_dbc_candidate_commit(
  const DbcCandidateUpload *upload,
  LargeDbcCandidateProgressCallback progress,
  void *progress_context,
  DbcCandidateDescriptor *published_candidate);

/*
 * Verifies current and previous independently, chooses current first, and
 * derives max(valid generations)+1.  available=false is a valid empty state.
 */
LargeDbcCandidateStm32Status stm32h750_large_dbc_candidate_recover(
  LargeDbcCandidateProgressCallback progress,
  void *progress_context,
  bool *available,
  DbcCandidateDescriptor *recovered_candidate,
  uint64_t *next_generation);

/*
 * Synchronous, non-reentrant DbcTask APIs.  Each call holds the existing FatFs
 * mutex, fully validates current and its referenced files, and releases all
 * files before returning.  The catalog result contains at most eight items.
 */
LargeDbcCandidateStm32Status stm32h750_large_dbc_candidate_query(
  const DbcCandidateCatalogQuery *query,
  LargeDbcCandidateProgressCallback progress,
  void *progress_context,
  LargeDbcCandidateCatalogResult *result);

/*
 * Persists a selection update as a new immutable generation.  Generation
 * files are copied/written through owned .tmp files and verified before the
 * recoverable current/previous manifest rotation.  No active/runtime state is
 * changed by this function.
 */
LargeDbcCandidateStm32Status stm32h750_large_dbc_candidate_update_selection(
  const DbcCandidateSelectionMutation *mutation,
  LargeDbcCandidateProgressCallback progress,
  void *progress_context,
  LargeDbcCandidateSnapshot *published_candidate);

/*
 * DbcTask-only active transaction.  All FatFs work completes before publish
 * is called; publish must only perform the short RTOS runtime/manifest flip.
 */
LargeDbcActiveStm32Status stm32h750_large_dbc_active_commit(
  const LargeDbcActiveRequest *request,
  LargeDbcCandidateProgressCallback progress,
  void *progress_context,
  LargeDbcActiveResult *result);

/* current is preferred; previous is used only when current is invalid. */
LargeDbcActiveStm32Status stm32h750_large_dbc_active_recover(
  const DbcSelectedRuleRequirement *rules,
  size_t rule_count,
  DbcSelectedRuntimeSnapshot *runtime_snapshot,
  LargeDbcActivePublishCallback publish,
  void *publish_context,
  LargeDbcCandidateProgressCallback progress,
  void *progress_context,
  bool *available,
  LargeDbcActiveResult *result,
  uint64_t *next_generation);

const char *stm32h750_large_dbc_active_status_string(
  LargeDbcActiveStm32Status status);

const char *stm32h750_large_dbc_candidate_status_string(
  LargeDbcCandidateStm32Status status);

#endif
