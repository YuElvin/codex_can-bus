#ifndef DBC_SELECTED_RUNTIME_H
#define DBC_SELECTED_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dbc_candidate_format.h"
#include "dbc_catalog_index.h"
#include "large_dbc_contract.h"

typedef enum {
  DBC_SELECTED_RUNTIME_OK = 0,
  DBC_SELECTED_RUNTIME_INVALID_ARGUMENT,
  DBC_SELECTED_RUNTIME_INVALID_SELECTION,
  DBC_SELECTED_RUNTIME_EMPTY_SELECTION,
  DBC_SELECTED_RUNTIME_INDEX_ERROR,
  DBC_SELECTED_RUNTIME_INDEX_MISMATCH,
  DBC_SELECTED_RUNTIME_SIGNAL_LIMIT,
  DBC_SELECTED_RUNTIME_MESSAGE_LIMIT,
  DBC_SELECTED_RUNTIME_RULE_KEY_MISSING,
  DBC_SELECTED_RUNTIME_RULE_DEFINITION_CONFLICT,
  DBC_SELECTED_RUNTIME_NOT_PREPARED
} DbcSelectedRuntimeStatus;

typedef struct {
  uint32_t normalized_id;
  uint16_t catalog_message_ordinal;
  uint16_t first_signal_index;
  uint16_t signal_count;
  uint8_t flags;
  uint8_t declared_payload_length;
} DbcSelectedRuntimeMessage;

typedef struct {
  uint16_t catalog_ordinal;
  uint16_t catalog_message_ordinal;
  uint16_t runtime_message_index;
  uint16_t value_state_index;
  uint32_t normalized_id;
  uint16_t start_bit;
  uint8_t bit_length;
  uint8_t flags;
  uint8_t declared_payload_length;
  double factor;
  double offset;
  double minimum;
  double maximum;
  uint64_t definition_hash;
  char key[LARGE_DBC_SIGNAL_RECORD_KEY_BYTES];
  char unit[LARGE_DBC_SIGNAL_RECORD_UNIT_BYTES];
} DbcSelectedRuntimeSignal;

typedef struct {
  uint64_t runtime_generation;
  uint64_t candidate_generation;
  uint64_t selection_generation;
  uint32_t source_size;
  uint32_t source_crc32;
  uint32_t selection_crc32;
  uint16_t message_count;
  uint16_t signal_count;
  DbcSelectedRuntimeMessage messages[LARGE_DBC_ACTIVE_MAX_MESSAGES];
  DbcSelectedRuntimeSignal signals[LARGE_DBC_ACTIVE_MAX_SIGNALS];
} DbcSelectedRuntime;

/* Only enabled rules participate in compatibility checks. */
typedef struct {
  bool enabled;
  const char *key;
  uint64_t definition_hash;
} DbcSelectedRuleRequirement;

typedef struct {
  const DbcCatalogIndexIo *index;
  const DbcCatalogIndexSummary *index_summary;
  const DbcSelectionV1 *selection;
  uint64_t runtime_generation;
  uint32_t selection_crc32;
  const DbcSelectedRuleRequirement *rules;
  size_t rule_count;
} DbcSelectedRuntimeBuildRequest;

typedef struct {
  DbcSelectedRuntime slots[2];
  uint8_t active_slot;
  uint8_t prepared_slot;
  bool has_active;
  bool has_prepared;
} DbcSelectedRuntimeSnapshot;

void dbc_selected_runtime_snapshot_init(DbcSelectedRuntimeSnapshot *snapshot);
DbcSelectedRuntimeStatus dbc_selected_runtime_build_inactive(
  DbcSelectedRuntimeSnapshot *snapshot,
  const DbcSelectedRuntimeBuildRequest *request);

/* Caller supplies the short platform critical section around this publication. */
DbcSelectedRuntimeStatus dbc_selected_runtime_publish_prepared(
  DbcSelectedRuntimeSnapshot *snapshot);

const DbcSelectedRuntime *dbc_selected_runtime_active(
  const DbcSelectedRuntimeSnapshot *snapshot);
const DbcSelectedRuntime *dbc_selected_runtime_prepared(
  const DbcSelectedRuntimeSnapshot *snapshot);
const DbcSelectedRuntimeSignal *dbc_selected_runtime_find_signal(
  const DbcSelectedRuntime *runtime,
  const char *key);

const char *dbc_selected_runtime_status_string(DbcSelectedRuntimeStatus status);

#endif
