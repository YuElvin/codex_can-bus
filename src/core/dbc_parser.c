#include "dbc_parser.h"

#include <stdio.h>
#include <string.h>

static const char *skip_space(const char *text) {
  while (*text == ' ' || *text == '\t') {
    ++text;
  }
  return text;
}

static void copy_name(char *dest, size_t dest_len, const char *src) {
  if (dest_len == 0u) {
    return;
  }
  snprintf(dest, dest_len, "%s", src);
}

void dbc_init(DbcDatabase *db) {
  if (db != NULL) {
    memset(db, 0, sizeof(*db));
    db->last_message_index = -1;
  }
}

static bool parse_message(DbcDatabase *db, const char *line) {
  unsigned long id = 0u;
  unsigned int dlc = 0u;
  char name[DBC_NAME_MAX] = {0};

  if (sscanf(line, "BO_ %lu %31[^:]: %u", &id, name, &dlc) != 3) {
    ++db->error_lines;
    return false;
  }
  if (db->message_count >= DBC_MAX_MESSAGES || dlc > CAN_FRAME_MAX_DATA_LEN) {
    ++db->error_lines;
    return false;
  }

  DbcMessage *message = &db->messages[db->message_count];
  message->id = (uint32_t)id;
  copy_name(message->name, sizeof(message->name), skip_space(name));
  message->dlc = (uint8_t)dlc;
  message->signal_start = db->signal_count;
  message->signal_count = 0u;
  db->last_message_index = (int)db->message_count;
  ++db->message_count;
  return true;
}

static bool parse_signal(DbcDatabase *db, const char *line) {
  if (db->last_message_index < 0 || db->signal_count >= DBC_MAX_SIGNALS) {
    ++db->error_lines;
    return false;
  }

  char name[DBC_NAME_MAX] = {0};
  char endian = '\0';
  char sign = '\0';
  char unit[DBC_UNIT_MAX] = {0};
  unsigned int start_bit = 0u;
  unsigned int bit_length = 0u;
  double factor = 1.0;
  double offset = 0.0;
  double minimum = 0.0;
  double maximum = 0.0;

  const int matched = sscanf(line,
                             "SG_ %31s : %u|%u@%c%c (%lf,%lf) [%lf|%lf] \"%15[^\"]\"",
                             name,
                             &start_bit,
                             &bit_length,
                             &endian,
                             &sign,
                             &factor,
                             &offset,
                             &minimum,
                             &maximum,
                             unit);
  if (matched < 9 || (endian != '0' && endian != '1') || (sign != '+' && sign != '-')) {
    ++db->error_lines;
    return false;
  }
  if (start_bit > 511u || bit_length == 0u || bit_length > 63u) {
    ++db->error_lines;
    return false;
  }

  DbcSignal *signal = &db->signals[db->signal_count];
  copy_name(signal->name, sizeof(signal->name), name);
  signal->spec.start_bit = (uint16_t)start_bit;
  signal->spec.bit_length = (uint8_t)bit_length;
  signal->spec.byte_order = (endian == '1') ? SIGNAL_ENDIAN_INTEL : SIGNAL_ENDIAN_MOTOROLA;
  signal->spec.is_signed = sign == '-';
  signal->spec.factor = factor;
  signal->spec.offset = offset;
  signal->spec.minimum = minimum;
  signal->spec.maximum = maximum;
  if (matched == 10) {
    copy_name(signal->unit, sizeof(signal->unit), unit);
  }

  DbcMessage *message = &db->messages[db->last_message_index];
  ++message->signal_count;
  ++db->signal_count;
  return true;
}

bool dbc_parse_line(DbcDatabase *db, const char *line) {
  if (db == NULL || line == NULL) {
    return false;
  }

  const char *trimmed = skip_space(line);
  if (strncmp(trimmed, "BO_ ", 4u) == 0) {
    return parse_message(db, trimmed);
  }
  if (strncmp(trimmed, "SG_ ", 4u) == 0) {
    return parse_signal(db, trimmed);
  }

  ++db->skipped_lines;
  return true;
}

const DbcMessage *dbc_find_message(const DbcDatabase *db, uint32_t id) {
  if (db == NULL) {
    return NULL;
  }

  for (size_t i = 0u; i < db->message_count; ++i) {
    if (db->messages[i].id == id) {
      return &db->messages[i];
    }
  }
  return NULL;
}

const DbcSignal *dbc_find_signal(const DbcDatabase *db, const DbcMessage *message, const char *name) {
  if (db == NULL || message == NULL || name == NULL) {
    return NULL;
  }

  const size_t end = message->signal_start + message->signal_count;
  for (size_t i = message->signal_start; i < end; ++i) {
    if (strcmp(db->signals[i].name, name) == 0) {
      return &db->signals[i];
    }
  }
  return NULL;
}

bool dbc_decode_signal_value(const DbcSignal *signal, const CanFrame *frame, double *phys_value) {
  if (signal == NULL || frame == NULL || frame->dlc > CAN_FRAME_MAX_DATA_LEN) {
    return false;
  }
  return signal_decode_phys(frame->data, frame->dlc, &signal->spec, phys_value);
}

bool dbc_encode_signal_value(const DbcSignal *signal, CanFrame *frame, double phys_value) {
  if (signal == NULL || frame == NULL || frame->dlc > CAN_FRAME_MAX_DATA_LEN) {
    return false;
  }
  return signal_encode_phys(frame->data, frame->dlc, &signal->spec, phys_value);
}
