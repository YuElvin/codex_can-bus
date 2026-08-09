#include "dbc_catalog_index.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  FILE *file;
} FileIo;

static bool file_read_at(void *context,
                         uint32_t offset,
                         uint8_t *data,
                         uint32_t size) {
  FileIo *io = context;
  return fseek(io->file, (long)offset, SEEK_SET) == 0 &&
         fread(data, 1u, size, io->file) == size;
}

static bool file_get_size(void *context, uint32_t *size) {
  FileIo *io = context;
  if (fseek(io->file, 0L, SEEK_END) != 0) {
    return false;
  }
  const long length = ftell(io->file);
  if (length < 0 || (unsigned long)length > UINT32_MAX) {
    return false;
  }
  *size = (uint32_t)length;
  return true;
}

static bool stdout_write(void *context, const char *text, size_t size) {
  (void)context;
  return fwrite(text, 1u, size, stdout) == size;
}

static bool parse_u32(const char *text, int base, uint32_t *value) {
  char *end = NULL;
  errno = 0;
  const unsigned long parsed = strtoul(text, &end, base);
  if (errno != 0 || end == text || *end != '\0' || parsed > UINT32_MAX) {
    return false;
  }
  *value = (uint32_t)parsed;
  return true;
}

static void usage(const char *program) {
  fprintf(stderr,
          "usage: %s verify|dump INDEX [SOURCE_SIZE SOURCE_CRC32_HEX]\n",
          program);
}

int main(int argc, char **argv) {
  if ((argc != 3 && argc != 5) ||
      (strcmp(argv[1], "verify") != 0 && strcmp(argv[1], "dump") != 0)) {
    usage(argv[0]);
    return 2;
  }
  DbcCatalogIndexExpectedSource expected = {0};
  if (argc == 5) {
    expected.enabled = true;
    if (!parse_u32(argv[3], 10, &expected.source_size) ||
        !parse_u32(argv[4], 16, &expected.source_crc32)) {
      usage(argv[0]);
      return 2;
    }
  }
  FILE *file = fopen(argv[2], "rb");
  if (file == NULL) {
    fprintf(stderr, "%s: %s\n", argv[2], strerror(errno));
    return 2;
  }
  FileIo file_context = {.file = file};
  DbcCatalogIndexIo io = {
    .context = &file_context,
    .read_at = file_read_at,
    .get_size = file_get_size
  };
  static DbcCatalogIndexVerifyWorkspace workspace;
  DbcCatalogIndexSummary summary;
  DbcCatalogIndexStatus status =
    dbc_catalog_index_verify(&io, &expected, &workspace, &summary);
  if (status == DBC_CATALOG_INDEX_OK && strcmp(argv[1], "dump") == 0) {
    status = dbc_catalog_index_dump(&io, &summary, stdout_write, NULL);
  }
  fclose(file);
  if (status != DBC_CATALOG_INDEX_OK) {
    fprintf(stderr, "index %s failed: %s\n",
            argv[1], dbc_catalog_index_status_string(status));
    return 1;
  }
  if (strcmp(argv[1], "verify") == 0) {
    printf("ok source_size=%lu source_crc32=%08lX messages=%u signals=%u total=%lu\n",
           (unsigned long)summary.source_size,
           (unsigned long)summary.source_crc32,
           (unsigned)summary.message_count,
           (unsigned)summary.signal_count,
           (unsigned long)summary.total_size);
  }
  return 0;
}
