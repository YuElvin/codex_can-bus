#include "dbc_active_commit.h"

#include <string.h>

static bool callbacks_valid(const DbcActiveCommitCallbacks *callbacks) {
  return callbacks != NULL && callbacks->verify_candidate != NULL &&
         callbacks->logging_is_active != NULL &&
         callbacks->check_rules != NULL &&
         callbacks->build_inactive_runtime != NULL &&
         callbacks->verify_inactive_runtime != NULL &&
         callbacks->write_generation_object != NULL &&
         callbacks->verify_generation_object != NULL &&
         callbacks->cross_verify_generation != NULL &&
         callbacks->publish_current_manifest != NULL &&
         callbacks->publish_runtime != NULL;
}

static bool candidate_valid(const DbcCandidateDescriptor *candidate) {
  return candidate != NULL && candidate->generation != 0u &&
         candidate->selection_generation == candidate->generation &&
         candidate->source_size != 0u &&
         candidate->source_size <= LARGE_DBC_SOURCE_MAX_BYTES &&
         candidate->index_size != 0u && candidate->selection_size != 0u &&
         candidate->catalog_message_count != 0u &&
         candidate->catalog_message_count <= LARGE_DBC_CATALOG_MAX_MESSAGES &&
         candidate->catalog_signal_count != 0u &&
         candidate->catalog_signal_count <= LARGE_DBC_CATALOG_MAX_SIGNALS &&
         candidate->selected_count <= candidate->catalog_signal_count &&
         candidate->selected_message_count <= candidate->catalog_message_count;
}

static DbcActiveDescriptor active_from_candidate(
  uint64_t active_generation, const DbcCandidateDescriptor *candidate) {
  const DbcActiveDescriptor active = {
    .active_generation = active_generation,
    .candidate_generation = candidate->generation,
    .selection_generation = candidate->selection_generation,
    .source_size = candidate->source_size,
    .source_crc32 = candidate->source_crc32,
    .index_size = candidate->index_size,
    .index_crc32 = candidate->index_crc32,
    .selection_size = candidate->selection_size,
    .selection_crc32 = candidate->selection_crc32,
    .catalog_message_count = candidate->catalog_message_count,
    .catalog_signal_count = candidate->catalog_signal_count,
    .selected_count = candidate->selected_count,
    .selected_message_count = candidate->selected_message_count
  };
  return active;
}

static DbcActiveCommitStatus write_failure(DbcCandidateObjectKind kind) {
  switch (kind) {
    case DBC_CANDIDATE_OBJECT_SOURCE:
      return DBC_ACTIVE_COMMIT_SOURCE_WRITE_FAILED;
    case DBC_CANDIDATE_OBJECT_INDEX:
      return DBC_ACTIVE_COMMIT_INDEX_WRITE_FAILED;
    case DBC_CANDIDATE_OBJECT_SELECTION:
      return DBC_ACTIVE_COMMIT_SELECTION_WRITE_FAILED;
    default:
      return DBC_ACTIVE_COMMIT_INVALID_ARGUMENT;
  }
}

static DbcActiveCommitStatus readback_failure(DbcCandidateObjectKind kind) {
  switch (kind) {
    case DBC_CANDIDATE_OBJECT_SOURCE:
      return DBC_ACTIVE_COMMIT_SOURCE_READBACK_FAILED;
    case DBC_CANDIDATE_OBJECT_INDEX:
      return DBC_ACTIVE_COMMIT_INDEX_READBACK_FAILED;
    case DBC_CANDIDATE_OBJECT_SELECTION:
      return DBC_ACTIVE_COMMIT_SELECTION_READBACK_FAILED;
    default:
      return DBC_ACTIVE_COMMIT_INVALID_ARGUMENT;
  }
}

DbcActiveCommitStatus dbc_active_commit(
  uint64_t active_generation,
  const DbcCandidateDescriptor *candidate,
  const DbcActiveCommitCallbacks *callbacks,
  DbcActiveDescriptor *published_active) {
  if (active_generation == 0u || !candidate_valid(candidate) ||
      !callbacks_valid(callbacks) || published_active == NULL) {
    return DBC_ACTIVE_COMMIT_INVALID_ARGUMENT;
  }
  if (candidate->selected_count == 0u ||
      candidate->selected_message_count == 0u) {
    return DBC_ACTIVE_COMMIT_EMPTY_SELECTION;
  }
  if (candidate->selected_count > LARGE_DBC_ACTIVE_MAX_SIGNALS ||
      candidate->selected_message_count > LARGE_DBC_ACTIVE_MAX_MESSAGES) {
    return DBC_ACTIVE_COMMIT_SELECTION_LIMIT;
  }
  if (!callbacks->verify_candidate(callbacks->context, candidate)) {
    return DBC_ACTIVE_COMMIT_CANDIDATE_VERIFY_FAILED;
  }
  if (callbacks->logging_is_active(callbacks->context)) {
    return DBC_ACTIVE_COMMIT_LOGGING_ACTIVE;
  }
  switch (callbacks->check_rules(callbacks->context, candidate)) {
    case DBC_ACTIVE_RULES_OK:
      break;
    case DBC_ACTIVE_RULES_KEY_MISSING:
      return DBC_ACTIVE_COMMIT_RULE_KEY_MISSING;
    case DBC_ACTIVE_RULES_DEFINITION_CONFLICT:
      return DBC_ACTIVE_COMMIT_RULE_DEFINITION_CONFLICT;
    default:
      return DBC_ACTIVE_COMMIT_INVALID_ARGUMENT;
  }

  const DbcActiveDescriptor active =
    active_from_candidate(active_generation, candidate);
  if (!callbacks->build_inactive_runtime(callbacks->context, &active)) {
    return DBC_ACTIVE_COMMIT_RUNTIME_BUILD_FAILED;
  }
  if (!callbacks->verify_inactive_runtime(callbacks->context, &active)) {
    return DBC_ACTIVE_COMMIT_RUNTIME_VERIFY_FAILED;
  }

  static const DbcCandidateObjectKind objects[] = {
    DBC_CANDIDATE_OBJECT_SOURCE,
    DBC_CANDIDATE_OBJECT_INDEX,
    DBC_CANDIDATE_OBJECT_SELECTION
  };
  for (size_t i = 0u; i < sizeof(objects) / sizeof(objects[0]); ++i) {
    if (!callbacks->write_generation_object(callbacks->context, objects[i],
                                             &active)) {
      return write_failure(objects[i]);
    }
  }
  for (size_t i = 0u; i < sizeof(objects) / sizeof(objects[0]); ++i) {
    if (!callbacks->verify_generation_object(callbacks->context, objects[i],
                                              &active)) {
      return readback_failure(objects[i]);
    }
  }
  if (!callbacks->cross_verify_generation(callbacks->context, &active)) {
    return DBC_ACTIVE_COMMIT_CROSS_VERIFY_FAILED;
  }
  if (!callbacks->publish_current_manifest(callbacks->context, &active)) {
    return DBC_ACTIVE_COMMIT_MANIFEST_PUBLISH_FAILED;
  }
  callbacks->publish_runtime(callbacks->context, &active);
  *published_active = active;
  return DBC_ACTIVE_COMMIT_OK;
}

const char *dbc_active_commit_status_string(DbcActiveCommitStatus status) {
  switch (status) {
    case DBC_ACTIVE_COMMIT_OK: return "ok";
    case DBC_ACTIVE_COMMIT_INVALID_ARGUMENT: return "invalid_argument";
    case DBC_ACTIVE_COMMIT_EMPTY_SELECTION: return "empty_selection";
    case DBC_ACTIVE_COMMIT_SELECTION_LIMIT: return "selection_limit";
    case DBC_ACTIVE_COMMIT_CANDIDATE_VERIFY_FAILED:
      return "candidate_verify_failed";
    case DBC_ACTIVE_COMMIT_LOGGING_ACTIVE: return "logging_active";
    case DBC_ACTIVE_COMMIT_RULE_KEY_MISSING: return "rule_key_missing";
    case DBC_ACTIVE_COMMIT_RULE_DEFINITION_CONFLICT:
      return "rule_definition_conflict";
    case DBC_ACTIVE_COMMIT_RUNTIME_BUILD_FAILED: return "runtime_build_failed";
    case DBC_ACTIVE_COMMIT_RUNTIME_VERIFY_FAILED:
      return "runtime_verify_failed";
    case DBC_ACTIVE_COMMIT_SOURCE_WRITE_FAILED: return "source_write_failed";
    case DBC_ACTIVE_COMMIT_INDEX_WRITE_FAILED: return "index_write_failed";
    case DBC_ACTIVE_COMMIT_SELECTION_WRITE_FAILED:
      return "selection_write_failed";
    case DBC_ACTIVE_COMMIT_SOURCE_READBACK_FAILED:
      return "source_readback_failed";
    case DBC_ACTIVE_COMMIT_INDEX_READBACK_FAILED:
      return "index_readback_failed";
    case DBC_ACTIVE_COMMIT_SELECTION_READBACK_FAILED:
      return "selection_readback_failed";
    case DBC_ACTIVE_COMMIT_CROSS_VERIFY_FAILED:
      return "cross_verify_failed";
    case DBC_ACTIVE_COMMIT_MANIFEST_PUBLISH_FAILED:
      return "manifest_publish_failed";
    default: return "unknown";
  }
}
