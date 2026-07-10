#include "signal_log_buffer.h"

#include <string.h>

#include "signal_csv.h"

void signal_log_buffer_init(SignalLogBuffer *buffer, char *data, size_t capacity) {
  if (buffer == NULL) {
    return;
  }
  buffer->data = data;
  buffer->capacity = capacity;
  buffer->length = 0u;
  if (data != NULL && capacity != 0u) {
    data[0] = '\0';
  }
}

SignalLogBufferResult signal_log_buffer_append_snapshot(SignalLogBuffer *buffer,
                                                         const SignalCacheEntry *entries,
                                                         size_t entry_count,
                                                         bool include_header) {
  char rows[512];
  const size_t length = signal_csv_build_rows(entries,
                                               entry_count,
                                               include_header,
                                               rows,
                                               sizeof(rows));
  if (buffer == NULL || buffer->data == NULL || buffer->capacity == 0u || length == 0u) {
    return SIGNAL_LOG_BUFFER_SERIALIZE_ERROR;
  }
  if (length >= buffer->capacity - buffer->length) {
    return SIGNAL_LOG_BUFFER_FULL;
  }
  memcpy(&buffer->data[buffer->length], rows, length);
  buffer->length += length;
  buffer->data[buffer->length] = '\0';
  return SIGNAL_LOG_BUFFER_OK;
}

bool signal_log_buffer_should_flush(const SignalLogBuffer *buffer,
                                    size_t flush_threshold,
                                    uint32_t last_flush_ms,
                                    uint32_t now_ms,
                                    uint32_t flush_interval_ms) {
  if (buffer == NULL || buffer->length == 0u) {
    return false;
  }
  return buffer->length >= flush_threshold || (uint32_t)(now_ms - last_flush_ms) >= flush_interval_ms;
}

SignalLogPathMode signal_log_select_path(int default_file_size_result) {
  return default_file_size_result == 0 || default_file_size_result == SIGNAL_LOG_FILE_NOT_FOUND
           ? SIGNAL_LOG_PATH_DEFAULT
           : SIGNAL_LOG_PATH_RECOVERY;
}

void signal_log_buffer_clear(SignalLogBuffer *buffer) {
  if (buffer == NULL) {
    return;
  }
  buffer->length = 0u;
  if (buffer->data != NULL && buffer->capacity != 0u) {
    buffer->data[0] = '\0';
  }
}
