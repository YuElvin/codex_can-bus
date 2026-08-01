#include "dbc_candidate_commit.h"

#include <string.h>

static bool callbacks_valid(const DbcCandidateCommitCallbacks *callbacks) {
  return callbacks != NULL && callbacks->build_index_from_upload != NULL &&
         callbacks->build_default_selection != NULL &&
         callbacks->write_generation_object != NULL &&
         callbacks->verify_generation_object != NULL &&
         callbacks->cross_verify_generation != NULL &&
         callbacks->publish_current_manifest != NULL;
}

static bool upload_valid(const DbcCandidateUpload *upload) {
  return upload != NULL && upload->generation != 0u && upload->source_size != 0u &&
         upload->source_size <= LARGE_DBC_SOURCE_MAX_BYTES;
}

static bool source_identity_matches(const DbcCandidateDescriptor *candidate,
                                    const DbcCandidateUpload *upload) {
  return candidate->generation == upload->generation &&
         candidate->source_size == upload->source_size &&
         candidate->source_crc32 == upload->source_crc32;
}

static bool indexed_descriptor_valid(const DbcCandidateDescriptor *candidate,
                                     const DbcCandidateUpload *upload) {
  return source_identity_matches(candidate, upload) && candidate->index_size != 0u &&
         candidate->catalog_message_count != 0u &&
         candidate->catalog_message_count <= LARGE_DBC_CATALOG_MAX_MESSAGES &&
         candidate->catalog_signal_count <= LARGE_DBC_CATALOG_MAX_SIGNALS;
}

static bool complete_descriptor_valid(const DbcCandidateDescriptor *candidate,
                                      const DbcCandidateUpload *upload) {
  if (!indexed_descriptor_valid(candidate, upload) ||
      candidate->selection_generation != candidate->generation ||
      candidate->selection_size == 0u ||
      candidate->selected_count > LARGE_DBC_ACTIVE_MAX_SIGNALS ||
      candidate->selected_count > candidate->catalog_signal_count ||
      candidate->selected_message_count > LARGE_DBC_ACTIVE_MAX_MESSAGES ||
      candidate->selected_message_count > candidate->catalog_message_count ||
      (candidate->selected_count == 0u) !=
        (candidate->selected_message_count == 0u)) {
    return false;
  }
  const uint16_t expected_default =
    candidate->catalog_signal_count <= LARGE_DBC_ACTIVE_MAX_SIGNALS ?
      candidate->catalog_signal_count : 0u;
  return candidate->selected_count == expected_default;
}

static DbcCandidateCommitStatus write_status(DbcCandidateObjectKind kind) {
  switch (kind) {
    case DBC_CANDIDATE_OBJECT_SOURCE:
      return DBC_CANDIDATE_COMMIT_SOURCE_WRITE_FAILED;
    case DBC_CANDIDATE_OBJECT_INDEX:
      return DBC_CANDIDATE_COMMIT_INDEX_WRITE_FAILED;
    case DBC_CANDIDATE_OBJECT_SELECTION:
      return DBC_CANDIDATE_COMMIT_SELECTION_WRITE_FAILED;
    default:
      return DBC_CANDIDATE_COMMIT_INVALID_ARGUMENT;
  }
}

static DbcCandidateCommitStatus readback_status(DbcCandidateObjectKind kind) {
  switch (kind) {
    case DBC_CANDIDATE_OBJECT_SOURCE:
      return DBC_CANDIDATE_COMMIT_SOURCE_READBACK_FAILED;
    case DBC_CANDIDATE_OBJECT_INDEX:
      return DBC_CANDIDATE_COMMIT_INDEX_READBACK_FAILED;
    case DBC_CANDIDATE_OBJECT_SELECTION:
      return DBC_CANDIDATE_COMMIT_SELECTION_READBACK_FAILED;
    default:
      return DBC_CANDIDATE_COMMIT_INVALID_ARGUMENT;
  }
}

DbcCandidateCommitStatus dbc_candidate_commit(
  const DbcCandidateUpload *upload,
  const DbcCandidateCommitCallbacks *callbacks,
  DbcCandidateDescriptor *published_candidate) {
  if (!upload_valid(upload) || !callbacks_valid(callbacks) ||
      published_candidate == NULL) {
    return DBC_CANDIDATE_COMMIT_INVALID_ARGUMENT;
  }

  DbcCandidateDescriptor candidate;
  memset(&candidate, 0, sizeof(candidate));
  candidate.generation = upload->generation;
  candidate.source_size = upload->source_size;
  candidate.source_crc32 = upload->source_crc32;
  const DbcCandidateIndexBuildResult build =
    callbacks->build_index_from_upload(callbacks->context, upload, &candidate);
  if (build != DBC_CANDIDATE_INDEX_BUILD_OK) {
    switch (build) {
      case DBC_CANDIDATE_INDEX_SOURCE_READ_FAILED:
        return DBC_CANDIDATE_COMMIT_SOURCE_READ_FAILED;
      case DBC_CANDIDATE_INDEX_PARSE_FAILED:
        return DBC_CANDIDATE_COMMIT_PARSE_FAILED;
      case DBC_CANDIDATE_INDEX_WRITE_OR_VERIFY_FAILED:
        return DBC_CANDIDATE_COMMIT_INDEX_FAILED;
      default:
        return DBC_CANDIDATE_COMMIT_DESCRIPTOR_INVALID;
    }
  }
  if (!indexed_descriptor_valid(&candidate, upload)) {
    return DBC_CANDIDATE_COMMIT_DESCRIPTOR_INVALID;
  }
  if (!callbacks->build_default_selection(callbacks->context, &candidate)) {
    return DBC_CANDIDATE_COMMIT_SELECTION_FAILED;
  }
  if (!complete_descriptor_valid(&candidate, upload)) {
    return DBC_CANDIDATE_COMMIT_DESCRIPTOR_INVALID;
  }

  static const DbcCandidateObjectKind objects[] = {
    DBC_CANDIDATE_OBJECT_SOURCE,
    DBC_CANDIDATE_OBJECT_INDEX,
    DBC_CANDIDATE_OBJECT_SELECTION
  };
  for (size_t i = 0u; i < sizeof(objects) / sizeof(objects[0]); ++i) {
    if (!callbacks->write_generation_object(callbacks->context, objects[i],
                                             &candidate)) {
      return write_status(objects[i]);
    }
  }
  for (size_t i = 0u; i < sizeof(objects) / sizeof(objects[0]); ++i) {
    if (!callbacks->verify_generation_object(callbacks->context, objects[i],
                                              &candidate)) {
      return readback_status(objects[i]);
    }
  }
  if (!callbacks->cross_verify_generation(callbacks->context, &candidate)) {
    return DBC_CANDIDATE_COMMIT_CROSS_VERIFY_FAILED;
  }
  if (!callbacks->publish_current_manifest(callbacks->context, &candidate)) {
    return DBC_CANDIDATE_COMMIT_MANIFEST_PUBLISH_FAILED;
  }

  *published_candidate = candidate;
  return DBC_CANDIDATE_COMMIT_OK;
}

static bool recovery_descriptor_valid(const DbcCandidateRecoverySlot *slot) {
  return slot != NULL && slot->state == DBC_CANDIDATE_SLOT_VALID &&
         slot->descriptor.generation != 0u &&
         slot->descriptor.source_size != 0u &&
         slot->descriptor.source_size <= LARGE_DBC_SOURCE_MAX_BYTES &&
         slot->descriptor.index_size != 0u && slot->descriptor.selection_size != 0u &&
         slot->descriptor.catalog_message_count != 0u &&
         slot->descriptor.catalog_message_count <= LARGE_DBC_CATALOG_MAX_MESSAGES &&
         slot->descriptor.catalog_signal_count <= LARGE_DBC_CATALOG_MAX_SIGNALS &&
         slot->descriptor.selected_count <= LARGE_DBC_ACTIVE_MAX_SIGNALS &&
         slot->descriptor.selected_count <= slot->descriptor.catalog_signal_count &&
         slot->descriptor.selected_message_count <= LARGE_DBC_ACTIVE_MAX_MESSAGES &&
         slot->descriptor.selected_message_count <=
           slot->descriptor.catalog_message_count;
}

DbcCandidateRecoveryDecision dbc_candidate_recovery_choose(
  const DbcCandidateRecoverySlot *current,
  const DbcCandidateRecoverySlot *previous,
  DbcCandidateDescriptor *recovered_candidate) {
  if (recovered_candidate == NULL) {
    return DBC_CANDIDATE_RECOVERY_NONE;
  }
  if (recovery_descriptor_valid(current)) {
    *recovered_candidate = current->descriptor;
    return DBC_CANDIDATE_RECOVERY_CURRENT;
  }
  if (recovery_descriptor_valid(previous)) {
    *recovered_candidate = previous->descriptor;
    return DBC_CANDIDATE_RECOVERY_PREVIOUS;
  }
  return DBC_CANDIDATE_RECOVERY_NONE;
}

const char *dbc_candidate_commit_status_string(DbcCandidateCommitStatus status) {
  switch (status) {
    case DBC_CANDIDATE_COMMIT_OK: return "ok";
    case DBC_CANDIDATE_COMMIT_INVALID_ARGUMENT: return "invalid_argument";
    case DBC_CANDIDATE_COMMIT_SOURCE_READ_FAILED: return "source_read_failed";
    case DBC_CANDIDATE_COMMIT_PARSE_FAILED: return "parse_failed";
    case DBC_CANDIDATE_COMMIT_INDEX_FAILED: return "index_failed";
    case DBC_CANDIDATE_COMMIT_DESCRIPTOR_INVALID: return "descriptor_invalid";
    case DBC_CANDIDATE_COMMIT_SELECTION_FAILED: return "selection_failed";
    case DBC_CANDIDATE_COMMIT_SOURCE_WRITE_FAILED: return "source_write_failed";
    case DBC_CANDIDATE_COMMIT_INDEX_WRITE_FAILED: return "index_write_failed";
    case DBC_CANDIDATE_COMMIT_SELECTION_WRITE_FAILED:
      return "selection_write_failed";
    case DBC_CANDIDATE_COMMIT_SOURCE_READBACK_FAILED:
      return "source_readback_failed";
    case DBC_CANDIDATE_COMMIT_INDEX_READBACK_FAILED:
      return "index_readback_failed";
    case DBC_CANDIDATE_COMMIT_SELECTION_READBACK_FAILED:
      return "selection_readback_failed";
    case DBC_CANDIDATE_COMMIT_CROSS_VERIFY_FAILED:
      return "cross_verify_failed";
    case DBC_CANDIDATE_COMMIT_MANIFEST_PUBLISH_FAILED:
      return "manifest_publish_failed";
    default: return "unknown";
  }
}
