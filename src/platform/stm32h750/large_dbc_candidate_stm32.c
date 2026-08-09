#include "platform/large_dbc_candidate_stm32.h"

#include "dbc_active_commit.h"
#include "dbc_candidate_catalog.h"
#include "dbc_candidate_format.h"
#include "dbc_catalog_index.h"
#include "dbc_stream_index.h"
#include "dbc_stream_parser.h"
#include "ff.h"

#include <stdio.h>
#include <string.h>

extern char SDPath[4];
extern int stm32h750_tf_fs_lock(void);
extern void stm32h750_tf_fs_unlock(void);

#define CANDIDATE_PATH_BYTES 72u
#define MESSAGE_SPOOL_BASE 0u
#define MESSAGE_SPOOL_CAPACITY \
  (LARGE_DBC_CATALOG_MAX_MESSAGES * LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE)
#define SIGNAL_SPOOL_BASE MESSAGE_SPOOL_CAPACITY
#define SIGNAL_SPOOL_CAPACITY \
  (LARGE_DBC_CATALOG_MAX_SIGNALS * LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE)
#define CANDIDATE_FILE_IO_BUFFER_BYTES LARGE_DBC_UPLOAD_CHUNK_BYTES

typedef struct {
  char upload_tmp[CANDIDATE_PATH_BYTES];
  char source_tmp[CANDIDATE_PATH_BYTES];
  char spool_tmp[CANDIDATE_PATH_BYTES];
  char index_tmp[CANDIDATE_PATH_BYTES];
  char selection_tmp[CANDIDATE_PATH_BYTES];
  char source_final[CANDIDATE_PATH_BYTES];
  char index_final[CANDIDATE_PATH_BYTES];
  char selection_final[CANDIDATE_PATH_BYTES];
  char current_tmp[CANDIDATE_PATH_BYTES];
  char current[CANDIDATE_PATH_BYTES];
  char previous[CANDIDATE_PATH_BYTES];
} CandidatePaths;

typedef struct {
  FIL *file;
  uint32_t base;
  uint32_t capacity;
  uint32_t logical_size;
  bool physical_resize;
  uint8_t *read_cache;
  uint32_t read_cache_capacity;
  uint32_t read_cache_offset;
  uint32_t read_cache_size;
} CandidateIndexIoContext;

typedef struct {
  bool valid;
  bool uses_previous;
  DbcManifestV1 manifest;
  uint8_t manifest_bytes[LARGE_DBC_MANIFEST_SIZE];
} CandidateQueryCache;

typedef struct {
  LargeDbcCandidateProgressCallback callback;
  void *context;
} CandidateProgress;

static CandidatePaths g_paths;
static CandidatePaths g_temp_paths;
static CandidatePaths g_active_paths;
static CandidatePaths g_active_candidate_paths;
static CandidateProgress g_progress;
static FIL g_source_file;
static FIL g_spool_file;
static FIL g_index_file;
static FIL g_aux_file;
static bool g_source_open;
static bool g_spool_open;
static bool g_index_open;
static bool g_aux_open;
static bool g_source_renamed;
static bool g_index_renamed;
static bool g_selection_renamed;
static bool g_current_manifest_published;
static bool g_candidate_previous_rotated;
static bool g_source_tmp_owned;
static bool g_index_tmp_owned;
static bool g_selection_tmp_owned;
static bool g_current_tmp_owned;
static DbcCatalogIndexBuilder g_builder;
static DbcStreamIndexAdapter g_stream_adapter;
static DbcStreamParser g_parser;
static CandidateIndexIoContext g_message_io_context;
static CandidateIndexIoContext g_signal_io_context;
static CandidateIndexIoContext g_index_io_context;
static uint8_t g_io_buffer[CANDIDATE_FILE_IO_BUFFER_BYTES]
  __attribute__((aligned(32)));
static uint8_t g_index_read_cache[CANDIDATE_FILE_IO_BUFFER_BYTES]
  __attribute__((aligned(32)));
static uint8_t g_selection_bytes[LARGE_DBC_SELECTION_TOTAL_SIZE];
static uint8_t g_manifest_bytes[LARGE_DBC_MANIFEST_SIZE];
static uint8_t g_current_manifest_bytes[LARGE_DBC_MANIFEST_SIZE];
static uint8_t g_previous_manifest_bytes[LARGE_DBC_MANIFEST_SIZE];
static DbcSelectionV1 g_selection;
static DbcCandidateCatalogWorkspace g_catalog_workspace;
static DbcCandidateSelectionUpdate g_selection_update;
static LargeDbcCandidateCatalogResult g_query_result;
static DbcManifestV1 g_work_manifest;
static DbcManifestV1 g_staged_manifest;
static DbcManifestV1 g_formal_manifest;
static DbcManifestReferenceFacts g_work_facts;
static DbcManifestReferenceFacts g_staged_facts;
static DbcManifestReferenceFacts g_formal_facts;
static DbcCatalogIndexSummary g_catalog_summary;
static DbcCatalogIndexIo g_catalog_index_io;
static FILINFO g_file_info;
static DbcManifestV1 g_active_candidate_manifest;
static DbcManifestV1 g_active_manifest;
static DbcActiveDescriptor g_active_descriptor;
static CandidateQueryCache g_candidate_query_cache;
static bool g_active_source_owned;
static bool g_active_index_owned;
static bool g_active_selection_owned;
static bool g_active_current_tmp_owned;
static bool g_active_source_renamed;
static bool g_active_index_renamed;
static bool g_active_selection_renamed;
static bool g_active_manifest_published;
static bool g_active_previous_rotated;
static DbcSelectedRuntimeStatus g_active_runtime_status;

#if defined(CAN_BUS_LARGE_DBC_H1_FAULT_INJECTION)
volatile uint32_t g_large_dbc_h1_fault_once
  __attribute__((used, externally_visible));
volatile uint32_t g_large_dbc_h1_fault_fire_count
  __attribute__((used, externally_visible));
volatile uint32_t g_large_dbc_h1_fault_last_point
  __attribute__((used, externally_visible));
volatile uint32_t g_large_dbc_h1_fault_last_operation
  __attribute__((used, externally_visible));
volatile uint32_t g_large_dbc_h1_fault_last_result
  __attribute__((used, externally_visible));

__attribute__((used, externally_visible, noinline, noclone))
bool large_dbc_h1_fault_consume(uint32_t point, uint32_t operation,
                                 uint32_t result) {
  if (g_large_dbc_h1_fault_once != point) {
    return false;
  }

  g_large_dbc_h1_fault_once = 0u;
  ++g_large_dbc_h1_fault_fire_count;
  g_large_dbc_h1_fault_last_point = point;
  g_large_dbc_h1_fault_last_operation = operation;
  g_large_dbc_h1_fault_last_result = result;
  return true;
}
#endif

_Static_assert(sizeof(g_io_buffer) == CANDIDATE_FILE_IO_BUFFER_BYTES,
               "candidate file I/O buffer size changed");
_Static_assert(sizeof(g_index_read_cache) == CANDIDATE_FILE_IO_BUFFER_BYTES,
               "candidate index read cache size changed");
_Static_assert(sizeof(DbcStreamParser) > LARGE_DBC_MAX_AUTOMATIC_OBJECT_BYTES,
               "parser is intentionally static");
_Static_assert(sizeof(DbcCatalogIndexBuilder) > LARGE_DBC_MAX_AUTOMATIC_OBJECT_BYTES,
               "builder is intentionally static");

static void report_io(LargeDbcCandidateIoOperation operation,
                      uint32_t bytes,
                      FRESULT result) {
  if (g_progress.callback != NULL) {
    g_progress.callback(g_progress.context, operation, bytes, (int)result);
  }
}

static FRESULT io_open(FIL *file, const char *path, BYTE mode) {
  const FRESULT result = f_open(file, path, mode);
  report_io(LARGE_DBC_CANDIDATE_IO_OPEN, 0u, result);
  return result;
}

static FRESULT io_close(FIL *file) {
  const FRESULT result = f_close(file);
  report_io(LARGE_DBC_CANDIDATE_IO_CLOSE, 0u, result);
  return result;
}

static FRESULT io_read(FIL *file, void *data, UINT size, UINT *read_size) {
  const FRESULT result = f_read(file, data, size, read_size);
  report_io(LARGE_DBC_CANDIDATE_IO_READ,
            result == FR_OK ? (uint32_t)*read_size : 0u, result);
  return result;
}

static FRESULT io_write(FIL *file, const void *data, UINT size,
                        UINT *written_size) {
  const FRESULT result = f_write(file, data, size, written_size);
  report_io(LARGE_DBC_CANDIDATE_IO_WRITE,
            result == FR_OK ? (uint32_t)*written_size : 0u, result);
  return result;
}

static FRESULT io_seek(FIL *file, FSIZE_t offset) {
  const FRESULT result = f_lseek(file, offset);
  report_io(LARGE_DBC_CANDIDATE_IO_SEEK, 0u, result);
  return result;
}

static FRESULT io_sync(FIL *file) {
  const FRESULT result = f_sync(file);
  report_io(LARGE_DBC_CANDIDATE_IO_SYNC, 0u, result);
  return result;
}

static FRESULT io_truncate(FIL *file) {
  const FRESULT result = f_truncate(file);
  report_io(LARGE_DBC_CANDIDATE_IO_TRUNCATE, 0u, result);
  return result;
}

static FRESULT io_stat(const char *path, FILINFO *info) {
  const FRESULT result = f_stat(path, info);
  report_io(LARGE_DBC_CANDIDATE_IO_STAT, 0u, result);
  return result;
}

static FRESULT io_unlink(const char *path) {
  const FRESULT result = f_unlink(path);
  report_io(LARGE_DBC_CANDIDATE_IO_UNLINK, 0u, result);
  return result;
}

static FRESULT io_rename(const char *old_path, const char *new_path) {
  const FRESULT result = f_rename(old_path, new_path);
  report_io(LARGE_DBC_CANDIDATE_IO_RENAME, 0u, result);
  return result;
}

static bool format_generation_path(char output[CANDIDATE_PATH_BYTES],
                                   const char *stem,
                                   const char *suffix,
                                   uint64_t generation) {
  const unsigned long high = (unsigned long)(uint32_t)(generation >> 32u);
  const unsigned long low = (unsigned long)(uint32_t)generation;
  const int length = snprintf(output, CANDIDATE_PATH_BYTES,
                              "%s/dbc/%s.%08lX%08lX%s",
                              SDPath, stem, high, low, suffix);
  return length > 0 && (size_t)length < CANDIDATE_PATH_BYTES;
}

static bool format_fixed_path(char output[CANDIDATE_PATH_BYTES],
                              const char *suffix) {
  const int length = snprintf(output, CANDIDATE_PATH_BYTES, "%s/dbc/%s",
                              SDPath, suffix);
  return length > 0 && (size_t)length < CANDIDATE_PATH_BYTES;
}

static bool build_paths(uint64_t generation, CandidatePaths *paths) {
  return format_generation_path(paths->upload_tmp, "upload", ".tmp",
                                generation) &&
         format_generation_path(paths->source_tmp, "candidate", ".dbc.tmp",
                                generation) &&
         format_generation_path(paths->spool_tmp, "candidate", ".spool.tmp",
                                generation) &&
         format_generation_path(paths->index_tmp, "candidate", ".idx.tmp",
                                generation) &&
         format_generation_path(paths->selection_tmp, "candidate", ".sel.tmp",
                                generation) &&
         format_generation_path(paths->source_final, "candidate", ".dbc",
                                generation) &&
         format_generation_path(paths->index_final, "candidate", ".idx",
                                generation) &&
         format_generation_path(paths->selection_final, "candidate", ".sel",
                                generation) &&
         format_fixed_path(paths->current_tmp, "candidate.current.tmp") &&
         format_fixed_path(paths->current, "candidate.current") &&
         format_fixed_path(paths->previous, "candidate.previous");
}

static bool build_active_paths(uint64_t generation, CandidatePaths *paths) {
  return format_generation_path(paths->source_tmp, "active", ".dbc.tmp",
                                generation) &&
         format_generation_path(paths->index_tmp, "active", ".idx.tmp",
                                generation) &&
         format_generation_path(paths->selection_tmp, "active", ".sel.tmp",
                                generation) &&
         format_generation_path(paths->source_final, "active", ".dbc",
                                generation) &&
         format_generation_path(paths->index_final, "active", ".idx",
                                generation) &&
         format_generation_path(paths->selection_final, "active", ".sel",
                                generation) &&
         format_fixed_path(paths->current_tmp, "active.current.tmp") &&
         format_fixed_path(paths->current, "active.current") &&
         format_fixed_path(paths->previous, "active.previous");
}

static bool index_read_at(void *context, uint32_t offset,
                          uint8_t *data, uint32_t size) {
  CandidateIndexIoContext *io = context;
  UINT read_size = 0u;
  if (offset > io->logical_size || size > io->logical_size - offset) {
    return false;
  }
  if (io->read_cache != NULL && size <= io->read_cache_capacity) {
    if (offset >= io->read_cache_offset &&
        offset - io->read_cache_offset <= io->read_cache_size &&
        size <= io->read_cache_size - (offset - io->read_cache_offset)) {
      memcpy(data, io->read_cache + (offset - io->read_cache_offset), size);
      return true;
    }
    uint32_t cached_size = io->logical_size - offset;
    if (cached_size > io->read_cache_capacity) {
      cached_size = io->read_cache_capacity;
    }
    if (io_seek(io->file, (FSIZE_t)(io->base + offset)) != FR_OK ||
        io_read(io->file, io->read_cache, (UINT)cached_size, &read_size) != FR_OK ||
        read_size != (UINT)cached_size) {
      io->read_cache_size = 0u;
      return false;
    }
    io->read_cache_offset = offset;
    io->read_cache_size = cached_size;
    memcpy(data, io->read_cache, size);
    return true;
  }
  if (io_seek(io->file, (FSIZE_t)(io->base + offset)) != FR_OK ||
      io_read(io->file, data, (UINT)size, &read_size) != FR_OK) {
    return false;
  }
  return read_size == (UINT)size;
}

static bool index_write_at(void *context, uint32_t offset,
                           const uint8_t *data, uint32_t size) {
  CandidateIndexIoContext *io = context;
  UINT written_size = 0u;
  io->read_cache_size = 0u;
  if (offset > io->capacity || size > io->capacity - offset ||
      io_seek(io->file, (FSIZE_t)(io->base + offset)) != FR_OK ||
      io_write(io->file, data, (UINT)size, &written_size) != FR_OK ||
      written_size != (UINT)size) {
    return false;
  }
  if (offset + size > io->logical_size) {
    io->logical_size = offset + size;
  }
  return true;
}

static bool index_resize(void *context, uint32_t size) {
  CandidateIndexIoContext *io = context;
  io->read_cache_size = 0u;
  if (size > io->capacity) {
    return false;
  }
  if (io->physical_resize &&
      (io_seek(io->file, (FSIZE_t)(io->base + size)) != FR_OK ||
       io_truncate(io->file) != FR_OK)) {
    return false;
  }
  io->logical_size = size;
  return true;
}

static bool index_get_size(void *context, uint32_t *size) {
  const CandidateIndexIoContext *io = context;
  *size = io->logical_size;
  return true;
}

static DbcCatalogIndexIo catalog_io(CandidateIndexIoContext *context) {
  const DbcCatalogIndexIo io = {
    .context = context,
    .read_at = index_read_at,
    .write_at = index_write_at,
    .resize = index_resize,
    .get_size = index_get_size
  };
  return io;
}

static void close_open_files(void) {
  if (g_aux_open) {
    (void)io_close(&g_aux_file);
    g_aux_open = false;
  }
  if (g_index_open) {
    (void)io_close(&g_index_file);
    g_index_open = false;
  }
  if (g_source_open) {
    (void)io_close(&g_source_file);
    g_source_open = false;
  }
  if (g_spool_open) {
    (void)io_close(&g_spool_file);
    g_spool_open = false;
  }
}

static bool file_crc_size(const char *path, uint32_t *size,
                          uint32_t *crc32) {
  uint32_t total = 0u;
  uint32_t crc = dbc_catalog_crc32_begin();
  if (io_open(&g_aux_file, path, FA_READ) != FR_OK) {
    return false;
  }
  g_aux_open = true;
  for (;;) {
    UINT read_size = 0u;
    if (io_read(&g_aux_file, g_io_buffer, sizeof(g_io_buffer), &read_size) != FR_OK ||
        total > UINT32_MAX - read_size) {
      (void)io_close(&g_aux_file);
      g_aux_open = false;
      return false;
    }
    if (read_size == 0u) {
      break;
    }
    crc = dbc_catalog_crc32_update(crc, g_io_buffer, read_size);
    total += read_size;
  }
  if (io_close(&g_aux_file) != FR_OK) {
    g_aux_open = false;
    return false;
  }
  g_aux_open = false;
  *size = total;
  *crc32 = dbc_catalog_crc32_finish(crc);
  return true;
}

static bool read_exact_file(const char *path, uint8_t *data, uint32_t size) {
  UINT read_size = 0u;
  UINT extra_size = 0u;
  uint8_t extra = 0u;
  if (io_open(&g_aux_file, path, FA_READ) != FR_OK) {
    return false;
  }
  g_aux_open = true;
  const bool ok = io_read(&g_aux_file, data, (UINT)size, &read_size) == FR_OK &&
                  read_size == (UINT)size &&
                  io_read(&g_aux_file, &extra, 1u, &extra_size) == FR_OK &&
                  extra_size == 0u;
  const FRESULT closed = io_close(&g_aux_file);
  g_aux_open = false;
  return ok && closed == FR_OK;
}

static bool write_sync_file(const char *path, const uint8_t *data,
                            uint32_t size) {
  UINT written_size = 0u;
  if (io_open(&g_aux_file, path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
    return false;
  }
  g_aux_open = true;
  const bool ok = io_write(&g_aux_file, data, (UINT)size, &written_size) == FR_OK &&
                  written_size == (UINT)size && io_sync(&g_aux_file) == FR_OK;
  const FRESULT closed = io_close(&g_aux_file);
  g_aux_open = false;
  return ok && closed == FR_OK;
}

static bool write_sync_new_file(const char *path, const uint8_t *data,
                                uint32_t size, bool *owned) {
  UINT written_size = 0u;
  *owned = false;
  if (io_open(&g_aux_file, path, FA_CREATE_NEW | FA_WRITE) != FR_OK) {
    return false;
  }
  g_aux_open = true;
  *owned = true;
  const bool ok = io_write(&g_aux_file, data, (UINT)size, &written_size) == FR_OK &&
                  written_size == (UINT)size && io_sync(&g_aux_file) == FR_OK;
  const FRESULT closed = io_close(&g_aux_file);
  g_aux_open = false;
  return ok && closed == FR_OK;
}

static __attribute__((noinline)) bool copy_sync_new_file(
  const char *source_path,
  const char *destination_path,
  uint32_t expected_size,
  uint32_t expected_crc32,
  bool *owned) {
  uint32_t total = 0u;
  uint32_t crc = dbc_catalog_crc32_begin();
  *owned = false;
  if (io_open(&g_source_file, source_path, FA_READ) != FR_OK) {
    return false;
  }
  g_source_open = true;
  if (io_open(&g_aux_file, destination_path,
              FA_CREATE_NEW | FA_WRITE) != FR_OK) {
    (void)io_close(&g_source_file);
    g_source_open = false;
    return false;
  }
  g_aux_open = true;
  *owned = true;
  bool ok = true;
  for (;;) {
    UINT read_size = 0u;
    UINT written_size = 0u;
    if (io_read(&g_source_file, g_io_buffer, sizeof(g_io_buffer), &read_size) !=
          FR_OK || total > UINT32_MAX - read_size) {
      ok = false;
      break;
    }
    if (read_size == 0u) {
      break;
    }
    if (io_write(&g_aux_file, g_io_buffer, read_size, &written_size) != FR_OK ||
        written_size != read_size) {
      ok = false;
      break;
    }
    crc = dbc_catalog_crc32_update(crc, g_io_buffer, read_size);
    total += read_size;
  }
  if (ok && io_sync(&g_aux_file) != FR_OK) {
    ok = false;
  }
  if (io_close(&g_aux_file) != FR_OK) {
    ok = false;
  }
  g_aux_open = false;
  if (io_close(&g_source_file) != FR_OK) {
    ok = false;
  }
  g_source_open = false;
  crc = dbc_catalog_crc32_finish(crc);
  if (!ok || total != expected_size || crc != expected_crc32) {
    return false;
  }
  uint32_t readback_size = 0u;
  uint32_t readback_crc = 0u;
  return file_crc_size(destination_path, &readback_size, &readback_crc) &&
         readback_size == expected_size && readback_crc == expected_crc32;
}

static __attribute__((noinline)) bool path_absent(const char *path) {
  const FRESULT result = io_stat(path, &g_file_info);
  return result == FR_NO_FILE;
}

static uint16_t selected_message_count(
  const DbcCatalogIndexIo *index,
  const DbcCatalogIndexSummary *summary,
  const DbcSelectionV1 *selection,
  bool *valid) {
  uint16_t count = 0u;
  uint16_t last_message = UINT16_MAX;
  *valid = true;
  for (uint16_t ordinal = 0u; ordinal < summary->signal_count; ++ordinal) {
    if (!dbc_selection_v1_is_selected(selection, ordinal)) {
      continue;
    }
    DbcCatalogIndexSignal signal;
    if (dbc_catalog_index_read_signal(index, summary, ordinal, &signal) !=
        DBC_CATALOG_INDEX_OK) {
      *valid = false;
      return 0u;
    }
    if (signal.message_ordinal != last_message) {
      if (count == UINT16_MAX) {
        *valid = false;
        return 0u;
      }
      ++count;
      last_message = signal.message_ordinal;
    }
  }
  return count;
}

static __attribute__((noinline)) LargeDbcCandidateStm32Status
build_index_and_selection(
  const DbcCandidateUpload *upload,
  DbcCatalogIndexSummary *summary,
  uint16_t *selected_messages) {
  if (io_open(&g_source_file, g_paths.upload_tmp, FA_READ) != FR_OK) {
    return LARGE_DBC_CANDIDATE_STM32_IO_FAILED;
  }
  g_source_open = true;
  if (io_open(&g_spool_file, g_paths.spool_tmp,
              FA_CREATE_ALWAYS | FA_READ | FA_WRITE) != FR_OK) {
    return LARGE_DBC_CANDIDATE_STM32_IO_FAILED;
  }
  g_spool_open = true;

  g_message_io_context = (CandidateIndexIoContext){
    .file = &g_spool_file,
    .base = MESSAGE_SPOOL_BASE,
    .capacity = MESSAGE_SPOOL_CAPACITY,
    .logical_size = 0u,
    .physical_resize = false
  };
  g_signal_io_context = (CandidateIndexIoContext){
    .file = &g_spool_file,
    .base = SIGNAL_SPOOL_BASE,
    .capacity = SIGNAL_SPOOL_CAPACITY,
    .logical_size = 0u,
    .physical_resize = false
  };
  const DbcCatalogIndexIo message_io = catalog_io(&g_message_io_context);
  const DbcCatalogIndexIo signal_io = catalog_io(&g_signal_io_context);
  if (dbc_catalog_index_builder_init(&g_builder, &message_io, &signal_io,
                                     upload->source_size, upload->source_crc32) !=
      DBC_CATALOG_INDEX_OK) {
    return LARGE_DBC_CANDIDATE_STM32_INDEX_FAILED;
  }
  dbc_stream_index_adapter_init(&g_stream_adapter, &g_builder);
  const DbcStreamParserCallbacks callbacks = dbc_stream_index_adapter_callbacks();
  dbc_stream_parser_init(&g_parser, &callbacks, &g_stream_adapter);

  uint32_t source_size = 0u;
  uint32_t source_crc = dbc_catalog_crc32_begin();
  for (;;) {
    UINT read_size = 0u;
    if (io_read(&g_source_file, g_io_buffer, sizeof(g_io_buffer), &read_size) != FR_OK ||
        source_size > LARGE_DBC_SOURCE_MAX_BYTES - read_size) {
      return LARGE_DBC_CANDIDATE_STM32_IO_FAILED;
    }
    if (read_size == 0u) {
      break;
    }
    source_size += read_size;
    source_crc = dbc_catalog_crc32_update(source_crc, g_io_buffer, read_size);
    if (!dbc_stream_parser_feed(&g_parser, g_io_buffer, read_size)) {
      return g_stream_adapter.status == DBC_CATALOG_INDEX_OK ?
        LARGE_DBC_CANDIDATE_STM32_PARSE_FAILED :
        LARGE_DBC_CANDIDATE_STM32_INDEX_FAILED;
    }
  }
  if (!dbc_stream_parser_finalize(&g_parser) ||
      !dbc_stream_index_adapter_complete(&g_stream_adapter, &g_parser)) {
    return g_stream_adapter.status == DBC_CATALOG_INDEX_OK ?
      LARGE_DBC_CANDIDATE_STM32_PARSE_FAILED :
      LARGE_DBC_CANDIDATE_STM32_INDEX_FAILED;
  }
  source_crc = dbc_catalog_crc32_finish(source_crc);
  if (source_size != upload->source_size || source_crc != upload->source_crc32) {
    return LARGE_DBC_CANDIDATE_STM32_SOURCE_MISMATCH;
  }
  if (io_close(&g_source_file) != FR_OK) {
    g_source_open = false;
    return LARGE_DBC_CANDIDATE_STM32_IO_FAILED;
  }
  g_source_open = false;

  if (io_open(&g_index_file, g_paths.index_tmp,
              FA_CREATE_ALWAYS | FA_READ | FA_WRITE) != FR_OK) {
    return LARGE_DBC_CANDIDATE_STM32_IO_FAILED;
  }
  g_index_open = true;
  g_index_io_context = (CandidateIndexIoContext){
    .file = &g_index_file,
    .base = 0u,
    .capacity = LARGE_DBC_INDEX_HEADER_SIZE + MESSAGE_SPOOL_CAPACITY +
                SIGNAL_SPOOL_CAPACITY,
    .logical_size = 0u,
    .physical_resize = true
  };
  const DbcCatalogIndexIo index_io = catalog_io(&g_index_io_context);
  if (dbc_catalog_index_builder_finalize(&g_builder, &index_io, summary) !=
        DBC_CATALOG_INDEX_OK || io_sync(&g_index_file) != FR_OK) {
    return LARGE_DBC_CANDIDATE_STM32_INDEX_FAILED;
  }
  const DbcCatalogIndexExpectedSource expected = {
    .enabled = true,
    .source_size = upload->source_size,
    .source_crc32 = upload->source_crc32
  };
  DbcCatalogIndexSummary verified;
  if (dbc_catalog_index_verify(&index_io, &expected,
                               &g_builder.duplicate_workspace,
                               &verified) != DBC_CATALOG_INDEX_OK ||
      memcmp(summary, &verified, sizeof(verified)) != 0) {
    return LARGE_DBC_CANDIDATE_STM32_INDEX_FAILED;
  }
  if (dbc_selection_v1_init_default(&g_selection, upload->generation,
                                    upload->generation, upload->source_size,
                                    upload->source_crc32,
                                    summary->signal_count) !=
        DBC_CANDIDATE_FORMAT_OK ||
      dbc_selection_v1_encode(&g_selection, g_selection_bytes) !=
        DBC_CANDIDATE_FORMAT_OK) {
    return LARGE_DBC_CANDIDATE_STM32_FORMAT_FAILED;
  }
  bool messages_valid = false;
  *selected_messages = selected_message_count(&index_io, summary, &g_selection,
                                               &messages_valid);
  if (!messages_valid ||
      dbc_selection_v1_verify_activation(&g_selection, *selected_messages) ==
        DBC_CANDIDATE_FORMAT_LIMIT_EXCEEDED) {
    return LARGE_DBC_CANDIDATE_STM32_FORMAT_FAILED;
  }
  if (io_close(&g_index_file) != FR_OK) {
    g_index_open = false;
    return LARGE_DBC_CANDIDATE_STM32_IO_FAILED;
  }
  g_index_open = false;
  if (io_close(&g_spool_file) != FR_OK) {
    g_spool_open = false;
    return LARGE_DBC_CANDIDATE_STM32_IO_FAILED;
  }
  g_spool_open = false;
  (void)io_unlink(g_paths.spool_tmp);
  if (!write_sync_file(g_paths.selection_tmp, g_selection_bytes,
                       sizeof(g_selection_bytes))) {
    return LARGE_DBC_CANDIDATE_STM32_IO_FAILED;
  }
  return LARGE_DBC_CANDIDATE_STM32_OK;
}

static __attribute__((noinline)) bool open_index_for_verify(const char *path,
                                  const DbcCandidateUpload *source,
                                  DbcCatalogIndexSummary *summary,
                                  uint32_t *full_crc32,
                                  DbcCatalogIndexIo *index_io) {
  if (io_open(&g_index_file, path, FA_READ) != FR_OK) {
    return false;
  }
  g_index_open = true;
  const FSIZE_t physical_size = f_size(&g_index_file);
  if (physical_size > UINT32_MAX) {
    return false;
  }
  g_index_io_context = (CandidateIndexIoContext){
    .file = &g_index_file,
    .base = 0u,
    .capacity = (uint32_t)physical_size,
    .logical_size = (uint32_t)physical_size,
    .physical_resize = false,
    .read_cache = g_index_read_cache,
    .read_cache_capacity = sizeof(g_index_read_cache)
  };
  *index_io = catalog_io(&g_index_io_context);
  const DbcCatalogIndexExpectedSource expected = {
    .enabled = true,
    .source_size = source->source_size,
    .source_crc32 = source->source_crc32
  };
  if (dbc_catalog_index_verify(index_io, &expected,
                               &g_builder.duplicate_workspace, summary) !=
      DBC_CATALOG_INDEX_OK || summary->total_size != (uint32_t)physical_size ||
      io_seek(&g_index_file, 0u) != FR_OK) {
    return false;
  }
  uint32_t crc = dbc_catalog_crc32_begin();
  uint32_t total = 0u;
  for (;;) {
    UINT read_size = 0u;
    if (io_read(&g_index_file, g_io_buffer, sizeof(g_io_buffer), &read_size) != FR_OK) {
      return false;
    }
    if (read_size == 0u) {
      break;
    }
    total += read_size;
    crc = dbc_catalog_crc32_update(crc, g_io_buffer, read_size);
  }
  if (total != summary->total_size) {
    return false;
  }
  *full_crc32 = dbc_catalog_crc32_finish(crc);
  return true;
}

static bool verify_candidate_files(const CandidatePaths *paths,
                                   uint64_t generation,
                                   const DbcManifestV1 *expected_manifest,
                                   DbcManifestV1 *actual_manifest,
                                   DbcManifestReferenceFacts *facts) {
  DbcCandidateUpload source = {.generation = generation};
  if (!file_crc_size(paths->source_final, &source.source_size,
                     &source.source_crc32) || source.source_size == 0u ||
      source.source_size > LARGE_DBC_SOURCE_MAX_BYTES) {
    return false;
  }
  DbcCatalogIndexSummary index_summary;
  uint32_t index_crc32 = 0u;
  DbcCatalogIndexIo index_io;
  if (!open_index_for_verify(paths->index_final, &source, &index_summary,
                             &index_crc32, &index_io) ||
      !read_exact_file(paths->selection_final, g_selection_bytes,
                       sizeof(g_selection_bytes)) ||
      dbc_selection_v1_decode(g_selection_bytes, sizeof(g_selection_bytes),
                              &g_selection) != DBC_CANDIDATE_FORMAT_OK) {
    if (g_index_open) {
      (void)io_close(&g_index_file);
      g_index_open = false;
    }
    return false;
  }
  bool messages_valid = false;
  const uint16_t selected_messages =
    selected_message_count(&index_io, &index_summary, &g_selection,
                           &messages_valid);
  if (io_close(&g_index_file) != FR_OK) {
    g_index_open = false;
    return false;
  }
  g_index_open = false;
  if (!messages_valid) {
    return false;
  }

  *actual_manifest = (DbcManifestV1){
    .object_kind = LARGE_DBC_MANIFEST_OBJECT_CANDIDATE,
    .generation = generation,
    .source_size = source.source_size,
    .source_crc32 = source.source_crc32,
    .index_size = index_summary.total_size,
    .index_crc32 = index_crc32,
    .selection_size = (uint32_t)sizeof(g_selection_bytes),
    .selection_crc32 = dbc_candidate_crc32(g_selection_bytes,
                                            sizeof(g_selection_bytes)),
    .selected_count = g_selection.selected_count,
    .selected_message_count = selected_messages,
    .catalog_message_count = index_summary.message_count,
    .catalog_signal_count = index_summary.signal_count
  };
  const DbcCandidateIndexFacts index_facts = {
    .source_size = source.source_size,
    .source_crc32 = source.source_crc32,
    .index_size = index_summary.total_size,
    .index_crc32 = index_crc32,
    .catalog_message_count = index_summary.message_count,
    .catalog_signal_count = index_summary.signal_count
  };
  if (dbc_candidate_v1_verify_set(actual_manifest, &index_facts,
                                  g_selection_bytes, sizeof(g_selection_bytes),
                                  selected_messages, NULL) !=
      DBC_CANDIDATE_FORMAT_OK) {
    return false;
  }
  *facts = (DbcManifestReferenceFacts){
    .generation = actual_manifest->generation,
    .source_size = actual_manifest->source_size,
    .source_crc32 = actual_manifest->source_crc32,
    .index_size = actual_manifest->index_size,
    .index_crc32 = actual_manifest->index_crc32,
    .selection_size = actual_manifest->selection_size,
    .selection_crc32 = actual_manifest->selection_crc32,
    .selected_count = actual_manifest->selected_count,
    .selected_message_count = actual_manifest->selected_message_count,
    .catalog_message_count = actual_manifest->catalog_message_count,
    .catalog_signal_count = actual_manifest->catalog_signal_count
  };
  return expected_manifest == NULL ||
         dbc_manifest_v1_verify_references(expected_manifest, facts) ==
           DBC_CANDIDATE_FORMAT_OK;
}

static bool verify_active_files(const CandidatePaths *paths,
                                uint64_t generation,
                                const DbcManifestV1 *expected_manifest,
                                DbcManifestV1 *actual_manifest,
                                DbcManifestReferenceFacts *facts) {
  DbcCandidateUpload source = {.generation = generation};
  if (!file_crc_size(paths->source_final, &source.source_size,
                     &source.source_crc32) || source.source_size == 0u ||
      source.source_size > LARGE_DBC_SOURCE_MAX_BYTES) {
    return false;
  }
  DbcCatalogIndexSummary index_summary;
  uint32_t index_crc32 = 0u;
  DbcCatalogIndexIo index_io;
  if (!open_index_for_verify(paths->index_final, &source, &index_summary,
                             &index_crc32, &index_io) ||
      !read_exact_file(paths->selection_final, g_selection_bytes,
                       sizeof(g_selection_bytes)) ||
      dbc_selection_v1_decode(g_selection_bytes, sizeof(g_selection_bytes),
                              &g_selection) != DBC_CANDIDATE_FORMAT_OK) {
    if (g_index_open) {
      (void)io_close(&g_index_file);
      g_index_open = false;
    }
    return false;
  }
  bool messages_valid = false;
  const uint16_t selected_messages =
    selected_message_count(&index_io, &index_summary, &g_selection,
                           &messages_valid);
  if (io_close(&g_index_file) != FR_OK) {
    g_index_open = false;
    return false;
  }
  g_index_open = false;
  if (!messages_valid) {
    return false;
  }
  *actual_manifest = (DbcManifestV1){
    .object_kind = LARGE_DBC_MANIFEST_OBJECT_ACTIVE,
    .generation = generation,
    .source_size = source.source_size,
    .source_crc32 = source.source_crc32,
    .index_size = index_summary.total_size,
    .index_crc32 = index_crc32,
    .selection_size = (uint32_t)sizeof(g_selection_bytes),
    .selection_crc32 = dbc_candidate_crc32(g_selection_bytes,
                                            sizeof(g_selection_bytes)),
    .selected_count = g_selection.selected_count,
    .selected_message_count = selected_messages,
    .catalog_message_count = index_summary.message_count,
    .catalog_signal_count = index_summary.signal_count
  };
  const DbcCandidateIndexFacts index_facts = {
    .source_size = source.source_size,
    .source_crc32 = source.source_crc32,
    .index_size = index_summary.total_size,
    .index_crc32 = index_crc32,
    .catalog_message_count = index_summary.message_count,
    .catalog_signal_count = index_summary.signal_count
  };
  if (dbc_active_v1_verify_set(actual_manifest, &index_facts,
                               g_selection_bytes, sizeof(g_selection_bytes),
                               selected_messages, NULL) !=
      DBC_CANDIDATE_FORMAT_OK) {
    return false;
  }
  *facts = (DbcManifestReferenceFacts){
    .generation = actual_manifest->generation,
    .source_size = actual_manifest->source_size,
    .source_crc32 = actual_manifest->source_crc32,
    .index_size = actual_manifest->index_size,
    .index_crc32 = actual_manifest->index_crc32,
    .selection_size = actual_manifest->selection_size,
    .selection_crc32 = actual_manifest->selection_crc32,
    .selected_count = actual_manifest->selected_count,
    .selected_message_count = actual_manifest->selected_message_count,
    .catalog_message_count = actual_manifest->catalog_message_count,
    .catalog_signal_count = actual_manifest->catalog_signal_count
  };
  return expected_manifest == NULL ||
         dbc_manifest_v1_verify_references(expected_manifest, facts) ==
           DBC_CANDIDATE_FORMAT_OK;
}

static bool read_manifest_with_references(
  const char *manifest_path,
  uint8_t bytes[LARGE_DBC_MANIFEST_SIZE],
  DbcManifestV1 *manifest,
  DbcManifestReferenceFacts *facts) {
  if (!read_exact_file(manifest_path, bytes, LARGE_DBC_MANIFEST_SIZE) ||
      dbc_manifest_v1_decode(bytes, LARGE_DBC_MANIFEST_SIZE, manifest) !=
        DBC_CANDIDATE_FORMAT_OK ||
      manifest->object_kind != LARGE_DBC_MANIFEST_OBJECT_CANDIDATE ||
      !build_paths(manifest->generation, &g_paths)) {
    return false;
  }
  DbcManifestV1 actual;
  return verify_candidate_files(&g_paths, manifest->generation, manifest,
                                &actual, facts);
}

static bool read_manifest_record(
  const char *manifest_path,
  uint8_t bytes[LARGE_DBC_MANIFEST_SIZE],
  uint8_t expected_object_kind,
  DbcManifestV1 *manifest) {
  return manifest_path != NULL && bytes != NULL && manifest != NULL &&
         read_exact_file(manifest_path, bytes, LARGE_DBC_MANIFEST_SIZE) &&
         dbc_manifest_v1_decode(bytes, LARGE_DBC_MANIFEST_SIZE, manifest) ==
           DBC_CANDIDATE_FORMAT_OK &&
         manifest->object_kind == expected_object_kind;
}

static bool read_active_manifest_with_references(
  const char *manifest_path,
  uint8_t bytes[LARGE_DBC_MANIFEST_SIZE],
  DbcManifestV1 *manifest,
  DbcManifestReferenceFacts *facts) {
  if (!read_exact_file(manifest_path, bytes, LARGE_DBC_MANIFEST_SIZE) ||
      dbc_manifest_v1_decode(bytes, LARGE_DBC_MANIFEST_SIZE, manifest) !=
        DBC_CANDIDATE_FORMAT_OK ||
      manifest->object_kind != LARGE_DBC_MANIFEST_OBJECT_ACTIVE ||
      !build_active_paths(manifest->generation, &g_active_paths)) {
    return false;
  }
  DbcManifestV1 actual;
  return verify_active_files(&g_active_paths, manifest->generation, manifest,
                             &actual, facts);
}

static bool open_manifest_catalog(const CandidatePaths *paths,
                                  const DbcManifestV1 *manifest,
                                  DbcCatalogIndexSummary *summary,
                                  DbcCatalogIndexIo *index_io,
                                  DbcSelectionV1 *selection) {
  const DbcCandidateUpload source = {
    .generation = manifest->generation,
    .source_size = manifest->source_size,
    .source_crc32 = manifest->source_crc32
  };
  uint32_t index_crc32 = 0u;
  if (!open_index_for_verify(paths->index_final, &source, summary,
                             &index_crc32, index_io) ||
      index_crc32 != manifest->index_crc32 ||
      summary->total_size != manifest->index_size ||
      summary->message_count != manifest->catalog_message_count ||
      summary->signal_count != manifest->catalog_signal_count ||
      !read_exact_file(paths->selection_final, g_selection_bytes,
                       sizeof(g_selection_bytes)) ||
      dbc_selection_v1_decode(g_selection_bytes, sizeof(g_selection_bytes),
                              selection) != DBC_CANDIDATE_FORMAT_OK) {
    if (g_index_open) {
      (void)io_close(&g_index_file);
      g_index_open = false;
    }
    return false;
  }
  bool messages_valid = false;
  const uint16_t selected_messages =
    selected_message_count(index_io, summary, selection, &messages_valid);
  const DbcCandidateIndexFacts index_facts = {
    .source_size = summary->source_size,
    .source_crc32 = summary->source_crc32,
    .index_size = summary->total_size,
    .index_crc32 = index_crc32,
    .catalog_message_count = summary->message_count,
    .catalog_signal_count = summary->signal_count
  };
  if (!messages_valid ||
      dbc_candidate_v1_verify_set(manifest, &index_facts,
                                  g_selection_bytes,
                                  sizeof(g_selection_bytes),
                                  selected_messages, NULL) !=
        DBC_CANDIDATE_FORMAT_OK) {
    (void)io_close(&g_index_file);
    g_index_open = false;
    return false;
  }
  return true;
}

static bool open_active_manifest_catalog(const CandidatePaths *paths,
                                         const DbcManifestV1 *manifest,
                                         DbcCatalogIndexSummary *summary,
                                         DbcCatalogIndexIo *index_io,
                                         DbcSelectionV1 *selection) {
  const DbcCandidateUpload source = {
    .generation = manifest->generation,
    .source_size = manifest->source_size,
    .source_crc32 = manifest->source_crc32
  };
  uint32_t index_crc32 = 0u;
  if (!open_index_for_verify(paths->index_final, &source, summary,
                             &index_crc32, index_io) ||
      index_crc32 != manifest->index_crc32 ||
      summary->total_size != manifest->index_size ||
      summary->message_count != manifest->catalog_message_count ||
      summary->signal_count != manifest->catalog_signal_count ||
      !read_exact_file(paths->selection_final, g_selection_bytes,
                       sizeof(g_selection_bytes)) ||
      dbc_selection_v1_decode(g_selection_bytes, sizeof(g_selection_bytes),
                              selection) != DBC_CANDIDATE_FORMAT_OK) {
    if (g_index_open) {
      (void)io_close(&g_index_file);
      g_index_open = false;
    }
    return false;
  }
  bool messages_valid = false;
  const uint16_t selected_messages =
    selected_message_count(index_io, summary, selection, &messages_valid);
  const DbcCandidateIndexFacts index_facts = {
    .source_size = summary->source_size,
    .source_crc32 = summary->source_crc32,
    .index_size = summary->total_size,
    .index_crc32 = index_crc32,
    .catalog_message_count = summary->message_count,
    .catalog_signal_count = summary->signal_count
  };
  if (!messages_valid ||
      dbc_active_v1_verify_set(manifest, &index_facts, g_selection_bytes,
                               sizeof(g_selection_bytes), selected_messages,
                               NULL) != DBC_CANDIDATE_FORMAT_OK) {
    (void)io_close(&g_index_file);
    g_index_open = false;
    return false;
  }
  return true;
}

static bool open_verified_manifest_catalog(
  const CandidatePaths *paths,
  const DbcManifestV1 *manifest,
  DbcCatalogIndexSummary *summary,
  DbcCatalogIndexIo *index_io,
  DbcSelectionV1 *selection) {
  if (paths == NULL || manifest == NULL || summary == NULL ||
      index_io == NULL || selection == NULL ||
      manifest->selection_size != sizeof(g_selection_bytes) ||
      !read_exact_file(paths->selection_final, g_selection_bytes,
                       sizeof(g_selection_bytes)) ||
      dbc_candidate_crc32(g_selection_bytes, sizeof(g_selection_bytes)) !=
        manifest->selection_crc32 ||
      dbc_selection_v1_decode(g_selection_bytes, sizeof(g_selection_bytes),
                              selection) != DBC_CANDIDATE_FORMAT_OK ||
      selection->source_size != manifest->source_size ||
      selection->source_crc32 != manifest->source_crc32 ||
      selection->catalog_signal_count != manifest->catalog_signal_count ||
      selection->selected_count != manifest->selected_count ||
      io_open(&g_index_file, paths->index_final, FA_READ) != FR_OK) {
    return false;
  }
  g_index_open = true;
  const FSIZE_t physical_size = f_size(&g_index_file);
  if (physical_size > UINT32_MAX ||
      (uint32_t)physical_size != manifest->index_size) {
    (void)io_close(&g_index_file);
    g_index_open = false;
    return false;
  }
  g_index_io_context = (CandidateIndexIoContext){
    .file = &g_index_file,
    .base = 0u,
    .capacity = (uint32_t)physical_size,
    .logical_size = (uint32_t)physical_size,
    .physical_resize = false,
    .read_cache = g_index_read_cache,
    .read_cache_capacity = sizeof(g_index_read_cache)
  };
  *index_io = catalog_io(&g_index_io_context);
  *summary = (DbcCatalogIndexSummary){
    .source_size = manifest->source_size,
    .source_crc32 = manifest->source_crc32,
    .message_count = manifest->catalog_message_count,
    .signal_count = manifest->catalog_signal_count,
    .total_size = manifest->index_size
  };
  return true;
}

static void descriptor_from_manifest(const DbcManifestV1 *manifest,
                                     DbcCandidateDescriptor *descriptor) {
  *descriptor = (DbcCandidateDescriptor){
    .generation = manifest->generation,
    .selection_generation = manifest->generation,
    .source_size = manifest->source_size,
    .source_crc32 = manifest->source_crc32,
    .index_size = manifest->index_size,
    .index_crc32 = manifest->index_crc32,
    .selection_size = manifest->selection_size,
    .selection_crc32 = manifest->selection_crc32,
    .catalog_message_count = manifest->catalog_message_count,
    .catalog_signal_count = manifest->catalog_signal_count,
    .selected_count = manifest->selected_count,
    .selected_message_count = manifest->selected_message_count
  };
}

static DbcActiveDescriptor active_descriptor_from_manifest(
  const DbcManifestV1 *manifest,
  const DbcSelectionV1 *selection) {
  const DbcActiveDescriptor descriptor = {
    .active_generation = manifest->generation,
    .candidate_generation = selection->candidate_generation,
    .selection_generation = selection->selection_generation,
    .source_size = manifest->source_size,
    .source_crc32 = manifest->source_crc32,
    .index_size = manifest->index_size,
    .index_crc32 = manifest->index_crc32,
    .selection_size = manifest->selection_size,
    .selection_crc32 = manifest->selection_crc32,
    .catalog_message_count = manifest->catalog_message_count,
    .catalog_signal_count = manifest->catalog_signal_count,
    .selected_count = manifest->selected_count,
    .selected_message_count = manifest->selected_message_count
  };
  return descriptor;
}

static DbcSelectedRuntimeStatus build_prepared_runtime(
  const CandidatePaths *paths,
  const DbcManifestV1 *manifest,
  bool active_manifest,
  bool references_verified,
  uint64_t runtime_generation,
  const DbcSelectedRuleRequirement *rules,
  size_t rule_count,
  DbcSelectedRuntimeSnapshot *snapshot) {
  const bool opened = references_verified ?
    open_verified_manifest_catalog(paths, manifest, &g_catalog_summary,
                                   &g_catalog_index_io, &g_selection) :
    (active_manifest ?
      open_active_manifest_catalog(paths, manifest, &g_catalog_summary,
                                   &g_catalog_index_io, &g_selection) :
      open_manifest_catalog(paths, manifest, &g_catalog_summary,
                            &g_catalog_index_io, &g_selection));
  if (!opened) {
    return DBC_SELECTED_RUNTIME_INDEX_ERROR;
  }
  const DbcSelectedRuntimeBuildRequest request = {
    .index = &g_catalog_index_io,
    .index_summary = &g_catalog_summary,
    .selection = &g_selection,
    .runtime_generation = runtime_generation,
    .selection_crc32 = manifest->selection_crc32,
    .rules = rules,
    .rule_count = rule_count
  };
  DbcSelectedRuntimeStatus status =
    dbc_selected_runtime_build_inactive(snapshot, &request);
  if (io_close(&g_index_file) != FR_OK && status == DBC_SELECTED_RUNTIME_OK) {
    status = DBC_SELECTED_RUNTIME_INDEX_ERROR;
  }
  g_index_open = false;
  if (status != DBC_SELECTED_RUNTIME_OK) {
    return status;
  }
  const DbcSelectedRuntime *prepared = dbc_selected_runtime_prepared(snapshot);
  return prepared != NULL &&
         prepared->runtime_generation == runtime_generation &&
         prepared->candidate_generation == g_selection.candidate_generation &&
         prepared->selection_generation == g_selection.selection_generation &&
         prepared->source_size == manifest->source_size &&
         prepared->source_crc32 == manifest->source_crc32 &&
         prepared->selection_crc32 == manifest->selection_crc32 &&
         prepared->message_count == manifest->selected_message_count &&
         prepared->signal_count == manifest->selected_count ?
    DBC_SELECTED_RUNTIME_OK : DBC_SELECTED_RUNTIME_INDEX_MISMATCH;
}

static bool snapshot_from_manifest(const DbcManifestV1 *manifest,
                                   LargeDbcCandidateSnapshot *snapshot) {
  descriptor_from_manifest(manifest, &snapshot->candidate);
  return dbc_candidate_token_format(manifest->generation,
                                    manifest->source_size,
                                    manifest->source_crc32,
                                    snapshot->candidate_token) ==
           DBC_CANDIDATE_FORMAT_OK &&
         dbc_candidate_token_verify_manifest(snapshot->candidate_token,
                                             manifest) ==
           DBC_CANDIDATE_FORMAT_OK;
}

static void candidate_query_cache_set(
  const DbcManifestV1 *manifest,
  const uint8_t manifest_bytes[LARGE_DBC_MANIFEST_SIZE],
  bool uses_previous) {
  if (manifest == NULL || manifest_bytes == NULL || manifest->generation == 0u) {
    g_candidate_query_cache.valid = false;
    return;
  }
  g_candidate_query_cache.manifest = *manifest;
  memcpy(g_candidate_query_cache.manifest_bytes, manifest_bytes,
         sizeof(g_candidate_query_cache.manifest_bytes));
  g_candidate_query_cache.uses_previous = uses_previous;
  g_candidate_query_cache.valid = true;
}

static bool open_cached_candidate_catalog(DbcCatalogIndexSummary *summary,
                                          DbcCatalogIndexIo *index_io,
                                          DbcSelectionV1 *selection) {
  if (!g_candidate_query_cache.valid || summary == NULL || index_io == NULL ||
      selection == NULL ||
      !build_paths(g_candidate_query_cache.manifest.generation, &g_paths)) {
    return false;
  }
  const char *manifest_path = g_candidate_query_cache.uses_previous ?
    g_paths.previous : g_paths.current;
  if (!read_exact_file(manifest_path, g_current_manifest_bytes,
                       sizeof(g_current_manifest_bytes)) ||
      memcmp(g_current_manifest_bytes,
             g_candidate_query_cache.manifest_bytes,
             sizeof(g_current_manifest_bytes)) != 0 ||
      !read_exact_file(g_paths.selection_final, g_selection_bytes,
                       sizeof(g_selection_bytes)) ||
      dbc_candidate_crc32(g_selection_bytes, sizeof(g_selection_bytes)) !=
        g_candidate_query_cache.manifest.selection_crc32 ||
      dbc_selection_v1_decode(g_selection_bytes, sizeof(g_selection_bytes),
                              selection) != DBC_CANDIDATE_FORMAT_OK ||
      selection->candidate_generation !=
        g_candidate_query_cache.manifest.generation ||
      selection->selection_generation !=
        g_candidate_query_cache.manifest.generation ||
      selection->source_size != g_candidate_query_cache.manifest.source_size ||
      selection->source_crc32 !=
        g_candidate_query_cache.manifest.source_crc32 ||
      selection->catalog_signal_count !=
        g_candidate_query_cache.manifest.catalog_signal_count ||
      selection->selected_count !=
        g_candidate_query_cache.manifest.selected_count ||
      io_open(&g_index_file, g_paths.index_final, FA_READ) != FR_OK) {
    return false;
  }
  g_index_open = true;
  const FSIZE_t physical_size = f_size(&g_index_file);
  if (physical_size > UINT32_MAX ||
      (uint32_t)physical_size != g_candidate_query_cache.manifest.index_size) {
    (void)io_close(&g_index_file);
    g_index_open = false;
    return false;
  }
  g_index_io_context = (CandidateIndexIoContext){
    .file = &g_index_file,
    .base = 0u,
    .capacity = (uint32_t)physical_size,
    .logical_size = (uint32_t)physical_size,
    .physical_resize = false,
    .read_cache = g_index_read_cache,
    .read_cache_capacity = sizeof(g_index_read_cache)
  };
  *index_io = catalog_io(&g_index_io_context);
  *summary = (DbcCatalogIndexSummary){
    .source_size = g_candidate_query_cache.manifest.source_size,
    .source_crc32 = g_candidate_query_cache.manifest.source_crc32,
    .message_count = g_candidate_query_cache.manifest.catalog_message_count,
    .signal_count = g_candidate_query_cache.manifest.catalog_signal_count,
    .total_size = g_candidate_query_cache.manifest.index_size
  };
  return true;
}

static __attribute__((noinline)) bool rename_generation_files(void) {
  if (!path_absent(g_paths.source_final) || !path_absent(g_paths.index_final) ||
      !path_absent(g_paths.selection_final)) {
    return false;
  }
  if (io_rename(g_paths.upload_tmp, g_paths.source_final) != FR_OK) {
    return false;
  }
  g_source_renamed = true;
  if (io_rename(g_paths.index_tmp, g_paths.index_final) != FR_OK) {
    return false;
  }
  g_index_renamed = true;
  if (io_rename(g_paths.selection_tmp, g_paths.selection_final) != FR_OK) {
    return false;
  }
  g_selection_renamed = true;
  return true;
}

static bool selection_update_paths_absent(void) {
  return path_absent(g_paths.source_tmp) &&
         path_absent(g_paths.index_tmp) &&
         path_absent(g_paths.selection_tmp) &&
         path_absent(g_paths.source_final) &&
         path_absent(g_paths.index_final) &&
         path_absent(g_paths.selection_final) &&
         path_absent(g_paths.current_tmp);
}

static __attribute__((noinline)) bool rename_selection_generation_files(void) {
  if (io_rename(g_paths.source_tmp, g_paths.source_final) != FR_OK) {
    return false;
  }
  g_source_tmp_owned = false;
  g_source_renamed = true;
  if (io_rename(g_paths.index_tmp, g_paths.index_final) != FR_OK) {
    return false;
  }
  g_index_tmp_owned = false;
  g_index_renamed = true;
  if (io_rename(g_paths.selection_tmp, g_paths.selection_final) != FR_OK) {
    return false;
  }
  g_selection_tmp_owned = false;
  g_selection_renamed = true;
  return true;
}

static bool restore_previous_candidate_manifest(void) {
  if (!g_candidate_previous_rotated) {
    return !g_current_manifest_published;
  }
  if (g_current_manifest_published) {
    const FRESULT removed_current = io_unlink(g_paths.current);
    if (removed_current != FR_OK && removed_current != FR_NO_FILE) {
      return false;
    }
    g_current_manifest_published = false;
  }
  if (io_rename(g_paths.previous, g_paths.current) != FR_OK) {
    return false;
  }
  g_candidate_previous_rotated = false;
  DbcManifestV1 restored;
  DbcManifestReferenceFacts restored_facts;
  return read_manifest_with_references(g_paths.current, g_current_manifest_bytes,
                                       &restored, &restored_facts);
}

static __attribute__((noinline)) bool publish_manifest(
  const DbcManifestV1 *manifest) {
  if (dbc_manifest_v1_encode(manifest, g_manifest_bytes) !=
      DBC_CANDIDATE_FORMAT_OK) {
    return false;
  }
  const FRESULT removed_tmp = io_unlink(g_paths.current_tmp);
  if (removed_tmp != FR_OK && removed_tmp != FR_NO_FILE) {
    return false;
  }
  if (!write_sync_file(g_paths.current_tmp, g_manifest_bytes,
                       sizeof(g_manifest_bytes))) {
    return false;
  }
  if (!read_manifest_with_references(g_paths.current_tmp, g_manifest_bytes,
                                     &g_staged_manifest, &g_staged_facts)) {
    return false;
  }

  const FRESULT current_state = io_stat(g_paths.current, &g_file_info);
  if (current_state == FR_OK) {
    const FRESULT removed_previous = io_unlink(g_paths.previous);
    if (removed_previous != FR_OK && removed_previous != FR_NO_FILE) {
      return false;
    }
    if (io_rename(g_paths.current, g_paths.previous) != FR_OK) {
      return false;
    }
    g_candidate_previous_rotated = true;
  } else if (current_state != FR_NO_FILE) {
    return false;
  }
  if (io_rename(g_paths.current_tmp, g_paths.current) != FR_OK) {
    (void)restore_previous_candidate_manifest();
    return false;
  }
  g_current_manifest_published = true;
  DbcManifestV1 committed;
  DbcManifestReferenceFacts committed_facts;
  const bool committed_ok =
    read_manifest_with_references(g_paths.current, g_manifest_bytes,
                                  &committed, &committed_facts) &&
    committed.generation == manifest->generation;
  if (!committed_ok) {
    (void)restore_previous_candidate_manifest();
  }
  return committed_ok;
}

static __attribute__((noinline)) bool publish_selection_manifest(
  const DbcManifestV1 *manifest) {
  const uint64_t expected_generation = manifest->generation;
  if (dbc_manifest_v1_encode(manifest, g_manifest_bytes) !=
        DBC_CANDIDATE_FORMAT_OK ||
      !write_sync_new_file(g_paths.current_tmp, g_manifest_bytes,
                           sizeof(g_manifest_bytes), &g_current_tmp_owned)) {
    return false;
  }
  DbcManifestV1 staged;
  DbcManifestReferenceFacts staged_facts;
  if (!read_manifest_with_references(g_paths.current_tmp, g_manifest_bytes,
                                     &staged, &staged_facts)) {
    return false;
  }

  FILINFO info;
  const FRESULT current_state = io_stat(g_paths.current, &info);
  if (current_state == FR_OK) {
    const FRESULT removed_previous = io_unlink(g_paths.previous);
    if (removed_previous != FR_OK && removed_previous != FR_NO_FILE) {
      return false;
    }
    if (io_rename(g_paths.current, g_paths.previous) != FR_OK) {
      return false;
    }
    g_candidate_previous_rotated = true;
  } else if (current_state != FR_NO_FILE) {
    return false;
  }
  if (io_rename(g_paths.current_tmp, g_paths.current) != FR_OK) {
    (void)restore_previous_candidate_manifest();
    return false;
  }
  g_current_tmp_owned = false;
  g_current_manifest_published = true;
  const bool committed_ok =
    read_manifest_with_references(g_paths.current, g_manifest_bytes,
                                  &g_formal_manifest, &g_formal_facts) &&
    g_formal_manifest.generation == expected_generation;
  if (!committed_ok) {
    (void)restore_previous_candidate_manifest();
  }
  return committed_ok;
}

static void cleanup_owned_formal_files(void) {
  /*
   * A failed final readback after current.tmp->current is an explicit rollback:
   * remove the just-published manifest and this invocation's renamed files.
   * The caller first restores previous to current when a rotation occurred.
   * Never remove a file not renamed by this call.
   */
  if (g_current_manifest_published) {
    (void)io_unlink(g_paths.current);
  }
  if (g_selection_renamed) {
    (void)io_unlink(g_paths.selection_final);
  }
  if (g_index_renamed) {
    (void)io_unlink(g_paths.index_final);
  }
  if (g_source_renamed) {
    (void)io_unlink(g_paths.source_final);
  }
}

static void cleanup_work_files(void) {
  close_open_files();
  (void)io_unlink(g_paths.spool_tmp);
  (void)io_unlink(g_paths.index_tmp);
  (void)io_unlink(g_paths.selection_tmp);
  (void)io_unlink(g_paths.current_tmp);
}

static void cleanup_selection_update_owned_files(void) {
  close_open_files();
  if (g_current_tmp_owned) {
    (void)io_unlink(g_paths.current_tmp);
    g_current_tmp_owned = false;
  }
  if (g_selection_tmp_owned) {
    (void)io_unlink(g_paths.selection_tmp);
    g_selection_tmp_owned = false;
  }
  if (g_index_tmp_owned) {
    (void)io_unlink(g_paths.index_tmp);
    g_index_tmp_owned = false;
  }
  if (g_source_tmp_owned) {
    (void)io_unlink(g_paths.source_tmp);
    g_source_tmp_owned = false;
  }
}

LargeDbcCandidateStm32Status stm32h750_large_dbc_candidate_commit(
  const DbcCandidateUpload *upload,
  LargeDbcCandidateProgressCallback progress,
  void *progress_context,
  DbcCandidateDescriptor *published_candidate) {
  if (upload == NULL || published_candidate == NULL || upload->generation == 0u ||
      upload->source_size == 0u ||
      upload->source_size > LARGE_DBC_SOURCE_MAX_BYTES) {
    return LARGE_DBC_CANDIDATE_STM32_INVALID_ARGUMENT;
  }
  g_progress = (CandidateProgress){progress, progress_context};
  g_source_renamed = false;
  g_index_renamed = false;
  g_selection_renamed = false;
  g_current_manifest_published = false;
  g_candidate_previous_rotated = false;
  if (!build_paths(upload->generation, &g_paths)) {
    return LARGE_DBC_CANDIDATE_STM32_PATH_FAILED;
  }
  if (stm32h750_tf_fs_lock() != 0) {
    return LARGE_DBC_CANDIDATE_STM32_LOCK_FAILED;
  }

  LargeDbcCandidateStm32Status status = LARGE_DBC_CANDIDATE_STM32_OK;
  DbcCatalogIndexSummary summary;
  uint16_t selected_messages = 0u;
  status = build_index_and_selection(upload, &summary, &selected_messages);
  if (status != LARGE_DBC_CANDIDATE_STM32_OK) {
    goto done;
  }

  g_temp_paths = g_paths;
  memcpy(g_temp_paths.source_final, g_paths.upload_tmp,
         sizeof(g_temp_paths.source_final));
  memcpy(g_temp_paths.index_final, g_paths.index_tmp,
         sizeof(g_temp_paths.index_final));
  memcpy(g_temp_paths.selection_final, g_paths.selection_tmp,
         sizeof(g_temp_paths.selection_final));
  DbcManifestV1 manifest;
  DbcManifestReferenceFacts facts;
  if (!verify_candidate_files(&g_temp_paths, upload->generation, NULL,
                              &manifest, &facts)) {
    status = LARGE_DBC_CANDIDATE_STM32_VERIFY_FAILED;
    goto done;
  }
  if (!rename_generation_files()) {
    status = LARGE_DBC_CANDIDATE_STM32_COLLISION;
    goto done;
  }
  DbcManifestV1 formal_manifest;
  if (!verify_candidate_files(&g_paths, upload->generation, &manifest,
                              &formal_manifest, &facts)) {
    status = LARGE_DBC_CANDIDATE_STM32_VERIFY_FAILED;
    goto done;
  }
  manifest = formal_manifest;
  if (!publish_manifest(&manifest)) {
    status = LARGE_DBC_CANDIDATE_STM32_MANIFEST_FAILED;
    goto done;
  }
  candidate_query_cache_set(&manifest, g_manifest_bytes, false);
  descriptor_from_manifest(&manifest, published_candidate);

done:
  close_open_files();
  if (status != LARGE_DBC_CANDIDATE_STM32_OK) {
    (void)restore_previous_candidate_manifest();
    cleanup_owned_formal_files();
    cleanup_work_files();
  }
  stm32h750_tf_fs_unlock();
  return status;
}

LargeDbcCandidateStm32Status stm32h750_large_dbc_candidate_recover(
  LargeDbcCandidateProgressCallback progress,
  void *progress_context,
  bool *available,
  DbcCandidateDescriptor *recovered_candidate,
  uint64_t *next_generation) {
  if (available == NULL || recovered_candidate == NULL || next_generation == NULL) {
    return LARGE_DBC_CANDIDATE_STM32_INVALID_ARGUMENT;
  }
  g_progress = (CandidateProgress){progress, progress_context};
  if (!build_paths(1u, &g_paths)) {
    return LARGE_DBC_CANDIDATE_STM32_PATH_FAILED;
  }
  if (stm32h750_tf_fs_lock() != 0) {
    return LARGE_DBC_CANDIDATE_STM32_LOCK_FAILED;
  }

  char current_path[CANDIDATE_PATH_BYTES];
  char previous_path[CANDIDATE_PATH_BYTES];
  memcpy(current_path, g_paths.current, sizeof(current_path));
  memcpy(previous_path, g_paths.previous, sizeof(previous_path));
  DbcManifestV1 current;
  DbcManifestV1 previous;
  DbcManifestReferenceFacts current_facts;
  DbcManifestReferenceFacts previous_facts;
  const bool current_record_valid = read_manifest_record(
    current_path, g_current_manifest_bytes,
    LARGE_DBC_MANIFEST_OBJECT_CANDIDATE, &current);
  const bool previous_record_valid = read_manifest_record(
    previous_path, g_previous_manifest_bytes,
    LARGE_DBC_MANIFEST_OBJECT_CANDIDATE, &previous);
  const bool current_valid = current_record_valid &&
    read_manifest_with_references(current_path, g_current_manifest_bytes,
                                  &current, &current_facts);
  const bool previous_valid = !current_valid && previous_record_valid &&
    read_manifest_with_references(previous_path, g_previous_manifest_bytes,
                                  &previous, &previous_facts);

  DbcManifestV1 selected;
  const DbcManifestRecoverySlot slot =
    dbc_manifest_v1_select_current_or_previous(
      current_valid ? g_current_manifest_bytes : NULL,
      current_valid ? sizeof(g_current_manifest_bytes) : 0u,
      current_valid ? &current_facts : NULL,
      previous_valid ? g_previous_manifest_bytes : NULL,
      previous_valid ? sizeof(g_previous_manifest_bytes) : 0u,
      previous_valid ? &previous_facts : NULL,
      LARGE_DBC_MANIFEST_OBJECT_CANDIDATE, &selected);

  uint64_t maximum_generation = 0u;
  if (current_record_valid && current.generation > maximum_generation) {
    maximum_generation = current.generation;
  }
  if (previous_record_valid && previous.generation > maximum_generation) {
    maximum_generation = previous.generation;
  }
  const DbcCandidateFormatStatus next_status =
    dbc_candidate_next_generation(maximum_generation, next_generation);
  if (next_status != DBC_CANDIDATE_FORMAT_OK) {
    stm32h750_tf_fs_unlock();
    return LARGE_DBC_CANDIDATE_STM32_GENERATION_EXHAUSTED;
  }
  *available = slot != DBC_MANIFEST_RECOVERY_NONE;
  if (*available) {
    candidate_query_cache_set(
      &selected,
      slot == DBC_MANIFEST_RECOVERY_CURRENT ? g_current_manifest_bytes :
                                              g_previous_manifest_bytes,
      slot == DBC_MANIFEST_RECOVERY_PREVIOUS);
    descriptor_from_manifest(&selected, recovered_candidate);
  } else {
    g_candidate_query_cache.valid = false;
  }
  stm32h750_tf_fs_unlock();
  return LARGE_DBC_CANDIDATE_STM32_OK;
}

static LargeDbcCandidateStm32Status map_catalog_status(
  DbcCandidateCatalogStatus status) {
  switch (status) {
    case DBC_CANDIDATE_CATALOG_OK:
      return LARGE_DBC_CANDIDATE_STM32_OK;
    case DBC_CANDIDATE_CATALOG_INVALID_ARGUMENT:
      return LARGE_DBC_CANDIDATE_STM32_INVALID_ARGUMENT;
    case DBC_CANDIDATE_CATALOG_IO_FAILED:
      return LARGE_DBC_CANDIDATE_STM32_INDEX_FAILED;
    case DBC_CANDIDATE_CATALOG_TOKEN_MISMATCH:
      return LARGE_DBC_CANDIDATE_STM32_TOKEN_MISMATCH;
    case DBC_CANDIDATE_CATALOG_WRITE_BLOCKED:
      return LARGE_DBC_CANDIDATE_STM32_WRITE_BLOCKED;
    case DBC_CANDIDATE_CATALOG_GENERATION_EXHAUSTED:
      return LARGE_DBC_CANDIDATE_STM32_GENERATION_EXHAUSTED;
    default:
      return LARGE_DBC_CANDIDATE_STM32_SELECTION_FAILED;
  }
}

LargeDbcCandidateStm32Status stm32h750_large_dbc_candidate_query(
  const DbcCandidateCatalogQuery *query,
  LargeDbcCandidateProgressCallback progress,
  void *progress_context,
  LargeDbcCandidateCatalogResult *result) {
  if (query == NULL || result == NULL) {
    return LARGE_DBC_CANDIDATE_STM32_INVALID_ARGUMENT;
  }
  g_progress = (CandidateProgress){progress, progress_context};
  if (!build_paths(1u, &g_paths)) {
    return LARGE_DBC_CANDIDATE_STM32_PATH_FAILED;
  }
  if (stm32h750_tf_fs_lock() != 0) {
    return LARGE_DBC_CANDIDATE_STM32_LOCK_FAILED;
  }
  LargeDbcCandidateStm32Status status = LARGE_DBC_CANDIDATE_STM32_OK;
  if (open_cached_candidate_catalog(&g_catalog_summary,
                                    &g_catalog_index_io,
                                    &g_selection)) {
    memset(&g_query_result, 0, sizeof(g_query_result));
    const DbcCandidateCatalogStatus cached_query_status =
      dbc_candidate_catalog_query(&g_catalog_index_io, &g_catalog_summary,
                                  &g_selection, query, &g_catalog_workspace,
                                  &g_query_result.page);
    const FRESULT cached_close_result = io_close(&g_index_file);
    g_index_open = false;
    if (cached_query_status == DBC_CANDIDATE_CATALOG_OK &&
        cached_close_result == FR_OK &&
        snapshot_from_manifest(&g_candidate_query_cache.manifest,
                               &g_query_result.snapshot)) {
      *result = g_query_result;
      goto query_done;
    }
    if (cached_query_status == DBC_CANDIDATE_CATALOG_INVALID_ARGUMENT) {
      status = LARGE_DBC_CANDIDATE_STM32_INVALID_ARGUMENT;
      goto query_done;
    }
  }
  g_candidate_query_cache.valid = false;
  close_open_files();
  if (!build_paths(1u, &g_paths)) {
    status = LARGE_DBC_CANDIDATE_STM32_PATH_FAILED;
    goto query_done;
  }
  const FRESULT current_state = io_stat(g_paths.current, &g_file_info);
  if (current_state == FR_NO_FILE) {
    status = LARGE_DBC_CANDIDATE_STM32_NOT_FOUND;
    goto query_done;
  }
  if (current_state != FR_OK) {
    status = LARGE_DBC_CANDIDATE_STM32_IO_FAILED;
    goto query_done;
  }
  if (!read_manifest_with_references(g_paths.current, g_current_manifest_bytes,
                                     &g_work_manifest, &g_work_facts)) {
    status = LARGE_DBC_CANDIDATE_STM32_VERIFY_FAILED;
    goto query_done;
  }
  if (!open_manifest_catalog(&g_paths, &g_work_manifest, &g_catalog_summary,
                             &g_catalog_index_io,
                             &g_selection)) {
    status = LARGE_DBC_CANDIDATE_STM32_VERIFY_FAILED;
    goto query_done;
  }
  memset(&g_query_result, 0, sizeof(g_query_result));
  const DbcCandidateCatalogStatus query_status =
    dbc_candidate_catalog_query(&g_catalog_index_io, &g_catalog_summary,
                                &g_selection, query, &g_catalog_workspace,
                                &g_query_result.page);
  if (io_close(&g_index_file) != FR_OK) {
    g_index_open = false;
    status = LARGE_DBC_CANDIDATE_STM32_IO_FAILED;
    goto query_done;
  }
  g_index_open = false;
  if (query_status != DBC_CANDIDATE_CATALOG_OK) {
    status = query_status == DBC_CANDIDATE_CATALOG_INVALID_ARGUMENT ?
      LARGE_DBC_CANDIDATE_STM32_INVALID_ARGUMENT :
      LARGE_DBC_CANDIDATE_STM32_QUERY_FAILED;
    goto query_done;
  }
  if (!snapshot_from_manifest(&g_work_manifest, &g_query_result.snapshot)) {
    status = LARGE_DBC_CANDIDATE_STM32_FORMAT_FAILED;
    goto query_done;
  }
  candidate_query_cache_set(&g_work_manifest, g_current_manifest_bytes, false);
  *result = g_query_result;

query_done:
  close_open_files();
  stm32h750_tf_fs_unlock();
  return status;
}

LargeDbcCandidateStm32Status stm32h750_large_dbc_candidate_update_selection(
  const DbcCandidateSelectionMutation *mutation,
  LargeDbcCandidateProgressCallback progress,
  void *progress_context,
  LargeDbcCandidateSnapshot *published_candidate) {
  if (mutation == NULL || published_candidate == NULL) {
    return LARGE_DBC_CANDIDATE_STM32_INVALID_ARGUMENT;
  }
  g_progress = (CandidateProgress){progress, progress_context};
  g_source_renamed = false;
  g_index_renamed = false;
  g_selection_renamed = false;
  g_current_manifest_published = false;
  g_candidate_previous_rotated = false;
  g_source_tmp_owned = false;
  g_index_tmp_owned = false;
  g_selection_tmp_owned = false;
  g_current_tmp_owned = false;
  if (!build_paths(1u, &g_paths)) {
    return LARGE_DBC_CANDIDATE_STM32_PATH_FAILED;
  }
  if (stm32h750_tf_fs_lock() != 0) {
    return LARGE_DBC_CANDIDATE_STM32_LOCK_FAILED;
  }

  LargeDbcCandidateStm32Status status = LARGE_DBC_CANDIDATE_STM32_OK;
  if (g_candidate_query_cache.valid &&
      !g_candidate_query_cache.uses_previous &&
      open_cached_candidate_catalog(&g_catalog_summary,
                                    &g_catalog_index_io,
                                    &g_selection)) {
    g_work_manifest = g_candidate_query_cache.manifest;
    g_temp_paths = g_paths;
  } else {
    g_candidate_query_cache.valid = false;
    close_open_files();
    if (!build_paths(1u, &g_paths)) {
      status = LARGE_DBC_CANDIDATE_STM32_PATH_FAILED;
      goto update_done;
    }
    const FRESULT current_state = io_stat(g_paths.current, &g_file_info);
    if (current_state == FR_NO_FILE) {
      status = LARGE_DBC_CANDIDATE_STM32_NOT_FOUND;
      goto update_done;
    }
    if (current_state != FR_OK) {
      status = LARGE_DBC_CANDIDATE_STM32_IO_FAILED;
      goto update_done;
    }
    if (!read_manifest_with_references(g_paths.current,
                                       g_current_manifest_bytes,
                                       &g_work_manifest, &g_work_facts)) {
      status = LARGE_DBC_CANDIDATE_STM32_VERIFY_FAILED;
      goto update_done;
    }
    g_temp_paths = g_paths;
    if (!open_manifest_catalog(&g_temp_paths, &g_work_manifest,
                               &g_catalog_summary, &g_catalog_index_io,
                               &g_selection)) {
      status = LARGE_DBC_CANDIDATE_STM32_VERIFY_FAILED;
      goto update_done;
    }
    /* The current manifest and catalog are now fully verified. Publish this
     * cache even when prepare_update later rejects a stale token, so the next
     * read immediately converges from a recovered previous snapshot. */
    candidate_query_cache_set(&g_work_manifest, g_current_manifest_bytes,
                              false);
  }
  const DbcCandidateCatalogStatus prepared =
    dbc_candidate_selection_prepare_update(
      &g_catalog_index_io, &g_catalog_summary, &g_work_manifest, &g_selection,
      mutation, &g_catalog_workspace, &g_selection_update);
  if (io_close(&g_index_file) != FR_OK) {
    g_index_open = false;
    status = LARGE_DBC_CANDIDATE_STM32_IO_FAILED;
    goto update_done;
  }
  g_index_open = false;
  if (prepared != DBC_CANDIDATE_CATALOG_OK) {
    status = map_catalog_status(prepared);
    goto update_done;
  }
  if (!build_paths(g_selection_update.manifest.generation, &g_paths)) {
    status = LARGE_DBC_CANDIDATE_STM32_PATH_FAILED;
    goto update_done;
  }
  if (!selection_update_paths_absent()) {
    status = LARGE_DBC_CANDIDATE_STM32_COLLISION;
    goto update_done;
  }
  if (!copy_sync_new_file(g_temp_paths.source_final, g_paths.source_tmp,
                          g_work_manifest.source_size,
                          g_work_manifest.source_crc32,
                          &g_source_tmp_owned) ||
      !copy_sync_new_file(g_temp_paths.index_final, g_paths.index_tmp,
                          g_work_manifest.index_size,
                          g_work_manifest.index_crc32,
                          &g_index_tmp_owned)) {
    status = LARGE_DBC_CANDIDATE_STM32_IO_FAILED;
    goto update_done;
  }
  if (g_selection_update.selection.candidate_generation !=
        g_selection_update.manifest.generation ||
      g_selection_update.selection.selection_generation !=
        g_selection_update.manifest.generation ||
      dbc_selection_v1_encode(&g_selection_update.selection,
                              g_selection_bytes) !=
        DBC_CANDIDATE_FORMAT_OK) {
    status = LARGE_DBC_CANDIDATE_STM32_FORMAT_FAILED;
    goto update_done;
  }
  if (!write_sync_new_file(g_paths.selection_tmp, g_selection_bytes,
                           sizeof(g_selection_bytes),
                           &g_selection_tmp_owned)) {
    status = LARGE_DBC_CANDIDATE_STM32_IO_FAILED;
    goto update_done;
  }

  g_temp_paths = g_paths;
  memcpy(g_temp_paths.source_final, g_paths.source_tmp,
         sizeof(g_temp_paths.source_final));
  memcpy(g_temp_paths.index_final, g_paths.index_tmp,
         sizeof(g_temp_paths.index_final));
  memcpy(g_temp_paths.selection_final, g_paths.selection_tmp,
         sizeof(g_temp_paths.selection_final));
  if (!verify_candidate_files(&g_temp_paths,
                              g_selection_update.manifest.generation,
                              &g_selection_update.manifest,
                              &g_staged_manifest, &g_staged_facts)) {
    status = LARGE_DBC_CANDIDATE_STM32_VERIFY_FAILED;
    goto update_done;
  }
  if (!rename_selection_generation_files()) {
    status = LARGE_DBC_CANDIDATE_STM32_COLLISION;
    goto update_done;
  }
  if (!verify_candidate_files(&g_paths,
                              g_selection_update.manifest.generation,
                              &g_staged_manifest,
                              &g_formal_manifest, &g_formal_facts)) {
    status = LARGE_DBC_CANDIDATE_STM32_VERIFY_FAILED;
    goto update_done;
  }
  if (!publish_selection_manifest(&g_formal_manifest)) {
    status = LARGE_DBC_CANDIDATE_STM32_MANIFEST_FAILED;
    goto update_done;
  }
  candidate_query_cache_set(&g_formal_manifest, g_manifest_bytes, false);
  if (!snapshot_from_manifest(&g_formal_manifest, published_candidate)) {
    status = LARGE_DBC_CANDIDATE_STM32_FORMAT_FAILED;
    goto update_done;
  }

update_done:
  close_open_files();
  if (status != LARGE_DBC_CANDIDATE_STM32_OK) {
    (void)restore_previous_candidate_manifest();
    cleanup_owned_formal_files();
    cleanup_selection_update_owned_files();
  }
  stm32h750_tf_fs_unlock();
  return status;
}

static DbcManifestV1 active_manifest_from_candidate(
  uint64_t active_generation,
  const DbcManifestV1 *candidate) {
  const DbcManifestV1 manifest = {
    .object_kind = LARGE_DBC_MANIFEST_OBJECT_ACTIVE,
    .generation = active_generation,
    .source_size = candidate->source_size,
    .source_crc32 = candidate->source_crc32,
    .index_size = candidate->index_size,
    .index_crc32 = candidate->index_crc32,
    .selection_size = candidate->selection_size,
    .selection_crc32 = candidate->selection_crc32,
    .selected_count = candidate->selected_count,
    .selected_message_count = candidate->selected_message_count,
    .catalog_message_count = candidate->catalog_message_count,
    .catalog_signal_count = candidate->catalog_signal_count
  };
  return manifest;
}

static bool active_valid_manifest_generations(bool *current_valid,
                                              DbcManifestV1 *current,
                                              bool *previous_valid,
                                              DbcManifestV1 *previous) {
  if (current_valid == NULL || current == NULL || previous_valid == NULL ||
      previous == NULL || !build_active_paths(1u, &g_active_paths)) {
    return false;
  }
  DbcManifestReferenceFacts current_facts;
  DbcManifestReferenceFacts previous_facts;
  *current_valid = read_active_manifest_with_references(
    g_active_paths.current, g_current_manifest_bytes, current, &current_facts);
  if (!build_active_paths(1u, &g_active_paths)) {
    return false;
  }
  *previous_valid = read_active_manifest_with_references(
    g_active_paths.previous, g_previous_manifest_bytes, previous,
    &previous_facts);
  return build_active_paths(1u, &g_active_paths);
}

static bool active_max_valid_generation(uint64_t *maximum_generation) {
  if (maximum_generation == NULL) {
    return false;
  }
  bool current_valid = false;
  bool previous_valid = false;
  DbcManifestV1 current;
  DbcManifestV1 previous;
  if (!active_valid_manifest_generations(&current_valid, &current,
                                         &previous_valid, &previous)) {
    return false;
  }
  *maximum_generation = 0u;
  if (current_valid && current.generation > *maximum_generation) {
    *maximum_generation = current.generation;
  }
  if (previous_valid && previous.generation > *maximum_generation) {
    *maximum_generation = previous.generation;
  }
  return true;
}

static bool unlink_if_present(const char *path) {
  const FRESULT result = io_unlink(path);
  return result == FR_OK || result == FR_NO_FILE;
}

/*
 * A reset before active.current is published leaves, at most, the generation
 * immediately following max(current, previous).  Neither valid manifest can
 * reference that generation, so it is safe to remove without a directory scan.
 */
static bool cleanup_unreferenced_next_active_generation(uint64_t generation) {
  if (generation == 0u || !build_active_paths(generation, &g_active_paths)) {
    return false;
  }
  return unlink_if_present(g_active_paths.current_tmp) &&
         unlink_if_present(g_active_paths.source_tmp) &&
         unlink_if_present(g_active_paths.index_tmp) &&
         unlink_if_present(g_active_paths.selection_tmp) &&
         unlink_if_present(g_active_paths.source_final) &&
         unlink_if_present(g_active_paths.index_final) &&
         unlink_if_present(g_active_paths.selection_final);
}

static bool active_paths_absent(void) {
  return path_absent(g_active_paths.source_tmp) &&
         path_absent(g_active_paths.index_tmp) &&
         path_absent(g_active_paths.selection_tmp) &&
         path_absent(g_active_paths.source_final) &&
         path_absent(g_active_paths.index_final) &&
         path_absent(g_active_paths.selection_final) &&
         path_absent(g_active_paths.current_tmp);
}

static bool rename_active_generation_files(void) {
  if (io_rename(g_active_paths.source_tmp,
                g_active_paths.source_final) != FR_OK) {
    return false;
  }
  g_active_source_owned = false;
  g_active_source_renamed = true;
  if (io_rename(g_active_paths.index_tmp,
                g_active_paths.index_final) != FR_OK) {
    return false;
  }
  g_active_index_owned = false;
  g_active_index_renamed = true;
  if (io_rename(g_active_paths.selection_tmp,
                g_active_paths.selection_final) != FR_OK) {
    return false;
  }
  g_active_selection_owned = false;
  g_active_selection_renamed = true;
  return true;
}

static bool restore_previous_active_manifest(void) {
  if (!g_active_previous_rotated) {
    return !g_active_manifest_published;
  }
  /*
   * read_active_manifest_with_references() rebuilds g_active_paths from the
   * restored manifest generation.  Keep the failed transaction's paths for
   * cleanup: its ownership flags still describe the staged generation, not
   * the restored active generation.
   */
  const CandidatePaths transaction_paths = g_active_paths;
  if (g_active_manifest_published) {
    const FRESULT removed_current = io_unlink(g_active_paths.current);
    if (removed_current != FR_OK && removed_current != FR_NO_FILE) {
      return false;
    }
    g_active_manifest_published = false;
  }
  if (io_rename(g_active_paths.previous, g_active_paths.current) != FR_OK) {
    return false;
  }
  g_active_previous_rotated = false;
  DbcManifestV1 restored;
  DbcManifestReferenceFacts restored_facts;
  const bool restored_ok = read_active_manifest_with_references(
    g_active_paths.current, g_current_manifest_bytes, &restored,
    &restored_facts);
  g_active_paths = transaction_paths;
  return restored_ok;
}

static bool publish_active_manifest(const DbcManifestV1 *manifest) {
  if (dbc_manifest_v1_encode(manifest, g_manifest_bytes) !=
        DBC_CANDIDATE_FORMAT_OK) {
    return false;
  }
#if defined(CAN_BUS_LARGE_DBC_H1_FAULT_INJECTION)
  if (large_dbc_h1_fault_consume(LARGE_DBC_H1_FAULT_ACTIVE_CURRENT_WRITE,
                                  LARGE_DBC_CANDIDATE_IO_WRITE,
                                  (uint32_t)FR_DISK_ERR)) {
    report_io(LARGE_DBC_CANDIDATE_IO_WRITE, 0u, FR_DISK_ERR);
    return false;
  }
#endif
  if (!write_sync_new_file(g_active_paths.current_tmp, g_manifest_bytes,
                           sizeof(g_manifest_bytes),
                           &g_active_current_tmp_owned)) {
    return false;
  }
  DbcManifestV1 staged;
  DbcManifestReferenceFacts staged_facts;
  if (!read_active_manifest_with_references(g_active_paths.current_tmp,
                                            g_manifest_bytes, &staged,
                                            &staged_facts)) {
    return false;
  }
  if (!build_active_paths(manifest->generation, &g_active_paths)) {
    return false;
  }
  const FRESULT current_state = io_stat(g_active_paths.current, &g_file_info);
  if (current_state == FR_OK) {
    const FRESULT removed_previous = io_unlink(g_active_paths.previous);
    if (removed_previous != FR_OK && removed_previous != FR_NO_FILE) {
      return false;
    }
    if (io_rename(g_active_paths.current,
                  g_active_paths.previous) != FR_OK) {
      return false;
    }
    g_active_previous_rotated = true;
  } else if (current_state != FR_NO_FILE) {
    return false;
  }
#if defined(CAN_BUS_LARGE_DBC_H1_FAULT_INJECTION)
  if (large_dbc_h1_fault_consume(LARGE_DBC_H1_FAULT_ACTIVE_CURRENT_RENAME,
                                  LARGE_DBC_CANDIDATE_IO_RENAME,
                                  (uint32_t)FR_DISK_ERR)) {
    report_io(LARGE_DBC_CANDIDATE_IO_RENAME, 0u, FR_DISK_ERR);
    (void)restore_previous_active_manifest();
    return false;
  }
#endif
  if (io_rename(g_active_paths.current_tmp,
                g_active_paths.current) != FR_OK) {
    (void)restore_previous_active_manifest();
    return false;
  }
  g_active_current_tmp_owned = false;
  g_active_manifest_published = true;
  DbcManifestV1 committed;
  DbcManifestReferenceFacts committed_facts;
  bool committed_ok;
#if defined(CAN_BUS_LARGE_DBC_H1_FAULT_INJECTION)
  if (large_dbc_h1_fault_consume(
        LARGE_DBC_H1_FAULT_ACTIVE_CURRENT_READBACK,
        LARGE_DBC_CANDIDATE_IO_READ, (uint32_t)FR_DISK_ERR)) {
    report_io(LARGE_DBC_CANDIDATE_IO_READ, 0u, FR_DISK_ERR);
    committed_ok = false;
  } else
#endif
  {
    committed_ok = read_active_manifest_with_references(
      g_active_paths.current, g_manifest_bytes, &committed, &committed_facts) &&
      committed.generation == manifest->generation;
  }
  if (!committed_ok) {
    (void)restore_previous_active_manifest();
  }
  return committed_ok;
}

static void cleanup_active_owned_files(void) {
  close_open_files();
  if (g_active_manifest_published) {
    (void)io_unlink(g_active_paths.current);
  }
  if (g_active_selection_renamed) {
    (void)io_unlink(g_active_paths.selection_final);
  }
  if (g_active_index_renamed) {
    (void)io_unlink(g_active_paths.index_final);
  }
  if (g_active_source_renamed) {
    (void)io_unlink(g_active_paths.source_final);
  }
  if (g_active_current_tmp_owned) {
    (void)io_unlink(g_active_paths.current_tmp);
  }
  if (g_active_selection_owned) {
    (void)io_unlink(g_active_paths.selection_tmp);
  }
  if (g_active_index_owned) {
    (void)io_unlink(g_active_paths.index_tmp);
  }
  if (g_active_source_owned) {
    (void)io_unlink(g_active_paths.source_tmp);
  }
}

static LargeDbcActiveStm32Status map_runtime_status(
  DbcSelectedRuntimeStatus status) {
  switch (status) {
    case DBC_SELECTED_RUNTIME_RULE_KEY_MISSING:
      return LARGE_DBC_ACTIVE_STM32_RULE_KEY_MISSING;
    case DBC_SELECTED_RUNTIME_RULE_DEFINITION_CONFLICT:
      return LARGE_DBC_ACTIVE_STM32_RULE_DEFINITION_CONFLICT;
    default:
      return LARGE_DBC_ACTIVE_STM32_RUNTIME_FAILED;
  }
}

__attribute__((noinline)) LargeDbcActiveStm32Status
stm32h750_large_dbc_active_commit(
  const LargeDbcActiveRequest *request,
  LargeDbcCandidateProgressCallback progress,
  void *progress_context,
  LargeDbcActiveResult *result) {
  if (request == NULL || result == NULL || request->active_generation == 0u ||
      request->runtime_snapshot == NULL || request->publish == NULL ||
      (request->rules == NULL && request->rule_count != 0u)) {
    return LARGE_DBC_ACTIVE_STM32_INVALID_ARGUMENT;
  }
  g_progress = (CandidateProgress){progress, progress_context};
  if (!build_paths(1u, &g_paths) ||
      !build_active_paths(request->active_generation, &g_active_paths)) {
    return LARGE_DBC_ACTIVE_STM32_INVALID_ARGUMENT;
  }
  if (stm32h750_tf_fs_lock() != 0) {
    return LARGE_DBC_ACTIVE_STM32_LOCK_FAILED;
  }
  LargeDbcActiveStm32Status status = LARGE_DBC_ACTIVE_STM32_OK;
  memset(result, 0, sizeof(*result));
  g_active_source_owned = false;
  g_active_index_owned = false;
  g_active_selection_owned = false;
  g_active_current_tmp_owned = false;
  g_active_source_renamed = false;
  g_active_index_renamed = false;
  g_active_selection_renamed = false;
  g_active_manifest_published = false;
  g_active_previous_rotated = false;

  uint64_t maximum_active_generation = 0u;
  if (!active_max_valid_generation(&maximum_active_generation)) {
    status = LARGE_DBC_ACTIVE_STM32_VERIFY_FAILED;
    goto active_done;
  }
  if (maximum_active_generation == UINT64_MAX) {
    status = LARGE_DBC_ACTIVE_STM32_GENERATION_EXHAUSTED;
    goto active_done;
  }
  if (request->active_generation != maximum_active_generation + 1u) {
    status = LARGE_DBC_ACTIVE_STM32_COLLISION;
    goto active_done;
  }
  if (!cleanup_unreferenced_next_active_generation(
        request->active_generation) ||
      !build_active_paths(request->active_generation, &g_active_paths)) {
    status = LARGE_DBC_ACTIVE_STM32_IO_FAILED;
    goto active_done;
  }

  if (!read_manifest_with_references(g_paths.current,
                                     g_current_manifest_bytes,
                                     &g_active_candidate_manifest,
                                     &g_work_facts)) {
    status = LARGE_DBC_ACTIVE_STM32_NOT_FOUND;
    goto active_done;
  }
  g_active_candidate_paths = g_paths;
  if (g_active_candidate_manifest.selected_count == 0u ||
      g_active_candidate_manifest.selected_message_count == 0u ||
      g_active_candidate_manifest.selected_count >
        LARGE_DBC_ACTIVE_MAX_SIGNALS ||
      g_active_candidate_manifest.selected_message_count >
        LARGE_DBC_ACTIVE_MAX_MESSAGES) {
    status = LARGE_DBC_ACTIVE_STM32_VERIFY_FAILED;
    goto active_done;
  }
  if (request->writes_blocked) {
    status = LARGE_DBC_ACTIVE_STM32_LOGGING_ACTIVE;
    goto active_done;
  }
  g_active_runtime_status = build_prepared_runtime(
    &g_active_candidate_paths, &g_active_candidate_manifest, false, true,
    request->active_generation, request->rules, request->rule_count,
    request->runtime_snapshot);
  if (g_active_runtime_status != DBC_SELECTED_RUNTIME_OK) {
    status = map_runtime_status(g_active_runtime_status);
    goto active_done;
  }
  if (!build_active_paths(request->active_generation, &g_active_paths) ||
      !active_paths_absent()) {
    status = LARGE_DBC_ACTIVE_STM32_COLLISION;
    goto active_done;
  }
  if (!copy_sync_new_file(g_active_candidate_paths.source_final,
                          g_active_paths.source_tmp,
                          g_active_candidate_manifest.source_size,
                          g_active_candidate_manifest.source_crc32,
                          &g_active_source_owned) ||
      !copy_sync_new_file(g_active_candidate_paths.index_final,
                          g_active_paths.index_tmp,
                          g_active_candidate_manifest.index_size,
                          g_active_candidate_manifest.index_crc32,
                          &g_active_index_owned) ||
      !copy_sync_new_file(g_active_candidate_paths.selection_final,
                          g_active_paths.selection_tmp,
                          g_active_candidate_manifest.selection_size,
                          g_active_candidate_manifest.selection_crc32,
                          &g_active_selection_owned)) {
    status = LARGE_DBC_ACTIVE_STM32_IO_FAILED;
    goto active_done;
  }
  g_active_manifest = active_manifest_from_candidate(
    request->active_generation, &g_active_candidate_manifest);
  g_temp_paths = g_active_paths;
  memcpy(g_temp_paths.source_final, g_active_paths.source_tmp,
         sizeof(g_temp_paths.source_final));
  memcpy(g_temp_paths.index_final, g_active_paths.index_tmp,
         sizeof(g_temp_paths.index_final));
  memcpy(g_temp_paths.selection_final, g_active_paths.selection_tmp,
         sizeof(g_temp_paths.selection_final));
  if (!verify_active_files(&g_temp_paths, request->active_generation,
                           &g_active_manifest, &g_staged_manifest,
                           &g_staged_facts)) {
    status = LARGE_DBC_ACTIVE_STM32_VERIFY_FAILED;
    goto active_done;
  }
  if (!rename_active_generation_files()) {
    status = LARGE_DBC_ACTIVE_STM32_COLLISION;
    goto active_done;
  }
  if (!verify_active_files(&g_active_paths, request->active_generation,
                           &g_active_manifest, &g_formal_manifest,
                           &g_formal_facts)) {
    status = LARGE_DBC_ACTIVE_STM32_VERIFY_FAILED;
    goto active_done;
  }
  const DbcSelectedRuntime *prepared =
    request->runtime_snapshot->prepared_slot < 2u ?
      dbc_selected_runtime_prepared(request->runtime_snapshot) : NULL;
  if (prepared == NULL ||
      prepared->runtime_generation != request->active_generation) {
    status = LARGE_DBC_ACTIVE_STM32_RUNTIME_FAILED;
    goto active_done;
  }
  g_active_descriptor = active_descriptor_from_manifest(
    &g_formal_manifest, &g_selection);
  result->active = g_active_descriptor;
  result->runtime_slot = request->runtime_snapshot->prepared_slot;
  if (!publish_active_manifest(&g_formal_manifest)) {
    status = LARGE_DBC_ACTIVE_STM32_MANIFEST_FAILED;
    goto active_done;
  }
#if defined(CAN_BUS_LARGE_DBC_H1_FAULT_INJECTION)
  if (large_dbc_h1_fault_consume(
        LARGE_DBC_H1_FAULT_ACTIVE_RUNTIME_PUBLISH,
        LARGE_DBC_H1_OPERATION_RUNTIME_PUBLISH,
        (uint32_t)DBC_SELECTED_RUNTIME_PREPARED_SLOT_MISMATCH)) {
    g_active_runtime_status = DBC_SELECTED_RUNTIME_PREPARED_SLOT_MISMATCH;
  } else
#endif
  {
    g_active_runtime_status = request->publish(
      request->publish_context, request->runtime_snapshot, &result->active,
      result->runtime_slot);
  }
  if (g_active_runtime_status != DBC_SELECTED_RUNTIME_OK) {
    status = map_runtime_status(g_active_runtime_status);
  }

active_done:
  close_open_files();
  if (status != LARGE_DBC_ACTIVE_STM32_OK) {
    (void)restore_previous_active_manifest();
    cleanup_active_owned_files();
    if (request->runtime_snapshot->has_prepared) {
      (void)dbc_selected_runtime_discard_prepared(request->runtime_snapshot);
    }
  }
  stm32h750_tf_fs_unlock();
  return status;
}

__attribute__((noinline)) LargeDbcActiveStm32Status
stm32h750_large_dbc_active_recover(
  const DbcSelectedRuleRequirement *rules,
  size_t rule_count,
  DbcSelectedRuntimeSnapshot *runtime_snapshot,
  LargeDbcActivePublishCallback publish,
  void *publish_context,
  LargeDbcCandidateProgressCallback progress,
  void *progress_context,
  bool *available,
  LargeDbcActiveResult *result,
  uint64_t *next_generation) {
  if ((rules == NULL && rule_count != 0u) || runtime_snapshot == NULL ||
      publish == NULL || available == NULL || result == NULL ||
      next_generation == NULL || !build_active_paths(1u, &g_active_paths)) {
    return LARGE_DBC_ACTIVE_STM32_INVALID_ARGUMENT;
  }
  g_progress = (CandidateProgress){progress, progress_context};
  if (stm32h750_tf_fs_lock() != 0) {
    return LARGE_DBC_ACTIVE_STM32_LOCK_FAILED;
  }
  char current_path[CANDIDATE_PATH_BYTES];
  char previous_path[CANDIDATE_PATH_BYTES];
  memcpy(current_path, g_active_paths.current, sizeof(current_path));
  memcpy(previous_path, g_active_paths.previous, sizeof(previous_path));
  DbcManifestV1 current;
  DbcManifestV1 previous;
  const bool current_record_valid = read_manifest_record(
    current_path, g_current_manifest_bytes, LARGE_DBC_MANIFEST_OBJECT_ACTIVE,
    &current);
  const bool previous_record_valid = read_manifest_record(
    previous_path, g_previous_manifest_bytes,
    LARGE_DBC_MANIFEST_OBJECT_ACTIVE, &previous);
  DbcManifestReferenceFacts current_facts;
  DbcManifestReferenceFacts previous_facts;
  const bool current_valid = current_record_valid &&
    read_active_manifest_with_references(current_path,
                                         g_current_manifest_bytes,
                                         &current, &current_facts);
  const bool previous_valid = !current_valid && previous_record_valid &&
    read_active_manifest_with_references(previous_path,
                                         g_previous_manifest_bytes,
                                         &previous, &previous_facts);
  DbcManifestV1 selected;
  const DbcManifestRecoverySlot slot =
    dbc_manifest_v1_select_current_or_previous(
      current_valid ? g_current_manifest_bytes : NULL,
      current_valid ? sizeof(g_current_manifest_bytes) : 0u,
      current_valid ? &current_facts : NULL,
      previous_valid ? g_previous_manifest_bytes : NULL,
      previous_valid ? sizeof(g_previous_manifest_bytes) : 0u,
      previous_valid ? &previous_facts : NULL,
      LARGE_DBC_MANIFEST_OBJECT_ACTIVE, &selected);
  uint64_t maximum_generation = 0u;
  if (current_record_valid && current.generation > maximum_generation) {
    maximum_generation = current.generation;
  }
  if (previous_record_valid && previous.generation > maximum_generation) {
    maximum_generation = previous.generation;
  }
  if (dbc_candidate_next_generation(maximum_generation, next_generation) !=
      DBC_CANDIDATE_FORMAT_OK) {
    stm32h750_tf_fs_unlock();
    return LARGE_DBC_ACTIVE_STM32_GENERATION_EXHAUSTED;
  }
  if (!cleanup_unreferenced_next_active_generation(*next_generation)) {
    stm32h750_tf_fs_unlock();
    return LARGE_DBC_ACTIVE_STM32_IO_FAILED;
  }
  *available = slot != DBC_MANIFEST_RECOVERY_NONE;
  if (!*available) {
    stm32h750_tf_fs_unlock();
    return LARGE_DBC_ACTIVE_STM32_OK;
  }
  if (!build_active_paths(selected.generation, &g_active_paths)) {
    stm32h750_tf_fs_unlock();
    return LARGE_DBC_ACTIVE_STM32_INVALID_ARGUMENT;
  }
  g_active_runtime_status = build_prepared_runtime(
    &g_active_paths, &selected, true, true, selected.generation,
    rules, rule_count,
    runtime_snapshot);
  if (g_active_runtime_status != DBC_SELECTED_RUNTIME_OK) {
    const LargeDbcActiveStm32Status failed =
      map_runtime_status(g_active_runtime_status);
    stm32h750_tf_fs_unlock();
    return failed;
  }
  result->active = active_descriptor_from_manifest(&selected, &g_selection);
  result->runtime_slot = runtime_snapshot->prepared_slot;
  g_active_runtime_status = publish(publish_context, runtime_snapshot,
                                    &result->active, result->runtime_slot);
  if (g_active_runtime_status != DBC_SELECTED_RUNTIME_OK) {
    if (runtime_snapshot->has_prepared) {
      (void)dbc_selected_runtime_discard_prepared(runtime_snapshot);
    }
    const LargeDbcActiveStm32Status failed =
      map_runtime_status(g_active_runtime_status);
    stm32h750_tf_fs_unlock();
    return failed;
  }
  stm32h750_tf_fs_unlock();
  return LARGE_DBC_ACTIVE_STM32_OK;
}

const char *stm32h750_large_dbc_active_status_string(
  LargeDbcActiveStm32Status status) {
  switch (status) {
    case LARGE_DBC_ACTIVE_STM32_OK: return "ok";
    case LARGE_DBC_ACTIVE_STM32_INVALID_ARGUMENT: return "invalid_argument";
    case LARGE_DBC_ACTIVE_STM32_LOCK_FAILED: return "lock_failed";
    case LARGE_DBC_ACTIVE_STM32_NOT_FOUND: return "not_found";
    case LARGE_DBC_ACTIVE_STM32_VERIFY_FAILED: return "verify_failed";
    case LARGE_DBC_ACTIVE_STM32_GENERATION_EXHAUSTED:
      return "generation_exhausted";
    case LARGE_DBC_ACTIVE_STM32_LOGGING_ACTIVE: return "logging_active";
    case LARGE_DBC_ACTIVE_STM32_RULE_KEY_MISSING: return "rule_key_missing";
    case LARGE_DBC_ACTIVE_STM32_RULE_DEFINITION_CONFLICT:
      return "rule_definition_conflict";
    case LARGE_DBC_ACTIVE_STM32_RUNTIME_FAILED: return "runtime_failed";
    case LARGE_DBC_ACTIVE_STM32_COLLISION: return "collision";
    case LARGE_DBC_ACTIVE_STM32_IO_FAILED: return "io_failed";
    case LARGE_DBC_ACTIVE_STM32_MANIFEST_FAILED: return "manifest_failed";
    default: return "unknown";
  }
}

const char *stm32h750_large_dbc_candidate_status_string(
  LargeDbcCandidateStm32Status status) {
  switch (status) {
    case LARGE_DBC_CANDIDATE_STM32_OK: return "ok";
    case LARGE_DBC_CANDIDATE_STM32_INVALID_ARGUMENT: return "invalid_argument";
    case LARGE_DBC_CANDIDATE_STM32_LOCK_FAILED: return "lock_failed";
    case LARGE_DBC_CANDIDATE_STM32_PATH_FAILED: return "path_failed";
    case LARGE_DBC_CANDIDATE_STM32_IO_FAILED: return "io_failed";
    case LARGE_DBC_CANDIDATE_STM32_COLLISION: return "collision";
    case LARGE_DBC_CANDIDATE_STM32_SOURCE_MISMATCH: return "source_mismatch";
    case LARGE_DBC_CANDIDATE_STM32_PARSE_FAILED: return "parse_failed";
    case LARGE_DBC_CANDIDATE_STM32_INDEX_FAILED: return "index_failed";
    case LARGE_DBC_CANDIDATE_STM32_FORMAT_FAILED: return "format_failed";
    case LARGE_DBC_CANDIDATE_STM32_VERIFY_FAILED: return "verify_failed";
    case LARGE_DBC_CANDIDATE_STM32_MANIFEST_FAILED: return "manifest_failed";
    case LARGE_DBC_CANDIDATE_STM32_GENERATION_EXHAUSTED:
      return "generation_exhausted";
    case LARGE_DBC_CANDIDATE_STM32_NOT_FOUND: return "not_found";
    case LARGE_DBC_CANDIDATE_STM32_QUERY_FAILED: return "query_failed";
    case LARGE_DBC_CANDIDATE_STM32_TOKEN_MISMATCH: return "token_mismatch";
    case LARGE_DBC_CANDIDATE_STM32_WRITE_BLOCKED: return "write_blocked";
    case LARGE_DBC_CANDIDATE_STM32_SELECTION_FAILED:
      return "selection_failed";
    default: return "unknown";
  }
}
