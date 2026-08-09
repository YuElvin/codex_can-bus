#include "dbc_stream_parser.h"

#include <math.h>
#include <string.h>

#define DBC_VECTOR_EXTENDED_FLAG UINT32_C(0x80000000)
#define DBC_EXTENDED_ID_MASK UINT32_C(0x1FFFFFFF)

static const char *skip_space(const char *cursor) {
  while (*cursor == ' ' || *cursor == '\t') {
    ++cursor;
  }
  return cursor;
}

static bool fail(DbcStreamParser *parser, DbcStreamParserError error) {
  if (parser->error == DBC_STREAM_ERROR_NONE) {
    parser->error = error;
    parser->error_line = parser->line_number;
    parser->error_offset = parser->line_source_offset;
  }
  return false;
}

static bool keyword_with_arguments(const char *text, const char *keyword) {
  const size_t length = strlen(keyword);
  return strncmp(text, keyword, length) == 0 &&
         (text[length] == ' ' || text[length] == '\t');
}

static bool incomplete_record_marker(const char *line, const char *marker) {
  const size_t length = strlen(line);
  const size_t marker_length = strlen(marker);
  return length > 0u && length <= marker_length &&
         strncmp(line, marker, length) == 0;
}

static bool is_identifier_start(char value) {
  return (value >= 'A' && value <= 'Z') ||
         (value >= 'a' && value <= 'z') || value == '_';
}

static bool is_identifier_continue(char value) {
  return is_identifier_start(value) || (value >= '0' && value <= '9');
}

static bool parse_identifier(const char **cursor, char *output, size_t capacity) {
  const char *text = skip_space(*cursor);
  size_t length = 0u;

  if (!is_identifier_start(*text)) {
    return false;
  }
  do {
    if (length + 1u >= capacity) {
      return false;
    }
    output[length++] = *text++;
  } while (is_identifier_continue(*text));
  output[length] = '\0';
  *cursor = text;
  return true;
}

static bool consume_char(const char **cursor, char expected) {
  const char *text = skip_space(*cursor);
  if (*text != expected) {
    return false;
  }
  *cursor = text + 1;
  return true;
}

static bool parse_u64(const char **cursor, uint64_t maximum, uint64_t *output) {
  const char *text = skip_space(*cursor);
  uint64_t value = 0u;

  if (*text < '0' || *text > '9') {
    return false;
  }
  do {
    const uint8_t digit = (uint8_t)(*text - '0');
    if (value > (maximum - digit) / 10u) {
      return false;
    }
    value = value * 10u + digit;
    ++text;
  } while (*text >= '0' && *text <= '9');
  *cursor = text;
  *output = value;
  return true;
}

static bool apply_decimal_exponent(double *value, int exponent) {
  static const double powers[] = {
      1e1, 1e2, 1e4, 1e8, 1e16, 1e32, 1e64, 1e128, 1e256};
  unsigned int magnitude = exponent < 0 ? (unsigned int)-exponent : (unsigned int)exponent;
  size_t index = 0u;

  while (magnitude != 0u) {
    if (index >= sizeof(powers) / sizeof(powers[0])) {
      return false;
    }
    if ((magnitude & 1u) != 0u) {
      *value = exponent < 0 ? *value / powers[index] : *value * powers[index];
      if (!isfinite(*value)) {
        return false;
      }
    }
    magnitude >>= 1u;
    ++index;
  }
  return *value != 0.0;
}

/* Locale-independent decimal/scientific parser; hexadecimal floats are rejected. */
static bool parse_finite_double(const char **cursor, double *output) {
  const char *text = skip_space(*cursor);
  bool negative = false;
  bool have_digit = false;
  bool have_nonzero = false;
  bool have_decimal_point = false;
  uint64_t significand = 0u;
  unsigned int stored_digits = 0u;
  unsigned int all_digits = 0u;
  unsigned int digits_before_point = 0u;
  unsigned int first_nonzero = 0u;
  uint8_t rounding_digit = 0u;
  int explicit_exponent = 0;
  bool exponent_negative = false;
  double value;

  if (*text == '+' || *text == '-') {
    negative = *text == '-';
    ++text;
  }
  while (true) {
    if (*text >= '0' && *text <= '9') {
      const uint8_t digit = (uint8_t)(*text - '0');
      have_digit = true;
      if (!have_decimal_point) {
        ++digits_before_point;
      }
      if (!have_nonzero && digit != 0u) {
        have_nonzero = true;
        first_nonzero = all_digits;
      }
      if (have_nonzero) {
        if (stored_digits < 19u) {
          significand = significand * 10u + digit;
          ++stored_digits;
        } else if (rounding_digit == 0u) {
          /* Store digit+1 so a discarded zero remains distinguishable. */
          rounding_digit = (uint8_t)(digit + 1u);
        }
      }
      ++all_digits;
      ++text;
    } else if (*text == '.' && !have_decimal_point) {
      have_decimal_point = true;
      ++text;
    } else {
      break;
    }
  }
  if (!have_digit) {
    return false;
  }
  if (*text == 'e' || *text == 'E') {
    bool exponent_digit = false;
    ++text;
    if (*text == '+' || *text == '-') {
      exponent_negative = *text == '-';
      ++text;
    }
    while (*text >= '0' && *text <= '9') {
      exponent_digit = true;
      if (explicit_exponent > 1000) {
        return false;
      }
      explicit_exponent = explicit_exponent * 10 + (*text - '0');
      ++text;
    }
    if (!exponent_digit || explicit_exponent > 1000) {
      return false;
    }
  }
  if (!have_nonzero) {
    value = 0.0;
  } else {
    int exponent;
    if (rounding_digit > 5u) {
      ++significand;
    }
    if (exponent_negative) {
      explicit_exponent = -explicit_exponent;
    }
    exponent = explicit_exponent + (int)digits_before_point -
               (int)first_nonzero - (int)stored_digits;
    value = (double)significand;
    if (!apply_decimal_exponent(&value, exponent)) {
      return false;
    }
  }
  *cursor = text;
  *output = negative && value != 0.0 ? -value : value;
  return true;
}

static bool valid_utf8(const uint8_t *text, size_t length) {
  size_t index = 0u;
  while (index < length) {
    const uint8_t first = text[index++];
    uint32_t codepoint;
    size_t continuation;

    if (first < 0x80u) {
      if (first < 0x20u || first == 0x7fu) {
        return false;
      }
      continue;
    }
    if (first >= 0xc2u && first <= 0xdfu) {
      codepoint = first & 0x1fu;
      continuation = 1u;
    } else if (first >= 0xe0u && first <= 0xefu) {
      codepoint = first & 0x0fu;
      continuation = 2u;
    } else if (first >= 0xf0u && first <= 0xf4u) {
      codepoint = first & 0x07u;
      continuation = 3u;
    } else {
      return false;
    }
    if (continuation > length - index) {
      return false;
    }
    for (size_t i = 0u; i < continuation; ++i) {
      const uint8_t next = text[index++];
      if ((next & 0xc0u) != 0x80u) {
        return false;
      }
      codepoint = (codepoint << 6) | (next & 0x3fu);
    }
    if ((continuation == 1u && codepoint < 0x80u) ||
        (continuation == 2u && codepoint < 0x800u) ||
        (continuation == 3u && codepoint < 0x10000u) ||
        (codepoint >= 0xd800u && codepoint <= 0xdfffu) ||
        codepoint > 0x10ffffu) {
      return false;
    }
  }
  return true;
}

static bool parse_unit(const char **cursor,
                       char output[LARGE_DBC_UNIT_MAX_BYTES + 1u],
                       uint8_t *output_length) {
  const char *text = skip_space(*cursor);
  size_t length = 0u;

  if (*text++ != '"') {
    return false;
  }
  while (*text != '\0' && *text != '"') {
    uint8_t value = (uint8_t)*text++;
    if (value == '\\') {
      if (*text != '\\' && *text != '"') {
        return false;
      }
      value = (uint8_t)*text++;
    }
    if (length >= LARGE_DBC_UNIT_MAX_BYTES) {
      return false;
    }
    output[length++] = (char)value;
  }
  if (*text != '"' || !valid_utf8((const uint8_t *)output, length)) {
    return false;
  }
  output[length] = '\0';
  *output_length = (uint8_t)length;
  *cursor = text + 1;
  return true;
}

static bool consume_receivers(const char **cursor) {
  const char *text = skip_space(*cursor);
  bool expect_identifier = true;

  while (*text != '\0') {
    if (expect_identifier) {
      if (!is_identifier_start(*text)) {
        return false;
      }
      do {
        ++text;
      } while (is_identifier_continue(*text));
      expect_identifier = false;
    } else if (*text == ',') {
      ++text;
      text = skip_space(text);
      expect_identifier = true;
    } else if (*text == ' ' || *text == '\t') {
      text = skip_space(text);
      if (*text != '\0') {
        return false;
      }
    } else {
      return false;
    }
  }
  if (expect_identifier) {
    return false;
  }
  *cursor = text;
  return true;
}

static bool signal_fits(uint16_t start_bit,
                        uint8_t bit_length,
                        bool motorola,
                        uint8_t payload_length) {
  const int frame_bits = (int)payload_length * 8;
  if ((int)start_bit >= frame_bits) {
    return false;
  }
  if (!motorola) {
    return (uint32_t)start_bit + bit_length <= (uint32_t)frame_bits;
  }
  int bit = start_bit;
  for (uint8_t i = 0u; i < bit_length; ++i) {
    if (bit < 0 || bit >= frame_bits) {
      return false;
    }
    bit = (bit % 8 == 0) ? bit + 15 : bit - 1;
  }
  return true;
}

static void store_u16_le(uint8_t *output, uint16_t value) {
  output[0] = (uint8_t)value;
  output[1] = (uint8_t)(value >> 8);
}

static void store_u32_le(uint8_t *output, uint32_t value) {
  for (size_t i = 0u; i < 4u; ++i) {
    output[i] = (uint8_t)(value >> (i * 8u));
  }
}

static void store_double_le(uint8_t *output, double value) {
  uint64_t bits;
  if (value == 0.0) {
    value = 0.0;
  }
  memcpy(&bits, &value, sizeof(bits));
  for (size_t i = 0u; i < 8u; ++i) {
    output[i] = (uint8_t)(bits >> (i * 8u));
  }
}

static uint64_t definition_hash(const DbcStreamSignalRecord *record) {
  uint8_t bytes[LARGE_DBC_DEFINITION_SERIALIZED_BYTES];
  size_t offset = 0u;
  uint64_t hash = LARGE_DBC_FNV1A64_OFFSET_BASIS;

  bytes[offset++] = LARGE_DBC_DEFINITION_FORMAT_VERSION;
  store_u32_le(&bytes[offset], record->normalized_id);
  offset += 4u;
  bytes[offset++] = record->flags;
  bytes[offset++] = record->declared_payload_length;
  store_u16_le(&bytes[offset], record->start_bit);
  offset += 2u;
  bytes[offset++] = record->bit_length;
  store_double_le(&bytes[offset], record->factor);
  offset += 8u;
  store_double_le(&bytes[offset], record->offset);
  offset += 8u;
  store_double_le(&bytes[offset], record->minimum);
  offset += 8u;
  store_double_le(&bytes[offset], record->maximum);
  offset += 8u;

  for (size_t i = 0u; i < offset; ++i) {
    hash ^= bytes[i];
    hash *= LARGE_DBC_FNV1A64_PRIME;
  }
  return hash;
}

static bool parse_message(DbcStreamParser *parser, const char *line) {
  const char *cursor = line + 4u;
  DbcStreamMessageRecord record;
  uint64_t raw_id;
  uint64_t dlc;
  char transmitter[LARGE_DBC_MESSAGE_NAME_MAX_BYTES + 1u];

  memset(&record, 0, sizeof(record));
  if (!parse_u64(&cursor, UINT32_MAX, &raw_id) ||
      !parse_identifier(&cursor, record.name, sizeof(record.name)) ||
      !consume_char(&cursor, ':') || !parse_u64(&cursor, 64u, &dlc) ||
      !parse_identifier(&cursor, transmitter, sizeof(transmitter)) ||
      *skip_space(cursor) != '\0') {
    return fail(parser, DBC_STREAM_ERROR_MALFORMED_MESSAGE);
  }

  if ((raw_id & DBC_VECTOR_EXTENDED_FLAG) != 0u) {
    if ((raw_id & ~(uint64_t)(DBC_VECTOR_EXTENDED_FLAG | DBC_EXTENDED_ID_MASK)) != 0u) {
      return fail(parser, DBC_STREAM_ERROR_CAN_ID);
    }
    record.normalized_id = (uint32_t)raw_id & DBC_EXTENDED_ID_MASK;
    record.flags |= LARGE_DBC_SIGNAL_FLAG_IDE;
  } else {
    if (raw_id > UINT32_C(0x7ff)) {
      return fail(parser, DBC_STREAM_ERROR_CAN_ID);
    }
    record.normalized_id = (uint32_t)raw_id;
  }
  if (dlc > 64u) {
    return fail(parser, DBC_STREAM_ERROR_DLC);
  }
  if (dlc > 8u) {
    record.flags |= LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED;
  }
  if (parser->message_count >= LARGE_DBC_CATALOG_MAX_MESSAGES) {
    return fail(parser, DBC_STREAM_ERROR_CATALOG_CAPACITY);
  }

  record.ordinal = parser->message_count;
  record.first_signal_ordinal = parser->signal_count;
  record.source_line = parser->line_number;
  record.source_offset = parser->line_source_offset;
  record.declared_payload_length = (uint8_t)dlc;
  parser->current_message = record;
  parser->have_current_message = true;
  ++parser->message_count;

  if (parser->callbacks.on_message != NULL &&
      !parser->callbacks.on_message(parser->callback_context, &record)) {
    return fail(parser, DBC_STREAM_ERROR_CALLBACK_REJECTED);
  }
  return true;
}

static bool parse_signal(DbcStreamParser *parser, const char *line) {
  const char *cursor = line + 4u;
  DbcStreamSignalRecord record;
  char signal_name[LARGE_DBC_SIGNAL_NAME_MAX_BYTES + 1u];
  uint64_t start_bit;
  uint64_t bit_length;
  char endian;
  char sign;
  size_t message_name_length;
  size_t signal_name_length;

  if (!parser->have_current_message) {
    return fail(parser, DBC_STREAM_ERROR_SIGNAL_WITHOUT_MESSAGE);
  }
  memset(&record, 0, sizeof(record));
  if (!parse_identifier(&cursor, signal_name, sizeof(signal_name))) {
    return fail(parser, DBC_STREAM_ERROR_IDENTIFIER);
  }
  cursor = skip_space(cursor);
  if (*cursor != ':') {
    if (*cursor == 'M' || *cursor == 'm') {
      return fail(parser, DBC_STREAM_ERROR_MULTIPLEX_UNSUPPORTED);
    }
    return fail(parser, DBC_STREAM_ERROR_MALFORMED_SIGNAL);
  }
  ++cursor;
  if (!parse_u64(&cursor, UINT16_MAX, &start_bit) || !consume_char(&cursor, '|') ||
      !parse_u64(&cursor, UINT8_MAX, &bit_length) || !consume_char(&cursor, '@')) {
    return fail(parser, DBC_STREAM_ERROR_MALFORMED_SIGNAL);
  }
  cursor = skip_space(cursor);
  endian = *cursor++;
  sign = *cursor++;
  if ((endian != '0' && endian != '1') || (sign != '+' && sign != '-') ||
      !consume_char(&cursor, '(') || !parse_finite_double(&cursor, &record.factor) ||
      !consume_char(&cursor, ',') || !parse_finite_double(&cursor, &record.offset) ||
      !consume_char(&cursor, ')') || !consume_char(&cursor, '[') ||
      !parse_finite_double(&cursor, &record.minimum) || !consume_char(&cursor, '|') ||
      !parse_finite_double(&cursor, &record.maximum) || !consume_char(&cursor, ']') ||
      !parse_unit(&cursor, record.unit, &record.unit_length) ||
      !consume_receivers(&cursor)) {
    return fail(parser, DBC_STREAM_ERROR_MALFORMED_SIGNAL);
  }
  if (bit_length == 0u || bit_length > 63u || record.factor == 0.0 ||
      record.minimum > record.maximum) {
    return fail(parser, DBC_STREAM_ERROR_NUMBER);
  }

  record.ordinal = parser->signal_count;
  record.message_ordinal = parser->current_message.ordinal;
  record.normalized_id = parser->current_message.normalized_id;
  record.source_line = parser->line_number;
  record.source_offset = parser->line_source_offset;
  record.start_bit = (uint16_t)start_bit;
  record.bit_length = (uint8_t)bit_length;
  record.flags = parser->current_message.flags;
  record.declared_payload_length = parser->current_message.declared_payload_length;
  if (endian == '0') {
    record.flags |= LARGE_DBC_SIGNAL_FLAG_MOTOROLA;
  }
  if (sign == '-') {
    record.flags |= LARGE_DBC_SIGNAL_FLAG_SIGNED;
  }
  if (!signal_fits(record.start_bit, record.bit_length,
                   endian == '0', record.declared_payload_length)) {
    return fail(parser, DBC_STREAM_ERROR_LAYOUT);
  }

  message_name_length = strlen(parser->current_message.name);
  signal_name_length = strlen(signal_name);
  if (message_name_length + 1u + signal_name_length > LARGE_DBC_KEY_MAX_BYTES) {
    return fail(parser, DBC_STREAM_ERROR_STRING_LIMIT);
  }
  memcpy(record.key, parser->current_message.name, message_name_length);
  record.key[message_name_length] = '.';
  memcpy(&record.key[message_name_length + 1u], signal_name, signal_name_length + 1u);
  record.key_length = (uint8_t)(message_name_length + 1u + signal_name_length);
  record.definition_hash = definition_hash(&record);

  if (parser->signal_count >= LARGE_DBC_CATALOG_MAX_SIGNALS) {
    return fail(parser, DBC_STREAM_ERROR_CATALOG_CAPACITY);
  }
  ++parser->signal_count;
  if (parser->callbacks.on_signal != NULL &&
      !parser->callbacks.on_signal(parser->callback_context, &record)) {
    return fail(parser, DBC_STREAM_ERROR_CALLBACK_REJECTED);
  }
  return true;
}

static bool is_unsupported_decode_syntax(const char *line) {
  return keyword_with_arguments(line, "SIG_VALTYPE_") ||
         keyword_with_arguments(line, "SGTYPE_") ||
         keyword_with_arguments(line, "SGTYPE_VAL_") ||
         keyword_with_arguments(line, "SIG_TYPE_REF_");
}

static bool dispatch_line(DbcStreamParser *parser) {
  const char *line;
  size_t length = parser->line_length;

  ++parser->line_number;
  if (parser->line_has_nul) {
    return fail(parser, DBC_STREAM_ERROR_NUL_BYTE);
  }
  if (!parser->line_overflow && length > 0u && parser->line[length - 1u] == '\r') {
    --length;
  }
  if (!parser->line_overflow) {
    parser->line[length] = '\0';
  } else {
    parser->line[LARGE_DBC_PARSER_LINE_BYTES - 1u] = '\0';
  }
  line = skip_space(parser->line);

  if (incomplete_record_marker(line, "BO_")) {
    return fail(parser, DBC_STREAM_ERROR_MALFORMED_MESSAGE);
  }
  if (incomplete_record_marker(line, "SG_")) {
    return fail(parser, DBC_STREAM_ERROR_MALFORMED_SIGNAL);
  }

  if (keyword_with_arguments(line, "SG_MUL_VAL_")) {
    return fail(parser, DBC_STREAM_ERROR_MULTIPLEX_UNSUPPORTED);
  }
  if (is_unsupported_decode_syntax(line) ||
      (keyword_with_arguments(line, "BA_") && parser->line_saw_vframe_format)) {
    return fail(parser, DBC_STREAM_ERROR_DECODE_SYNTAX_UNSUPPORTED);
  }
  if (parser->line_overflow) {
    if (keyword_with_arguments(line, "BO_") || keyword_with_arguments(line, "SG_")) {
      return fail(parser, DBC_STREAM_ERROR_RELEVANT_LINE_TOO_LONG);
    }
    ++parser->skipped_lines;
    return true;
  }
  if (keyword_with_arguments(line, "BO_")) {
    return parse_message(parser, line);
  }
  if (keyword_with_arguments(line, "SG_")) {
    return parse_signal(parser, line);
  }
  if (strcmp(line, "BO_") == 0) {
    return fail(parser, DBC_STREAM_ERROR_MALFORMED_MESSAGE);
  }
  if (strcmp(line, "SG_") == 0) {
    return fail(parser, DBC_STREAM_ERROR_MALFORMED_SIGNAL);
  }
  ++parser->skipped_lines;
  return true;
}

static void reset_line(DbcStreamParser *parser) {
  parser->line_length = 0u;
  parser->line_overflow = false;
  parser->line_has_nul = false;
  parser->line_saw_vframe_format = false;
  parser->vframe_match_length = 0u;
  parser->line_source_offset = parser->source_bytes;
}

static void observe_vframe_byte(DbcStreamParser *parser, uint8_t value) {
  static const char marker[] = "VFrameFormat";
  if (value == (uint8_t)marker[parser->vframe_match_length]) {
    ++parser->vframe_match_length;
    if (parser->vframe_match_length == sizeof(marker) - 1u) {
      parser->line_saw_vframe_format = true;
      parser->vframe_match_length = 0u;
    }
  } else {
    parser->vframe_match_length = value == (uint8_t)marker[0] ? 1u : 0u;
  }
}

void dbc_stream_parser_init(DbcStreamParser *parser,
                            const DbcStreamParserCallbacks *callbacks,
                            void *callback_context) {
  if (parser == NULL) {
    return;
  }
  memset(parser, 0, sizeof(*parser));
  if (callbacks != NULL) {
    parser->callbacks = *callbacks;
  }
  parser->callback_context = callback_context;
}

bool dbc_stream_parser_feed(DbcStreamParser *parser,
                            const uint8_t *data,
                            size_t size) {
  if (parser == NULL || (data == NULL && size != 0u)) {
    if (parser != NULL) {
      (void)fail(parser, DBC_STREAM_ERROR_INVALID_ARGUMENT);
    }
    return false;
  }
  if (parser->error != DBC_STREAM_ERROR_NONE) {
    return false;
  }
  if (parser->finalized) {
    return fail(parser, DBC_STREAM_ERROR_ALREADY_FINALIZED);
  }
  for (size_t i = 0u; i < size; ++i) {
    const uint8_t value = data[i];
    if (parser->source_bytes == LARGE_DBC_SOURCE_MAX_BYTES) {
      return fail(parser, DBC_STREAM_ERROR_SOURCE_LIMIT);
    }
    ++parser->source_bytes;
    if (value == '\n') {
      if (!dispatch_line(parser)) {
        return false;
      }
      reset_line(parser);
      continue;
    }
    if (value == 0u) {
      parser->line_has_nul = true;
    }
    observe_vframe_byte(parser, value);
    if (parser->line_length + 1u < LARGE_DBC_PARSER_LINE_BYTES) {
      parser->line[parser->line_length++] = (char)value;
    } else {
      parser->line_overflow = true;
    }
  }
  return true;
}

bool dbc_stream_parser_finalize(DbcStreamParser *parser) {
  if (parser == NULL) {
    return false;
  }
  if (parser->error != DBC_STREAM_ERROR_NONE) {
    return false;
  }
  if (parser->finalized) {
    return fail(parser, DBC_STREAM_ERROR_ALREADY_FINALIZED);
  }
  if (parser->line_length != 0u || parser->line_overflow || parser->line_has_nul) {
    if (!dispatch_line(parser)) {
      return false;
    }
  }
  parser->finalized = true;
  return true;
}

const char *dbc_stream_parser_error_name(DbcStreamParserError error) {
  static const char *const names[] = {
      "NONE", "INVALID_ARGUMENT", "ALREADY_FINALIZED", "NUL_BYTE",
      "RELEVANT_LINE_TOO_LONG", "MALFORMED_MESSAGE", "MALFORMED_SIGNAL",
      "SIGNAL_WITHOUT_MESSAGE", "MULTIPLEX_UNSUPPORTED",
      "DECODE_SYNTAX_UNSUPPORTED", "IDENTIFIER", "STRING_LIMIT",
      "INVALID_UTF8", "NUMBER", "CAN_ID", "DLC", "LAYOUT",
      "CATALOG_CAPACITY", "SOURCE_LIMIT", "CALLBACK_REJECTED"};
  const size_t count = sizeof(names) / sizeof(names[0]);
  return (size_t)error < count ? names[error] : "UNKNOWN";
}
