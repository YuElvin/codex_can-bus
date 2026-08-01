#include "dbc_upload.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_TRUE(condition) do { if (!(condition)) { \
  fprintf(stderr, "ASSERT failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
  return false; } } while (0)

typedef struct {
  uint8_t *bytes;
  uint32_t capacity;
  uint32_t size;
  uint64_t tmp_generation;
  uint64_t published_generation;
  uint32_t expected_size;
  unsigned begin_count;
  unsigned write_count;
  unsigned finalize_count;
  unsigned abort_count;
  unsigned fail_write_call;
  bool fail_begin;
  bool fail_finalize;
  bool tmp_ready;
} MockSink;

static bool mock_begin(void *context, uint64_t generation,
                       uint32_t content_length) {
  MockSink *sink = context;
  ++sink->begin_count;
  sink->tmp_generation = generation;
  sink->expected_size = content_length;
  sink->size = 0u;
  sink->tmp_ready = false;
  return !sink->fail_begin;
}

static bool mock_write(void *context, const uint8_t *data, uint32_t size) {
  MockSink *sink = context;
  ++sink->write_count;
  if (sink->fail_write_call == sink->write_count ||
      size > sink->capacity - sink->size) {
    return false;
  }
  memcpy(sink->bytes + sink->size, data, size);
  sink->size += size;
  return true;
}

static bool mock_finalize(void *context) {
  MockSink *sink = context;
  ++sink->finalize_count;
  if (sink->fail_finalize) {
    return false;
  }
  sink->tmp_ready = true;
  return true;
}

static void mock_abort(void *context) {
  MockSink *sink = context;
  ++sink->abort_count;
  sink->tmp_ready = false;
}

static bool mock_init(MockSink *mock, uint32_t capacity,
                      DbcUploadState *upload) {
  memset(mock, 0, sizeof(*mock));
  mock->bytes = malloc(capacity == 0u ? 1u : capacity);
  ASSERT_TRUE(mock->bytes != NULL);
  mock->capacity = capacity;
  mock->published_generation = UINT64_C(0x1122334455667788);
  const DbcUploadSink sink = {
    .context = mock,
    .begin = mock_begin,
    .write = mock_write,
    .finalize = mock_finalize,
    .abort = mock_abort
  };
  ASSERT_TRUE(dbc_upload_init(upload, &sink) == DBC_UPLOAD_STATUS_OK);
  return true;
}

static void mock_release(MockSink *mock) {
  free(mock->bytes);
  memset(mock, 0, sizeof(*mock));
}

static uint32_t reference_crc32(const uint8_t *data, size_t size) {
  uint32_t crc = UINT32_C(0xFFFFFFFF);
  for (size_t i = 0u; i < size; ++i) {
    crc ^= data[i];
    for (unsigned bit = 0u; bit < 8u; ++bit) {
      const uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
      crc = (crc >> 1u) ^ (UINT32_C(0xEDB88320) & mask);
    }
  }
  return crc ^ UINT32_C(0xFFFFFFFF);
}

static DbcUploadResult sentinel_result(void) {
  const DbcUploadResult result = {
    .generation = UINT64_C(0xDEADBEEFDEADBEEF),
    .source_size = UINT32_C(0xA5A5A5A5),
    .source_crc32 = UINT32_C(0x5A5A5A5A)
  };
  return result;
}

static bool result_is_sentinel(const DbcUploadResult *result) {
  return result->generation == UINT64_C(0xDEADBEEFDEADBEEF) &&
         result->source_size == UINT32_C(0xA5A5A5A5) &&
         result->source_crc32 == UINT32_C(0x5A5A5A5A);
}

static bool test_length_contract(void) {
  MockSink mock;
  DbcUploadState upload;
  ASSERT_TRUE(mock_init(&mock, 1u, &upload));
  ASSERT_TRUE(dbc_upload_start(&upload, 1u, 0u, 0u) ==
              DBC_UPLOAD_STATUS_LENGTH_REQUIRED);
  ASSERT_TRUE(dbc_upload_start(&upload, 1u,
                               LARGE_DBC_SOURCE_MAX_BYTES + 1u, 0u) ==
              DBC_UPLOAD_STATUS_LENGTH_LIMIT);
  ASSERT_TRUE(dbc_upload_start(&upload, 0u, 1u, 0u) ==
              DBC_UPLOAD_STATUS_INVALID_ARGUMENT);
  ASSERT_TRUE(upload.phase == DBC_UPLOAD_STATE_IDLE);
  ASSERT_TRUE(mock.begin_count == 0u && mock.abort_count == 0u);
  ASSERT_TRUE(mock.published_generation == UINT64_C(0x1122334455667788));
  mock_release(&mock);
  return true;
}

static bool test_tmp_path_contract(void) {
  char path[40];
  ASSERT_TRUE(dbc_upload_format_tmp_path(path, sizeof(path),
                                        UINT64_C(0x123456789ABCDEF0)));
  ASSERT_TRUE(strcmp(path, "/dbc/upload.123456789ABCDEF0.tmp") == 0);
  ASSERT_TRUE(dbc_upload_format_tmp_path(path, 32u, 1u) == false);
  ASSERT_TRUE(dbc_upload_format_tmp_path(path, sizeof(path), 0u) == false);
  ASSERT_TRUE(dbc_upload_format_tmp_path(NULL, sizeof(path), 1u) == false);
  return true;
}

static bool test_one_byte_and_crc_golden(void) {
  MockSink mock;
  DbcUploadState upload;
  DbcUploadResult result = sentinel_result();
  ASSERT_TRUE(mock_init(&mock, 9u, &upload));
  const uint8_t one = 0xA5u;
  ASSERT_TRUE(dbc_upload_start(&upload, 7u, 1u, 10u) == DBC_UPLOAD_STATUS_OK);
  ASSERT_TRUE(dbc_upload_feed(&upload, &one, 1u, 11u) == DBC_UPLOAD_STATUS_OK);
  ASSERT_TRUE(dbc_upload_finish(&upload, 12u, &result) == DBC_UPLOAD_STATUS_OK);
  ASSERT_TRUE(result.generation == 7u && result.source_size == 1u &&
              result.source_crc32 == reference_crc32(&one, 1u));
  ASSERT_TRUE(mock.tmp_generation == 7u && mock.expected_size == 1u &&
              mock.tmp_ready && mock.finalize_count == 1u && mock.abort_count == 0u);
  ASSERT_TRUE(mock.published_generation == UINT64_C(0x1122334455667788));
  mock_release(&mock);

  static const uint8_t golden[] = "123456789";
  ASSERT_TRUE(mock_init(&mock, sizeof(golden) - 1u, &upload));
  ASSERT_TRUE(dbc_upload_start(&upload, 8u, sizeof(golden) - 1u, 0u) ==
              DBC_UPLOAD_STATUS_OK);
  ASSERT_TRUE(dbc_upload_feed(&upload, golden, 4u, 1u) == DBC_UPLOAD_STATUS_OK);
  ASSERT_TRUE(dbc_upload_feed(&upload, golden + 4u, 5u, 2u) == DBC_UPLOAD_STATUS_OK);
  ASSERT_TRUE(dbc_upload_finish(&upload, 3u, &result) == DBC_UPLOAD_STATUS_OK);
  ASSERT_TRUE(result.source_crc32 == UINT32_C(0xCBF43926));
  mock_release(&mock);
  return true;
}

static bool test_exact_max_arbitrary_chunks(void) {
  const uint32_t size = LARGE_DBC_SOURCE_MAX_BYTES;
  uint8_t *source = malloc(size);
  ASSERT_TRUE(source != NULL);
  for (uint32_t i = 0u; i < size; ++i) {
    source[i] = (uint8_t)((i * 37u + i / 251u) & 0xFFu);
  }
  MockSink mock;
  DbcUploadState upload;
  DbcUploadResult result = sentinel_result();
  ASSERT_TRUE(mock_init(&mock, size, &upload));
  ASSERT_TRUE(dbc_upload_start(&upload, UINT64_C(0x123456789ABCDEF0),
                               size, 100u) == DBC_UPLOAD_STATUS_OK);
  static const size_t chunks[] = {1u, 137u, 512u, 7u, 256u, 31u};
  uint32_t offset = 0u;
  size_t chunk_index = 0u;
  uint32_t now = 100u;
  while (offset < size) {
    size_t amount = chunks[chunk_index % (sizeof(chunks) / sizeof(chunks[0]))];
    const uint32_t remaining = size - offset;
    if (amount > remaining) {
      amount = remaining;
    }
    ++now;
    ASSERT_TRUE(dbc_upload_feed(&upload, source + offset, amount, now) ==
                DBC_UPLOAD_STATUS_OK);
    offset += (uint32_t)amount;
    ++chunk_index;
  }
  ASSERT_TRUE(dbc_upload_finish(&upload, now + 1u, &result) ==
              DBC_UPLOAD_STATUS_OK);
  ASSERT_TRUE(result.generation == UINT64_C(0x123456789ABCDEF0) &&
              result.source_size == size &&
              result.source_crc32 == reference_crc32(source, size));
  ASSERT_TRUE(mock.size == size && memcmp(mock.bytes, source, size) == 0);
  ASSERT_TRUE(mock.tmp_ready && mock.published_generation ==
              UINT64_C(0x1122334455667788));
  mock_release(&mock);
  free(source);
  return true;
}

static bool test_short_overrun_and_chunk_limit(void) {
  static const uint8_t bytes[513] = {1u};
  MockSink mock;
  DbcUploadState upload;
  DbcUploadResult result = sentinel_result();

  ASSERT_TRUE(mock_init(&mock, sizeof(bytes), &upload));
  ASSERT_TRUE(dbc_upload_start(&upload, 1u, 2u, 0u) == DBC_UPLOAD_STATUS_OK);
  ASSERT_TRUE(dbc_upload_feed(&upload, bytes, 1u, 1u) == DBC_UPLOAD_STATUS_OK);
  ASSERT_TRUE(dbc_upload_finish(&upload, 2u, &result) ==
              DBC_UPLOAD_STATUS_LENGTH_MISMATCH);
  ASSERT_TRUE(result_is_sentinel(&result) && mock.abort_count == 1u &&
              mock.published_generation == UINT64_C(0x1122334455667788));
  mock_release(&mock);

  ASSERT_TRUE(mock_init(&mock, sizeof(bytes), &upload));
  ASSERT_TRUE(dbc_upload_start(&upload, 2u, 1u, 0u) == DBC_UPLOAD_STATUS_OK);
  ASSERT_TRUE(dbc_upload_feed(&upload, bytes, 2u, 1u) ==
              DBC_UPLOAD_STATUS_LENGTH_MISMATCH);
  ASSERT_TRUE(upload.received_size == 0u && mock.write_count == 0u &&
              mock.abort_count == 1u && result_is_sentinel(&result));
  mock_release(&mock);

  ASSERT_TRUE(mock_init(&mock, sizeof(bytes), &upload));
  ASSERT_TRUE(dbc_upload_start(&upload, 3u, sizeof(bytes), 0u) ==
              DBC_UPLOAD_STATUS_OK);
  ASSERT_TRUE(dbc_upload_feed(&upload, bytes, sizeof(bytes), 1u) ==
              DBC_UPLOAD_STATUS_CHUNK_LIMIT);
  ASSERT_TRUE(upload.received_size == 0u && mock.write_count == 0u &&
              mock.abort_count == 1u);
  mock_release(&mock);
  return true;
}

static bool test_timeouts_and_wrap(void) {
  uint8_t byte = 1u;
  MockSink mock;
  DbcUploadState upload;
  ASSERT_TRUE(mock_init(&mock, 32u, &upload));
  ASSERT_TRUE(dbc_upload_start(&upload, 1u, 1u, 100u) == DBC_UPLOAD_STATUS_OK);
  ASSERT_TRUE(dbc_upload_check_timeout(&upload, 5099u) == DBC_UPLOAD_STATUS_OK);
  ASSERT_TRUE(dbc_upload_check_timeout(&upload, 5100u) ==
              DBC_UPLOAD_STATUS_IDLE_TIMEOUT);
  ASSERT_TRUE(mock.abort_count == 1u);
  ASSERT_TRUE(dbc_upload_check_timeout(&upload, 5101u) ==
              DBC_UPLOAD_STATUS_INVALID_STATE);
  ASSERT_TRUE(mock.abort_count == 1u);
  mock_release(&mock);

  ASSERT_TRUE(mock_init(&mock, 32u, &upload));
  ASSERT_TRUE(dbc_upload_start(&upload, 2u, 30u, 0u) == DBC_UPLOAD_STATUS_OK);
  for (uint32_t i = 1u; i <= 24u; ++i) {
    ASSERT_TRUE(dbc_upload_feed(&upload, &byte, 1u, i * 4999u) ==
                DBC_UPLOAD_STATUS_OK);
  }
  ASSERT_TRUE(dbc_upload_check_timeout(&upload, 120000u) ==
              DBC_UPLOAD_STATUS_TOTAL_TIMEOUT);
  ASSERT_TRUE(mock.abort_count == 1u);
  mock_release(&mock);

  ASSERT_TRUE(mock_init(&mock, 1u, &upload));
  ASSERT_TRUE(dbc_upload_start(&upload, 3u, 1u, UINT32_MAX - 1000u) ==
              DBC_UPLOAD_STATUS_OK);
  ASSERT_TRUE(dbc_upload_feed(&upload, &byte, 1u, 100u) == DBC_UPLOAD_STATUS_OK);
  DbcUploadResult result = sentinel_result();
  ASSERT_TRUE(dbc_upload_finish(&upload, 101u, &result) == DBC_UPLOAD_STATUS_OK);
  mock_release(&mock);
  return true;
}

static bool test_sink_failures_cancel_and_result_isolation(void) {
  const uint8_t byte = 1u;
  MockSink mock;
  DbcUploadState upload;
  DbcUploadResult result = sentinel_result();

  ASSERT_TRUE(mock_init(&mock, 1u, &upload));
  mock.fail_begin = true;
  ASSERT_TRUE(dbc_upload_start(&upload, 1u, 1u, 0u) ==
              DBC_UPLOAD_STATUS_SINK_BEGIN_FAILED);
  ASSERT_TRUE(mock.abort_count == 1u && result_is_sentinel(&result));
  mock_release(&mock);

  ASSERT_TRUE(mock_init(&mock, 1u, &upload));
  mock.fail_write_call = 1u;
  ASSERT_TRUE(dbc_upload_start(&upload, 2u, 1u, 0u) == DBC_UPLOAD_STATUS_OK);
  ASSERT_TRUE(dbc_upload_feed(&upload, &byte, 1u, 1u) ==
              DBC_UPLOAD_STATUS_SINK_WRITE_FAILED);
  ASSERT_TRUE(upload.received_size == 0u && mock.abort_count == 1u &&
              result_is_sentinel(&result));
  mock_release(&mock);

  ASSERT_TRUE(mock_init(&mock, 1u, &upload));
  mock.fail_finalize = true;
  ASSERT_TRUE(dbc_upload_start(&upload, 3u, 1u, 0u) == DBC_UPLOAD_STATUS_OK);
  ASSERT_TRUE(dbc_upload_feed(&upload, &byte, 1u, 1u) == DBC_UPLOAD_STATUS_OK);
  ASSERT_TRUE(dbc_upload_finish(&upload, 2u, &result) ==
              DBC_UPLOAD_STATUS_SINK_FINALIZE_FAILED);
  ASSERT_TRUE(mock.abort_count == 1u && !mock.tmp_ready &&
              result_is_sentinel(&result));
  ASSERT_TRUE(mock.published_generation == UINT64_C(0x1122334455667788));
  mock_release(&mock);

  ASSERT_TRUE(mock_init(&mock, 1u, &upload));
  ASSERT_TRUE(dbc_upload_start(&upload, 4u, 1u, 0u) == DBC_UPLOAD_STATUS_OK);
  ASSERT_TRUE(dbc_upload_cancel(&upload) == DBC_UPLOAD_STATUS_CANCELLED);
  ASSERT_TRUE(dbc_upload_cancel(&upload) == DBC_UPLOAD_STATUS_INVALID_STATE);
  ASSERT_TRUE(mock.abort_count == 1u && result_is_sentinel(&result));
  mock_release(&mock);
  return true;
}

static DbcUploadHttpStatus parse_http(const char *request,
                                      DbcUploadHttpHeader *header) {
  return dbc_upload_http_parse_header((const uint8_t *)request,
                                      strlen(request), header);
}

static bool test_http_header_success_and_body_boundary(void) {
  static const char request[] =
    "POST /api/dbc/upload HTTP/1.1\r\n"
    "Host: 192.168.1.100\r\n"
    "content-length:\t 4 \t\r\n"
    "CONTENT-TYPE: TEXT/PLAIN\r\n"
    "X-DBC-Name: \xE6\xB5\x8B\xE8\xAF\x95.dbc\r\n"
    "Connection: close\r\n\r\nABC";
  DbcUploadHttpHeader header = {0u, 0u, 0u};
  ASSERT_TRUE(parse_http(request, &header) == DBC_UPLOAD_HTTP_OK);
  ASSERT_TRUE(header.content_length == 4u && header.body_bytes == 3u);
  ASSERT_TRUE(header.header_bytes + header.body_bytes == strlen(request));

  static const char header_only[] =
    "POST /api/dbc/upload HTTP/1.1\r\n"
    "Content-Length: 262144\r\n"
    "Content-Type: text/plain\r\n\r\n";
  ASSERT_TRUE(parse_http(header_only, &header) == DBC_UPLOAD_HTTP_OK);
  ASSERT_TRUE(header.content_length == LARGE_DBC_SOURCE_MAX_BYTES &&
              header.body_bytes == 0u && header.header_bytes == strlen(header_only));
  return true;
}

static bool test_http_header_length_and_transfer_rejections(void) {
  DbcUploadHttpHeader header = {
    .content_length = 77u,
    .header_bytes = 88u,
    .body_bytes = 99u
  };
  static const char missing[] =
    "POST /api/dbc/upload HTTP/1.1\r\nContent-Type: text/plain\r\n\r\n";
  static const char duplicate_same[] =
    "POST /api/dbc/upload HTTP/1.1\r\nContent-Length: 1\r\n"
    "Content-Length: 1\r\nContent-Type: text/plain\r\n\r\n";
  static const char duplicate_conflict[] =
    "POST /api/dbc/upload HTTP/1.1\r\nContent-Length: 1\r\n"
    "Content-Length: 2\r\nContent-Type: text/plain\r\n\r\n";
  static const char zero[] =
    "POST /api/dbc/upload HTTP/1.1\r\nContent-Length: 0\r\n"
    "Content-Type: text/plain\r\n\r\n";
  static const char too_large[] =
    "POST /api/dbc/upload HTTP/1.1\r\nContent-Length: 262145\r\n"
    "Content-Type: text/plain\r\n\r\n";
  static const char signed_length[] =
    "POST /api/dbc/upload HTTP/1.1\r\nContent-Length: +1\r\n"
    "Content-Type: text/plain\r\n\r\n";
  static const char suffix_length[] =
    "POST /api/dbc/upload HTTP/1.1\r\nContent-Length: 1x\r\n"
    "Content-Type: text/plain\r\n\r\n";
  static const char overflow[] =
    "POST /api/dbc/upload HTTP/1.1\r\nContent-Length: 99999999999999999999\r\n"
    "Content-Type: text/plain\r\n\r\n";
  static const char chunked[] =
    "POST /api/dbc/upload HTTP/1.1\r\nContent-Length: 1\r\n"
    "Content-Type: text/plain\r\nTransfer-Encoding: chunked\r\n\r\n";
  static const char transfer_identity[] =
    "POST /api/dbc/upload HTTP/1.1\r\nContent-Length: 1\r\n"
    "Content-Type: text/plain\r\nTransfer-Encoding: identity\r\n\r\n";
  ASSERT_TRUE(parse_http(missing, &header) ==
              DBC_UPLOAD_HTTP_CONTENT_LENGTH_MISSING);
  ASSERT_TRUE(parse_http(duplicate_same, &header) ==
              DBC_UPLOAD_HTTP_CONTENT_LENGTH_DUPLICATE);
  ASSERT_TRUE(parse_http(duplicate_conflict, &header) ==
              DBC_UPLOAD_HTTP_CONTENT_LENGTH_DUPLICATE);
  ASSERT_TRUE(parse_http(zero, &header) == DBC_UPLOAD_HTTP_CONTENT_LENGTH_INVALID);
  ASSERT_TRUE(parse_http(too_large, &header) ==
              DBC_UPLOAD_HTTP_CONTENT_LENGTH_LIMIT);
  ASSERT_TRUE(parse_http(signed_length, &header) ==
              DBC_UPLOAD_HTTP_CONTENT_LENGTH_INVALID);
  ASSERT_TRUE(parse_http(suffix_length, &header) ==
              DBC_UPLOAD_HTTP_CONTENT_LENGTH_INVALID);
  ASSERT_TRUE(parse_http(overflow, &header) ==
              DBC_UPLOAD_HTTP_CONTENT_LENGTH_INVALID);
  ASSERT_TRUE(parse_http(chunked, &header) ==
              DBC_UPLOAD_HTTP_TRANSFER_ENCODING_REJECTED);
  ASSERT_TRUE(parse_http(transfer_identity, &header) ==
              DBC_UPLOAD_HTTP_TRANSFER_ENCODING_REJECTED);
  ASSERT_TRUE(header.content_length == 77u && header.header_bytes == 88u &&
              header.body_bytes == 99u);
  return true;
}

static bool test_http_header_route_type_and_bounds(void) {
  DbcUploadHttpHeader header = {0u, 0u, 0u};
  static const char wrong_method[] =
    "PUT /api/dbc/upload HTTP/1.1\r\nContent-Length: 1\r\n"
    "Content-Type: text/plain\r\n\r\n";
  static const char wrong_path[] =
    "POST /api/dbc/upload?q=x HTTP/1.1\r\nContent-Length: 1\r\n"
    "Content-Type: text/plain\r\n\r\n";
  static const char wrong_version[] =
    "POST /api/dbc/upload HTTP/1.0\r\nContent-Length: 1\r\n"
    "Content-Type: text/plain\r\n\r\n";
  static const char missing_type[] =
    "POST /api/dbc/upload HTTP/1.1\r\nContent-Length: 1\r\n\r\n";
  static const char multipart[] =
    "POST /api/dbc/upload HTTP/1.1\r\nContent-Length: 1\r\n"
    "Content-Type: multipart/form-data; boundary=x\r\n\r\n";
  static const char other_type[] =
    "POST /api/dbc/upload HTTP/1.1\r\nContent-Length: 1\r\n"
    "Content-Type: application/octet-stream\r\n\r\n";
  static const char duplicate_type[] =
    "POST /api/dbc/upload HTTP/1.1\r\nContent-Length: 1\r\n"
    "Content-Type: text/plain\r\nContent-Type: text/plain\r\n\r\n";
  static const char incomplete[] =
    "POST /api/dbc/upload HTTP/1.1\r\nContent-Length: 1\r\n";
  static const char overrun[] =
    "POST /api/dbc/upload HTTP/1.1\r\nContent-Length: 1\r\n"
    "Content-Type: text/plain\r\n\r\nAB";
  ASSERT_TRUE(parse_http(wrong_method, &header) ==
              DBC_UPLOAD_HTTP_INVALID_REQUEST_LINE);
  ASSERT_TRUE(parse_http(wrong_path, &header) ==
              DBC_UPLOAD_HTTP_INVALID_REQUEST_LINE);
  ASSERT_TRUE(parse_http(wrong_version, &header) ==
              DBC_UPLOAD_HTTP_INVALID_REQUEST_LINE);
  ASSERT_TRUE(parse_http(missing_type, &header) ==
              DBC_UPLOAD_HTTP_CONTENT_TYPE_MISSING);
  ASSERT_TRUE(parse_http(multipart, &header) ==
              DBC_UPLOAD_HTTP_CONTENT_TYPE_REJECTED);
  ASSERT_TRUE(parse_http(other_type, &header) ==
              DBC_UPLOAD_HTTP_CONTENT_TYPE_REJECTED);
  ASSERT_TRUE(parse_http(duplicate_type, &header) ==
              DBC_UPLOAD_HTTP_CONTENT_TYPE_REJECTED);
  ASSERT_TRUE(parse_http(incomplete, &header) == DBC_UPLOAD_HTTP_INCOMPLETE);
  ASSERT_TRUE(parse_http(overrun, &header) == DBC_UPLOAD_HTTP_BODY_OVERRUN);

  uint8_t bounded[DBC_UPLOAD_HTTP_HEADER_MAX_BYTES];
  memset(bounded, 'A', sizeof(bounded));
  ASSERT_TRUE(dbc_upload_http_parse_header(bounded, sizeof(bounded), &header) ==
              DBC_UPLOAD_HTTP_HEADER_TOO_LARGE);

  static const char prefix[] =
    "POST /api/dbc/upload HTTP/1.1\r\nContent-Length: 1\r\n"
    "Content-Type: text/plain\r\nX-Pad: ";
  static const char suffix[] = "\r\n\r\n";
  const size_t pad = sizeof(bounded) - (sizeof(prefix) - 1u) -
                     (sizeof(suffix) - 1u);
  memcpy(bounded, prefix, sizeof(prefix) - 1u);
  memset(bounded + sizeof(prefix) - 1u, 'P', pad);
  memcpy(bounded + sizeof(prefix) - 1u + pad, suffix, sizeof(suffix) - 1u);
  ASSERT_TRUE(dbc_upload_http_parse_header(bounded, sizeof(bounded), &header) ==
              DBC_UPLOAD_HTTP_OK);
  ASSERT_TRUE(header.header_bytes == sizeof(bounded) && header.body_bytes == 0u);
  return true;
}

int main(void) {
  if (!test_length_contract() || !test_tmp_path_contract() ||
      !test_one_byte_and_crc_golden() ||
      !test_exact_max_arbitrary_chunks() ||
      !test_short_overrun_and_chunk_limit() || !test_timeouts_and_wrap() ||
      !test_sink_failures_cancel_and_result_isolation() ||
      !test_http_header_success_and_body_boundary() ||
      !test_http_header_length_and_transfer_rejections() ||
      !test_http_header_route_type_and_bounds()) {
    return 1;
  }
  puts("dbc upload stream tests passed");
  return 0;
}
