#include "dbc_selected_runtime.h"

#include <string.h>

_Static_assert(sizeof(DbcSelectedRuntimeSnapshot) <=
                 LARGE_DBC_STATIC_RAM_BUDGET_BYTES,
               "selected runtime and value slots exceeded the P0 RAM budget");
_Static_assert(sizeof(SignalValueState) <= 32u,
               "signal value state must remain a small numeric slot");
_Static_assert(sizeof(SignalValueSnapshot) <= 32u,
               "signal value snapshot must remain a small numeric copy");

static uint8_t inactive_slot(const DbcSelectedRuntimeSnapshot *snapshot) {
  return snapshot->has_active ? (uint8_t)(snapshot->active_slot ^ 1u) : 0u;
}

void dbc_selected_runtime_snapshot_init(DbcSelectedRuntimeSnapshot *snapshot) {
  if (snapshot != NULL) {
    memset(snapshot, 0, sizeof(*snapshot));
  }
}

static DbcSelectedRuntimeStatus validate_request(
  const DbcSelectedRuntimeBuildRequest *request) {
  if (request == NULL || request->index == NULL ||
      request->index_summary == NULL || request->selection == NULL ||
      request->runtime_generation == 0u ||
      (request->rules == NULL && request->rule_count != 0u)) {
    return DBC_SELECTED_RUNTIME_INVALID_ARGUMENT;
  }
  if (request->index_summary->message_count == 0u ||
      request->index_summary->message_count > LARGE_DBC_CATALOG_MAX_MESSAGES ||
      request->index_summary->signal_count == 0u ||
      request->index_summary->signal_count > LARGE_DBC_CATALOG_MAX_SIGNALS ||
      request->selection->candidate_generation == 0u ||
      request->selection->selection_generation == 0u ||
      request->selection->source_size != request->index_summary->source_size ||
      request->selection->source_crc32 != request->index_summary->source_crc32 ||
      request->selection->catalog_signal_count !=
        request->index_summary->signal_count) {
    return DBC_SELECTED_RUNTIME_INDEX_MISMATCH;
  }
  uint8_t encoded[LARGE_DBC_SELECTION_TOTAL_SIZE];
  const DbcCandidateFormatStatus selection_status =
    dbc_selection_v1_encode(request->selection, encoded);
  if (selection_status == DBC_CANDIDATE_FORMAT_EMPTY_SELECTION) {
    return DBC_SELECTED_RUNTIME_EMPTY_SELECTION;
  }
  if (selection_status != DBC_CANDIDATE_FORMAT_OK) {
    return DBC_SELECTED_RUNTIME_INVALID_SELECTION;
  }
  if (dbc_candidate_crc32(encoded, sizeof(encoded)) != request->selection_crc32) {
    return DBC_SELECTED_RUNTIME_INVALID_SELECTION;
  }
  if (request->selection->selected_count == 0u) {
    return DBC_SELECTED_RUNTIME_EMPTY_SELECTION;
  }
  if (request->selection->selected_count > LARGE_DBC_ACTIVE_MAX_SIGNALS) {
    return DBC_SELECTED_RUNTIME_SIGNAL_LIMIT;
  }
  return DBC_SELECTED_RUNTIME_OK;
}

static DbcSelectedRuntimeStatus read_signal(
  const DbcSelectedRuntimeBuildRequest *request,
  uint16_t ordinal,
  DbcCatalogIndexSignal *signal) {
  const DbcCatalogIndexStatus status =
    dbc_catalog_index_read_signal(request->index,
                                  request->index_summary,
                                  ordinal,
                                  signal);
  if (status != DBC_CATALOG_INDEX_OK) {
    return DBC_SELECTED_RUNTIME_INDEX_ERROR;
  }
  return signal->ordinal == ordinal ? DBC_SELECTED_RUNTIME_OK :
    DBC_SELECTED_RUNTIME_INDEX_MISMATCH;
}

static DbcSelectedRuntimeStatus read_message(
  const DbcSelectedRuntimeBuildRequest *request,
  uint16_t ordinal,
  DbcCatalogIndexMessage *message) {
  const DbcCatalogIndexStatus status =
    dbc_catalog_index_read_message(request->index,
                                   request->index_summary,
                                   ordinal,
                                   message);
  return status == DBC_CATALOG_INDEX_OK ? DBC_SELECTED_RUNTIME_OK :
    DBC_SELECTED_RUNTIME_INDEX_ERROR;
}

static int find_runtime_message(const DbcSelectedRuntime *runtime,
                                uint16_t catalog_message_ordinal) {
  for (uint16_t i = 0u; i < runtime->message_count; ++i) {
    if (runtime->messages[i].catalog_message_ordinal == catalog_message_ordinal) {
      return (int)i;
    }
  }
  return -1;
}

static DbcSelectedRuntimeStatus add_message(
  DbcSelectedRuntime *runtime,
  const DbcCatalogIndexMessage *catalog_message,
  uint16_t *runtime_message_index) {
  const int existing =
    find_runtime_message(runtime, catalog_message->ordinal);
  if (existing >= 0) {
    *runtime_message_index = (uint16_t)existing;
    return DBC_SELECTED_RUNTIME_OK;
  }
  if (runtime->message_count >= LARGE_DBC_ACTIVE_MAX_MESSAGES) {
    return DBC_SELECTED_RUNTIME_MESSAGE_LIMIT;
  }
  const uint16_t index = runtime->message_count;
  DbcSelectedRuntimeMessage *message = &runtime->messages[index];
  message->normalized_id = catalog_message->normalized_id;
  message->catalog_message_ordinal = catalog_message->ordinal;
  message->first_signal_index = runtime->signal_count;
  message->signal_count = 0u;
  message->flags = catalog_message->flags;
  message->declared_payload_length = catalog_message->declared_payload_length;
  ++runtime->message_count;
  *runtime_message_index = index;
  return DBC_SELECTED_RUNTIME_OK;
}

static DbcSelectedRuntimeStatus append_signal(
  DbcSelectedRuntime *runtime,
  const DbcCatalogIndexSignal *catalog_signal,
  const DbcCatalogIndexMessage *catalog_message) {
  if (runtime->signal_count >= LARGE_DBC_ACTIVE_MAX_SIGNALS) {
    return DBC_SELECTED_RUNTIME_SIGNAL_LIMIT;
  }
  const uint8_t identity_flags =
    (uint8_t)(catalog_signal->flags &
              (LARGE_DBC_SIGNAL_FLAG_IDE | LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED));
  if (catalog_signal->message_ordinal != catalog_message->ordinal ||
      catalog_signal->normalized_id != catalog_message->normalized_id ||
      identity_flags != catalog_message->flags ||
      catalog_signal->declared_payload_length !=
        catalog_message->declared_payload_length ||
      catalog_signal->ordinal < catalog_message->first_signal_ordinal ||
      catalog_signal->ordinal >=
        (uint32_t)catalog_message->first_signal_ordinal +
          catalog_message->signal_count) {
    return DBC_SELECTED_RUNTIME_INDEX_MISMATCH;
  }
  uint16_t message_index = 0u;
  DbcSelectedRuntimeStatus status =
    add_message(runtime, catalog_message, &message_index);
  if (status != DBC_SELECTED_RUNTIME_OK) {
    return status;
  }
  const uint16_t signal_index = runtime->signal_count;
  DbcSelectedRuntimeSignal *signal = &runtime->signals[signal_index];
  signal->catalog_ordinal = catalog_signal->ordinal;
  signal->catalog_message_ordinal = catalog_signal->message_ordinal;
  signal->runtime_message_index = message_index;
  signal->value_state_index = signal_index;
  signal->normalized_id = catalog_signal->normalized_id;
  signal->start_bit = catalog_signal->start_bit;
  signal->bit_length = catalog_signal->bit_length;
  signal->flags = catalog_signal->flags;
  signal->declared_payload_length = catalog_signal->declared_payload_length;
  signal->factor = catalog_signal->factor;
  signal->offset = catalog_signal->offset;
  signal->minimum = catalog_signal->minimum;
  signal->maximum = catalog_signal->maximum;
  signal->definition_hash = catalog_signal->definition_hash;
  memcpy(signal->key, catalog_signal->key, sizeof(signal->key));
  memcpy(signal->unit, catalog_signal->unit, sizeof(signal->unit));
  ++runtime->signal_count;
  ++runtime->messages[message_index].signal_count;
  return DBC_SELECTED_RUNTIME_OK;
}

const DbcSelectedRuntimeSignal *dbc_selected_runtime_find_signal(
  const DbcSelectedRuntime *runtime,
  const char *key) {
  if (runtime == NULL || key == NULL) {
    return NULL;
  }
  for (uint16_t i = 0u; i < runtime->signal_count; ++i) {
    if (strcmp(runtime->signals[i].key, key) == 0) {
      return &runtime->signals[i];
    }
  }
  return NULL;
}

static DbcSelectedRuntimeStatus check_rules(
  const DbcSelectedRuntime *runtime,
  const DbcSelectedRuleRequirement *rules,
  size_t rule_count) {
  for (size_t i = 0u; i < rule_count; ++i) {
    if (!rules[i].enabled) {
      continue;
    }
    if (rules[i].key == NULL) {
      return DBC_SELECTED_RUNTIME_INVALID_ARGUMENT;
    }
    size_t key_length = 0u;
    while (key_length < LARGE_DBC_SIGNAL_RECORD_KEY_BYTES &&
           rules[i].key[key_length] != '\0') {
      ++key_length;
    }
    if (key_length == 0u || key_length == LARGE_DBC_SIGNAL_RECORD_KEY_BYTES) {
      return DBC_SELECTED_RUNTIME_INVALID_ARGUMENT;
    }
    const DbcSelectedRuntimeSignal *signal =
      dbc_selected_runtime_find_signal(runtime, rules[i].key);
    if (signal == NULL) {
      return DBC_SELECTED_RUNTIME_RULE_KEY_MISSING;
    }
    if (signal->definition_hash != rules[i].definition_hash) {
      return DBC_SELECTED_RUNTIME_RULE_DEFINITION_CONFLICT;
    }
  }
  return DBC_SELECTED_RUNTIME_OK;
}

static DbcSelectedRuntimeStatus construct_runtime(
  DbcSelectedRuntime *runtime,
  const DbcSelectedRuntimeBuildRequest *request) {
  memset(runtime, 0, sizeof(*runtime));
  runtime->runtime_generation = request->runtime_generation;
  runtime->candidate_generation = request->selection->candidate_generation;
  runtime->selection_generation = request->selection->selection_generation;
  runtime->source_size = request->selection->source_size;
  runtime->source_crc32 = request->selection->source_crc32;
  runtime->selection_crc32 = request->selection_crc32;

  for (uint16_t ordinal = 0u;
       ordinal < request->index_summary->signal_count;
       ++ordinal) {
    if (!dbc_selection_v1_is_selected(request->selection, ordinal)) {
      continue;
    }
    DbcCatalogIndexSignal catalog_signal;
    DbcSelectedRuntimeStatus status =
      read_signal(request, ordinal, &catalog_signal);
    if (status != DBC_SELECTED_RUNTIME_OK) {
      return status;
    }
    DbcCatalogIndexMessage catalog_message;
    status = read_message(request,
                          catalog_signal.message_ordinal,
                          &catalog_message);
    if (status != DBC_SELECTED_RUNTIME_OK) {
      return status;
    }
    status = append_signal(runtime, &catalog_signal, &catalog_message);
    if (status != DBC_SELECTED_RUNTIME_OK) {
      return status;
    }
  }
  if (runtime->signal_count != request->selection->selected_count) {
    return DBC_SELECTED_RUNTIME_INDEX_MISMATCH;
  }
  const DbcCandidateFormatStatus activation =
    dbc_selection_v1_verify_activation(request->selection,
                                       runtime->message_count);
  if (activation == DBC_CANDIDATE_FORMAT_EMPTY_SELECTION) {
    return DBC_SELECTED_RUNTIME_EMPTY_SELECTION;
  }
  if (activation == DBC_CANDIDATE_FORMAT_LIMIT_EXCEEDED) {
    return runtime->message_count > LARGE_DBC_ACTIVE_MAX_MESSAGES ?
      DBC_SELECTED_RUNTIME_MESSAGE_LIMIT : DBC_SELECTED_RUNTIME_SIGNAL_LIMIT;
  }
  if (activation != DBC_CANDIDATE_FORMAT_OK) {
    return DBC_SELECTED_RUNTIME_INVALID_SELECTION;
  }
  return check_rules(runtime, request->rules, request->rule_count);
}

__attribute__((noinline)) DbcSelectedRuntimeStatus
dbc_selected_runtime_build_inactive(
  DbcSelectedRuntimeSnapshot *snapshot,
  const DbcSelectedRuntimeBuildRequest *request) {
  if (snapshot == NULL) {
    return DBC_SELECTED_RUNTIME_INVALID_ARGUMENT;
  }
  snapshot->has_prepared = false;
  DbcSelectedRuntimeStatus status = validate_request(request);
  if (status != DBC_SELECTED_RUNTIME_OK) {
    return status;
  }
  const uint8_t slot = inactive_slot(snapshot);
  memset(snapshot->value_slots[slot], 0,
         sizeof(snapshot->value_slots[slot]));
  status = construct_runtime(&snapshot->slots[slot], request);
  if (status != DBC_SELECTED_RUNTIME_OK) {
    memset(&snapshot->slots[slot], 0, sizeof(snapshot->slots[slot]));
    return status;
  }
  snapshot->prepared_slot = slot;
  snapshot->has_prepared = true;
  return DBC_SELECTED_RUNTIME_OK;
}

DbcSelectedRuntimeStatus dbc_selected_runtime_discard_prepared(
  DbcSelectedRuntimeSnapshot *snapshot) {
  if (snapshot == NULL) {
    return DBC_SELECTED_RUNTIME_INVALID_ARGUMENT;
  }
  if (!snapshot->has_prepared) {
    return DBC_SELECTED_RUNTIME_NOT_PREPARED;
  }

  const uint8_t slot = snapshot->prepared_slot;
  snapshot->has_prepared = false;
  snapshot->prepared_slot = 0u;
  if (slot >= 2u || (snapshot->has_active && slot == snapshot->active_slot)) {
    return DBC_SELECTED_RUNTIME_PREPARED_SLOT_MISMATCH;
  }
  memset(&snapshot->slots[slot], 0, sizeof(snapshot->slots[slot]));
  memset(snapshot->value_slots[slot], 0,
         sizeof(snapshot->value_slots[slot]));
  return DBC_SELECTED_RUNTIME_OK;
}

__attribute__((noinline)) DbcSelectedRuntimeStatus
dbc_selected_runtime_publish_prepared_checked(
  DbcSelectedRuntimeSnapshot *snapshot,
  uint64_t expected_runtime_generation,
  uint8_t expected_prepared_slot) {
  if (snapshot == NULL) {
    return DBC_SELECTED_RUNTIME_INVALID_ARGUMENT;
  }
  if (!snapshot->has_prepared) {
    return DBC_SELECTED_RUNTIME_NOT_PREPARED;
  }
  if (expected_prepared_slot >= 2u ||
      snapshot->prepared_slot != expected_prepared_slot ||
      (snapshot->has_active &&
       snapshot->prepared_slot == snapshot->active_slot)) {
    return DBC_SELECTED_RUNTIME_PREPARED_SLOT_MISMATCH;
  }
  if (expected_runtime_generation == 0u ||
      snapshot->slots[snapshot->prepared_slot].runtime_generation !=
        expected_runtime_generation) {
    return DBC_SELECTED_RUNTIME_PREPARED_GENERATION_MISMATCH;
  }
  snapshot->active_slot = snapshot->prepared_slot;
  snapshot->has_active = true;
  snapshot->has_prepared = false;
  return DBC_SELECTED_RUNTIME_OK;
}

__attribute__((noinline)) DbcSelectedRuntimeStatus
dbc_selected_runtime_publish_prepared(
  DbcSelectedRuntimeSnapshot *snapshot) {
  if (snapshot == NULL) {
    return DBC_SELECTED_RUNTIME_INVALID_ARGUMENT;
  }
  if (!snapshot->has_prepared || snapshot->prepared_slot >= 2u) {
    return snapshot->has_prepared ?
      DBC_SELECTED_RUNTIME_PREPARED_SLOT_MISMATCH :
      DBC_SELECTED_RUNTIME_NOT_PREPARED;
  }
  return dbc_selected_runtime_publish_prepared_checked(
    snapshot,
    snapshot->slots[snapshot->prepared_slot].runtime_generation,
    snapshot->prepared_slot);
}

const DbcSelectedRuntime *dbc_selected_runtime_active(
  const DbcSelectedRuntimeSnapshot *snapshot) {
  return snapshot != NULL && snapshot->has_active ?
    &snapshot->slots[snapshot->active_slot] : NULL;
}

const DbcSelectedRuntime *dbc_selected_runtime_prepared(
  const DbcSelectedRuntimeSnapshot *snapshot) {
  return snapshot != NULL && snapshot->has_prepared &&
         snapshot->prepared_slot < 2u &&
         (!snapshot->has_active ||
          snapshot->prepared_slot != snapshot->active_slot) ?
    &snapshot->slots[snapshot->prepared_slot] : NULL;
}

SignalValueState *dbc_selected_runtime_active_value_slots(
  DbcSelectedRuntimeSnapshot *snapshot) {
  return snapshot != NULL && snapshot->has_active ?
    snapshot->value_slots[snapshot->active_slot] : NULL;
}

bool dbc_selected_runtime_copy_active_value(
  const DbcSelectedRuntimeSnapshot *snapshot,
  uint16_t value_state_index,
  SignalValueSnapshot *out_value) {
  const DbcSelectedRuntime *runtime =
    dbc_selected_runtime_active(snapshot);
  if (runtime == NULL || out_value == NULL ||
      value_state_index >= runtime->signal_count) {
    return false;
  }

  const SignalValueState *state =
    &snapshot->value_slots[snapshot->active_slot][value_state_index];
  for (uint8_t attempt = 0u; attempt < 3u; ++attempt) {
    const uint32_t before = state->update_seq;
    if ((before & 1u) != 0u) {
      continue;
    }
    SignalValueSnapshot value = {
      .value = state->value,
      .raw = state->raw,
      .updated_ms = state->updated_ms,
      .update_seq = before,
      .quality = (SignalValueQuality)state->quality
    };
    const uint32_t after = state->update_seq;
    if (before == after && (after & 1u) == 0u) {
      value.update_seq = after;
      *out_value = value;
      return true;
    }
  }
  return false;
}

const char *dbc_selected_runtime_status_string(DbcSelectedRuntimeStatus status) {
  switch (status) {
    case DBC_SELECTED_RUNTIME_OK: return "ok";
    case DBC_SELECTED_RUNTIME_INVALID_ARGUMENT: return "invalid_argument";
    case DBC_SELECTED_RUNTIME_INVALID_SELECTION: return "invalid_selection";
    case DBC_SELECTED_RUNTIME_EMPTY_SELECTION: return "empty_selection";
    case DBC_SELECTED_RUNTIME_INDEX_ERROR: return "index_error";
    case DBC_SELECTED_RUNTIME_INDEX_MISMATCH: return "index_mismatch";
    case DBC_SELECTED_RUNTIME_SIGNAL_LIMIT: return "signal_limit";
    case DBC_SELECTED_RUNTIME_MESSAGE_LIMIT: return "message_limit";
    case DBC_SELECTED_RUNTIME_RULE_KEY_MISSING: return "rule_key_missing";
    case DBC_SELECTED_RUNTIME_RULE_DEFINITION_CONFLICT:
      return "rule_definition_conflict";
    case DBC_SELECTED_RUNTIME_NOT_PREPARED: return "not_prepared";
    case DBC_SELECTED_RUNTIME_PREPARED_GENERATION_MISMATCH:
      return "prepared_generation_mismatch";
    case DBC_SELECTED_RUNTIME_PREPARED_SLOT_MISMATCH:
      return "prepared_slot_mismatch";
    default: return "unknown";
  }
}
