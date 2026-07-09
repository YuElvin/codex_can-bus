#if defined(CAN_BUS_USE_STM32_HAL) || defined(STM32H750xx)

#include "main.h"
#include "dbc_parser.h"
#include "platform/stm32h750_bringup.h"

#include <stdio.h>
#include <string.h>

extern SPI_HandleTypeDef hspi2;

volatile uint32_t g_w5500_init_result = 0xffffffffu;
volatile uint32_t g_w5500_version = 0xffffffffu;
volatile uint32_t g_w5500_phycfgr = 0xffffffffu;
volatile uint32_t g_w5500_link_up = 0u;
volatile uint32_t g_w5500_network_configured = 0u;
volatile uint32_t g_w5500_http_status = 0xffffffffu;
volatile uint32_t g_w5500_http_socket_sr = 0xffffffffu;
volatile uint32_t g_w5500_http_request_count = 0u;
volatile uint32_t g_w5500_http_last_path = 0u;
volatile uint32_t g_w5500_http_last_code = 0u;
volatile uint32_t g_w5500_http_last_rx_size = 0u;
volatile uint32_t g_w5500_http_last_tx_size = 0u;
volatile uint32_t g_w5500_http_error_count = 0u;
volatile uint32_t g_w5500_http_static_count = 0u;
volatile uint32_t g_w5500_http_static_read_result = 0xffffffffu;
volatile uint32_t g_w5500_http_static_file_size = 0u;
volatile uint32_t g_w5500_http_static_bytes_sent = 0u;
volatile uint32_t g_w5500_http_dbc_upload_count = 0u;
volatile uint32_t g_w5500_http_dbc_upload_result = 0xffffffffu;
volatile uint32_t g_w5500_http_dbc_upload_bytes = 0u;
volatile uint32_t g_w5500_http_dbc_upload_lines = 0u;
volatile uint32_t g_w5500_http_dbc_upload_messages = 0u;
volatile uint32_t g_w5500_http_dbc_upload_signals = 0u;
volatile uint32_t g_w5500_http_dbc_upload_skipped = 0u;
volatile uint32_t g_w5500_http_dbc_upload_errors = 0u;
volatile uint32_t g_w5500_http_dbc_candidate_load_result = 0xffffffffu;
volatile uint32_t g_w5500_http_dbc_candidate_read_len = 0u;
volatile uint32_t g_w5500_http_dbc_candidate_valid = 0u;
volatile uint32_t g_w5500_http_dbc_active_count = 0u;
volatile uint32_t g_w5500_http_dbc_active_result = 0xffffffffu;
volatile uint32_t g_w5500_http_dbc_active_bytes = 0u;
volatile uint32_t g_w5500_http_dbc_active_lines = 0u;
volatile uint32_t g_w5500_http_dbc_active_messages = 0u;
volatile uint32_t g_w5500_http_dbc_active_signals = 0u;
volatile uint32_t g_w5500_http_dbc_active_skipped = 0u;
volatile uint32_t g_w5500_http_dbc_active_errors = 0u;
volatile uint32_t g_w5500_http_dbc_active_valid = 0u;

extern volatile int g_tf_card_bringup_status;
extern volatile int g_w5500_bringup_status;
extern volatile int g_w25q128_bringup_status;
extern volatile int g_can2_analyzer_bringup_status;
extern volatile uint32_t g_freertos_task_started;
extern volatile uint32_t g_freertos_loop_count;
extern volatile uint32_t g_freertos_bringup_complete;
extern volatile uint32_t g_can2_tx_count;
extern volatile uint32_t g_can2_rx_count;
extern volatile uint32_t g_can2_error_count;
extern volatile uint32_t g_can2_bus_off;
extern volatile uint32_t g_can2_tec;
extern volatile uint32_t g_can2_rec;
extern volatile uint32_t g_can2_send_result;
extern volatile uint32_t g_can2_poll_count;
extern volatile uint32_t g_w25q128_jedec_id;

#define W5500_S0_REG_BLOCK 0x01u
#define W5500_S0_TX_BLOCK 0x02u
#define W5500_S0_RX_BLOCK 0x03u
#define W5500_S0_MR 0x0000u
#define W5500_S0_CR 0x0001u
#define W5500_S0_IR 0x0002u
#define W5500_S0_SR 0x0003u
#define W5500_S0_PORT 0x0004u
#define W5500_S0_TX_FSR 0x0020u
#define W5500_S0_TX_WR 0x0024u
#define W5500_S0_RX_RSR 0x0026u
#define W5500_S0_RX_RD 0x0028u
#define W5500_SOCKET_BUFFER_SIZE 2048u
#define W5500_SOCKET_BUFFER_MASK (W5500_SOCKET_BUFFER_SIZE - 1u)

#define W5500_S0_CR_OPEN 0x01u
#define W5500_S0_CR_LISTEN 0x02u
#define W5500_S0_CR_DISCON 0x08u
#define W5500_S0_CR_CLOSE 0x10u
#define W5500_S0_CR_SEND 0x20u
#define W5500_S0_CR_RECV 0x40u

#define W5500_S0_IR_SENDOK 0x10u
#define W5500_S0_IR_TIMEOUT 0x08u

#define W5500_S0_MR_TCP 0x01u
#define W5500_S0_SR_CLOSED 0x00u
#define W5500_S0_SR_INIT 0x13u
#define W5500_S0_SR_LISTEN 0x14u
#define W5500_S0_SR_ESTABLISHED 0x17u
#define W5500_S0_SR_CLOSE_WAIT 0x1cu
#define W5500_HTTP_PORT 80u
#define W5500_HTTP_PATH_STATUS 1u
#define W5500_HTTP_PATH_CAN_STATUS 2u
#define W5500_HTTP_PATH_INDEX 3u
#define W5500_HTTP_PATH_DBC_UPLOAD 4u
#define W5500_HTTP_PATH_DBC_ACTIVE 5u
#define W5500_HTTP_STATIC_CHUNK_SIZE 512u
#define W5500_HTTP_REQUEST_BUFFER_SIZE 1536u
#define W5500_HTTP_UPLOAD_BODY_MAX 1024u
#define W5500_HTTP_DBC_UPLOAD_TMP_PATH "/dbc/upload.write.tmp"
#define W5500_HTTP_DBC_ACTIVE_TMP_PATH "/dbc/active.write.tmp"
#define W5500_HTTP_DBC_CANDIDATE_PATH "/dbc/candidate.dbc"
#define W5500_HTTP_DBC_CANDIDATE_BACKUP_PATH "/dbc/candidate.prev.dbc"
#define W5500_HTTP_DBC_ACTIVE_PATH "/dbc/active.dbc"
#define W5500_HTTP_DBC_ACTIVE_BACKUP_PATH "/dbc/active.prev.dbc"
#define W5500_HTTP_STATIC_OK 0
#define W5500_HTTP_STATIC_SEND_ERROR 1
#define W5500_HTTP_STATIC_NOT_FOUND 2
#define W5500_HTTP_HANDLE_OK 0
#define W5500_HTTP_HANDLE_ERROR 1
#define W5500_HTTP_HANDLE_WAIT 2

typedef struct {
  size_t bytes;
  size_t lines;
  size_t messages;
  size_t signals;
  size_t skipped;
  size_t errors;
} DbcUploadReport;

static Stm32W5500Context g_w5500_ctx;
static W5500Port g_w5500_port;
static uint8_t g_w5500_bound;
static char g_http_request_buffer[W5500_HTTP_REQUEST_BUFFER_SIZE];
static char g_http_response_body[384];
static uint8_t g_http_static_chunk[W5500_HTTP_STATIC_CHUNK_SIZE];
static char g_http_dbc_candidate_buffer[W5500_HTTP_UPLOAD_BODY_MAX + 1u];
static DbcDatabase g_http_dbc_candidate_db;

static W5500Result s0_read_u8(uint16_t address, uint8_t *value) {
  return w5500_port_read_block(&g_w5500_port, W5500_S0_REG_BLOCK, address, value, 1u);
}

static W5500Result s0_write_u8(uint16_t address, uint8_t value) {
  return w5500_port_write_block(&g_w5500_port, W5500_S0_REG_BLOCK, address, &value, 1u);
}

static W5500Result s0_read_u16(uint16_t address, uint16_t *value) {
  uint8_t data[2] = {0};
  W5500Result result = w5500_port_read_block(&g_w5500_port, W5500_S0_REG_BLOCK, address, data, sizeof(data));
  if (result == W5500_OK) {
    *value = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
  }
  return result;
}

static W5500Result s0_write_u16(uint16_t address, uint16_t value) {
  const uint8_t data[2] = {(uint8_t)(value >> 8), (uint8_t)value};
  return w5500_port_write_block(&g_w5500_port, W5500_S0_REG_BLOCK, address, data, sizeof(data));
}

static W5500Result s0_command(uint8_t command) {
  if (s0_write_u8(W5500_S0_CR, command) != W5500_OK) {
    return W5500_ERROR;
  }
  for (uint32_t i = 0u; i < 1000u; ++i) {
    uint8_t value = 0xffu;
    if (s0_read_u8(W5500_S0_CR, &value) != W5500_OK) {
      return W5500_ERROR;
    }
    if (value == 0u) {
      return W5500_OK;
    }
    if ((i % 100u) == 0u) {
      g_w5500_port.ops->delay_ms(g_w5500_port.ctx, 1u);
    }
  }
  return W5500_ERROR;
}

static W5500Result socket_buffer_read(uint8_t block, uint16_t ptr, uint8_t *data, size_t len) {
  const uint16_t offset = (uint16_t)(ptr & W5500_SOCKET_BUFFER_MASK);
  size_t first = W5500_SOCKET_BUFFER_SIZE - offset;
  if (first > len) {
    first = len;
  }
  if (w5500_port_read_block(&g_w5500_port, block, offset, data, first) != W5500_OK) {
    return W5500_ERROR;
  }
  if (first < len &&
      w5500_port_read_block(&g_w5500_port, block, 0u, &data[first], len - first) != W5500_OK) {
    return W5500_ERROR;
  }
  return W5500_OK;
}

static W5500Result socket_buffer_write(uint8_t block, uint16_t ptr, const uint8_t *data, size_t len) {
  const uint16_t offset = (uint16_t)(ptr & W5500_SOCKET_BUFFER_MASK);
  size_t first = W5500_SOCKET_BUFFER_SIZE - offset;
  if (first > len) {
    first = len;
  }
  if (w5500_port_write_block(&g_w5500_port, block, offset, data, first) != W5500_OK) {
    return W5500_ERROR;
  }
  if (first < len &&
      w5500_port_write_block(&g_w5500_port, block, 0u, &data[first], len - first) != W5500_OK) {
    return W5500_ERROR;
  }
  return W5500_OK;
}

static int http_close_socket(void) {
  (void)s0_command(W5500_S0_CR_CLOSE);
  (void)s0_write_u8(W5500_S0_IR, 0x1fu);
  return 0;
}

static int http_open_listener(void) {
  uint8_t sr = 0u;
  (void)http_close_socket();
  if (s0_write_u8(W5500_S0_MR, W5500_S0_MR_TCP) != W5500_OK ||
      s0_write_u16(W5500_S0_PORT, W5500_HTTP_PORT) != W5500_OK ||
      s0_command(W5500_S0_CR_OPEN) != W5500_OK ||
      s0_read_u8(W5500_S0_SR, &sr) != W5500_OK ||
      sr != W5500_S0_SR_INIT ||
      s0_command(W5500_S0_CR_LISTEN) != W5500_OK ||
      s0_read_u8(W5500_S0_SR, &sr) != W5500_OK ||
      sr != W5500_S0_SR_LISTEN) {
    g_w5500_http_status = 2u;
    g_w5500_http_error_count++;
    return 1;
  }
  g_w5500_http_socket_sr = sr;
  g_w5500_http_status = 0u;
  return 0;
}

static bool request_path_is(const char *request, const char *method, const char *path) {
  const size_t method_len = strlen(method);
  const size_t path_len = strlen(path);
  if (strncmp(request, method, method_len) != 0 || request[method_len] != ' ') {
    return false;
  }
  const char *actual = request + method_len + 1u;
  return strncmp(actual, path, path_len) == 0 && (actual[path_len] == ' ' || actual[path_len] == '?');
}

static const char *http_status_text(uint16_t code) {
  switch (code) {
    case 200u:
      return "OK";
    case 400u:
      return "Bad Request";
    case 404u:
      return "Not Found";
    case 405u:
      return "Method Not Allowed";
    case 413u:
      return "Payload Too Large";
    case 500u:
      return "Internal Server Error";
    default:
      return "Error";
  }
}

static size_t build_status_body(char *body, size_t len) {
  return (size_t)snprintf(body,
                          len,
                          "{\"ok\":true,\"data\":{\"rtos\":{\"started\":%lu,\"ready\":%lu,\"loop\":%lu},"
                          "\"w5500\":{\"status\":%d,\"link\":%lu,\"version\":%lu,\"phycfgr\":%lu},"
                          "\"tf\":{\"status\":%d},\"qspi\":{\"status\":%d,\"jedec\":%lu}}}",
                          (unsigned long)g_freertos_task_started,
                          (unsigned long)g_freertos_bringup_complete,
                          (unsigned long)g_freertos_loop_count,
                          g_w5500_bringup_status,
                          (unsigned long)g_w5500_link_up,
                          (unsigned long)g_w5500_version,
                          (unsigned long)g_w5500_phycfgr,
                          g_tf_card_bringup_status,
                          g_w25q128_bringup_status,
                          (unsigned long)g_w25q128_jedec_id);
}

static size_t build_can_status_body(char *body, size_t len) {
  return (size_t)snprintf(body,
                          len,
                          "{\"ok\":true,\"data\":{\"can2\":{\"status\":%d,\"tx\":%lu,\"rx\":%lu,"
                          "\"errors\":%lu,\"busOff\":%lu,\"tec\":%lu,\"rec\":%lu,"
                          "\"sendResult\":%lu,\"poll\":%lu}}}",
                          g_can2_analyzer_bringup_status,
                          (unsigned long)g_can2_tx_count,
                          (unsigned long)g_can2_rx_count,
                          (unsigned long)g_can2_error_count,
                          (unsigned long)g_can2_bus_off,
                          (unsigned long)g_can2_tec,
                          (unsigned long)g_can2_rec,
                          (unsigned long)g_can2_send_result,
                          (unsigned long)g_can2_poll_count);
}

static size_t build_not_found_body(char *body, size_t len) {
  return (size_t)snprintf(body, len, "{\"ok\":false,\"error\":{\"code\":\"not_found\",\"message\":\"not found\"}}");
}

static size_t build_error_body(char *body, size_t len, const char *code, const char *message) {
  return (size_t)snprintf(body,
                          len,
                          "{\"ok\":false,\"error\":{\"code\":\"%s\",\"message\":\"%s\"}}",
                          code,
                          message);
}

static size_t build_dbc_upload_body(char *body, size_t len, const DbcUploadReport *report) {
  return (size_t)snprintf(body,
                          len,
                          "{\"ok\":true,\"data\":{\"candidate\":\"%s\",\"candidateBackup\":\"%s\","
                          "\"active\":\"%s\",\"activeBackup\":\"%s\",\"maxBytes\":%lu,\"bytes\":%lu,"
                          "\"lines\":%lu,\"messages\":%lu,\"signals\":%lu,"
                          "\"skipped\":%lu,\"errors\":%lu,\"valid\":%s}}",
                          W5500_HTTP_DBC_CANDIDATE_PATH,
                          W5500_HTTP_DBC_CANDIDATE_BACKUP_PATH,
                          W5500_HTTP_DBC_ACTIVE_PATH,
                          W5500_HTTP_DBC_ACTIVE_BACKUP_PATH,
                          (unsigned long)W5500_HTTP_UPLOAD_BODY_MAX,
                          (unsigned long)report->bytes,
                          (unsigned long)report->lines,
                          (unsigned long)report->messages,
                          (unsigned long)report->signals,
                          (unsigned long)report->skipped,
                          (unsigned long)report->errors,
                          report->errors == 0u ? "true" : "false");
}

static size_t build_dbc_active_body(char *body, size_t len, const DbcUploadReport *report) {
  return (size_t)snprintf(body,
                          len,
                          "{\"ok\":true,\"data\":{\"candidate\":\"%s\",\"active\":\"%s\","
                          "\"activeBackup\":\"%s\",\"bytes\":%lu,\"lines\":%lu,"
                          "\"messages\":%lu,\"signals\":%lu,\"skipped\":%lu,"
                          "\"errors\":%lu,\"valid\":%s,\"activated\":true}}",
                          W5500_HTTP_DBC_CANDIDATE_PATH,
                          W5500_HTTP_DBC_ACTIVE_PATH,
                          W5500_HTTP_DBC_ACTIVE_BACKUP_PATH,
                          (unsigned long)report->bytes,
                          (unsigned long)report->lines,
                          (unsigned long)report->messages,
                          (unsigned long)report->signals,
                          (unsigned long)report->skipped,
                          (unsigned long)report->errors,
                          report->errors == 0u ? "true" : "false");
}

static const char *skip_http_space(const char *text) {
  while (*text == ' ' || *text == '\t') {
    ++text;
  }
  return text;
}

static bool parse_decimal_size(const char *text, const char *end, size_t *value) {
  size_t result = 0u;
  bool has_digit = false;
  text = skip_http_space(text);
  while (text < end && *text >= '0' && *text <= '9') {
    result = (result * 10u) + (size_t)(*text - '0');
    has_digit = true;
    ++text;
  }
  if (!has_digit) {
    return false;
  }
  *value = result;
  return true;
}

static bool http_parse_content_length(const char *request, const char *header_end, size_t *content_length) {
  const char *line = request;
  static const char header_name[] = "Content-Length:";
  while (line < header_end) {
    const char *line_end = strstr(line, "\r\n");
    if (line_end == NULL || line_end > header_end) {
      line_end = header_end;
    }
    if ((size_t)(line_end - line) > sizeof(header_name) - 1u &&
        strncmp(line, header_name, sizeof(header_name) - 1u) == 0) {
      return parse_decimal_size(line + sizeof(header_name) - 1u, line_end, content_length);
    }
    if (line_end == header_end) {
      break;
    }
    line = line_end + 2u;
  }
  return false;
}

static int dbc_load_candidate_report(DbcUploadReport *report) {
  size_t read_len = 0u;
  size_t line_count = 0u;
  memset(report, 0, sizeof(*report));

  g_w5500_http_dbc_candidate_load_result =
    (uint32_t)stm32h750_tf_read_file_locked(W5500_HTTP_DBC_CANDIDATE_PATH,
                                            (uint8_t *)g_http_dbc_candidate_buffer,
                                            sizeof(g_http_dbc_candidate_buffer),
                                            &read_len);
  g_w5500_http_dbc_candidate_read_len = (uint32_t)read_len;
  if (g_w5500_http_dbc_candidate_load_result != 0u) {
    return 1;
  }
  if (read_len > W5500_HTTP_UPLOAD_BODY_MAX) {
    report->bytes = read_len;
    report->errors = 1u;
    g_w5500_http_dbc_candidate_valid = 0u;
    return 2;
  }

  g_http_dbc_candidate_buffer[read_len] = '\0';
  g_w5500_http_dbc_candidate_valid =
    dbc_parse_text(&g_http_dbc_candidate_db, g_http_dbc_candidate_buffer, read_len, &line_count) ? 1u : 0u;
  report->bytes = read_len;
  report->lines = line_count;
  report->messages = g_http_dbc_candidate_db.message_count;
  report->signals = g_http_dbc_candidate_db.signal_count;
  report->skipped = g_http_dbc_candidate_db.skipped_lines;
  report->errors = g_http_dbc_candidate_db.error_lines;
  return 0;
}

static int http_send_bytes(const uint8_t *data, size_t len) {
  uint16_t tx_free = 0u;
  uint16_t tx_wr = 0u;
  if (data == NULL || len == 0u || len > W5500_SOCKET_BUFFER_SIZE) {
    return 1;
  }

  for (uint32_t i = 0u; i < 1000u; ++i) {
    if (s0_read_u16(W5500_S0_TX_FSR, &tx_free) != W5500_OK) {
      return 1;
    }
    if (tx_free >= len) {
      break;
    }
    if ((i % 100u) == 0u) {
      g_w5500_port.ops->delay_ms(g_w5500_port.ctx, 1u);
    }
  }
  if (tx_free < len ||
      s0_read_u16(W5500_S0_TX_WR, &tx_wr) != W5500_OK ||
      socket_buffer_write(W5500_S0_TX_BLOCK, tx_wr, data, len) != W5500_OK ||
      s0_write_u16(W5500_S0_TX_WR, (uint16_t)(tx_wr + len)) != W5500_OK ||
      s0_command(W5500_S0_CR_SEND) != W5500_OK) {
    return 1;
  }

  for (uint32_t i = 0u; i < 2000u; ++i) {
    uint8_t ir = 0u;
    if (s0_read_u8(W5500_S0_IR, &ir) != W5500_OK) {
      return 1;
    }
    if ((ir & W5500_S0_IR_SENDOK) != 0u) {
      (void)s0_write_u8(W5500_S0_IR, W5500_S0_IR_SENDOK);
      g_w5500_http_last_tx_size += (uint32_t)len;
      return 0;
    }
    if ((ir & W5500_S0_IR_TIMEOUT) != 0u) {
      (void)s0_write_u8(W5500_S0_IR, W5500_S0_IR_TIMEOUT);
      return 1;
    }
    if ((i % 100u) == 0u) {
      g_w5500_port.ops->delay_ms(g_w5500_port.ctx, 1u);
    }
  }
  return 1;
}

static int http_send_header(uint16_t code, const char *content_type, size_t body_len) {
  char header[192];
  const char *status_text = http_status_text(code);
  const int header_len = snprintf(header,
                                  sizeof(header),
                                  "HTTP/1.1 %u %s\r\n"
                                  "Content-Type: %s\r\n"
                                  "Content-Length: %lu\r\n"
                                  "Connection: close\r\n"
                                  "\r\n",
                                  (unsigned int)code,
                                  status_text,
                                  content_type,
                                  (unsigned long)body_len);
  if (header_len <= 0 || (size_t)header_len >= sizeof(header)) {
    return 1;
  }
  return http_send_bytes((const uint8_t *)header, (size_t)header_len);
}

static int http_send_response(uint16_t code, const char *content_type, const char *body, size_t body_len) {
  g_w5500_http_last_tx_size = 0u;
  if (http_send_header(code, content_type, body_len) != 0) {
    return 1;
  }
  if (body_len == 0u) {
    return 0;
  }
  return http_send_bytes((const uint8_t *)body, body_len);
}

static int http_send_static_index(void) {
  size_t file_size = 0u;
  size_t offset = 0u;

  g_w5500_http_static_bytes_sent = 0u;
  g_w5500_http_static_read_result =
    (uint32_t)stm32h750_tf_file_size_locked("/www/index.html", &file_size);
  if (g_w5500_http_static_read_result != 0u || file_size == 0u) {
    return W5500_HTTP_STATIC_NOT_FOUND;
  }
  g_w5500_http_static_file_size = (uint32_t)file_size;
  g_w5500_http_last_tx_size = 0u;
  if (http_send_header(200u, "text/html; charset=utf-8", file_size) != 0) {
    return W5500_HTTP_STATIC_SEND_ERROR;
  }

  while (offset < file_size) {
    size_t chunk_len = file_size - offset;
    size_t read_len = 0u;
    if (chunk_len > sizeof(g_http_static_chunk)) {
      chunk_len = sizeof(g_http_static_chunk);
    }
    g_w5500_http_static_read_result =
      (uint32_t)stm32h750_tf_read_file_chunk_locked("/www/index.html",
                                                    offset,
                                                    g_http_static_chunk,
                                                    chunk_len,
                                                    &read_len);
    if (g_w5500_http_static_read_result != 0u || read_len == 0u || read_len > chunk_len) {
      return W5500_HTTP_STATIC_SEND_ERROR;
    }
    if (http_send_bytes(g_http_static_chunk, read_len) != 0) {
      return W5500_HTTP_STATIC_SEND_ERROR;
    }
    offset += read_len;
    g_w5500_http_static_bytes_sent = (uint32_t)offset;
  }
  g_w5500_http_static_count++;
  return W5500_HTTP_STATIC_OK;
}

static int http_consume_rx(uint16_t rx_rd, uint16_t rx_size) {
  return s0_write_u16(W5500_S0_RX_RD, (uint16_t)(rx_rd + rx_size)) == W5500_OK &&
         s0_command(W5500_S0_CR_RECV) == W5500_OK
           ? 0
           : 1;
}

static int http_send_json_error(uint16_t code, const char *error_code, const char *message) {
  char body[192];
  const size_t body_len = build_error_body(body, sizeof(body), error_code, message);
  if (body_len >= sizeof(body)) {
    return 1;
  }
  return http_send_response(code, "application/json", body, body_len);
}

static void http_record_request(uint32_t path_code, uint16_t code) {
  g_w5500_http_request_count++;
  g_w5500_http_last_path = path_code;
  g_w5500_http_last_code = code;
}

static int http_handle_dbc_upload(const uint8_t *body_start, size_t content_length) {
  DbcUploadReport report;

  g_w5500_http_dbc_upload_result =
    (uint32_t)stm32h750_tf_replace_file_with_backup_locked(W5500_HTTP_DBC_UPLOAD_TMP_PATH,
                                                           W5500_HTTP_DBC_CANDIDATE_PATH,
                                                           W5500_HTTP_DBC_CANDIDATE_BACKUP_PATH,
                                                           body_start,
                                                           content_length);
  if (g_w5500_http_dbc_upload_result != 0u) {
    http_record_request(W5500_HTTP_PATH_DBC_UPLOAD, 500u);
    return http_send_json_error(500u, "save_failed", "dbc tmp save failed");
  }

  if (dbc_load_candidate_report(&report) != 0) {
    http_record_request(W5500_HTTP_PATH_DBC_UPLOAD, 500u);
    return http_send_json_error(500u, "candidate_load_failed", "dbc candidate load failed");
  }

  g_w5500_http_dbc_upload_bytes = (uint32_t)report.bytes;
  g_w5500_http_dbc_upload_lines = (uint32_t)report.lines;
  g_w5500_http_dbc_upload_messages = (uint32_t)report.messages;
  g_w5500_http_dbc_upload_signals = (uint32_t)report.signals;
  g_w5500_http_dbc_upload_skipped = (uint32_t)report.skipped;
  g_w5500_http_dbc_upload_errors = (uint32_t)report.errors;

  const size_t response_len = build_dbc_upload_body(g_http_response_body, sizeof(g_http_response_body), &report);
  if (response_len >= sizeof(g_http_response_body)) {
    return 1;
  }
  g_w5500_http_dbc_upload_count++;
  http_record_request(W5500_HTTP_PATH_DBC_UPLOAD, 200u);
  return http_send_response(200u, "application/json", g_http_response_body, response_len);
}

static void dbc_record_active_report(const DbcUploadReport *report, uint32_t valid) {
  g_w5500_http_dbc_active_bytes = (uint32_t)report->bytes;
  g_w5500_http_dbc_active_lines = (uint32_t)report->lines;
  g_w5500_http_dbc_active_messages = (uint32_t)report->messages;
  g_w5500_http_dbc_active_signals = (uint32_t)report->signals;
  g_w5500_http_dbc_active_skipped = (uint32_t)report->skipped;
  g_w5500_http_dbc_active_errors = (uint32_t)report->errors;
  g_w5500_http_dbc_active_valid = valid;
}

static int http_handle_dbc_active(void) {
  DbcUploadReport report;
  if (dbc_load_candidate_report(&report) != 0) {
    g_w5500_http_dbc_active_result = 1u;
    http_record_request(W5500_HTTP_PATH_DBC_ACTIVE, 500u);
    return http_send_json_error(500u, "candidate_load_failed", "dbc candidate load failed");
  }

  if (report.errors != 0u) {
    g_w5500_http_dbc_active_result = 2u;
    dbc_record_active_report(&report, 0u);
    http_record_request(W5500_HTTP_PATH_DBC_ACTIVE, 400u);
    return http_send_json_error(400u, "candidate_invalid", "dbc candidate invalid");
  }

  g_w5500_http_dbc_active_result =
    (uint32_t)stm32h750_tf_replace_file_with_backup_locked(W5500_HTTP_DBC_ACTIVE_TMP_PATH,
                                                           W5500_HTTP_DBC_ACTIVE_PATH,
                                                           W5500_HTTP_DBC_ACTIVE_BACKUP_PATH,
                                                           (const uint8_t *)g_http_dbc_candidate_buffer,
                                                           report.bytes);
  if (g_w5500_http_dbc_active_result != 0u) {
    dbc_record_active_report(&report, 0u);
    http_record_request(W5500_HTTP_PATH_DBC_ACTIVE, 500u);
    return http_send_json_error(500u, "active_save_failed", "dbc active save failed");
  }

  dbc_record_active_report(&report, 1u);
  const size_t response_len = build_dbc_active_body(g_http_response_body, sizeof(g_http_response_body), &report);
  if (response_len >= sizeof(g_http_response_body)) {
    return 1;
  }
  g_w5500_http_dbc_active_count++;
  http_record_request(W5500_HTTP_PATH_DBC_ACTIVE, 200u);
  return http_send_response(200u, "application/json", g_http_response_body, response_len);
}

static int http_handle_request(uint16_t rx_size) {
  char *request = g_http_request_buffer;
  char *body = g_http_response_body;
  uint16_t rx_rd = 0u;
  size_t read_len = rx_size;
  if (read_len >= W5500_HTTP_REQUEST_BUFFER_SIZE) {
    read_len = W5500_HTTP_REQUEST_BUFFER_SIZE - 1u;
  }
  if (s0_read_u16(W5500_S0_RX_RD, &rx_rd) != W5500_OK ||
      socket_buffer_read(W5500_S0_RX_BLOCK, rx_rd, (uint8_t *)request, read_len) != W5500_OK) {
    return 1;
  }
  request[read_len] = '\0';
  g_w5500_http_last_rx_size = rx_size;

  const char *header_end = strstr(request, "\r\n\r\n");
  if (request_path_is(request, "POST", "/api/dbc/upload")) {
    if (header_end == NULL) {
      if (read_len + 1u < W5500_HTTP_REQUEST_BUFFER_SIZE) {
        return W5500_HTTP_HANDLE_WAIT;
      }
      if (http_consume_rx(rx_rd, rx_size) != 0) {
        return W5500_HTTP_HANDLE_ERROR;
      }
      http_record_request(W5500_HTTP_PATH_DBC_UPLOAD, 400u);
      return http_send_json_error(400u, "bad_request", "header too large");
    }
    const size_t body_offset = (size_t)((header_end + 4u) - request);
    if (body_offset > read_len) {
      return W5500_HTTP_HANDLE_WAIT;
    }
    size_t content_length = 0u;
    if (!http_parse_content_length(request, header_end, &content_length) || content_length == 0u) {
      if (http_consume_rx(rx_rd, rx_size) != 0) {
        return W5500_HTTP_HANDLE_ERROR;
      }
      http_record_request(W5500_HTTP_PATH_DBC_UPLOAD, 400u);
      return http_send_json_error(400u, "bad_request", "missing content length");
    }
    if (content_length > W5500_HTTP_UPLOAD_BODY_MAX) {
      if (http_consume_rx(rx_rd, rx_size) != 0) {
        return W5500_HTTP_HANDLE_ERROR;
      }
      http_record_request(W5500_HTTP_PATH_DBC_UPLOAD, 413u);
      return http_send_json_error(413u, "payload_too_large", "dbc upload too large");
    }
    if ((size_t)rx_size < body_offset + content_length) {
      return W5500_HTTP_HANDLE_WAIT;
    }
    if (body_offset + content_length > read_len) {
      if (http_consume_rx(rx_rd, rx_size) != 0) {
        return W5500_HTTP_HANDLE_ERROR;
      }
      http_record_request(W5500_HTTP_PATH_DBC_UPLOAD, 413u);
      return http_send_json_error(413u, "payload_too_large", "dbc request too large");
    }
    if (http_consume_rx(rx_rd, rx_size) != 0) {
      return W5500_HTTP_HANDLE_ERROR;
    }
    const int upload_result = http_handle_dbc_upload((const uint8_t *)&request[body_offset], content_length);
    return upload_result == 0 ? W5500_HTTP_HANDLE_OK : W5500_HTTP_HANDLE_ERROR;
  }

  if (request_path_is(request, "POST", "/api/dbc/active")) {
    if (header_end == NULL) {
      if (read_len + 1u < W5500_HTTP_REQUEST_BUFFER_SIZE) {
        return W5500_HTTP_HANDLE_WAIT;
      }
      if (http_consume_rx(rx_rd, rx_size) != 0) {
        return W5500_HTTP_HANDLE_ERROR;
      }
      http_record_request(W5500_HTTP_PATH_DBC_ACTIVE, 400u);
      return http_send_json_error(400u, "bad_request", "header too large");
    }
    const size_t body_offset = (size_t)((header_end + 4u) - request);
    if (body_offset > read_len) {
      return W5500_HTTP_HANDLE_WAIT;
    }
    size_t content_length = 0u;
    if (http_parse_content_length(request, header_end, &content_length) && content_length > 0u) {
      if ((size_t)rx_size < body_offset + content_length) {
        return W5500_HTTP_HANDLE_WAIT;
      }
      if (http_consume_rx(rx_rd, rx_size) != 0) {
        return W5500_HTTP_HANDLE_ERROR;
      }
      http_record_request(W5500_HTTP_PATH_DBC_ACTIVE, 400u);
      return http_send_json_error(400u, "unsupported_body", "dbc active body unsupported");
    }
    if (body_offset > (size_t)rx_size) {
      return W5500_HTTP_HANDLE_WAIT;
    }
    if (http_consume_rx(rx_rd, rx_size) != 0) {
      return W5500_HTTP_HANDLE_ERROR;
    }
    const int active_result = http_handle_dbc_active();
    return active_result == 0 ? W5500_HTTP_HANDLE_OK : W5500_HTTP_HANDLE_ERROR;
  }

  if (rx_size > read_len || http_consume_rx(rx_rd, rx_size) != 0) {
    return W5500_HTTP_HANDLE_ERROR;
  }

  uint16_t code = 404u;
  uint32_t path_code = 0u;
  size_t body_len = 0u;
  const char *content_type = "application/json";
  if (request_path_is(request, "GET", "/api/status")) {
    code = 200u;
    path_code = W5500_HTTP_PATH_STATUS;
    body_len = build_status_body(body, sizeof(g_http_response_body));
  } else if (request_path_is(request, "GET", "/api/can/status")) {
    code = 200u;
    path_code = W5500_HTTP_PATH_CAN_STATUS;
    body_len = build_can_status_body(body, sizeof(g_http_response_body));
  } else if (request_path_is(request, "GET", "/") || request_path_is(request, "GET", "/index.html")) {
    const int static_result = http_send_static_index();
    if (static_result == W5500_HTTP_STATIC_OK) {
      g_w5500_http_request_count++;
      g_w5500_http_last_path = W5500_HTTP_PATH_INDEX;
      g_w5500_http_last_code = 200u;
      return 0;
    }
    if (static_result == W5500_HTTP_STATIC_SEND_ERROR) {
      return 1;
    }
    code = 404u;
    body_len = build_not_found_body(body, sizeof(g_http_response_body));
  } else {
    body_len = build_not_found_body(body, sizeof(g_http_response_body));
  }

  if (body_len >= sizeof(g_http_response_body)) {
    return 1;
  }
  g_w5500_http_request_count++;
  g_w5500_http_last_path = path_code;
  g_w5500_http_last_code = code;
  return http_send_response(code, content_type, body, body_len);
}

static void w5500_capture_status(W5500Port *port) {
  W5500Status status;
  if (w5500_port_get_status(port, &status) == W5500_OK) {
    g_w5500_version = status.version;
    g_w5500_phycfgr = status.phycfgr;
    g_w5500_link_up = status.link_up ? 1u : 0u;
    g_w5500_network_configured = status.network_configured ? 1u : 0u;
  }
}

int w5500_bringup_run(void) {
  const W5500Config config = {
    .mac = {0x02u, 0x00u, 0x00u, 0x12u, 0x34u, 0x56u},
    .ip = {192u, 168u, 1u, 88u},
    .netmask = {255u, 255u, 255u, 0u},
    .gateway = {192u, 168u, 1u, 1u},
    .retry_time_100us = 2000u,
    .retry_count = 8u,
  };

  stm32h750_w5500_bind(&g_w5500_port,
                       &g_w5500_ctx,
                       &hspi2,
                       W5500_CS_GPIO_Port,
                       W5500_CS_Pin,
                       W5500_RST_GPIO_Port,
                       W5500_RST_Pin);
  g_w5500_bound = 1u;

  const W5500Result result = w5500_port_init(&g_w5500_port, &config);
  g_w5500_init_result = (uint32_t)result;
  w5500_capture_status(&g_w5500_port);

  if (result == W5500_OK) {
    return 0;
  }
  if (result == W5500_VERSION_MISMATCH) {
    return 2;
  }
  return 1;
}

int w5500_bringup_poll(void) {
  if (g_w5500_bound == 0u) {
    return 1;
  }

  w5500_capture_status(&g_w5500_port);
  return 0;
}

int w5500_http_status_poll(void) {
  if (g_w5500_bound == 0u || g_w5500_network_configured == 0u) {
    g_w5500_http_status = 1u;
    return 1;
  }

  uint8_t sr = 0u;
  if (s0_read_u8(W5500_S0_SR, &sr) != W5500_OK) {
    g_w5500_http_status = 3u;
    g_w5500_http_error_count++;
    return 1;
  }
  g_w5500_http_socket_sr = sr;

  if (sr == W5500_S0_SR_CLOSED || sr == W5500_S0_SR_INIT) {
    return http_open_listener();
  }
  if (sr == W5500_S0_SR_LISTEN) {
    g_w5500_http_status = 0u;
    return 0;
  }
  if (sr == W5500_S0_SR_ESTABLISHED || sr == W5500_S0_SR_CLOSE_WAIT) {
    uint16_t rx_size = 0u;
    if (s0_read_u16(W5500_S0_RX_RSR, &rx_size) != W5500_OK) {
      g_w5500_http_status = 4u;
      g_w5500_http_error_count++;
      return 1;
    }
    if (rx_size > 0u) {
      const int request_result = http_handle_request(rx_size);
      if (request_result == W5500_HTTP_HANDLE_WAIT) {
        return 0;
      }
      if (request_result != W5500_HTTP_HANDLE_OK) {
        g_w5500_http_status = 5u;
        g_w5500_http_error_count++;
        (void)http_close_socket();
        return 1;
      }
    }
    (void)s0_command(W5500_S0_CR_DISCON);
    (void)http_close_socket();
    return 0;
  }

  (void)http_close_socket();
  return 0;
}

#endif
