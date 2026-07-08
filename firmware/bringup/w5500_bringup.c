#if defined(CAN_BUS_USE_STM32_HAL) || defined(STM32H750xx)

#include "main.h"
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

static Stm32W5500Context g_w5500_ctx;
static W5500Port g_w5500_port;
static uint8_t g_w5500_bound;

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

static bool request_path_is(const char *request, const char *path) {
  const size_t path_len = strlen(path);
  if (strncmp(request, "GET ", 4u) != 0) {
    return false;
  }
  const char *actual = request + 4u;
  return strncmp(actual, path, path_len) == 0 && (actual[path_len] == ' ' || actual[path_len] == '?');
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

static int http_send_response(uint16_t code, const char *content_type, const char *body, size_t body_len) {
  char response[768];
  const char *status_text = code == 200u ? "OK" : "Not Found";
  const int header_len = snprintf(response,
                                  sizeof(response),
                                  "HTTP/1.1 %u %s\r\n"
                                  "Content-Type: %s\r\n"
                                  "Content-Length: %lu\r\n"
                                  "Connection: close\r\n"
                                  "\r\n",
                                  (unsigned int)code,
                                  status_text,
                                  content_type,
                                  (unsigned long)body_len);
  if (header_len <= 0 || (size_t)header_len + body_len > sizeof(response)) {
    return 1;
  }
  memcpy(&response[header_len], body, body_len);
  const size_t response_len = (size_t)header_len + body_len;

  uint16_t tx_free = 0u;
  uint16_t tx_wr = 0u;
  if (s0_read_u16(W5500_S0_TX_FSR, &tx_free) != W5500_OK || tx_free < response_len ||
      s0_read_u16(W5500_S0_TX_WR, &tx_wr) != W5500_OK ||
      socket_buffer_write(W5500_S0_TX_BLOCK, tx_wr, (const uint8_t *)response, response_len) != W5500_OK ||
      s0_write_u16(W5500_S0_TX_WR, (uint16_t)(tx_wr + response_len)) != W5500_OK ||
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
      g_w5500_http_last_tx_size = (uint32_t)response_len;
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

static int http_handle_request(uint16_t rx_size) {
  char request[160];
  char body[384];
  uint16_t rx_rd = 0u;
  size_t read_len = rx_size;
  if (read_len >= sizeof(request)) {
    read_len = sizeof(request) - 1u;
  }
  if (s0_read_u16(W5500_S0_RX_RD, &rx_rd) != W5500_OK ||
      socket_buffer_read(W5500_S0_RX_BLOCK, rx_rd, (uint8_t *)request, read_len) != W5500_OK ||
      s0_write_u16(W5500_S0_RX_RD, (uint16_t)(rx_rd + rx_size)) != W5500_OK ||
      s0_command(W5500_S0_CR_RECV) != W5500_OK) {
    return 1;
  }
  request[read_len] = '\0';
  g_w5500_http_last_rx_size = rx_size;

  uint16_t code = 404u;
  uint32_t path_code = 0u;
  size_t body_len = 0u;
  const char *content_type = "application/json";
  if (request_path_is(request, "/api/status")) {
    code = 200u;
    path_code = W5500_HTTP_PATH_STATUS;
    body_len = build_status_body(body, sizeof(body));
  } else if (request_path_is(request, "/api/can/status")) {
    code = 200u;
    path_code = W5500_HTTP_PATH_CAN_STATUS;
    body_len = build_can_status_body(body, sizeof(body));
  } else if (request_path_is(request, "/") || request_path_is(request, "/index.html")) {
    size_t file_len = 0u;
    g_w5500_http_static_read_result =
      (uint32_t)stm32h750_tf_read_file_locked("/www/index.html", (uint8_t *)body, sizeof(body), &file_len);
    if (g_w5500_http_static_read_result == 0u && file_len > 0u && file_len < sizeof(body)) {
      code = 200u;
      path_code = W5500_HTTP_PATH_INDEX;
      content_type = "text/html; charset=utf-8";
      body_len = file_len;
      g_w5500_http_static_count++;
    } else {
      body_len = build_not_found_body(body, sizeof(body));
    }
  } else {
    body_len = build_not_found_body(body, sizeof(body));
  }

  if (body_len >= sizeof(body)) {
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
    if (rx_size > 0u && http_handle_request(rx_size) != 0) {
      g_w5500_http_status = 5u;
      g_w5500_http_error_count++;
      (void)http_close_socket();
      return 1;
    }
    (void)s0_command(W5500_S0_CR_DISCON);
    (void)http_close_socket();
    return 0;
  }

  (void)http_close_socket();
  return 0;
}

#endif
