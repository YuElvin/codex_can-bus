#include "dbc_stream_index.h"

#include <string.h>

static bool reject_invalid(DbcStreamIndexAdapter *adapter) {
  adapter->status = DBC_CATALOG_INDEX_INVALID_RECORD;
  return false;
}

static bool on_message(void *context, const DbcStreamMessageRecord *record) {
  DbcStreamIndexAdapter *adapter = context;
  if (adapter == NULL || record == NULL || adapter->builder == NULL ||
      adapter->status != DBC_CATALOG_INDEX_OK ||
      record->ordinal != adapter->next_message_ordinal ||
      record->first_signal_ordinal != adapter->next_signal_ordinal ||
      (record->flags & ~(LARGE_DBC_SIGNAL_FLAG_IDE |
                         LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED)) != 0u ||
      ((record->flags & LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED) != 0u) !=
        (record->declared_payload_length > 8u)) {
    return adapter != NULL ? reject_invalid(adapter) : false;
  }

  const DbcCatalogIndexMessageInput input = {
    .normalized_id = record->normalized_id,
    .source_line = record->source_line,
    .declared_payload_length = record->declared_payload_length,
    .ide = (record->flags & LARGE_DBC_SIGNAL_FLAG_IDE) != 0u
  };
  adapter->status =
    dbc_catalog_index_builder_begin_message(adapter->builder, &input);
  if (adapter->status != DBC_CATALOG_INDEX_OK) {
    return false;
  }
  adapter->current_message_ordinal = record->ordinal;
  adapter->current_message_id = record->normalized_id;
  adapter->current_message_flags = record->flags;
  adapter->current_message_dlc = record->declared_payload_length;
  adapter->have_message = true;
  ++adapter->next_message_ordinal;
  return true;
}

static bool on_signal(void *context, const DbcStreamSignalRecord *record) {
  DbcStreamIndexAdapter *adapter = context;
  const uint8_t message_flags =
    (uint8_t)(LARGE_DBC_SIGNAL_FLAG_IDE | LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED);
  if (adapter == NULL || record == NULL || adapter->builder == NULL ||
      adapter->status != DBC_CATALOG_INDEX_OK || !adapter->have_message ||
      record->ordinal != adapter->next_signal_ordinal ||
      record->message_ordinal != adapter->current_message_ordinal ||
      record->normalized_id != adapter->current_message_id ||
      record->declared_payload_length != adapter->current_message_dlc ||
      (record->flags & ~(LARGE_DBC_SIGNAL_FLAG_IDE |
                         LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED |
                         LARGE_DBC_SIGNAL_FLAG_MOTOROLA |
                         LARGE_DBC_SIGNAL_FLAG_SIGNED)) != 0u ||
      (record->flags & message_flags) !=
        (adapter->current_message_flags & message_flags) ||
      record->key_length == 0u ||
      record->key_length != strlen(record->key) ||
      record->unit_length != strlen(record->unit)) {
    return adapter != NULL ? reject_invalid(adapter) : false;
  }

  DbcCatalogIndexSignalInput input = {
    .start_bit = record->start_bit,
    .bit_length = record->bit_length,
    .motorola = (record->flags & LARGE_DBC_SIGNAL_FLAG_MOTOROLA) != 0u,
    .is_signed = (record->flags & LARGE_DBC_SIGNAL_FLAG_SIGNED) != 0u,
    .factor = record->factor,
    .offset = record->offset,
    .minimum = record->minimum,
    .maximum = record->maximum,
    .definition_hash = record->definition_hash,
    .source_line = record->source_line,
    .source_offset = record->source_offset,
    .key = record->key,
    .key_length = record->key_length,
    .unit = record->unit,
    .unit_length = record->unit_length
  };
  const DbcCatalogIndexMessageInput message = {
    .normalized_id = adapter->current_message_id,
    .source_line = 1u,
    .declared_payload_length = adapter->current_message_dlc,
    .ide = (adapter->current_message_flags & LARGE_DBC_SIGNAL_FLAG_IDE) != 0u
  };
  if (dbc_catalog_definition_hash_v1(&message, &input) != record->definition_hash) {
    return reject_invalid(adapter);
  }
  adapter->status = dbc_catalog_index_builder_add_signal(adapter->builder, &input);
  if (adapter->status != DBC_CATALOG_INDEX_OK) {
    return false;
  }
  ++adapter->next_signal_ordinal;
  return true;
}

void dbc_stream_index_adapter_init(DbcStreamIndexAdapter *adapter,
                                   DbcCatalogIndexBuilder *builder) {
  if (adapter == NULL) {
    return;
  }
  memset(adapter, 0, sizeof(*adapter));
  adapter->builder = builder;
  adapter->status = builder != NULL ? DBC_CATALOG_INDEX_OK :
                                      DBC_CATALOG_INDEX_INVALID_ARGUMENT;
}

DbcStreamParserCallbacks dbc_stream_index_adapter_callbacks(void) {
  const DbcStreamParserCallbacks callbacks = {on_message, on_signal};
  return callbacks;
}

bool dbc_stream_index_adapter_complete(const DbcStreamIndexAdapter *adapter,
                                       const DbcStreamParser *parser) {
  return adapter != NULL && parser != NULL && adapter->builder != NULL &&
         adapter->status == DBC_CATALOG_INDEX_OK && parser->finalized &&
         parser->error == DBC_STREAM_ERROR_NONE &&
         adapter->next_message_ordinal == parser->message_count &&
         adapter->next_signal_ordinal == parser->signal_count;
}
