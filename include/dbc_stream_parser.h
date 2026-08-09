#ifndef DBC_STREAM_PARSER_H
#define DBC_STREAM_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "large_dbc_contract.h"

/*
 * The parser emits bounded intermediate records and never retains a catalog.
 * The catalog/index callback is responsible for rejecting duplicate
 * (normalized_id, IDE) message identities and duplicate normalized keys.  A
 * callback rejection is fatal and leaves the parser in a failed state.
 */

typedef struct {
  uint16_t ordinal;
  uint16_t first_signal_ordinal;
  uint32_t normalized_id;
  uint32_t source_line;
  uint32_t source_offset;
  uint8_t flags;
  uint8_t declared_payload_length;
  char name[LARGE_DBC_MESSAGE_NAME_MAX_BYTES + 1u];
} DbcStreamMessageRecord;

typedef struct {
  uint16_t ordinal;
  uint16_t message_ordinal;
  uint32_t normalized_id;
  uint32_t source_line;
  uint32_t source_offset;
  uint64_t definition_hash;
  uint16_t start_bit;
  uint8_t bit_length;
  uint8_t flags;
  uint8_t declared_payload_length;
  uint8_t key_length;
  uint8_t unit_length;
  double factor;
  double offset;
  double minimum;
  double maximum;
  char key[LARGE_DBC_KEY_MAX_BYTES + 1u];
  char unit[LARGE_DBC_UNIT_MAX_BYTES + 1u];
} DbcStreamSignalRecord;

typedef bool (*DbcStreamMessageCallback)(void *context,
                                         const DbcStreamMessageRecord *record);
typedef bool (*DbcStreamSignalCallback)(void *context,
                                        const DbcStreamSignalRecord *record);

typedef struct {
  DbcStreamMessageCallback on_message;
  DbcStreamSignalCallback on_signal;
} DbcStreamParserCallbacks;

typedef enum {
  DBC_STREAM_ERROR_NONE = 0,
  DBC_STREAM_ERROR_INVALID_ARGUMENT,
  DBC_STREAM_ERROR_ALREADY_FINALIZED,
  DBC_STREAM_ERROR_NUL_BYTE,
  DBC_STREAM_ERROR_RELEVANT_LINE_TOO_LONG,
  DBC_STREAM_ERROR_MALFORMED_MESSAGE,
  DBC_STREAM_ERROR_MALFORMED_SIGNAL,
  DBC_STREAM_ERROR_SIGNAL_WITHOUT_MESSAGE,
  DBC_STREAM_ERROR_MULTIPLEX_UNSUPPORTED,
  DBC_STREAM_ERROR_DECODE_SYNTAX_UNSUPPORTED,
  DBC_STREAM_ERROR_IDENTIFIER,
  DBC_STREAM_ERROR_STRING_LIMIT,
  DBC_STREAM_ERROR_INVALID_UTF8,
  DBC_STREAM_ERROR_NUMBER,
  DBC_STREAM_ERROR_CAN_ID,
  DBC_STREAM_ERROR_DLC,
  DBC_STREAM_ERROR_LAYOUT,
  DBC_STREAM_ERROR_CATALOG_CAPACITY,
  DBC_STREAM_ERROR_SOURCE_LIMIT,
  DBC_STREAM_ERROR_CALLBACK_REJECTED
} DbcStreamParserError;

typedef struct {
  DbcStreamParserCallbacks callbacks;
  void *callback_context;
  char line[LARGE_DBC_PARSER_LINE_BYTES];
  size_t line_length;
  uint32_t line_number;
  uint32_t line_source_offset;
  uint32_t source_bytes;
  uint32_t skipped_lines;
  uint16_t message_count;
  uint16_t signal_count;
  DbcStreamMessageRecord current_message;
  DbcStreamParserError error;
  uint32_t error_line;
  uint32_t error_offset;
  uint8_t vframe_match_length;
  bool line_overflow;
  bool line_has_nul;
  bool line_saw_vframe_format;
  bool have_current_message;
  bool finalized;
} DbcStreamParser;

/* This context owns the 512-byte scratch and must use static/caller storage in firmware. */

void dbc_stream_parser_init(DbcStreamParser *parser,
                            const DbcStreamParserCallbacks *callbacks,
                            void *callback_context);

bool dbc_stream_parser_feed(DbcStreamParser *parser,
                            const uint8_t *data,
                            size_t size);

bool dbc_stream_parser_finalize(DbcStreamParser *parser);

const char *dbc_stream_parser_error_name(DbcStreamParserError error);

#endif
