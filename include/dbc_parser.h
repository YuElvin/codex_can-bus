#ifndef DBC_PARSER_H
#define DBC_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "can_types.h"
#include "signal_codec.h"

#define DBC_MAX_MESSAGES 64u
#define DBC_MAX_SIGNALS 256u
#define DBC_NAME_MAX 32u
#define DBC_UNIT_MAX 16u

typedef struct {
  char name[DBC_NAME_MAX];
  SignalSpec spec;
  char unit[DBC_UNIT_MAX];
} DbcSignal;

typedef struct {
  uint32_t id;
  char name[DBC_NAME_MAX];
  uint8_t dlc;
  size_t signal_start;
  size_t signal_count;
} DbcMessage;

typedef struct {
  DbcMessage messages[DBC_MAX_MESSAGES];
  DbcSignal signals[DBC_MAX_SIGNALS];
  size_t message_count;
  size_t signal_count;
  size_t skipped_lines;
  size_t error_lines;
  int last_message_index;
} DbcDatabase;

void dbc_init(DbcDatabase *db);
bool dbc_parse_line(DbcDatabase *db, const char *line);
bool dbc_parse_text(DbcDatabase *db, const char *text, size_t len, size_t *line_count);
const DbcMessage *dbc_find_message(const DbcDatabase *db, uint32_t id);
const DbcSignal *dbc_find_signal(const DbcDatabase *db, const DbcMessage *message, const char *name);
bool dbc_decode_signal_value(const DbcSignal *signal, const CanFrame *frame, double *phys_value);
bool dbc_encode_signal_value(const DbcSignal *signal, CanFrame *frame, double phys_value);

#endif
