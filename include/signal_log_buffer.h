#ifndef SIGNAL_LOG_BUFFER_H
#define SIGNAL_LOG_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "signal_cache.h"

typedef struct {
  char *data;
  size_t capacity;
  size_t length;
} SignalLogBuffer;

typedef enum {
  SIGNAL_LOG_BUFFER_OK = 0,
  SIGNAL_LOG_BUFFER_SERIALIZE_ERROR = 1,
  SIGNAL_LOG_BUFFER_FULL = 2,
} SignalLogBufferResult;

typedef enum {
  SIGNAL_LOG_PATH_DEFAULT = 0,
  SIGNAL_LOG_PATH_RECOVERY = 1,
} SignalLogPathMode;

#define SIGNAL_LOG_FILE_NOT_FOUND 4

void signal_log_buffer_init(SignalLogBuffer *buffer, char *data, size_t capacity);
SignalLogBufferResult signal_log_buffer_append_snapshot(SignalLogBuffer *buffer,
                                                         const SignalCacheEntry *entries,
                                                         size_t entry_count,
                                                         bool include_header);
bool signal_log_buffer_should_flush(const SignalLogBuffer *buffer,
                                    size_t flush_threshold,
                                    uint32_t last_flush_ms,
                                    uint32_t now_ms,
                                    uint32_t flush_interval_ms);
SignalLogPathMode signal_log_select_path(int default_file_size_result);
void signal_log_buffer_clear(SignalLogBuffer *buffer);

#endif
