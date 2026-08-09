#include "dbc_catalog_index.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_TRUE(condition)                                                   \
  do {                                                                           \
    if (!(condition)) {                                                          \
      fprintf(stderr, "assertion failed: %s:%d: %s\n", __FILE__, __LINE__,     \
              #condition);                                                       \
      return false;                                                              \
    }                                                                            \
  } while (0)

#define ASSERT_STATUS(actual, expected) ASSERT_TRUE((actual) == (expected))

#define MAX_MESSAGE_SPOOL_BYTES \
  (LARGE_DBC_CATALOG_MAX_MESSAGES * LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE)
#define MAX_SIGNAL_SPOOL_BYTES \
  (LARGE_DBC_CATALOG_MAX_SIGNALS * LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE)
#define MAX_INDEX_BYTES \
  (LARGE_DBC_INDEX_HEADER_SIZE + MAX_MESSAGE_SPOOL_BYTES + MAX_SIGNAL_SPOOL_BYTES)

typedef struct {
  uint8_t *data;
  uint32_t size;
  uint32_t capacity;
} MemoryFile;

static uint8_t g_message_bytes[MAX_MESSAGE_SPOOL_BYTES];
static uint8_t g_signal_bytes[MAX_SIGNAL_SPOOL_BYTES];
static uint8_t g_index_bytes[MAX_INDEX_BYTES];
static MemoryFile g_messages = {g_message_bytes, 0u, sizeof(g_message_bytes)};
static MemoryFile g_signals = {g_signal_bytes, 0u, sizeof(g_signal_bytes)};
static MemoryFile g_index = {g_index_bytes, 0u, sizeof(g_index_bytes)};
static DbcCatalogIndexBuilder g_builder;
static DbcCatalogIndexVerifyWorkspace g_verify_workspace;

typedef struct {
  char data[2048];
  size_t size;
} TextOutput;

static bool text_write(void *context, const char *text, size_t size) {
  TextOutput *output = context;
  if (size > sizeof(output->data) - output->size - 1u) {
    return false;
  }
  memcpy(output->data + output->size, text, size);
  output->size += size;
  output->data[output->size] = '\0';
  return true;
}

static bool memory_read(void *context,
                        uint32_t offset,
                        uint8_t *data,
                        uint32_t size) {
  MemoryFile *file = context;
  if (offset > file->size || size > file->size - offset) {
    return false;
  }
  memcpy(data, file->data + offset, size);
  return true;
}

static bool memory_write(void *context,
                         uint32_t offset,
                         const uint8_t *data,
                         uint32_t size) {
  MemoryFile *file = context;
  if (offset > file->capacity || size > file->capacity - offset) {
    return false;
  }
  memcpy(file->data + offset, data, size);
  if (offset + size > file->size) {
    file->size = offset + size;
  }
  return true;
}

static bool memory_resize(void *context, uint32_t size) {
  MemoryFile *file = context;
  if (size > file->capacity) {
    return false;
  }
  if (size > file->size) {
    memset(file->data + file->size, 0, size - file->size);
  }
  file->size = size;
  return true;
}

static bool memory_size(void *context, uint32_t *size) {
  MemoryFile *file = context;
  *size = file->size;
  return true;
}

static DbcCatalogIndexIo memory_io(MemoryFile *file) {
  DbcCatalogIndexIo io = {
    .context = file,
    .read_at = memory_read,
    .write_at = memory_write,
    .resize = memory_resize,
    .get_size = memory_size
  };
  return io;
}

static void reset_files(void) {
  g_messages.size = 0u;
  g_signals.size = 0u;
  g_index.size = 0u;
}

static uint32_t test_get_u32(const uint8_t *data) {
  return (uint32_t)data[0] | ((uint32_t)data[1] << 8u) |
         ((uint32_t)data[2] << 16u) | ((uint32_t)data[3] << 24u);
}

static void test_put_u32(uint8_t *data, uint32_t value) {
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8u);
  data[2] = (uint8_t)(value >> 16u);
  data[3] = (uint8_t)(value >> 24u);
}

static uint32_t crc32_bytes(const uint8_t *data, size_t size) {
  uint32_t crc = UINT32_C(0xFFFFFFFF);
  while (size-- > 0u) {
    crc ^= *data++;
    for (unsigned bit = 0u; bit < 8u; ++bit) {
      crc = (crc >> 1u) ^ ((crc & 1u) ? UINT32_C(0xEDB88320) : 0u);
    }
  }
  return crc ^ UINT32_C(0xFFFFFFFF);
}

static void repair_header_crc(MemoryFile *index) {
  test_put_u32(index->data + LARGE_DBC_INDEX_HEADER_CRC32_OFFSET,
               crc32_bytes(index->data, LARGE_DBC_INDEX_HEADER_CRC32_COVERED_BYTES));
}

static DbcCatalogIndexSignalInput signal_input(
  const DbcCatalogIndexMessageInput *message,
  const char *key,
  uint16_t start_bit) {
  DbcCatalogIndexSignalInput signal = {
    .start_bit = start_bit,
    .bit_length = 1u,
    .motorola = false,
    .is_signed = false,
    .factor = 1.0,
    .offset = 0.0,
    .minimum = 0.0,
    .maximum = 1.0,
    .source_line = 3u,
    .source_offset = 20u,
    .key = key,
    .key_length = (uint8_t)strlen(key),
    .unit = "V",
    .unit_length = 1u
  };
  signal.definition_hash = dbc_catalog_definition_hash_v1(message, &signal);
  return signal;
}

static bool begin_builder(uint32_t source_size, uint32_t source_crc) {
  reset_files();
  const DbcCatalogIndexIo message_io = memory_io(&g_messages);
  const DbcCatalogIndexIo signal_io = memory_io(&g_signals);
  ASSERT_STATUS(dbc_catalog_index_builder_init(&g_builder,
                                               &message_io,
                                               &signal_io,
                                               source_size,
                                               source_crc),
                DBC_CATALOG_INDEX_OK);
  return true;
}

static bool build_golden_index(DbcCatalogIndexSummary *summary) {
  ASSERT_TRUE(begin_builder(100071u, UINT32_C(0x4B88D9CE)));
  const DbcCatalogIndexMessageInput message = {
    .normalized_id = 0x123u,
    .source_line = 2u,
    .declared_payload_length = 8u,
    .ide = false
  };
  ASSERT_STATUS(dbc_catalog_index_builder_begin_message(&g_builder, &message),
                DBC_CATALOG_INDEX_OK);
  DbcCatalogIndexSignalInput signal = signal_input(&message, "Msg.Speed", 0u);
  signal.bit_length = 16u;
  signal.factor = 0.1;
  signal.offset = -40.0;
  signal.minimum = -40.0;
  signal.maximum = 215.5;
  signal.definition_hash = dbc_catalog_definition_hash_v1(&message, &signal);
  ASSERT_TRUE(signal.definition_hash == LARGE_DBC_DEFINITION_HASH_GOLDEN_V1);
  ASSERT_STATUS(dbc_catalog_index_builder_add_signal(&g_builder, &signal),
                DBC_CATALOG_INDEX_OK);
  const DbcCatalogIndexIo output = memory_io(&g_index);
  ASSERT_STATUS(dbc_catalog_index_builder_finalize(&g_builder, &output, summary),
                DBC_CATALOG_INDEX_OK);
  return true;
}

static bool test_crc_and_real_source(const char *fixture_path) {
  static const uint8_t check[] = "123456789";
  ASSERT_TRUE(dbc_catalog_crc32_finish(
                dbc_catalog_crc32_update(dbc_catalog_crc32_begin(),
                                         check,
                                         sizeof(check) - 1u)) ==
              UINT32_C(0xCBF43926));
  if (fixture_path == NULL) {
    return true;
  }
  FILE *file = fopen(fixture_path, "rb");
  ASSERT_TRUE(file != NULL);
  uint8_t buffer[512];
  uint32_t crc = dbc_catalog_crc32_begin();
  size_t total = 0u;
  size_t count = 0u;
  while ((count = fread(buffer, 1u, sizeof(buffer), file)) != 0u) {
    crc = dbc_catalog_crc32_update(crc, buffer, count);
    total += count;
  }
  ASSERT_TRUE(ferror(file) == 0);
  fclose(file);
  ASSERT_TRUE(total == 100071u);
  ASSERT_TRUE(dbc_catalog_crc32_finish(crc) == UINT32_C(0x4B88D9CE));
  return true;
}

static bool test_golden_verify_read_dump(void) {
  DbcCatalogIndexSummary built;
  ASSERT_TRUE(build_golden_index(&built));
  ASSERT_TRUE(built.message_count == 1u && built.signal_count == 1u);
  ASSERT_TRUE(built.total_size == 256u);
  static const uint8_t golden_message[16] = {
    0x23, 0x01, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x08, 0x00, 0x00
  };
  ASSERT_TRUE(memcmp(g_index.data + LARGE_DBC_INDEX_HEADER_SIZE,
                     golden_message,
                     sizeof(golden_message)) == 0);
  ASSERT_TRUE(g_index.data[LARGE_DBC_INDEX_SIGNAL_RECORDS_OFFSET_OFFSET] == 96u);
  ASSERT_TRUE(memcmp(g_index.data + 96u + LARGE_DBC_SIGNAL_RECORD_KEY_OFFSET,
                     "Msg.Speed",
                     9u) == 0);
  /* Full-file CRC freezes all 256 v1 bytes, including floating-point encoding. */
  ASSERT_TRUE(crc32_bytes(g_index.data, g_index.size) == UINT32_C(0x27001AF0));
  const DbcCatalogIndexExpectedSource expected = {
    .enabled = true,
    .source_size = 100071u,
    .source_crc32 = UINT32_C(0x4B88D9CE)
  };
  const DbcCatalogIndexIo index_io = memory_io(&g_index);
  DbcCatalogIndexSummary verified;
  ASSERT_STATUS(dbc_catalog_index_verify(&index_io,
                                         &expected,
                                         &g_verify_workspace,
                                         &verified),
                DBC_CATALOG_INDEX_OK);
  DbcCatalogIndexMessage message;
  DbcCatalogIndexSignal signal;
  ASSERT_STATUS(dbc_catalog_index_read_message(&index_io, &verified, 0u, &message),
                DBC_CATALOG_INDEX_OK);
  ASSERT_STATUS(dbc_catalog_index_read_signal(&index_io, &verified, 0u, &signal),
                DBC_CATALOG_INDEX_OK);
  ASSERT_TRUE(message.normalized_id == 0x123u && message.signal_count == 1u);
  ASSERT_TRUE(strcmp(signal.key, "Msg.Speed") == 0 && signal.factor == 0.1);
  TextOutput dump = {{0}, 0u};
  ASSERT_STATUS(dbc_catalog_index_dump(&index_io, &verified, text_write, &dump),
                DBC_CATALOG_INDEX_OK);
  ASSERT_TRUE(strstr(dump.data,
                     "INDEX v1 source_size=100071 source_crc32=4B88D9CE messages=1 signals=1 total=256") != NULL);
  ASSERT_TRUE(strstr(dump.data,
                     "MESSAGE ordinal=0 id=00000123 ide=0 frame=CLASSIC_OR_FD length=8") != NULL);
  ASSERT_TRUE(strstr(dump.data,
                     "SIGNAL ordinal=0 message=0 id=00000123") != NULL);
  ASSERT_TRUE(strstr(dump.data,
                     "definition_hash=9C93DAD64DB4DC23") != NULL);
  ASSERT_TRUE(strstr(dump.data,
                     "factor_bits=3FB999999999999A offset_bits=C044000000000000") != NULL);
  ASSERT_TRUE(strstr(dump.data,
                     "SIGNAL_TEXT ordinal=0 key_hex=4D73672E5370656564 unit_hex=56") != NULL);
  return true;
}

static bool test_string_semantics(void) {
  static const char *invalid_keys[] = {
    "1Msg.Speed", "MsgSpeed", "Msg.Speed.More", "Msg.S-peed"
  };
  for (size_t i = 0u; i < sizeof(invalid_keys) / sizeof(invalid_keys[0]); ++i) {
    ASSERT_TRUE(begin_builder(12u, 34u));
    const DbcCatalogIndexMessageInput message = {
      .normalized_id = 0x321u, .source_line = 1u,
      .declared_payload_length = 8u, .ide = false
    };
    ASSERT_STATUS(dbc_catalog_index_builder_begin_message(&g_builder, &message),
                  DBC_CATALOG_INDEX_OK);
    DbcCatalogIndexSignalInput signal = signal_input(&message, invalid_keys[i], 0u);
    ASSERT_STATUS(dbc_catalog_index_builder_add_signal(&g_builder, &signal),
                  DBC_CATALOG_INDEX_INVALID_RECORD);
  }
  ASSERT_TRUE(begin_builder(12u, 34u));
  const DbcCatalogIndexMessageInput message = {
    .normalized_id = 0x321u, .source_line = 1u,
    .declared_payload_length = 8u, .ide = false
  };
  ASSERT_STATUS(dbc_catalog_index_builder_begin_message(&g_builder, &message),
                DBC_CATALOG_INDEX_OK);
  DbcCatalogIndexSignalInput upper = signal_input(&message, "Msg.Speed", 0u);
  DbcCatalogIndexSignalInput lower = signal_input(&message, "msg.Speed", 1u);
  ASSERT_STATUS(dbc_catalog_index_builder_add_signal(&g_builder, &upper),
                DBC_CATALOG_INDEX_OK);
  ASSERT_STATUS(dbc_catalog_index_builder_add_signal(&g_builder, &lower),
                DBC_CATALOG_INDEX_OK);
  static const char invalid_utf8[] = "\xC2";
  DbcCatalogIndexSignalInput unit = signal_input(&message, "Msg.Unit", 2u);
  unit.unit = invalid_utf8;
  unit.unit_length = 1u;
  ASSERT_STATUS(dbc_catalog_index_builder_add_signal(&g_builder, &unit),
                DBC_CATALOG_INDEX_INVALID_RECORD);
  unit.unit = "\x1F";
  ASSERT_STATUS(dbc_catalog_index_builder_add_signal(&g_builder, &unit),
                DBC_CATALOG_INDEX_INVALID_RECORD);
  return true;
}

static bool test_duplicate_rejection(void) {
  ASSERT_TRUE(begin_builder(12u, 34u));
  const DbcCatalogIndexMessageInput message = {
    .normalized_id = 0x321u, .source_line = 1u,
    .declared_payload_length = 8u, .ide = false
  };
  ASSERT_STATUS(dbc_catalog_index_builder_begin_message(&g_builder, &message),
                DBC_CATALOG_INDEX_OK);
  ASSERT_STATUS(dbc_catalog_index_builder_begin_message(&g_builder, &message),
                DBC_CATALOG_INDEX_DUPLICATE_MESSAGE);
  DbcCatalogIndexSignalInput signal = signal_input(&message, "M.S", 0u);
  ASSERT_STATUS(dbc_catalog_index_builder_add_signal(&g_builder, &signal),
                DBC_CATALOG_INDEX_OK);
  signal.start_bit = 1u;
  signal.definition_hash = dbc_catalog_definition_hash_v1(&message, &signal);
  ASSERT_STATUS(dbc_catalog_index_builder_add_signal(&g_builder, &signal),
                DBC_CATALOG_INDEX_DUPLICATE_KEY);
  return true;
}

static bool test_max_key_utf8_unit_and_negative_zero(void) {
  ASSERT_TRUE(begin_builder(12u, 34u));
  const DbcCatalogIndexMessageInput message = {
    .normalized_id = 0x321u, .source_line = 1u,
    .declared_payload_length = 8u, .ide = false
  };
  ASSERT_STATUS(dbc_catalog_index_builder_begin_message(&g_builder, &message),
                DBC_CATALOG_INDEX_OK);
  static const char key[] =
    "M123456789012345678901234567890.S123456789012345678901234567890";
  static const char unit[] = "\xE6\xB8\xA9\xE5\xBA\xA6\xE2\x84\x83";
  ASSERT_TRUE(strlen(key) == LARGE_DBC_KEY_MAX_BYTES);
  DbcCatalogIndexSignalInput signal = signal_input(&message, key, 0u);
  signal.offset = -0.0;
  signal.minimum = -0.0;
  signal.unit = unit;
  signal.unit_length = (uint8_t)strlen(unit);
  const uint64_t negative_zero_hash =
    dbc_catalog_definition_hash_v1(&message, &signal);
  signal.offset = 0.0;
  signal.minimum = 0.0;
  ASSERT_TRUE(negative_zero_hash == dbc_catalog_definition_hash_v1(&message, &signal));
  signal.definition_hash = negative_zero_hash;
  ASSERT_STATUS(dbc_catalog_index_builder_add_signal(&g_builder, &signal),
                DBC_CATALOG_INDEX_OK);
  DbcCatalogIndexSummary summary;
  const DbcCatalogIndexIo output = memory_io(&g_index);
  ASSERT_STATUS(dbc_catalog_index_builder_finalize(&g_builder, &output, &summary),
                DBC_CATALOG_INDEX_OK);
  const DbcCatalogIndexIo io = memory_io(&g_index);
  ASSERT_STATUS(dbc_catalog_index_verify(&io, NULL, &g_verify_workspace, &summary),
                DBC_CATALOG_INDEX_OK);
  DbcCatalogIndexSignal decoded;
  ASSERT_STATUS(dbc_catalog_index_read_signal(&io, &summary, 0u, &decoded),
                DBC_CATALOG_INDEX_OK);
  ASSERT_TRUE(strlen(decoded.key) == LARGE_DBC_KEY_MAX_BYTES);
  ASSERT_TRUE(strcmp(decoded.unit, unit) == 0);
  return true;
}

static bool test_corruption_truncation_and_source(void) {
  DbcCatalogIndexSummary summary;
  ASSERT_TRUE(build_golden_index(&summary));
  const DbcCatalogIndexIo io = memory_io(&g_index);
  const DbcCatalogIndexExpectedSource wrong = {
    .enabled = true, .source_size = 100071u, .source_crc32 = 1u
  };
  ASSERT_STATUS(dbc_catalog_index_verify(&io, &wrong, &g_verify_workspace, &summary),
                DBC_CATALOG_INDEX_SOURCE_MISMATCH);
  const uint32_t original_size = g_index.size;
  --g_index.size;
  ASSERT_STATUS(dbc_catalog_index_verify(&io, NULL, &g_verify_workspace, &summary),
                DBC_CATALOG_INDEX_INVALID_FORMAT);
  g_index.size = original_size;
  g_index.data[100u] ^= 1u;
  ASSERT_STATUS(dbc_catalog_index_verify(&io, NULL, &g_verify_workspace, &summary),
                DBC_CATALOG_INDEX_CRC_MISMATCH);
  g_index.data[100u] ^= 1u;
  return true;
}

static bool test_header_count_and_offset_errors(void) {
  DbcCatalogIndexSummary summary;
  ASSERT_TRUE(build_golden_index(&summary));
  const DbcCatalogIndexIo io = memory_io(&g_index);
  const uint32_t count = test_get_u32(
    g_index.data + LARGE_DBC_INDEX_MESSAGE_COUNT_OFFSET);
  test_put_u32(g_index.data + LARGE_DBC_INDEX_MESSAGE_COUNT_OFFSET, 0u);
  repair_header_crc(&g_index);
  ASSERT_STATUS(dbc_catalog_index_verify(&io, NULL, &g_verify_workspace, &summary),
                DBC_CATALOG_INDEX_INVALID_FORMAT);
  test_put_u32(g_index.data + LARGE_DBC_INDEX_MESSAGE_COUNT_OFFSET, count);
  test_put_u32(g_index.data + LARGE_DBC_INDEX_SIGNAL_RECORDS_OFFSET_OFFSET, 97u);
  repair_header_crc(&g_index);
  ASSERT_STATUS(dbc_catalog_index_verify(&io, NULL, &g_verify_workspace, &summary),
                DBC_CATALOG_INDEX_INVALID_FORMAT);
  return true;
}

static void repair_signal_and_header_crc(void) {
  const uint32_t signal_offset = test_get_u32(
    g_index.data + LARGE_DBC_INDEX_SIGNAL_RECORDS_OFFSET_OFFSET);
  const uint32_t signal_size = test_get_u32(
    g_index.data + LARGE_DBC_INDEX_SIGNAL_RECORDS_SIZE_OFFSET);
  test_put_u32(g_index.data + LARGE_DBC_INDEX_SIGNAL_RECORDS_CRC32_OFFSET,
               crc32_bytes(g_index.data + signal_offset, signal_size));
  repair_header_crc(&g_index);
}

static bool expect_corrupt_signal_text_rejected(size_t relative_offset,
                                                uint8_t replacement) {
  DbcCatalogIndexSummary summary;
  ASSERT_TRUE(build_golden_index(&summary));
  const uint32_t signal_offset = test_get_u32(
    g_index.data + LARGE_DBC_INDEX_SIGNAL_RECORDS_OFFSET_OFFSET);
  g_index.data[signal_offset + relative_offset] = replacement;
  repair_signal_and_header_crc();
  const DbcCatalogIndexIo io = memory_io(&g_index);
  ASSERT_STATUS(dbc_catalog_index_verify(&io, NULL, &g_verify_workspace, &summary),
                DBC_CATALOG_INDEX_INVALID_RECORD);
  return true;
}

static bool test_verified_string_semantics(void) {
  ASSERT_TRUE(expect_corrupt_signal_text_rejected(
    LARGE_DBC_SIGNAL_RECORD_KEY_OFFSET, (uint8_t)'1'));
  ASSERT_TRUE(expect_corrupt_signal_text_rejected(
    LARGE_DBC_SIGNAL_RECORD_KEY_OFFSET + 3u, (uint8_t)'_'));
  ASSERT_TRUE(expect_corrupt_signal_text_rejected(
    LARGE_DBC_SIGNAL_RECORD_KEY_OFFSET + 5u, (uint8_t)'.'));
  ASSERT_TRUE(expect_corrupt_signal_text_rejected(
    LARGE_DBC_SIGNAL_RECORD_KEY_OFFSET + 5u, (uint8_t)'-'));
  ASSERT_TRUE(expect_corrupt_signal_text_rejected(
    LARGE_DBC_SIGNAL_RECORD_UNIT_OFFSET, UINT8_C(0xC2)));
  ASSERT_TRUE(expect_corrupt_signal_text_rejected(
    LARGE_DBC_SIGNAL_RECORD_UNIT_OFFSET, UINT8_C(0x1F)));
  return true;
}

static bool test_record_semantic_errors(void) {
  DbcCatalogIndexSummary summary;
  ASSERT_TRUE(build_golden_index(&summary));
  const DbcCatalogIndexIo io = memory_io(&g_index);
  const uint32_t signal_offset = test_get_u32(
    g_index.data + LARGE_DBC_INDEX_SIGNAL_RECORDS_OFFSET_OFFSET);
  g_index.data[signal_offset + LARGE_DBC_SIGNAL_RECORD_ORDINAL_OFFSET] = 1u;
  repair_signal_and_header_crc();
  ASSERT_STATUS(dbc_catalog_index_verify(&io, NULL, &g_verify_workspace, &summary),
                DBC_CATALOG_INDEX_INVALID_RECORD);

  ASSERT_TRUE(build_golden_index(&summary));
  const uint32_t message_offset = LARGE_DBC_INDEX_HEADER_SIZE;
  g_index.data[message_offset + LARGE_DBC_MESSAGE_RECORD_FIRST_SIGNAL_OFFSET] = 1u;
  test_put_u32(g_index.data + LARGE_DBC_INDEX_MESSAGE_RECORDS_CRC32_OFFSET,
               crc32_bytes(g_index.data + message_offset,
                           LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE));
  repair_header_crc(&g_index);
  ASSERT_STATUS(dbc_catalog_index_verify(&io, NULL, &g_verify_workspace, &summary),
                DBC_CATALOG_INDEX_INVALID_RECORD);

  ASSERT_TRUE(build_golden_index(&summary));
  const uint32_t fresh_signal_offset = test_get_u32(
    g_index.data + LARGE_DBC_INDEX_SIGNAL_RECORDS_OFFSET_OFFSET);
  g_index.data[fresh_signal_offset + LARGE_DBC_SIGNAL_RECORD_KEY_OFFSET + 10u] = 1u;
  repair_signal_and_header_crc();
  ASSERT_STATUS(dbc_catalog_index_verify(&io, NULL, &g_verify_workspace, &summary),
                DBC_CATALOG_INDEX_INVALID_RECORD);

  ASSERT_TRUE(build_golden_index(&summary));
  const uint32_t hash_offset = test_get_u32(
    g_index.data + LARGE_DBC_INDEX_SIGNAL_RECORDS_OFFSET_OFFSET) +
    LARGE_DBC_SIGNAL_RECORD_DEFINITION_HASH_OFFSET;
  g_index.data[hash_offset] ^= 1u;
  repair_signal_and_header_crc();
  ASSERT_STATUS(dbc_catalog_index_verify(&io, NULL, &g_verify_workspace, &summary),
                DBC_CATALOG_INDEX_INVALID_RECORD);
  return true;
}

static bool test_signal_2048_2049_boundary(void) {
  ASSERT_TRUE(begin_builder(1u, 2u));
  const DbcCatalogIndexMessageInput message = {
    .normalized_id = 1u, .source_line = 1u,
    .declared_payload_length = 8u, .ide = false
  };
  ASSERT_STATUS(dbc_catalog_index_builder_begin_message(&g_builder, &message),
                DBC_CATALOG_INDEX_OK);
  char key[32];
  for (unsigned i = 0u; i < LARGE_DBC_CATALOG_MAX_SIGNALS; ++i) {
    const int length = snprintf(key, sizeof(key), "M.S%04u", i);
    ASSERT_TRUE(length > 0 && (size_t)length < sizeof(key));
    DbcCatalogIndexSignalInput signal = signal_input(&message, key, (uint16_t)(i % 64u));
    ASSERT_STATUS(dbc_catalog_index_builder_add_signal(&g_builder, &signal),
                  DBC_CATALOG_INDEX_OK);
  }
  DbcCatalogIndexSignalInput overflow = signal_input(&message, "M.Overflow", 0u);
  ASSERT_STATUS(dbc_catalog_index_builder_add_signal(&g_builder, &overflow),
                DBC_CATALOG_INDEX_LIMIT_EXCEEDED);
  DbcCatalogIndexSummary summary;
  const DbcCatalogIndexIo output = memory_io(&g_index);
  ASSERT_STATUS(dbc_catalog_index_builder_finalize(&g_builder, &output, &summary),
                DBC_CATALOG_INDEX_OK);
  ASSERT_TRUE(summary.signal_count == 2048u && summary.message_count == 1u);
  const DbcCatalogIndexIo io = memory_io(&g_index);
  ASSERT_STATUS(dbc_catalog_index_verify(&io, NULL, &g_verify_workspace, &summary),
                DBC_CATALOG_INDEX_OK);
  return true;
}

static bool test_real_catalog_shape_offsets(void) {
  ASSERT_TRUE(begin_builder(100071u, UINT32_C(0x4B88D9CE)));
  char key[32];
  for (unsigned message_ordinal = 0u; message_ordinal < 112u; ++message_ordinal) {
    const DbcCatalogIndexMessageInput message = {
      .normalized_id = 256u + message_ordinal,
      .source_line = 2u + message_ordinal * 9u,
      .declared_payload_length = 8u,
      .ide = false
    };
    ASSERT_STATUS(dbc_catalog_index_builder_begin_message(&g_builder, &message),
                  DBC_CATALOG_INDEX_OK);
    for (unsigned in_message = 0u; in_message < 8u; ++in_message) {
      const int length = snprintf(key,
                                  sizeof(key),
                                  "M%03u.S%u",
                                  message_ordinal,
                                  in_message);
      ASSERT_TRUE(length > 0 && (size_t)length < sizeof(key));
      DbcCatalogIndexSignalInput signal =
        signal_input(&message, key, (uint16_t)(in_message * 8u));
      signal.bit_length = 8u;
      signal.source_line = message.source_line + 1u + in_message;
      signal.definition_hash = dbc_catalog_definition_hash_v1(&message, &signal);
      ASSERT_STATUS(dbc_catalog_index_builder_add_signal(&g_builder, &signal),
                    DBC_CATALOG_INDEX_OK);
    }
  }
  DbcCatalogIndexSummary summary;
  const DbcCatalogIndexIo output = memory_io(&g_index);
  ASSERT_STATUS(dbc_catalog_index_builder_finalize(&g_builder, &output, &summary),
                DBC_CATALOG_INDEX_OK);
  ASSERT_TRUE(summary.message_count == 112u && summary.signal_count == 896u);
  ASSERT_TRUE(test_get_u32(g_index.data +
                           LARGE_DBC_INDEX_MESSAGE_RECORDS_OFFSET_OFFSET) == 80u);
  ASSERT_TRUE(test_get_u32(g_index.data +
                           LARGE_DBC_INDEX_MESSAGE_RECORDS_SIZE_OFFSET) == 1792u);
  ASSERT_TRUE(test_get_u32(g_index.data +
                           LARGE_DBC_INDEX_SIGNAL_RECORDS_OFFSET_OFFSET) == 1872u);
  ASSERT_TRUE(test_get_u32(g_index.data +
                           LARGE_DBC_INDEX_SIGNAL_RECORDS_SIZE_OFFSET) == 143360u);
  ASSERT_TRUE(summary.total_size == 145232u);
  const DbcCatalogIndexIo io = memory_io(&g_index);
  ASSERT_STATUS(dbc_catalog_index_verify(&io, NULL, &g_verify_workspace, &summary),
                DBC_CATALOG_INDEX_OK);
  return true;
}

static bool test_message_2048_2049_boundary(void) {
  ASSERT_TRUE(begin_builder(1u, 2u));
  DbcCatalogIndexMessageInput message = {
    .source_line = 1u, .declared_payload_length = 8u, .ide = true
  };
  for (unsigned i = 0u; i < LARGE_DBC_CATALOG_MAX_MESSAGES; ++i) {
    message.normalized_id = i;
    message.source_line = i + 1u;
    ASSERT_STATUS(dbc_catalog_index_builder_begin_message(&g_builder, &message),
                  DBC_CATALOG_INDEX_OK);
  }
  message.normalized_id = LARGE_DBC_CATALOG_MAX_MESSAGES;
  ASSERT_STATUS(dbc_catalog_index_builder_begin_message(&g_builder, &message),
                DBC_CATALOG_INDEX_LIMIT_EXCEEDED);
  DbcCatalogIndexSummary summary;
  const DbcCatalogIndexIo output = memory_io(&g_index);
  ASSERT_STATUS(dbc_catalog_index_builder_finalize(&g_builder, &output, &summary),
                DBC_CATALOG_INDEX_OK);
  ASSERT_TRUE(summary.message_count == 2048u && summary.signal_count == 0u);
  const DbcCatalogIndexIo io = memory_io(&g_index);
  ASSERT_STATUS(dbc_catalog_index_verify(&io, NULL, &g_verify_workspace, &summary),
                DBC_CATALOG_INDEX_OK);
  return true;
}

int main(int argc, char **argv) {
  ASSERT_TRUE(argc == 1 || argc == 2);
  ASSERT_TRUE(test_crc_and_real_source(argc == 2 ? argv[1] : NULL));
  ASSERT_TRUE(test_golden_verify_read_dump());
  ASSERT_TRUE(test_duplicate_rejection());
  ASSERT_TRUE(test_string_semantics());
  ASSERT_TRUE(test_max_key_utf8_unit_and_negative_zero());
  ASSERT_TRUE(test_corruption_truncation_and_source());
  ASSERT_TRUE(test_header_count_and_offset_errors());
  ASSERT_TRUE(test_record_semantic_errors());
  ASSERT_TRUE(test_verified_string_semantics());
  ASSERT_TRUE(test_real_catalog_shape_offsets());
  ASSERT_TRUE(test_signal_2048_2049_boundary());
  ASSERT_TRUE(test_message_2048_2049_boundary());
  puts("DBC catalog index v1 tests passed");
  return 0;
}
