#define _POSIX_C_SOURCE 200809L

#include "dbc_catalog_index.h"
#include "dbc_stream_index.h"
#include "dbc_stream_parser.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static DbcCatalogIndexBuilder g_builder;
static DbcStreamIndexAdapter g_adapter;
static DbcStreamParser g_parser;
static DbcCatalogIndexVerifyWorkspace g_verify_workspace;

static bool file_read_at(void *context, uint32_t offset,
                         uint8_t *data, uint32_t size) {
  FILE *file = context;
  return fseek(file, (long)offset, SEEK_SET) == 0 &&
         fread(data, 1u, size, file) == size;
}

static bool file_write_at(void *context, uint32_t offset,
                          const uint8_t *data, uint32_t size) {
  FILE *file = context;
  return fseek(file, (long)offset, SEEK_SET) == 0 &&
         fwrite(data, 1u, size, file) == size;
}

static bool file_resize(void *context, uint32_t size) {
  FILE *file = context;
  return fflush(file) == 0 && ftruncate(fileno(file), (off_t)size) == 0;
}

static bool file_get_size(void *context, uint32_t *size) {
  FILE *file = context;
  if (fflush(file) != 0 || fseek(file, 0L, SEEK_END) != 0) {
    return false;
  }
  const long end = ftell(file);
  if (end < 0L || (unsigned long)end > UINT32_MAX) {
    return false;
  }
  *size = (uint32_t)end;
  return true;
}

static DbcCatalogIndexIo file_io(FILE *file) {
  const DbcCatalogIndexIo io = {
    .context = file,
    .read_at = file_read_at,
    .write_at = file_write_at,
    .resize = file_resize,
    .get_size = file_get_size
  };
  return io;
}

static bool source_identity(FILE *source, uint32_t *source_size,
                            uint32_t *source_crc32) {
  uint8_t chunk[512];
  uint32_t size = 0u;
  uint32_t crc = dbc_catalog_crc32_begin();
  size_t count;
  while ((count = fread(chunk, 1u, sizeof(chunk), source)) != 0u) {
    if (size > LARGE_DBC_SOURCE_MAX_BYTES - count) {
      return false;
    }
    size += (uint32_t)count;
    crc = dbc_catalog_crc32_update(crc, chunk, count);
  }
  if (ferror(source) != 0 || size == 0u || fseek(source, 0L, SEEK_SET) != 0) {
    return false;
  }
  *source_size = size;
  *source_crc32 = dbc_catalog_crc32_finish(crc);
  return true;
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s SOURCE.dbc OUTPUT.index\n", argv[0]);
    return 2;
  }
  FILE *source = fopen(argv[1], "rb");
  FILE *message_spool = tmpfile();
  FILE *signal_spool = tmpfile();
  FILE *output = fopen(argv[2], "w+b");
  if (source == NULL || message_spool == NULL || signal_spool == NULL ||
      output == NULL) {
    fprintf(stderr, "open failed: %s\n", strerror(errno));
    goto fail;
  }

  uint32_t source_size = 0u;
  uint32_t source_crc32 = 0u;
  if (!source_identity(source, &source_size, &source_crc32)) {
    fprintf(stderr, "invalid source or source exceeds %u bytes\n",
            LARGE_DBC_SOURCE_MAX_BYTES);
    goto fail;
  }

  const DbcCatalogIndexIo message_io = file_io(message_spool);
  const DbcCatalogIndexIo signal_io = file_io(signal_spool);
  const DbcCatalogIndexIo output_io = file_io(output);
  DbcCatalogIndexStatus status = dbc_catalog_index_builder_init(
    &g_builder, &message_io, &signal_io, source_size, source_crc32);
  if (status != DBC_CATALOG_INDEX_OK) {
    fprintf(stderr, "builder init failed: %s\n",
            dbc_catalog_index_status_string(status));
    goto fail;
  }
  dbc_stream_index_adapter_init(&g_adapter, &g_builder);
  const DbcStreamParserCallbacks callbacks =
    dbc_stream_index_adapter_callbacks();
  dbc_stream_parser_init(&g_parser, &callbacks, &g_adapter);

  uint8_t chunk[257];
  size_t count;
  while ((count = fread(chunk, 1u, sizeof(chunk), source)) != 0u) {
    if (!dbc_stream_parser_feed(&g_parser, chunk, count)) {
      break;
    }
  }
  if (ferror(source) != 0 || !dbc_stream_parser_finalize(&g_parser) ||
      !dbc_stream_index_adapter_complete(&g_adapter, &g_parser)) {
    fprintf(stderr, "parse/index failed: parser=%s line=%u offset=%u index=%s\n",
            dbc_stream_parser_error_name(g_parser.error), g_parser.error_line,
            g_parser.error_offset,
            dbc_catalog_index_status_string(g_adapter.status));
    goto fail;
  }

  DbcCatalogIndexSummary built;
  status = dbc_catalog_index_builder_finalize(&g_builder, &output_io, &built);
  if (status != DBC_CATALOG_INDEX_OK || fflush(output) != 0) {
    fprintf(stderr, "index finalize failed: %s\n",
            dbc_catalog_index_status_string(status));
    goto fail;
  }
  const DbcCatalogIndexExpectedSource expected = {
    .enabled = true,
    .source_size = source_size,
    .source_crc32 = source_crc32
  };
  DbcCatalogIndexSummary verified;
  status = dbc_catalog_index_verify(&output_io, &expected,
                                    &g_verify_workspace, &verified);
  if (status != DBC_CATALOG_INDEX_OK ||
      memcmp(&built, &verified, sizeof(built)) != 0) {
    fprintf(stderr, "index verify failed: %s\n",
            dbc_catalog_index_status_string(status));
    goto fail;
  }

  printf("source_size=%u source_crc32=%08X messages=%u signals=%u index_size=%u\n",
         verified.source_size, verified.source_crc32, verified.message_count,
         verified.signal_count, verified.total_size);
  fclose(output);
  fclose(signal_spool);
  fclose(message_spool);
  fclose(source);
  return 0;

fail:
  if (output != NULL) {
    fclose(output);
  }
  if (signal_spool != NULL) {
    fclose(signal_spool);
  }
  if (message_spool != NULL) {
    fclose(message_spool);
  }
  if (source != NULL) {
    fclose(source);
  }
  return 1;
}
