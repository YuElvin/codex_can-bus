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

static bool parse_char(const char **cursor, char expected) {
  const char *text = skip_space(*cursor);
  if (*text != expected) {
    return false;
  }
  *cursor = text + 1;
  return true;
}

static bool parse_unsigned_value(const char **cursor, unsigned long *value) {
  const char *text = skip_space(*cursor);
  unsigned long result = 0u;
  if (*text < '0' || *text > '9') {
    return false;
  }
  while (*text >= '0' && *text <= '9') {
    result = (result * 10u) + (unsigned long)(*text - '0');
    ++text;
  }
  *value = result;
  *cursor = text;
  return true;
}

static bool parse_double_value(const char **cursor, double *value) {
  const char *text = skip_space(*cursor);
  double result = 0.0;
  double place = 0.1;
  bool negative = false;
  bool has_digit = false;

  if (*text == '-' || *text == '+') {
    negative = *text == '-';
    ++text;
  }
  while (*text >= '0' && *text <= '9') {
    result = (result * 10.0) + (double)(*text - '0');
    has_digit = true;
    ++text;
  }
  if (*text == '.') {
    ++text;
    while (*text >= '0' && *text <= '9') {
      result += (double)(*text - '0') * place;
      place *= 0.1;
      has_digit = true;
      ++text;
    }
  }
  if (!has_digit) {
    return false;
  }
  *value = negative ? -result : result;
  *cursor = text;
  return true;
}

static bool parse_name_token(const char **cursor, char *name, size_t name_len) {
  const char *text = skip_space(*cursor);
  size_t len = 0u;
  if (*text == '\0' || *text == ':') {
    return false;
  }
  while (text[len] != '\0' && text[len] != ' ' && text[len] != '\t' && text[len] != ':') {
    ++len;
  }
  if (len == 0u || len >= name_len) {
    return false;
  }
  memcpy(name, text, len);
  name[len] = '\0';
  *cursor = text + len;
  return true;
}

static bool parse_quoted_unit(const char **cursor, char *unit, size_t unit_len) {
  const char *text = skip_space(*cursor);
  size_t len = 0u;
  if (*text != '"') {
    unit[0] = '\0';
    return true;
  }
  ++text;
  while (text[len] != '\0' && text[len] != '"') {
    ++len;
  }
  if (text[len] != '"' || len >= unit_len) {
    return false;
  }
  memcpy(unit, text, len);
  unit[len] = '\0';
  *cursor = text + len + 1u;
  return true;
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
  if (db->message_count >= DBC_MAX_MESSAGES || id > 0x7ffu || dlc > CAN_FRAME_MAX_DATA_LEN) {
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

static bool signal_fits_message(unsigned long start_bit,
                                unsigned long bit_length,
                                char endian,
                                uint8_t dlc) {
  const int frame_bits = (int)dlc * 8;

  if (start_bit >= (unsigned long)frame_bits) {
    return false;
  }
  if (endian == '1') {
    return start_bit + bit_length <= (unsigned long)frame_bits;
  }

  int bit = (int)start_bit;
  for (unsigned long i = 0u; i < bit_length; ++i) {
    if (bit < 0 || bit >= frame_bits) {
      return false;
    }
    bit = (bit % 8 == 0) ? bit + 15 : bit - 1;
  }
  return true;
}

static bool parse_signal(DbcDatabase *db, const char *line) {
  if (db->last_message_index < 0 || db->signal_count >= DBC_MAX_SIGNALS) {
    ++db->error_lines;
    return false;
  }

  const char *cursor = line + 3u;
  char name[DBC_NAME_MAX] = {0};
  char endian = '\0';
  char sign = '\0';
  char unit[DBC_UNIT_MAX] = {0};
  unsigned long start_bit = 0u;
  unsigned long bit_length = 0u;
  double factor = 1.0;
  double offset = 0.0;
  double minimum = 0.0;
  double maximum = 0.0;

  if (!parse_name_token(&cursor, name, sizeof(name)) ||
      !parse_char(&cursor, ':') ||
      !parse_unsigned_value(&cursor, &start_bit) ||
      !parse_char(&cursor, '|') ||
      !parse_unsigned_value(&cursor, &bit_length) ||
      !parse_char(&cursor, '@')) {
    ++db->error_lines;
    return false;
  }

  endian = *cursor++;
  sign = *cursor++;
  if ((endian != '0' && endian != '1') || (sign != '+' && sign != '-') ||
      !parse_char(&cursor, '(') ||
      !parse_double_value(&cursor, &factor) ||
      !parse_char(&cursor, ',') ||
      !parse_double_value(&cursor, &offset) ||
      !parse_char(&cursor, ')') ||
      !parse_char(&cursor, '[') ||
      !parse_double_value(&cursor, &minimum) ||
      !parse_char(&cursor, '|') ||
      !parse_double_value(&cursor, &maximum) ||
      !parse_char(&cursor, ']') ||
      !parse_quoted_unit(&cursor, unit, sizeof(unit))) {
    ++db->error_lines;
    return false;
  }
  const DbcMessage *message = &db->messages[db->last_message_index];
  if (start_bit > 511u || bit_length == 0u || bit_length > 63u || factor == 0.0 ||
      minimum > maximum || !signal_fits_message(start_bit, bit_length, endian, message->dlc)) {
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
  copy_name(signal->unit, sizeof(signal->unit), unit);

  ++db->messages[db->last_message_index].signal_count;
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

bool dbc_parse_text(DbcDatabase *db, const char *text, size_t len, size_t *line_count) {
  size_t offset = 0u;
  size_t lines = 0u;
  char line[128];

  if (db == NULL || (text == NULL && len > 0u)) {
    return false;
  }

  dbc_init(db);
  while (offset < len) {
    size_t line_len = 0u;
    while (offset + line_len < len && text[offset + line_len] != '\n') {
      ++line_len;
    }

    size_t copy_len = line_len;
    if (copy_len > 0u && text[offset + copy_len - 1u] == '\r') {
      --copy_len;
    }
    ++lines;

    if (copy_len >= sizeof(line)) {
      ++db->error_lines;
    } else {
      memcpy(line, &text[offset], copy_len);
      line[copy_len] = '\0';
      (void)dbc_parse_line(db, line);
    }

    offset += line_len;
    if (offset < len && text[offset] == '\n') {
      ++offset;
    }
  }

  if (line_count != NULL) {
    *line_count = lines;
  }
  return db->error_lines == 0u;
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
