#include "dbc_upload.h"

#include <stdio.h>
#include <string.h>

_Static_assert(LARGE_DBC_UPLOAD_CHUNK_BYTES <= LARGE_DBC_SOURCE_MAX_BYTES,
               "upload chunk must fit source limit");
_Static_assert(sizeof(DbcUploadState) <= 96u,
               "portable upload state unexpectedly grew");

static uint32_t crc32_update(uint32_t state, const uint8_t *data, size_t size) {
  while (size-- != 0u) {
    state ^= *data++;
    for (unsigned bit = 0u; bit < 8u; ++bit) {
      state = (state >> 1u) ^ ((state & 1u) != 0u ?
        LARGE_DBC_CRC32_REFLECTED_POLYNOMIAL : 0u);
    }
  }
  return state;
}

static DbcUploadStatus fail_upload(DbcUploadState *upload,
                                   DbcUploadStatus status) {
  if (upload->sink_started && !upload->sink_aborted) {
    upload->sink.abort(upload->sink.context);
    upload->sink_aborted = true;
  }
  upload->status = status;
  upload->phase = DBC_UPLOAD_STATE_FAILED;
  return status;
}

static bool sink_valid(const DbcUploadSink *sink) {
  return sink != NULL && sink->begin != NULL && sink->write != NULL &&
         sink->finalize != NULL && sink->abort != NULL;
}

bool dbc_upload_format_tmp_path(char *path, size_t path_size,
                                uint64_t generation) {
  if (path == NULL || path_size == 0u || generation == 0u) {
    return false;
  }
  const int written = snprintf(path,
                               path_size,
                               "/dbc/upload.%08lX%08lX.tmp",
                               (unsigned long)(generation >> 32u),
                               (unsigned long)(uint32_t)generation);
  return written > 0 && (size_t)written < path_size;
}

static uint8_t ascii_lower(uint8_t value) {
  return value >= (uint8_t)'A' && value <= (uint8_t)'Z' ?
    (uint8_t)(value + ((uint8_t)'a' - (uint8_t)'A')) : value;
}

static bool span_equals_case(const uint8_t *data, size_t size,
                             const char *expected) {
  size_t i = 0u;
  while (i < size && expected[i] != '\0') {
    if (ascii_lower(data[i]) != ascii_lower((uint8_t)expected[i])) {
      return false;
    }
    ++i;
  }
  return i == size && expected[i] == '\0';
}

static void trim_ows(const uint8_t **data, size_t *size) {
  while (*size != 0u && (**data == (uint8_t)' ' || **data == (uint8_t)'\t')) {
    ++*data;
    --*size;
  }
  while (*size != 0u && ((*data)[*size - 1u] == (uint8_t)' ' ||
                         (*data)[*size - 1u] == (uint8_t)'\t')) {
    --*size;
  }
}

static DbcUploadHttpStatus parse_content_length(const uint8_t *data,
                                                size_t size,
                                                uint32_t *content_length) {
  trim_ows(&data, &size);
  if (size == 0u) {
    return DBC_UPLOAD_HTTP_CONTENT_LENGTH_INVALID;
  }
  uint32_t value = 0u;
  for (size_t i = 0u; i < size; ++i) {
    if (data[i] < (uint8_t)'0' || data[i] > (uint8_t)'9') {
      return DBC_UPLOAD_HTTP_CONTENT_LENGTH_INVALID;
    }
    const uint32_t digit = (uint32_t)(data[i] - (uint8_t)'0');
    if (value > (UINT32_MAX - digit) / 10u) {
      return DBC_UPLOAD_HTTP_CONTENT_LENGTH_INVALID;
    }
    value = value * 10u + digit;
  }
  if (value == 0u) {
    return DBC_UPLOAD_HTTP_CONTENT_LENGTH_INVALID;
  }
  if (value > LARGE_DBC_SOURCE_MAX_BYTES) {
    return DBC_UPLOAD_HTTP_CONTENT_LENGTH_LIMIT;
  }
  *content_length = value;
  return DBC_UPLOAD_HTTP_OK;
}

DbcUploadHttpStatus dbc_upload_http_parse_header(
  const uint8_t *data,
  size_t size,
  DbcUploadHttpHeader *header) {
  static const char request_line[] = "POST /api/dbc/upload HTTP/1.1";
  if (data == NULL || header == NULL) {
    return DBC_UPLOAD_HTTP_INVALID_HEADER;
  }

  size_t header_bytes = 0u;
  const size_t scan_size = size < DBC_UPLOAD_HTTP_HEADER_MAX_BYTES ?
    size : DBC_UPLOAD_HTTP_HEADER_MAX_BYTES;
  for (size_t i = 0u; i + 3u < scan_size; ++i) {
    if (data[i] == (uint8_t)'\r' && data[i + 1u] == (uint8_t)'\n' &&
        data[i + 2u] == (uint8_t)'\r' && data[i + 3u] == (uint8_t)'\n') {
      header_bytes = i + 4u;
      break;
    }
  }
  if (header_bytes == 0u) {
    return size >= DBC_UPLOAD_HTTP_HEADER_MAX_BYTES ?
      DBC_UPLOAD_HTTP_HEADER_TOO_LARGE : DBC_UPLOAD_HTTP_INCOMPLETE;
  }

  for (size_t i = 0u; i < header_bytes; ++i) {
    const uint8_t value = data[i];
    if (value == 0u || (value < 0x20u && value != (uint8_t)'\r' &&
                        value != (uint8_t)'\n' && value != (uint8_t)'\t') ||
        value == 0x7Fu) {
      return DBC_UPLOAD_HTTP_INVALID_HEADER;
    }
  }

  size_t line_end = 0u;
  while (line_end + 1u < header_bytes &&
         !(data[line_end] == (uint8_t)'\r' &&
           data[line_end + 1u] == (uint8_t)'\n')) {
    ++line_end;
  }
  if (line_end != sizeof(request_line) - 1u ||
      memcmp(data, request_line, sizeof(request_line) - 1u) != 0) {
    return DBC_UPLOAD_HTTP_INVALID_REQUEST_LINE;
  }

  bool saw_content_length = false;
  bool saw_content_type = false;
  uint32_t content_length = 0u;
  size_t cursor = line_end + 2u;
  while (cursor + 1u < header_bytes) {
    if (data[cursor] == (uint8_t)'\r' &&
        data[cursor + 1u] == (uint8_t)'\n') {
      cursor += 2u;
      break;
    }
    size_t end = cursor;
    while (end + 1u < header_bytes &&
           !(data[end] == (uint8_t)'\r' && data[end + 1u] == (uint8_t)'\n')) {
      ++end;
    }
    if (end + 1u >= header_bytes || end == cursor) {
      return DBC_UPLOAD_HTTP_INVALID_HEADER;
    }
    size_t colon = cursor;
    while (colon < end && data[colon] != (uint8_t)':') {
      if (data[colon] <= 0x20u || data[colon] >= 0x7Fu) {
        return DBC_UPLOAD_HTTP_INVALID_HEADER;
      }
      ++colon;
    }
    if (colon == cursor || colon == end) {
      return DBC_UPLOAD_HTTP_INVALID_HEADER;
    }
    const uint8_t *value = data + colon + 1u;
    size_t value_size = end - colon - 1u;
    trim_ows(&value, &value_size);
    for (size_t i = 0u; i < value_size; ++i) {
      if (value[i] == (uint8_t)'\r' || value[i] == (uint8_t)'\n') {
        return DBC_UPLOAD_HTTP_INVALID_HEADER;
      }
    }

    const size_t name_size = colon - cursor;
    if (span_equals_case(data + cursor, name_size, "Content-Length")) {
      if (saw_content_length) {
        return DBC_UPLOAD_HTTP_CONTENT_LENGTH_DUPLICATE;
      }
      saw_content_length = true;
      const DbcUploadHttpStatus length_status =
        parse_content_length(value, value_size, &content_length);
      if (length_status != DBC_UPLOAD_HTTP_OK) {
        return length_status;
      }
    } else if (span_equals_case(data + cursor, name_size, "Transfer-Encoding")) {
      return DBC_UPLOAD_HTTP_TRANSFER_ENCODING_REJECTED;
    } else if (span_equals_case(data + cursor, name_size, "Content-Type")) {
      if (saw_content_type ||
          !span_equals_case(value, value_size, "text/plain")) {
        return DBC_UPLOAD_HTTP_CONTENT_TYPE_REJECTED;
      }
      saw_content_type = true;
    }
    cursor = end + 2u;
  }
  if (cursor != header_bytes) {
    return DBC_UPLOAD_HTTP_INVALID_HEADER;
  }
  if (!saw_content_length) {
    return DBC_UPLOAD_HTTP_CONTENT_LENGTH_MISSING;
  }
  if (!saw_content_type) {
    return DBC_UPLOAD_HTTP_CONTENT_TYPE_MISSING;
  }
  const size_t body_bytes = size - header_bytes;
  if (body_bytes > content_length) {
    return DBC_UPLOAD_HTTP_BODY_OVERRUN;
  }
  const DbcUploadHttpHeader parsed = {
    .content_length = content_length,
    .header_bytes = header_bytes,
    .body_bytes = body_bytes
  };
  *header = parsed;
  return DBC_UPLOAD_HTTP_OK;
}

DbcUploadStatus dbc_upload_init(DbcUploadState *upload,
                                const DbcUploadSink *sink) {
  if (upload == NULL || !sink_valid(sink)) {
    return DBC_UPLOAD_STATUS_INVALID_ARGUMENT;
  }
  memset(upload, 0, sizeof(*upload));
  upload->sink = *sink;
  upload->status = DBC_UPLOAD_STATUS_OK;
  upload->phase = DBC_UPLOAD_STATE_IDLE;
  return DBC_UPLOAD_STATUS_OK;
}

DbcUploadStatus dbc_upload_start(DbcUploadState *upload,
                                 uint64_t generation,
                                 uint32_t content_length,
                                 uint32_t now_ms) {
  if (upload == NULL) {
    return DBC_UPLOAD_STATUS_INVALID_ARGUMENT;
  }
  if (upload->phase != DBC_UPLOAD_STATE_IDLE) {
    return DBC_UPLOAD_STATUS_INVALID_STATE;
  }
  if (generation == 0u) {
    upload->status = DBC_UPLOAD_STATUS_INVALID_ARGUMENT;
    return upload->status;
  }
  if (content_length == 0u) {
    upload->status = DBC_UPLOAD_STATUS_LENGTH_REQUIRED;
    return upload->status;
  }
  if (content_length > LARGE_DBC_SOURCE_MAX_BYTES) {
    upload->status = DBC_UPLOAD_STATUS_LENGTH_LIMIT;
    return upload->status;
  }

  upload->generation = generation;
  upload->expected_size = content_length;
  upload->received_size = 0u;
  upload->crc32_state = LARGE_DBC_CRC32_INITIAL_VALUE;
  upload->started_ms = now_ms;
  upload->last_activity_ms = now_ms;
  upload->sink_aborted = false;
  upload->sink_started = true;
  if (!upload->sink.begin(upload->sink.context, generation, content_length)) {
    return fail_upload(upload, DBC_UPLOAD_STATUS_SINK_BEGIN_FAILED);
  }
  upload->status = DBC_UPLOAD_STATUS_OK;
  upload->phase = DBC_UPLOAD_STATE_RECEIVING;
  return DBC_UPLOAD_STATUS_OK;
}

DbcUploadStatus dbc_upload_check_timeout(DbcUploadState *upload,
                                         uint32_t now_ms) {
  if (upload == NULL) {
    return DBC_UPLOAD_STATUS_INVALID_ARGUMENT;
  }
  if (upload->phase != DBC_UPLOAD_STATE_RECEIVING) {
    return DBC_UPLOAD_STATUS_INVALID_STATE;
  }
  if ((uint32_t)(now_ms - upload->started_ms) >=
      LARGE_DBC_UPLOAD_TOTAL_TIMEOUT_MS) {
    return fail_upload(upload, DBC_UPLOAD_STATUS_TOTAL_TIMEOUT);
  }
  if ((uint32_t)(now_ms - upload->last_activity_ms) >=
      LARGE_DBC_UPLOAD_IDLE_TIMEOUT_MS) {
    return fail_upload(upload, DBC_UPLOAD_STATUS_IDLE_TIMEOUT);
  }
  return DBC_UPLOAD_STATUS_OK;
}

DbcUploadStatus dbc_upload_feed(DbcUploadState *upload,
                                const uint8_t *data,
                                size_t size,
                                uint32_t now_ms) {
  if (upload == NULL) {
    return DBC_UPLOAD_STATUS_INVALID_ARGUMENT;
  }
  if (upload->phase != DBC_UPLOAD_STATE_RECEIVING) {
    return DBC_UPLOAD_STATUS_INVALID_STATE;
  }
  DbcUploadStatus status = dbc_upload_check_timeout(upload, now_ms);
  if (status != DBC_UPLOAD_STATUS_OK) {
    return status;
  }
  if (data == NULL || size == 0u) {
    return fail_upload(upload, DBC_UPLOAD_STATUS_INVALID_ARGUMENT);
  }
  if (size > LARGE_DBC_UPLOAD_CHUNK_BYTES) {
    return fail_upload(upload, DBC_UPLOAD_STATUS_CHUNK_LIMIT);
  }
  if (size > (size_t)(upload->expected_size - upload->received_size)) {
    return fail_upload(upload, DBC_UPLOAD_STATUS_LENGTH_MISMATCH);
  }
  if (!upload->sink.write(upload->sink.context, data, (uint32_t)size)) {
    return fail_upload(upload, DBC_UPLOAD_STATUS_SINK_WRITE_FAILED);
  }
  upload->crc32_state = crc32_update(upload->crc32_state, data, size);
  upload->received_size += (uint32_t)size;
  upload->last_activity_ms = now_ms;
  return DBC_UPLOAD_STATUS_OK;
}

DbcUploadStatus dbc_upload_finish(DbcUploadState *upload,
                                  uint32_t now_ms,
                                  DbcUploadResult *result) {
  if (upload == NULL || result == NULL) {
    return DBC_UPLOAD_STATUS_INVALID_ARGUMENT;
  }
  if (upload->phase != DBC_UPLOAD_STATE_RECEIVING) {
    return DBC_UPLOAD_STATUS_INVALID_STATE;
  }
  DbcUploadStatus status = dbc_upload_check_timeout(upload, now_ms);
  if (status != DBC_UPLOAD_STATUS_OK) {
    return status;
  }
  if (upload->received_size != upload->expected_size) {
    return fail_upload(upload, DBC_UPLOAD_STATUS_LENGTH_MISMATCH);
  }
  if (!upload->sink.finalize(upload->sink.context)) {
    return fail_upload(upload, DBC_UPLOAD_STATUS_SINK_FINALIZE_FAILED);
  }

  const DbcUploadResult completed = {
    .generation = upload->generation,
    .source_size = upload->received_size,
    .source_crc32 = upload->crc32_state ^ LARGE_DBC_CRC32_FINAL_XOR
  };
  *result = completed;
  upload->status = DBC_UPLOAD_STATUS_OK;
  upload->phase = DBC_UPLOAD_STATE_COMPLETE;
  return DBC_UPLOAD_STATUS_OK;
}

DbcUploadStatus dbc_upload_cancel(DbcUploadState *upload) {
  if (upload == NULL) {
    return DBC_UPLOAD_STATUS_INVALID_ARGUMENT;
  }
  if (upload->phase != DBC_UPLOAD_STATE_RECEIVING) {
    return DBC_UPLOAD_STATUS_INVALID_STATE;
  }
  return fail_upload(upload, DBC_UPLOAD_STATUS_CANCELLED);
}

const char *dbc_upload_status_string(DbcUploadStatus status) {
  switch (status) {
    case DBC_UPLOAD_STATUS_OK: return "ok";
    case DBC_UPLOAD_STATUS_INVALID_ARGUMENT: return "invalid_argument";
    case DBC_UPLOAD_STATUS_INVALID_STATE: return "invalid_state";
    case DBC_UPLOAD_STATUS_LENGTH_REQUIRED: return "length_required";
    case DBC_UPLOAD_STATUS_LENGTH_LIMIT: return "length_limit";
    case DBC_UPLOAD_STATUS_CHUNK_LIMIT: return "chunk_limit";
    case DBC_UPLOAD_STATUS_LENGTH_MISMATCH: return "length_mismatch";
    case DBC_UPLOAD_STATUS_IDLE_TIMEOUT: return "idle_timeout";
    case DBC_UPLOAD_STATUS_TOTAL_TIMEOUT: return "total_timeout";
    case DBC_UPLOAD_STATUS_CANCELLED: return "cancelled";
    case DBC_UPLOAD_STATUS_SINK_BEGIN_FAILED: return "sink_begin_failed";
    case DBC_UPLOAD_STATUS_SINK_WRITE_FAILED: return "sink_write_failed";
    case DBC_UPLOAD_STATUS_SINK_FINALIZE_FAILED: return "sink_finalize_failed";
    default: return "unknown";
  }
}

const char *dbc_upload_http_status_string(DbcUploadHttpStatus status) {
  switch (status) {
    case DBC_UPLOAD_HTTP_OK: return "ok";
    case DBC_UPLOAD_HTTP_INCOMPLETE: return "incomplete";
    case DBC_UPLOAD_HTTP_HEADER_TOO_LARGE: return "header_too_large";
    case DBC_UPLOAD_HTTP_INVALID_REQUEST_LINE: return "invalid_request_line";
    case DBC_UPLOAD_HTTP_INVALID_HEADER: return "invalid_header";
    case DBC_UPLOAD_HTTP_CONTENT_LENGTH_MISSING: return "content_length_missing";
    case DBC_UPLOAD_HTTP_CONTENT_LENGTH_DUPLICATE: return "content_length_duplicate";
    case DBC_UPLOAD_HTTP_CONTENT_LENGTH_INVALID: return "content_length_invalid";
    case DBC_UPLOAD_HTTP_CONTENT_LENGTH_LIMIT: return "content_length_limit";
    case DBC_UPLOAD_HTTP_TRANSFER_ENCODING_REJECTED:
      return "transfer_encoding_rejected";
    case DBC_UPLOAD_HTTP_CONTENT_TYPE_MISSING: return "content_type_missing";
    case DBC_UPLOAD_HTTP_CONTENT_TYPE_REJECTED: return "content_type_rejected";
    case DBC_UPLOAD_HTTP_BODY_OVERRUN: return "body_overrun";
    default: return "unknown";
  }
}
