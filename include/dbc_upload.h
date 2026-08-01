#ifndef DBC_UPLOAD_H
#define DBC_UPLOAD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "large_dbc_contract.h"

#define DBC_UPLOAD_HTTP_HEADER_MAX_BYTES 1536u

/*
 * The sink binds begin(generation) to upload.<GEN16>.tmp.  finalize must make
 * that one tmp object durable (for example sync + close); it must not rename,
 * publish, or modify candidate/active manifests.  abort closes and cleans up,
 * or deliberately retains the unreferenced tmp for diagnosis.
 */
typedef bool (*DbcUploadSinkBegin)(void *context, uint64_t generation,
                                   uint32_t content_length);
typedef bool (*DbcUploadSinkWrite)(void *context, const uint8_t *data,
                                   uint32_t size);
typedef bool (*DbcUploadSinkFinalize)(void *context);
typedef void (*DbcUploadSinkAbort)(void *context);

typedef struct {
  void *context;
  DbcUploadSinkBegin begin;
  DbcUploadSinkWrite write;
  DbcUploadSinkFinalize finalize;
  DbcUploadSinkAbort abort;
} DbcUploadSink;

typedef enum {
  DBC_UPLOAD_STATUS_OK = 0,
  DBC_UPLOAD_STATUS_INVALID_ARGUMENT,
  DBC_UPLOAD_STATUS_INVALID_STATE,
  DBC_UPLOAD_STATUS_LENGTH_REQUIRED,
  DBC_UPLOAD_STATUS_LENGTH_LIMIT,
  DBC_UPLOAD_STATUS_CHUNK_LIMIT,
  DBC_UPLOAD_STATUS_LENGTH_MISMATCH,
  DBC_UPLOAD_STATUS_IDLE_TIMEOUT,
  DBC_UPLOAD_STATUS_TOTAL_TIMEOUT,
  DBC_UPLOAD_STATUS_CANCELLED,
  DBC_UPLOAD_STATUS_SINK_BEGIN_FAILED,
  DBC_UPLOAD_STATUS_SINK_WRITE_FAILED,
  DBC_UPLOAD_STATUS_SINK_FINALIZE_FAILED
} DbcUploadStatus;

typedef enum {
  DBC_UPLOAD_STATE_IDLE = 0,
  DBC_UPLOAD_STATE_RECEIVING,
  DBC_UPLOAD_STATE_COMPLETE,
  DBC_UPLOAD_STATE_FAILED
} DbcUploadPhase;

typedef struct {
  uint64_t generation;
  uint32_t source_size;
  uint32_t source_crc32;
} DbcUploadResult;

typedef struct {
  DbcUploadSink sink;
  uint64_t generation;
  uint32_t expected_size;
  uint32_t received_size;
  uint32_t crc32_state;
  uint32_t started_ms;
  uint32_t last_activity_ms;
  DbcUploadStatus status;
  DbcUploadPhase phase;
  bool sink_started;
  bool sink_aborted;
} DbcUploadState;

typedef enum {
  DBC_UPLOAD_HTTP_OK = 0,
  DBC_UPLOAD_HTTP_INCOMPLETE,
  DBC_UPLOAD_HTTP_HEADER_TOO_LARGE,
  DBC_UPLOAD_HTTP_INVALID_REQUEST_LINE,
  DBC_UPLOAD_HTTP_INVALID_HEADER,
  DBC_UPLOAD_HTTP_CONTENT_LENGTH_MISSING,
  DBC_UPLOAD_HTTP_CONTENT_LENGTH_DUPLICATE,
  DBC_UPLOAD_HTTP_CONTENT_LENGTH_INVALID,
  DBC_UPLOAD_HTTP_CONTENT_LENGTH_LIMIT,
  DBC_UPLOAD_HTTP_TRANSFER_ENCODING_REJECTED,
  DBC_UPLOAD_HTTP_CONTENT_TYPE_MISSING,
  DBC_UPLOAD_HTTP_CONTENT_TYPE_REJECTED,
  DBC_UPLOAD_HTTP_BODY_OVERRUN
} DbcUploadHttpStatus;

typedef struct {
  uint32_t content_length;
  size_t header_bytes;
  size_t body_bytes;
} DbcUploadHttpHeader;

/*
 * Parses only a complete bounded header already present in data.  data may
 * also contain the first body bytes; their offset/count are returned without
 * copying or retaining them.  The function never requires the complete body.
 */
DbcUploadHttpStatus dbc_upload_http_parse_header(
  const uint8_t *data,
  size_t size,
  DbcUploadHttpHeader *header);

DbcUploadStatus dbc_upload_init(DbcUploadState *upload,
                                const DbcUploadSink *sink);

DbcUploadStatus dbc_upload_start(DbcUploadState *upload,
                                 uint64_t generation,
                                 uint32_t content_length,
                                 uint32_t now_ms);

DbcUploadStatus dbc_upload_feed(DbcUploadState *upload,
                                const uint8_t *data,
                                size_t size,
                                uint32_t now_ms);

DbcUploadStatus dbc_upload_check_timeout(DbcUploadState *upload,
                                         uint32_t now_ms);

DbcUploadStatus dbc_upload_finish(DbcUploadState *upload,
                                  uint32_t now_ms,
                                  DbcUploadResult *result);

DbcUploadStatus dbc_upload_cancel(DbcUploadState *upload);

const char *dbc_upload_status_string(DbcUploadStatus status);

const char *dbc_upload_http_status_string(DbcUploadHttpStatus status);

bool dbc_upload_format_tmp_path(char *path, size_t path_size,
                                uint64_t generation);

#endif
