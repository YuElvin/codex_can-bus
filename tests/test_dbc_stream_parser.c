#include "dbc_stream_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_TRUE(condition) do { if (!(condition)) { \
  fprintf(stderr, "ASSERT failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
  return false; } } while (0)

typedef struct {
  DbcStreamMessageRecord messages[LARGE_DBC_CATALOG_MAX_MESSAGES];
  DbcStreamSignalRecord signals[LARGE_DBC_CATALOG_MAX_SIGNALS];
  size_t message_count;
  size_t signal_count;
} Capture;

static uint32_t crc32_update(uint32_t state, const uint8_t *data, size_t size) {
  while (size-- > 0u) {
    state ^= *data++;
    for (unsigned bit = 0u; bit < 8u; ++bit) {
      state = (state >> 1u) ^ ((state & 1u) != 0u ?
        LARGE_DBC_CRC32_REFLECTED_POLYNOMIAL : 0u);
    }
  }
  return state;
}

static bool capture_message(void *context, const DbcStreamMessageRecord *record) {
  Capture *capture = context;
  for (size_t i = 0u; i < capture->message_count; ++i) {
    if (capture->messages[i].normalized_id == record->normalized_id &&
        ((capture->messages[i].flags ^ record->flags) & LARGE_DBC_SIGNAL_FLAG_IDE) == 0u) {
      return false;
    }
  }
  capture->messages[capture->message_count++] = *record;
  return true;
}

static bool capture_signal(void *context, const DbcStreamSignalRecord *record) {
  Capture *capture = context;
  for (size_t i = 0u; i < capture->signal_count; ++i) {
    if (strcmp(capture->signals[i].key, record->key) == 0) {
      return false;
    }
  }
  capture->signals[capture->signal_count++] = *record;
  return true;
}

static void init_parser(DbcStreamParser *parser, Capture *capture) {
  const DbcStreamParserCallbacks callbacks = {capture_message, capture_signal};
  memset(capture, 0, sizeof(*capture));
  dbc_stream_parser_init(parser, &callbacks, capture);
}

static bool parse_chunks(const uint8_t *data, size_t size, size_t chunk,
                         DbcStreamParser *parser, Capture *capture) {
  init_parser(parser, capture);
  for (size_t offset = 0u; offset < size;) {
    const size_t available = size - offset;
    const size_t amount = available < chunk ? available : chunk;
    if (!dbc_stream_parser_feed(parser, data + offset, amount)) {
      return false;
    }
    offset += amount;
  }
  return dbc_stream_parser_finalize(parser);
}

static bool expect_failure(const char *text, DbcStreamParserError error) {
  DbcStreamParser parser;
  Capture capture;
  ASSERT_TRUE(!parse_chunks((const uint8_t *)text, strlen(text), 3u, &parser, &capture));
  ASSERT_TRUE(parser.error == error);
  return true;
}

static bool test_supported_matrix(void) {
  static const char text[] =
      "VERSION \"x\"\r\n"
      "BO_ 256 Msg: 8 Vector__XXX\r\n"
      " SG_ Intel : 0|16@1+ (1e-2,-2.5E+1) [-25|6.5e2] \"km/h\" Node\r\n"
      " SG_ Moto : 7|8@0- (1,0) [-128|127] \"\" Node\r\n"
      "BO_ 2147483939 Ext: 9 Vector__XXX\r\n"
      " SG_ Wide : 0|63@1+ (1,0) [0|9.22e18] \"u\xC2\xB5\" Node";
  DbcStreamParser parser;
  Capture capture;

  ASSERT_TRUE(parse_chunks((const uint8_t *)text, strlen(text), 1u, &parser, &capture));
  ASSERT_TRUE(capture.message_count == 2u && capture.signal_count == 3u);
  ASSERT_TRUE(capture.messages[0].normalized_id == 256u);
  ASSERT_TRUE(capture.messages[1].normalized_id == 291u);
  ASSERT_TRUE((capture.messages[1].flags & LARGE_DBC_SIGNAL_FLAG_IDE) != 0u);
  ASSERT_TRUE((capture.messages[1].flags & LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED) != 0u);
  ASSERT_TRUE(strcmp(capture.signals[0].key, "Msg.Intel") == 0);
  ASSERT_TRUE(capture.signals[0].factor == 0.01 && capture.signals[0].offset == -25.0);
  ASSERT_TRUE((capture.signals[1].flags & LARGE_DBC_SIGNAL_FLAG_MOTOROLA) != 0u);
  ASSERT_TRUE((capture.signals[1].flags & LARGE_DBC_SIGNAL_FLAG_SIGNED) != 0u);
  ASSERT_TRUE(capture.signals[2].bit_length == 63u);
  return true;
}

static bool test_definition_hash_and_case(void) {
  static const char text[] =
      "BO_ 291 Msg: 8 Node\n"
      " SG_ Speed : 0|16@1+ (0.1,-40) [-40|215.5] \"\" Node\n"
      "BO_ 292 msg: 8 Node\n"
      " SG_ Speed : 0|1@1+ (1,0) [0|1] \"\" Node\n";
  DbcStreamParser parser;
  Capture capture;
  ASSERT_TRUE(parse_chunks((const uint8_t *)text, strlen(text), 7u, &parser, &capture));
  ASSERT_TRUE(strcmp(capture.signals[0].key, "Msg.Speed") == 0);
  ASSERT_TRUE(strcmp(capture.signals[1].key, "msg.Speed") == 0);
  if (capture.signals[0].definition_hash != LARGE_DBC_DEFINITION_HASH_GOLDEN_V1) {
    fprintf(stderr, "definition hash %016llX, values %.17g %.17g %.17g %.17g\n",
            (unsigned long long)capture.signals[0].definition_hash,
            capture.signals[0].factor, capture.signals[0].offset,
            capture.signals[0].minimum, capture.signals[0].maximum);
    return false;
  }
  return true;
}

static bool test_metadata_and_line_endings(void) {
  char *text = malloc(900u);
  DbcStreamParser parser;
  Capture capture;
  ASSERT_TRUE(text != NULL);
  memcpy(text, "CM_ \"", 5u);
  memset(text + 5u, 'x', 700u);
  strcpy(text + 705u,
         "\";\nBA_ \"GenMsgCycleTime\" BO_ 256 10;\n"
         "VAL_ 256 S 0 \"off\" 1 \"on\";\n"
         "BO_ 256 Msg: 8 Node\n SG_ S : 0|1@1+ (1,0) [0|1] \"\" Node\n");
  ASSERT_TRUE(parse_chunks((const uint8_t *)text, strlen(text), 17u, &parser, &capture));
  ASSERT_TRUE(capture.message_count == 1u && capture.signal_count == 1u);
  ASSERT_TRUE(parser.skipped_lines == 3u);
  free(text);
  return true;
}

static bool test_rejections(void) {
  ASSERT_TRUE(expect_failure("B", DBC_STREAM_ERROR_MALFORMED_MESSAGE));
  ASSERT_TRUE(expect_failure("BO", DBC_STREAM_ERROR_MALFORMED_MESSAGE));
  ASSERT_TRUE(expect_failure("BO_", DBC_STREAM_ERROR_MALFORMED_MESSAGE));
  ASSERT_TRUE(expect_failure("S", DBC_STREAM_ERROR_MALFORMED_SIGNAL));
  ASSERT_TRUE(expect_failure("SG", DBC_STREAM_ERROR_MALFORMED_SIGNAL));
  ASSERT_TRUE(expect_failure("SG_", DBC_STREAM_ERROR_MALFORMED_SIGNAL));
  ASSERT_TRUE(expect_failure("BO_ 256 M: 8 N\n SG_ S M : 0|1@1+ (1,0) [0|1] \"\" N\n",
                             DBC_STREAM_ERROR_MULTIPLEX_UNSUPPORTED));
  ASSERT_TRUE(expect_failure("SG_MUL_VAL_ 256 S M 0-1;\n",
                             DBC_STREAM_ERROR_MULTIPLEX_UNSUPPORTED));
  ASSERT_TRUE(expect_failure("SIG_VALTYPE_ 256 S : 1;\n",
                             DBC_STREAM_ERROR_DECODE_SYNTAX_UNSUPPORTED));
  ASSERT_TRUE(expect_failure("BA_ \"VFrameFormat\" BO_ 256 14;\n",
                             DBC_STREAM_ERROR_DECODE_SYNTAX_UNSUPPORTED));
  ASSERT_TRUE(expect_failure("BO_ 256 M: 8 N\n SG_ S : 0|0@1+ (1,0) [0|1] \"\" N\n",
                             DBC_STREAM_ERROR_NUMBER));
  ASSERT_TRUE(expect_failure("BO_ 256 M: 8 N\n SG_ S : 0|64@1+ (1,0) [0|1] \"\" N\n",
                             DBC_STREAM_ERROR_NUMBER));
  ASSERT_TRUE(expect_failure("BO_ 256 M: 1 N\n SG_ S : 0|9@1+ (1,0) [0|1] \"\" N\n",
                             DBC_STREAM_ERROR_LAYOUT));
  ASSERT_TRUE(expect_failure("BO_ 2048 M: 8 N\n", DBC_STREAM_ERROR_CAN_ID));
  ASSERT_TRUE(expect_failure("BO_ 256 M: 65 N\n", DBC_STREAM_ERROR_MALFORMED_MESSAGE));
  ASSERT_TRUE(expect_failure("BO_ 256 M: 8 N\n SG_ S : 0|1@1+ (nan,0) [0|1] \"\" N\n",
                             DBC_STREAM_ERROR_MALFORMED_SIGNAL));
  ASSERT_TRUE(expect_failure("BO_ 256 M: 8 N\n SG_ S : 0|1@1+ (1e9999,0) [0|1] \"\" N\n",
                             DBC_STREAM_ERROR_MALFORMED_SIGNAL));
  ASSERT_TRUE(expect_failure("BO_ 256 M: 8 N\n SG_ S : 0|1@1+ (0x1p0,0) [0|1] \"\" N\n",
                             DBC_STREAM_ERROR_MALFORMED_SIGNAL));
  ASSERT_TRUE(expect_failure("BO_ 256 M: 8 N\nBO_ 256 Again: 8 N\n",
                             DBC_STREAM_ERROR_CALLBACK_REJECTED));
  ASSERT_TRUE(expect_failure("BO_ 256 M: 8 N\n SG_ S : 0|1@1+ (1,0) [0|1] \"\" N\n"
                             "BO_ 257 M: 8 N\n SG_ S : 0|1@1+ (1,0) [0|1] \"\" N\n",
                             DBC_STREAM_ERROR_CALLBACK_REJECTED));
  ASSERT_TRUE(expect_failure("BO_ 256 M: 8 N\n SG_ S : 0|1@1+ (1,0)",
                             DBC_STREAM_ERROR_MALFORMED_SIGNAL));
  ASSERT_TRUE(expect_failure("BO_", DBC_STREAM_ERROR_MALFORMED_MESSAGE));
  ASSERT_TRUE(expect_failure("SG_", DBC_STREAM_ERROR_MALFORMED_SIGNAL));
  return true;
}

static bool test_name_unit_and_key_limits(void) {
  ASSERT_TRUE(expect_failure("BO_ 256 MessageNameThatIsDefinitelyLongerThan31Bytes: 8 N\n",
                             DBC_STREAM_ERROR_MALFORMED_MESSAGE));
  ASSERT_TRUE(expect_failure("BO_ 256 M: 8 N\n SG_ SignalNameThatIsDefinitelyLongerThan31Bytes : 0|1@1+ (1,0) [0|1] \"\" N\n",
                             DBC_STREAM_ERROR_IDENTIFIER));
  ASSERT_TRUE(expect_failure("BO_ 256 M: 8 N\n SG_ S : 0|1@1+ (1,0) [0|1] \"12345678901234567890123456789012\" N\n",
                             DBC_STREAM_ERROR_MALFORMED_SIGNAL));
  ASSERT_TRUE(expect_failure("BO_ 256 M: 8 N\n SG_ S : 0|1@1+ (1,0) [0|1] \"\xC0\x80\" N\n",
                             DBC_STREAM_ERROR_MALFORMED_SIGNAL));
  return true;
}

static bool test_dlc_zero_and_long_relevant(void) {
  static const char empty_message[] = "BO_ 0 Empty: 0 N";
  static const char full_message[] =
      "BO_ 1 Full: 64 N\n SG_ Last : 511|1@1+ (1,0) [0|1] \"\" N\n";
  DbcStreamParser parser;
  Capture capture;
  char text[700];
  ASSERT_TRUE(parse_chunks((const uint8_t *)empty_message, strlen(empty_message), 5u,
                           &parser, &capture));
  ASSERT_TRUE(capture.messages[0].declared_payload_length == 0u);
  ASSERT_TRUE(parse_chunks((const uint8_t *)full_message, strlen(full_message),
                           5u, &parser, &capture));
  ASSERT_TRUE(capture.messages[0].declared_payload_length == 64u);
  ASSERT_TRUE(capture.signals[0].start_bit == 511u);

  memcpy(text, "BO_ 1 ", 6u);
  memset(text + 6u, 'A', 600u);
  strcpy(text + 606u, ": 8 N\n");
  ASSERT_TRUE(!parse_chunks((const uint8_t *)text, strlen(text), 29u, &parser, &capture));
  ASSERT_TRUE(parser.error == DBC_STREAM_ERROR_RELEVANT_LINE_TOO_LONG);
  return true;
}

static bool test_catalog_caps(void) {
  static const char first_message[] = "BO_ 1 M: 8 N\n";
  static const char extra_message[] = "BO_ 2147483648 Extra: 8 N\n";
  DbcStreamParser parser;
  Capture capture;
  char line[96];
  int length;

  init_parser(&parser, &capture);
  ASSERT_TRUE(dbc_stream_parser_feed(&parser, (const uint8_t *)first_message,
                                     strlen(first_message)));
  for (unsigned int i = 0u; i < LARGE_DBC_CATALOG_MAX_SIGNALS; ++i) {
    length = snprintf(line, sizeof(line),
                      " SG_ S%u : 0|1@1+ (1,0) [0|1] \"\" N\n", i);
    ASSERT_TRUE(length > 0);
    ASSERT_TRUE(dbc_stream_parser_feed(&parser, (const uint8_t *)line, (size_t)length));
  }
  length = snprintf(line, sizeof(line),
                    " SG_ S%u : 0|1@1+ (1,0) [0|1] \"\" N\n",
                    LARGE_DBC_CATALOG_MAX_SIGNALS);
  ASSERT_TRUE(!dbc_stream_parser_feed(&parser, (const uint8_t *)line, (size_t)length));
  ASSERT_TRUE(parser.error == DBC_STREAM_ERROR_CATALOG_CAPACITY);

  init_parser(&parser, &capture);
  for (unsigned int i = 0u; i < LARGE_DBC_CATALOG_MAX_MESSAGES; ++i) {
    length = snprintf(line, sizeof(line), "BO_ %u M%u: 8 N\n", i, i);
    ASSERT_TRUE(length > 0);
    ASSERT_TRUE(dbc_stream_parser_feed(&parser, (const uint8_t *)line, (size_t)length));
  }
  ASSERT_TRUE(!dbc_stream_parser_feed(&parser,
                                      (const uint8_t *)extra_message,
                                      strlen(extra_message)));
  ASSERT_TRUE(parser.error == DBC_STREAM_ERROR_CATALOG_CAPACITY);
  return true;
}

static uint32_t fuzz_next(uint32_t *state) {
  *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
  return *state;
}

static bool test_deterministic_fuzz(void) {
  uint32_t state = UINT32_C(0x12345678);
  uint8_t data[300];
  for (size_t run = 0u; run < 500u; ++run) {
    DbcStreamParser parser;
    Capture capture;
    const size_t length = fuzz_next(&state) % sizeof(data);
    for (size_t i = 0u; i < length; ++i) {
      data[i] = (uint8_t)fuzz_next(&state);
    }
    (void)parse_chunks(data, length, (fuzz_next(&state) % 31u) + 1u, &parser, &capture);
    ASSERT_TRUE(parser.message_count <= LARGE_DBC_CATALOG_MAX_MESSAGES);
    ASSERT_TRUE(parser.signal_count <= LARGE_DBC_CATALOG_MAX_SIGNALS);
  }
  return true;
}

static bool test_source_size_boundary(void) {
  uint8_t *data = malloc(LARGE_DBC_SOURCE_MAX_BYTES + 1u);
  DbcStreamParser parser;
  Capture capture;
  ASSERT_TRUE(data != NULL);
  memset(data, '\n', LARGE_DBC_SOURCE_MAX_BYTES + 1u);
  init_parser(&parser, &capture);
  ASSERT_TRUE(dbc_stream_parser_feed(&parser, data, LARGE_DBC_SOURCE_MAX_BYTES));
  ASSERT_TRUE(parser.source_bytes == LARGE_DBC_SOURCE_MAX_BYTES);
  ASSERT_TRUE(dbc_stream_parser_finalize(&parser));
  init_parser(&parser, &capture);
  ASSERT_TRUE(!dbc_stream_parser_feed(&parser,
                                      data,
                                      LARGE_DBC_SOURCE_MAX_BYTES + 1u));
  ASSERT_TRUE(parser.error == DBC_STREAM_ERROR_SOURCE_LIMIT);
  ASSERT_TRUE(parser.source_bytes == LARGE_DBC_SOURCE_MAX_BYTES);
  free(data);
  return true;
}

static bool test_real_file(const char *path) {
  FILE *file = fopen(path, "rb");
  uint8_t chunk[137];
  DbcStreamParser parser;
  Capture capture;
  size_t amount;
  uint32_t crc = LARGE_DBC_CRC32_INITIAL_VALUE;
  ASSERT_TRUE(file != NULL);
  init_parser(&parser, &capture);
  while ((amount = fread(chunk, 1u, sizeof(chunk), file)) != 0u) {
    crc = crc32_update(crc, chunk, amount);
    if (!dbc_stream_parser_feed(&parser, chunk, amount)) {
      fprintf(stderr, "real DBC parser error %s at line %u offset %u\n",
              dbc_stream_parser_error_name(parser.error), parser.error_line,
              parser.error_offset);
      fclose(file);
      return false;
    }
  }
  ASSERT_TRUE(!ferror(file));
  fclose(file);
  ASSERT_TRUE(dbc_stream_parser_finalize(&parser));
  ASSERT_TRUE(parser.source_bytes == 100071u);
  ASSERT_TRUE((crc ^ LARGE_DBC_CRC32_FINAL_XOR) == UINT32_C(0x4B88D9CE));
  ASSERT_TRUE(parser.line_number == 1606u);
  ASSERT_TRUE(capture.message_count == 112u);
  ASSERT_TRUE(capture.signal_count == 896u);
  ASSERT_TRUE(capture.messages[0].normalized_id == 256u);
  ASSERT_TRUE(capture.messages[111].normalized_id == 367u);
  ASSERT_TRUE(capture.messages[0].source_line == 40u);
  ASSERT_TRUE(capture.messages[0].source_offset == 615u);
  ASSERT_TRUE(capture.messages[111].source_line == 1594u);
  ASSERT_TRUE(capture.messages[111].source_offset == 99184u);
  ASSERT_TRUE(capture.signals[0].ordinal == 0u);
  ASSERT_TRUE(capture.signals[895].ordinal == 895u);
  ASSERT_TRUE(capture.signals[0].source_line == 41u);
  ASSERT_TRUE(capture.signals[0].source_offset == 668u);
  ASSERT_TRUE(capture.signals[895].source_line == 1602u);
  ASSERT_TRUE(capture.signals[895].source_offset == 99739u);
  ASSERT_TRUE(strcmp(capture.signals[0].key,
                     "ClassicCanTest_BattMod_001.PackVoltage_Module001") == 0);
  ASSERT_TRUE(strcmp(capture.signals[895].key,
                     "ClassicCanTest_BattMod_112.Reserved_Module112") == 0);
  for (size_t i = 0u; i < capture.message_count; ++i) {
    ASSERT_TRUE(capture.messages[i].declared_payload_length == 8u);
    ASSERT_TRUE((capture.messages[i].flags & LARGE_DBC_SIGNAL_FLAG_IDE) == 0u);
  }
  for (size_t i = 0u; i < capture.signal_count; ++i) {
    ASSERT_TRUE(capture.signals[i].message_ordinal == i / 8u);
    ASSERT_TRUE(capture.signals[i].declared_payload_length == 8u);
    ASSERT_TRUE((capture.signals[i].flags &
                 (LARGE_DBC_SIGNAL_FLAG_IDE |
                  LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED |
                  LARGE_DBC_SIGNAL_FLAG_MOTOROLA)) == 0u);
  }
  return true;
}

int main(int argc, char **argv) {
  ASSERT_TRUE(test_supported_matrix());
  ASSERT_TRUE(test_definition_hash_and_case());
  ASSERT_TRUE(test_metadata_and_line_endings());
  ASSERT_TRUE(test_rejections());
  ASSERT_TRUE(test_name_unit_and_key_limits());
  ASSERT_TRUE(test_dlc_zero_and_long_relevant());
  ASSERT_TRUE(test_catalog_caps());
  ASSERT_TRUE(test_deterministic_fuzz());
  ASSERT_TRUE(test_source_size_boundary());
  if (argc > 1) {
    ASSERT_TRUE(test_real_file(argv[1]));
  }
  puts("dbc stream parser tests passed");
  return 0;
}
