#ifndef DBC_STREAM_INDEX_H
#define DBC_STREAM_INDEX_H

#include <stdbool.h>
#include <stdint.h>

#include "dbc_catalog_index.h"
#include "dbc_stream_parser.h"

/*
 * Strict callback adapter from the streaming parser to index v1.  It retains
 * only ordinal/cross-record state; the parser and builder scratch remain
 * caller-owned so firmware can place both in static storage.
 */
typedef struct {
  DbcCatalogIndexBuilder *builder;
  DbcCatalogIndexStatus status;
  uint16_t next_message_ordinal;
  uint16_t next_signal_ordinal;
  uint16_t current_message_ordinal;
  uint32_t current_message_id;
  uint8_t current_message_flags;
  uint8_t current_message_dlc;
  bool have_message;
} DbcStreamIndexAdapter;

void dbc_stream_index_adapter_init(DbcStreamIndexAdapter *adapter,
                                   DbcCatalogIndexBuilder *builder);

DbcStreamParserCallbacks dbc_stream_index_adapter_callbacks(void);

bool dbc_stream_index_adapter_complete(const DbcStreamIndexAdapter *adapter,
                                       const DbcStreamParser *parser);

#endif
