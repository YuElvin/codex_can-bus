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
  DBC_SELECTED_RUNTIME_NOT_PREPARED,
  DBC_SELECTED_RUNTIME_PREPARED_GENERATION_MISMATCH,
  DBC_SELECTED_RUNTIME_PREPARED_SLOT_MISMATCH
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

typedef enum {
  SIGNAL_VALUE_QUALITY_MISSING = 0,
  SIGNAL_VALUE_QUALITY_GOOD,
  SIGNAL_VALUE_QUALITY_STALE,
  SIGNAL_VALUE_QUALITY_ERROR
} SignalValueQuality;

/*
 * Numeric state is kept separately from the runtime string/decode catalog.
 * update_seq is even while stable and odd while the single decoder writer is
 * publishing a new value.
 */
typedef struct {
  volatile double value;
  volatile int64_t raw;
  volatile uint32_t updated_ms;
  volatile uint32_t update_seq;
  volatile uint8_t quality;
} SignalValueState;

typedef struct {
  double value;
  int64_t raw;
  uint32_t updated_ms;
  uint32_t update_seq;
  SignalValueQuality quality;
} SignalValueSnapshot;

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
  SignalValueState value_slots[2][LARGE_DBC_ACTIVE_MAX_SIGNALS];
  uint8_t active_slot;
  uint8_t prepared_slot;
  bool has_active;
  bool has_prepared;
} DbcSelectedRuntimeSnapshot;

void dbc_selected_runtime_snapshot_init(DbcSelectedRuntimeSnapshot *snapshot);
DbcSelectedRuntimeStatus dbc_selected_runtime_build_inactive(
  DbcSelectedRuntimeSnapshot *snapshot,
  const DbcSelectedRuntimeBuildRequest *request);

/* Clears an unpublished runtime/value slot without touching the active slot. */
DbcSelectedRuntimeStatus dbc_selected_runtime_discard_prepared(
  DbcSelectedRuntimeSnapshot *snapshot);

/* Caller supplies the short platform critical section around this publication. */
DbcSelectedRuntimeStatus dbc_selected_runtime_publish_prepared(
  DbcSelectedRuntimeSnapshot *snapshot);

/*
 * Publishes only when the pending slot and runtime generation match the
 * identity that was durably committed. A mismatch changes no slot state.
 */
DbcSelectedRuntimeStatus dbc_selected_runtime_publish_prepared_checked(
  DbcSelectedRuntimeSnapshot *snapshot,
  uint64_t expected_runtime_generation,
  uint8_t expected_prepared_slot);

const DbcSelectedRuntime *dbc_selected_runtime_active(
  const DbcSelectedRuntimeSnapshot *snapshot);
const DbcSelectedRuntime *dbc_selected_runtime_prepared(
  const DbcSelectedRuntimeSnapshot *snapshot);
const DbcSelectedRuntimeSignal *dbc_selected_runtime_find_signal(
  const DbcSelectedRuntime *runtime,
  const char *key);

/* Decoder-only mutable access; slot identity always follows the active runtime. */
SignalValueState *dbc_selected_runtime_active_value_slots(
  DbcSelectedRuntimeSnapshot *snapshot);

/* Copies exactly one active numeric slot using the sequence/recheck contract. */
bool dbc_selected_runtime_copy_active_value(
  const DbcSelectedRuntimeSnapshot *snapshot,
  uint16_t value_state_index,
  SignalValueSnapshot *out_value);

const char *dbc_selected_runtime_status_string(DbcSelectedRuntimeStatus status);

#endif
