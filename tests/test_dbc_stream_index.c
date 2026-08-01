#include "dbc_catalog_index.h"
#include "dbc_stream_index.h"
#include "dbc_stream_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INDEX_MAX_BYTES \
  (LARGE_DBC_INDEX_HEADER_SIZE + \
   LARGE_DBC_CATALOG_MAX_MESSAGES * LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE + \
   LARGE_DBC_CATALOG_MAX_SIGNALS * LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE)

#define ASSERT_TRUE(condition) do { if (!(condition)) { \
  fprintf(stderr, "ASSERT failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
  return false; } } while (0)

typedef struct {
  uint8_t *data;
  uint32_t capacity;
  uint32_t size;
} MemoryIo;

typedef struct {
  MemoryIo message_spool;
  MemoryIo signal_spool;
  MemoryIo output;
  DbcCatalogIndexSummary summary;
} BuiltIndex;

static DbcCatalogIndexBuilder g_builder;
static DbcStreamIndexAdapter g_adapter;
static DbcStreamParser g_parser;
static DbcCatalogIndexVerifyWorkspace g_verify_workspace;

static bool memory_read_at(void *context, uint32_t offset,
                           uint8_t *data, uint32_t size) {
  const MemoryIo *memory = context;
  if (offset > memory->size || size > memory->size - offset) {
    return false;
  }
  memcpy(data, memory->data + offset, size);
  return true;
}

static bool memory_write_at(void *context, uint32_t offset,
                            const uint8_t *data, uint32_t size) {
  MemoryIo *memory = context;
  if (offset > memory->capacity || size > memory->capacity - offset) {
    return false;
  }
  memcpy(memory->data + offset, data, size);
  if (offset + size > memory->size) {
    memory->size = offset + size;
  }
  return true;
}

static bool memory_resize(void *context, uint32_t size) {
  MemoryIo *memory = context;
  if (size > memory->capacity) {
    return false;
  }
  if (size > memory->size) {
    memset(memory->data + memory->size, 0, size - memory->size);
  }
  memory->size = size;
  return true;
}

static bool memory_get_size(void *context, uint32_t *size) {
  const MemoryIo *memory = context;
  *size = memory->size;
  return true;
}

static DbcCatalogIndexIo memory_io(MemoryIo *memory) {
  const DbcCatalogIndexIo io = {
    .context = memory,
    .read_at = memory_read_at,
    .write_at = memory_write_at,
    .resize = memory_resize,
    .get_size = memory_get_size
  };
  return io;
}

static bool allocate_index(BuiltIndex *built) {
  memset(built, 0, sizeof(*built));
  built->message_spool.capacity =
    LARGE_DBC_CATALOG_MAX_MESSAGES * LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE;
  built->signal_spool.capacity =
    LARGE_DBC_CATALOG_MAX_SIGNALS * LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE;
  built->output.capacity = INDEX_MAX_BYTES;
  built->message_spool.data = malloc(built->message_spool.capacity);
  built->signal_spool.data = malloc(built->signal_spool.capacity);
  built->output.data = malloc(built->output.capacity);
  return built->message_spool.data != NULL && built->signal_spool.data != NULL &&
         built->output.data != NULL;
}

static void release_index(BuiltIndex *built) {
  free(built->output.data);
  free(built->signal_spool.data);
  free(built->message_spool.data);
  memset(built, 0, sizeof(*built));
}

static bool build_file_twice(const char *path, size_t parser_chunk_bytes,
                             BuiltIndex *built) {
  FILE *source = fopen(path, "rb");
  uint8_t chunk[512];
  uint32_t source_size = 0u;
  uint32_t crc = dbc_catalog_crc32_begin();
  size_t count;
  ASSERT_TRUE(parser_chunk_bytes >= 1u && parser_chunk_bytes <= sizeof(chunk));
  ASSERT_TRUE(source != NULL);
  while ((count = fread(chunk, 1u, sizeof(chunk), source)) != 0u) {
    ASSERT_TRUE(source_size <= LARGE_DBC_SOURCE_MAX_BYTES - count);
    source_size += (uint32_t)count;
    crc = dbc_catalog_crc32_update(crc, chunk, count);
  }
  ASSERT_TRUE(ferror(source) == 0 && source_size != 0u);
  ASSERT_TRUE(fseek(source, 0L, SEEK_SET) == 0);
  const uint32_t source_crc32 = dbc_catalog_crc32_finish(crc);

  ASSERT_TRUE(allocate_index(built));
  const DbcCatalogIndexIo message_io = memory_io(&built->message_spool);
  const DbcCatalogIndexIo signal_io = memory_io(&built->signal_spool);
  const DbcCatalogIndexIo output_io = memory_io(&built->output);
  ASSERT_TRUE(dbc_catalog_index_builder_init(&g_builder, &message_io, &signal_io,
                                             source_size, source_crc32) ==
              DBC_CATALOG_INDEX_OK);
  dbc_stream_index_adapter_init(&g_adapter, &g_builder);
  const DbcStreamParserCallbacks callbacks =
    dbc_stream_index_adapter_callbacks();
  dbc_stream_parser_init(&g_parser, &callbacks, &g_adapter);

  while ((count = fread(chunk, 1u, parser_chunk_bytes, source)) != 0u) {
    ASSERT_TRUE(dbc_stream_parser_feed(&g_parser, chunk, count));
  }
  ASSERT_TRUE(ferror(source) == 0);
  ASSERT_TRUE(dbc_stream_parser_finalize(&g_parser));
  ASSERT_TRUE(dbc_stream_index_adapter_complete(&g_adapter, &g_parser));
  ASSERT_TRUE(fclose(source) == 0);

  ASSERT_TRUE(dbc_catalog_index_builder_finalize(&g_builder, &output_io,
                                                  &built->summary) ==
              DBC_CATALOG_INDEX_OK);
  const DbcCatalogIndexExpectedSource expected = {
    .enabled = true,
    .source_size = source_size,
    .source_crc32 = source_crc32
  };
  DbcCatalogIndexSummary verified;
  ASSERT_TRUE(dbc_catalog_index_verify(&output_io, &expected,
                                        &g_verify_workspace, &verified) ==
              DBC_CATALOG_INDEX_OK);
  ASSERT_TRUE(memcmp(&built->summary, &verified, sizeof(verified)) == 0);
  return true;
}

static uint32_t test_get_u32(const uint8_t *data) {
  return (uint32_t)data[0] | ((uint32_t)data[1] << 8u) |
         ((uint32_t)data[2] << 16u) | ((uint32_t)data[3] << 24u);
}

typedef struct {
  char first_line[256];
  bool captured;
} DumpCapture;

static bool capture_first_dump_line(void *context, const char *text, size_t size) {
  DumpCapture *capture = context;
  if (!capture->captured) {
    const size_t copy = size < sizeof(capture->first_line) - 1u ?
      size : sizeof(capture->first_line) - 1u;
    memcpy(capture->first_line, text, copy);
    capture->first_line[copy] = '\0';
    capture->captured = true;
  }
  return true;
}

static bool test_real_dbc(const char *path) {
  BuiltIndex one_byte;
  BuiltIndex one_thirty_seven;
  BuiltIndex built;
  ASSERT_TRUE(build_file_twice(path, 1u, &one_byte));
  ASSERT_TRUE(build_file_twice(path, 137u, &one_thirty_seven));
  ASSERT_TRUE(build_file_twice(path, 512u, &built));
  ASSERT_TRUE(one_byte.output.size == built.output.size &&
              one_thirty_seven.output.size == built.output.size);
  ASSERT_TRUE(memcmp(one_byte.output.data, built.output.data,
                     built.output.size) == 0);
  ASSERT_TRUE(memcmp(one_thirty_seven.output.data, built.output.data,
                     built.output.size) == 0);
  release_index(&one_thirty_seven);
  release_index(&one_byte);
  ASSERT_TRUE(built.summary.source_size == 100071u);
  ASSERT_TRUE(built.summary.source_crc32 == UINT32_C(0x4B88D9CE));
  ASSERT_TRUE(built.summary.message_count == 112u);
  ASSERT_TRUE(built.summary.signal_count == 896u);
  ASSERT_TRUE(built.summary.total_size == 145232u);
  ASSERT_TRUE(test_get_u32(built.output.data +
                           LARGE_DBC_INDEX_MESSAGE_RECORDS_OFFSET_OFFSET) == 80u);
  ASSERT_TRUE(test_get_u32(built.output.data +
                           LARGE_DBC_INDEX_MESSAGE_RECORDS_SIZE_OFFSET) == 1792u);
  ASSERT_TRUE(test_get_u32(built.output.data +
                           LARGE_DBC_INDEX_SIGNAL_RECORDS_OFFSET_OFFSET) == 1872u);
  ASSERT_TRUE(test_get_u32(built.output.data +
                           LARGE_DBC_INDEX_SIGNAL_RECORDS_SIZE_OFFSET) == 143360u);

  const DbcCatalogIndexIo output_io = memory_io(&built.output);
  DbcCatalogIndexMessage first_message;
  DbcCatalogIndexMessage last_message;
  DbcCatalogIndexSignal first_signal;
  DbcCatalogIndexSignal last_signal;
  ASSERT_TRUE(dbc_catalog_index_read_message(&output_io, &built.summary, 0u,
                                              &first_message) ==
              DBC_CATALOG_INDEX_OK);
  ASSERT_TRUE(dbc_catalog_index_read_message(&output_io, &built.summary, 111u,
                                              &last_message) ==
              DBC_CATALOG_INDEX_OK);
  ASSERT_TRUE(first_message.ordinal == 0u && first_message.normalized_id == 256u);
  ASSERT_TRUE(first_message.source_line == 40u &&
              first_message.first_signal_ordinal == 0u &&
              first_message.signal_count == 8u && first_message.flags == 0u &&
              first_message.declared_payload_length == 8u);
  ASSERT_TRUE(last_message.ordinal == 111u && last_message.normalized_id == 367u);
  ASSERT_TRUE(last_message.source_line == 1594u &&
              last_message.first_signal_ordinal == 888u &&
              last_message.signal_count == 8u && last_message.flags == 0u &&
              last_message.declared_payload_length == 8u);

  ASSERT_TRUE(dbc_catalog_index_read_signal(&output_io, &built.summary, 0u,
                                             &first_signal) ==
              DBC_CATALOG_INDEX_OK);
  ASSERT_TRUE(dbc_catalog_index_read_signal(&output_io, &built.summary, 895u,
                                             &last_signal) ==
              DBC_CATALOG_INDEX_OK);
  ASSERT_TRUE(first_signal.ordinal == 0u && first_signal.message_ordinal == 0u &&
              first_signal.normalized_id == 256u && first_signal.flags == 0u &&
              first_signal.start_bit == 0u && first_signal.bit_length == 16u &&
              first_signal.declared_payload_length == 8u &&
              first_signal.source_line == 41u && first_signal.source_offset == 668u);
  ASSERT_TRUE(strcmp(first_signal.key,
                     "ClassicCanTest_BattMod_001.PackVoltage_Module001") == 0);
  ASSERT_TRUE(strcmp(first_signal.unit, "V") == 0);
  ASSERT_TRUE(last_signal.ordinal == 895u && last_signal.message_ordinal == 111u &&
              last_signal.normalized_id == 367u && last_signal.flags == 0u &&
              last_signal.start_bit == 62u && last_signal.bit_length == 2u &&
              last_signal.declared_payload_length == 8u &&
              last_signal.source_line == 1602u && last_signal.source_offset == 99739u);
  ASSERT_TRUE(strcmp(last_signal.key,
                     "ClassicCanTest_BattMod_112.Reserved_Module112") == 0);
  ASSERT_TRUE(strcmp(last_signal.unit, "") == 0);
  DumpCapture dump = {{0}, false};
  ASSERT_TRUE(dbc_catalog_index_dump(&output_io, &built.summary,
                                      capture_first_dump_line, &dump) ==
              DBC_CATALOG_INDEX_OK);
  ASSERT_TRUE(strstr(dump.first_line,
                     "INDEX v1 source_size=100071 source_crc32=4B88D9CE messages=112 signals=896 total=145232") != NULL);
  release_index(&built);
  return true;
}

static bool test_legacy_dbc(const char *path) {
  BuiltIndex built;
  ASSERT_TRUE(build_file_twice(path, 137u, &built));
  ASSERT_TRUE(built.summary.source_size == 151u);
  ASSERT_TRUE(built.summary.source_crc32 == UINT32_C(0xF2852B8E));
  ASSERT_TRUE(built.summary.message_count == 1u);
  ASSERT_TRUE(built.summary.signal_count == 2u);
  ASSERT_TRUE(built.summary.total_size == 416u);

  const DbcCatalogIndexIo output_io = memory_io(&built.output);
  DbcCatalogIndexMessage message;
  DbcCatalogIndexSignal marker;
  DbcCatalogIndexSignal sequence;
  ASSERT_TRUE(dbc_catalog_index_read_message(&output_io, &built.summary, 0u,
                                              &message) == DBC_CATALOG_INDEX_OK);
  ASSERT_TRUE(message.normalized_id == 801u && message.source_line == 1u &&
              message.first_signal_ordinal == 0u && message.signal_count == 2u &&
              message.flags == 0u && message.declared_payload_length == 8u);
  ASSERT_TRUE(dbc_catalog_index_read_signal(&output_io, &built.summary, 0u,
                                             &marker) == DBC_CATALOG_INDEX_OK);
  ASSERT_TRUE(dbc_catalog_index_read_signal(&output_io, &built.summary, 1u,
                                             &sequence) == DBC_CATALOG_INDEX_OK);
  ASSERT_TRUE(marker.ordinal == 0u && marker.message_ordinal == 0u &&
              marker.source_line == 2u && marker.source_offset == 32u &&
              strcmp(marker.key, "Can2Data.marker") == 0 &&
              marker.definition_hash == UINT64_C(0x500B127C931DC023));
  ASSERT_TRUE(sequence.ordinal == 1u && sequence.message_ordinal == 0u &&
              sequence.source_line == 3u && sequence.source_offset == 90u &&
              strcmp(sequence.key, "Can2Data.sequence") == 0 &&
              sequence.definition_hash == UINT64_C(0x85FDB427A917D133));
  release_index(&built);
  return true;
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s REAL.dbc LEGACY.dbc\n", argv[0]);
    return 2;
  }
  if (!test_real_dbc(argv[1]) || !test_legacy_dbc(argv[2])) {
    return 1;
  }
  puts("dbc stream->index real and legacy tests passed");
  return 0;
}
