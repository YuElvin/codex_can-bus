#if defined(CAN_BUS_USE_STM32_HAL) || defined(STM32H750xx)

#include "main.h"
#include "dbc_candidate_catalog.h"
#include "dbc_candidate_http.h"
#include "dbc_decoder.h"
#include "dbc_parser.h"
#include "dbc_upload.h"
#include "platform/large_dbc_candidate_stm32.h"
#include "platform/stm32h750_bringup.h"
#include "rule_file.h"
#include "selected_signal_log.h"
#include "signal_api.h"
#include "signal_log_control.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <stdio.h>
#include <stdlib.h>
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
volatile uint32_t g_w5500_http_last_nonclosed_close = 0u;
volatile uint32_t g_w5500_http_recovery_count = 0u;
volatile uint32_t g_w5500_http_recovery_last_sr = 0xffffffffu;
volatile uint32_t g_w5500_http_network_repair_count = 0u;
volatile uint32_t g_w5500_http_network_repair_failure_count = 0u;
volatile uint32_t g_w5500_http_network_repair_result = 0xffffffffu;
volatile uint32_t g_w5500_http_ack_wait_pending = 0u;
volatile uint32_t g_w5500_http_ack_wait_count = 0u;
volatile uint32_t g_w5500_http_ack_wait_initial_fsr = 0xffffffffu;
volatile uint32_t g_w5500_http_ack_wait_final_fsr = 0xffffffffu;
volatile uint32_t g_w5500_http_ack_wait_elapsed_ms = 0u;
volatile uint32_t g_w5500_http_ack_wait_timeout_count = 0u;
volatile uint32_t g_w5500_http_idle_connection_pending = 0u;
volatile uint32_t g_w5500_http_idle_connection_elapsed_ms = 0u;
volatile uint32_t g_w5500_http_idle_connection_timeout_count = 0u;
volatile uint32_t g_w5500_http_idle_connection_last_timeout_ms = 0u;
volatile uint32_t g_w5500_http_socket_ir = 0xffffffffu;
volatile uint32_t g_w5500_http_trace_seq = 0u;
volatile uint32_t g_w5500_http_trace_active = 0u;
volatile uint32_t g_w5500_http_trace_mutex_wait_start_tick = 0u;
volatile uint32_t g_w5500_http_trace_mutex_wait_end_tick = 0u;
volatile uint32_t g_w5500_http_trace_mutex_wait_ms = 0u;
volatile uint32_t g_w5500_http_trace_rx_ready_tick = 0u;
volatile uint32_t g_w5500_http_trace_rx_size = 0u;
volatile uint32_t g_w5500_http_trace_handle_enter_tick = 0u;
volatile uint32_t g_w5500_http_trace_record_tick = 0u;
volatile uint32_t g_w5500_http_trace_handler_return_tick = 0u;
volatile uint32_t g_w5500_http_trace_handler_result = 0xffffffffu;
volatile uint32_t g_w5500_http_trace_disconnect_start_tick = 0u;
volatile uint32_t g_w5500_http_trace_disconnect_end_tick = 0u;
volatile uint32_t g_w5500_http_trace_handle_wait_count = 0u;
volatile uint32_t g_w5500_http_trace_handle_wait_first_tick = 0u;
volatile uint32_t g_w5500_http_trace_handle_wait_last_rx_size = 0u;
volatile uint32_t g_w5500_http_trace_poll_enter_tick = 0u;
volatile uint32_t g_w5500_http_trace_poll_gap_ms = 0u;
volatile uint32_t g_w5500_http_trace_sr_read_start_tick = 0u;
volatile uint32_t g_w5500_http_trace_sr_read_end_tick = 0u;
volatile uint32_t g_w5500_http_trace_ir_read_start_tick = 0u;
volatile uint32_t g_w5500_http_trace_ir_read_end_tick = 0u;
volatile uint32_t g_w5500_http_trace_rx_rsr_read_start_tick = 0u;
volatile uint32_t g_w5500_http_trace_rx_rsr_read_end_tick = 0u;
volatile uint32_t g_w5500_http_trace_request_read_start_tick = 0u;
volatile uint32_t g_w5500_http_trace_request_read_end_tick = 0u;
volatile uint32_t g_w5500_http_trace_rx_consume_start_tick = 0u;
volatile uint32_t g_w5500_http_trace_rx_consume_end_tick = 0u;
volatile uint32_t g_w5500_http_trace_status_body_start_tick = 0u;
volatile uint32_t g_w5500_http_trace_status_body_end_tick = 0u;
volatile uint32_t g_w5500_http_trace_header_send_enter_tick = 0u;
volatile uint32_t g_w5500_http_trace_header_send_issued_tick = 0u;
volatile uint32_t g_w5500_http_trace_header_sendok_tick = 0u;
volatile uint32_t g_w5500_http_trace_send_count = 0u;
volatile uint32_t g_w5500_http_trace_last_send_len = 0u;
volatile uint32_t g_w5500_http_trace_tx_total = 0u;
volatile uint32_t g_w5500_http_trace_post_sendok_tx_fsr_result = 0u;
volatile uint32_t g_w5500_http_trace_post_sendok_tx_fsr_value = 0u;
volatile uint32_t g_w5500_http_pretrace_seq = 0u;
volatile uint32_t g_w5500_http_pretrace_sr = 0xffffffffu;
volatile uint32_t g_w5500_http_pretrace_ir = 0xffffffffu;
volatile uint32_t g_w5500_http_pretrace_rx_rsr_result = 0xffffffffu;
volatile uint32_t g_w5500_http_pretrace_rx_rsr = 0xffffffffu;
volatile uint32_t g_w5500_http_pretrace_poll_tick = 0u;
volatile uint32_t g_w5500_http_pretrace_poll_gap_ms = 0u;
volatile uint32_t g_w5500_http_pretrace_mutex_wait_ms = 0u;
volatile uint32_t g_w5500_http_no_trace_seq = 0u;
volatile uint32_t g_w5500_http_no_trace_sr_seen_mask = 0u;
volatile uint32_t g_w5500_http_no_trace_last_sr = 0xffffffffu;
volatile uint32_t g_w5500_http_no_trace_last_ir_result = 0xffffffffu;
volatile uint32_t g_w5500_http_no_trace_last_ir = 0xffffffffu;
volatile uint32_t g_w5500_http_no_trace_last_rx_rsr_result = 0xffffffffu;
volatile uint32_t g_w5500_http_no_trace_last_rx_rsr = 0xffffffffu;
volatile uint32_t g_w5500_http_no_trace_last_poll_gap_ms = 0u;
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
volatile uint32_t g_w5500_http_dbc_upload_phase = DBC_UPLOAD_STATE_IDLE;
volatile uint32_t g_w5500_http_dbc_upload_expected_bytes = 0u;
volatile uint32_t g_w5500_http_dbc_upload_crc32 = 0u;
volatile uint32_t g_w5500_http_dbc_upload_generation_hi = 0u;
volatile uint32_t g_w5500_http_dbc_upload_generation_lo = 0u;
volatile uint32_t g_w5500_http_dbc_upload_idle_timeout_count = 0u;
volatile uint32_t g_w5500_http_dbc_upload_total_timeout_count = 0u;
volatile uint32_t g_w5500_http_dbc_upload_abort_count = 0u;
volatile uint32_t g_w5500_http_candidate_available = 0u;
volatile uint32_t g_w5500_http_candidate_pending = 0u;
volatile uint32_t g_w5500_http_candidate_active = 0u;
volatile uint32_t g_w5500_http_candidate_complete = 0u;
volatile uint32_t g_w5500_http_candidate_result = 0xffffffffu;
volatile uint32_t g_w5500_http_candidate_progress_count = 0u;
volatile uint32_t g_w5500_http_candidate_progress_bytes = 0u;
volatile uint32_t g_w5500_http_candidate_last_io_operation = 0xffffffffu;
volatile uint32_t g_w5500_http_candidate_last_io_result = 0xffffffffu;
volatile uint32_t g_w5500_http_candidate_generation_hi = 0u;
volatile uint32_t g_w5500_http_candidate_generation_lo = 0u;
volatile uint32_t g_w5500_http_candidate_source_size = 0u;
volatile uint32_t g_w5500_http_candidate_source_crc32 = 0u;
volatile uint32_t g_w5500_http_candidate_index_size = 0u;
volatile uint32_t g_w5500_http_candidate_index_crc32 = 0u;
volatile uint32_t g_w5500_http_candidate_selection_crc32 = 0u;
volatile uint32_t g_w5500_http_candidate_catalog_messages = 0u;
volatile uint32_t g_w5500_http_candidate_catalog_signals = 0u;
volatile uint32_t g_w5500_http_candidate_selected_count = 0u;
volatile uint32_t g_w5500_http_candidate_selected_messages = 0u;
volatile uint32_t g_w5500_http_candidate_recovery_result = 0xffffffffu;
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
volatile uint32_t g_w5500_http_dbc_runtime_load_count = 0u;
volatile uint32_t g_w5500_http_dbc_runtime_result = 0xffffffffu;
volatile uint32_t g_w5500_http_dbc_runtime_bytes = 0u;
volatile uint32_t g_w5500_http_dbc_runtime_lines = 0u;
volatile uint32_t g_w5500_http_dbc_runtime_messages = 0u;
volatile uint32_t g_w5500_http_dbc_runtime_signals = 0u;
volatile uint32_t g_w5500_http_dbc_runtime_skipped = 0u;
volatile uint32_t g_w5500_http_dbc_runtime_errors = 0u;
volatile uint32_t g_w5500_http_dbc_runtime_valid = 0u;
volatile uint32_t g_w5500_http_dbc_runtime_generation = 0u;
volatile uint32_t g_w5500_http_dbc_runtime_active_slot = 0xffffffffu;
volatile uint32_t g_w5500_http_active_generation_hi;
volatile uint32_t g_w5500_http_active_generation_lo;
volatile uint32_t g_w5500_http_active_candidate_generation_hi;
volatile uint32_t g_w5500_http_active_candidate_generation_lo;
volatile uint32_t g_w5500_http_active_selection_crc32;
volatile uint32_t g_w5500_http_active_recovery_result = 0xffffffffu;
volatile uint32_t g_w5500_http_active_pending;
volatile uint32_t g_w5500_http_active_complete;
volatile uint32_t g_w5500_http_active_transaction_result = 0xffffffffu;
volatile uint32_t g_w5500_http_signals_count = 0u;
static volatile uint32_t g_w5500_http_dbc_reload_complete = 0u;
static volatile uint32_t g_w5500_http_dbc_reload_result = 0xffffffffu;
volatile uint32_t g_w5500_http_dbc_reload_queue_ready;
volatile uint32_t g_w5500_http_dbc_reload_enqueue_count;
volatile uint32_t g_w5500_http_dbc_reload_queue_drop_count;

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
extern volatile uint32_t g_rule_task_config_result;
extern volatile uint32_t g_rule_task_config_generation;
extern volatile uint32_t g_rule_task_engine_reload;
extern volatile uint32_t g_rule_file_v5_load_result;
extern volatile uint32_t g_rule_file_v5_save_request;
extern volatile uint32_t g_rule_file_v5_save_result;
extern RuleFileV5 g_rule_file_v5_current;
extern RuleFileV5 g_rule_file_v5_pending;
extern SignalLogControl g_signal_log_control;
extern SelectedSignalLogSession g_selected_signal_log_session;

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
#define W5500_S0_IR_RECV 0x04u

#define W5500_HTTP_SR_SEEN_LISTEN (1u << 0)
#define W5500_HTTP_SR_SEEN_SYNRECV (1u << 1)
#define W5500_HTTP_SR_SEEN_ESTABLISHED (1u << 2)
#define W5500_HTTP_SR_SEEN_CLOSE_WAIT (1u << 3)
#define W5500_HTTP_SR_SEEN_OTHER (1u << 4)

#define W5500_S0_MR_TCP 0x01u
#define W5500_S0_SR_CLOSED 0x00u
#define W5500_S0_SR_INIT 0x13u
#define W5500_S0_SR_LISTEN 0x14u
#define W5500_S0_SR_SYNRECV 0x16u
#define W5500_S0_SR_ESTABLISHED 0x17u
#define W5500_S0_SR_CLOSE_WAIT 0x1cu
#define W5500_HTTP_PORT 80u
#define W5500_HTTP_PATH_STATUS 1u
#define W5500_HTTP_PATH_CAN_STATUS 2u
#define W5500_HTTP_PATH_INDEX 3u
#define W5500_HTTP_PATH_DBC_UPLOAD 4u
#define W5500_HTTP_PATH_DBC_ACTIVE 5u
#define W5500_HTTP_PATH_DBC_RUNTIME 6u
#define W5500_HTTP_PATH_SIGNALS 7u
#define W5500_HTTP_PATH_RULE_CONFIG 8u
#define W5500_HTTP_PATH_RULES 9u
#define W5500_HTTP_PATH_RELAY_MANUAL 10u
#define W5500_HTTP_PATH_CAN_TX 11u
#define W5500_HTTP_PATH_DBC_SIGNALS 13u
#define W5500_HTTP_PATH_CAN_TX_SIGNALS 12u
#define W5500_HTTP_PATH_LOG_CONTROL 14u
#define W5500_HTTP_PATH_TIME_SYNC 15u
#define W5500_HTTP_PATH_DBC_CANDIDATE_SIGNALS 16u
#define W5500_HTTP_PATH_DBC_SELECTION 17u
#define W5500_HTTP_STATIC_CHUNK_SIZE 512u
#define W5500_HTTP_REQUEST_BUFFER_SIZE 1536u
#define W5500_HTTP_UPLOAD_BODY_MAX 1024u
#define W5500_HTTP_RULES_BODY_MAX 384u
#define W5500_HTTP_MANUAL_BODY_MAX 64u
#define W5500_HTTP_CAN_TX_BODY_MAX 96u
#define W5500_HTTP_LOG_CONTROL_BODY_MAX 96u
#define W5500_HTTP_TIME_SYNC_BODY_MAX 64u
#define W5500_HTTP_DBC_ACTIVE_TMP_PATH "/dbc/active.write.tmp"
#define W5500_HTTP_DBC_CANDIDATE_PATH "/dbc/candidate.dbc"
#define W5500_HTTP_DBC_ACTIVE_PATH "/dbc/active.dbc"
#define W5500_HTTP_DBC_ACTIVE_BACKUP_PATH "/dbc/active.prev.dbc"
#define W5500_HTTP_STATIC_OK 0
#define W5500_HTTP_STATIC_SEND_ERROR 1
#define W5500_HTTP_STATIC_NOT_FOUND 2
#define W5500_HTTP_HANDLE_OK 0
#define W5500_HTTP_HANDLE_ERROR 1
#define W5500_HTTP_HANDLE_WAIT 2
#define W5500_HTTP_DISCONNECT_RECOVERY_TIMEOUT_MS 500u
#define W5500_HTTP_IDLE_CONNECTION_TIMEOUT_MS 100u

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
static const W5500Config g_w5500_config = {
  .mac = {0x02u, 0x00u, 0x00u, 0x12u, 0x34u, 0x56u},
  .ip = {192u, 168u, 1u, 88u},
  .netmask = {255u, 255u, 255u, 0u},
  .gateway = {192u, 168u, 1u, 1u},
  .retry_time_100us = 2000u,
  .retry_count = 8u,
};
static uint8_t g_w5500_bound;
static SemaphoreHandle_t g_w5500_dbc_mutex;
static QueueHandle_t g_w5500_dbc_reload_queue;
static char g_http_request_buffer[W5500_HTTP_REQUEST_BUFFER_SIZE];
static char g_http_response_body[LARGE_DBC_HTTP_JSON_BODY_BYTES];
static uint8_t g_http_static_chunk[W5500_HTTP_STATIC_CHUNK_SIZE];
static char g_http_dbc_candidate_buffer[W5500_HTTP_UPLOAD_BODY_MAX + 1u];
static DbcDatabase g_http_dbc_candidate_db;
static DbcDatabase g_http_dbc_runtime_db[2];
static const DbcDatabase *g_http_dbc_runtime_active_db;
static DbcUploadState g_http_dbc_upload;
static DbcUploadResult g_http_dbc_upload_completed;
static uint64_t g_http_dbc_upload_next_generation = 1u;
static DbcCandidateUpload g_http_candidate_request;
static DbcCandidateDescriptor g_http_candidate_snapshot;
static volatile uint32_t g_http_candidate_mutation_busy;
static DbcCandidateCatalogQuery g_http_candidate_query_request;
static char g_http_candidate_query_text[LARGE_DBC_QUERY_MAX_BYTES + 1u];
static DbcCandidateCatalogPage g_http_candidate_query_page;
static DbcCandidateDescriptor g_http_candidate_query_snapshot;
static char g_http_candidate_query_token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES];
static LargeDbcCandidateCatalogResult g_http_candidate_query_backend_result;
static volatile uint32_t g_http_candidate_query_pending;
static volatile uint32_t g_http_candidate_query_active;
static volatile uint32_t g_http_candidate_query_complete;
static volatile uint32_t g_http_candidate_query_result = 0xffffffffu;
static DbcCandidateSelectionMutation g_http_candidate_selection_request;
static char g_http_candidate_selection_token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES];
static uint16_t g_http_candidate_selection_set[LARGE_DBC_SELECTION_UPDATE_MAX_ORDINALS];
static uint16_t g_http_candidate_selection_clear[LARGE_DBC_SELECTION_UPDATE_MAX_ORDINALS];
static DbcCandidateDescriptor g_http_candidate_selection_snapshot;
static LargeDbcCandidateSnapshot g_http_candidate_selection_backend_result;
static volatile uint32_t g_http_candidate_selection_pending;
static volatile uint32_t g_http_candidate_selection_complete;
static volatile uint32_t g_http_candidate_selection_result = 0xffffffffu;
static DbcSelectedRuntimeSnapshot g_http_selected_runtime;
static DbcActiveDescriptor g_http_active_descriptor;
static LargeDbcActiveRequest g_http_active_request;
static LargeDbcActiveResult g_http_active_backend_result;
static DbcSelectedRuleRequirement
  g_http_active_rule_requirements[RULE_FILE_V2_RULE_COUNT];
static char g_http_active_rule_keys[RULE_FILE_V2_RULE_COUNT][RULE_SIGNAL_KEY_MAX];
static uint64_t g_http_active_next_generation = 1u;
static uint8_t g_w5500_http_disconnect_pending;
static uint32_t g_w5500_http_disconnect_pending_start_tick;
static uint32_t g_w5500_http_ack_wait_start_tick;
static uint32_t g_w5500_http_idle_connection_start_tick;
static uint32_t g_w5500_http_last_mutex_wait_start_tick;
static uint32_t g_w5500_http_last_mutex_wait_end_tick;
static uint32_t g_w5500_http_last_mutex_wait_ms;
static uint32_t g_w5500_http_last_poll_enter_tick;
static uint8_t g_w5500_http_trace_header_send_active;
static uint32_t g_w5500_http_connection_sr_seen_mask;
static uint32_t g_w5500_http_connection_last_sr = 0xffffffffu;
static uint32_t g_w5500_http_connection_last_ir_result = 0xffffffffu;
static uint32_t g_w5500_http_connection_last_ir = 0xffffffffu;
static uint32_t g_w5500_http_connection_last_rx_rsr_result = 0xffffffffu;
static uint32_t g_w5500_http_connection_last_rx_rsr = 0xffffffffu;

static int http_upload_error_response(uint16_t code,
                                      const char *error_code,
                                      const char *message);
static void candidate_mutation_end(void);
static uint32_t g_w5500_http_connection_last_poll_gap_ms;
static uint8_t g_w5500_http_connection_trace_started;
static uint8_t g_w5500_http_no_trace_frozen;

void w5500_http_trace_mutex_wait(uint32_t start_tick, uint32_t end_tick) {
  g_w5500_http_last_mutex_wait_start_tick = start_tick;
  g_w5500_http_last_mutex_wait_end_tick = end_tick;
  g_w5500_http_last_mutex_wait_ms = (end_tick - start_tick) * portTICK_PERIOD_MS;
  if (g_w5500_http_trace_active != 0u) {
    g_w5500_http_trace_mutex_wait_start_tick = start_tick;
    g_w5500_http_trace_mutex_wait_end_tick = end_tick;
    g_w5500_http_trace_mutex_wait_ms = g_w5500_http_last_mutex_wait_ms;
  }
}

static void http_trace_begin(uint16_t rx_size,
                             uint32_t poll_enter_tick,
                             uint32_t poll_gap_ms,
                             uint32_t sr_read_start_tick,
                             uint32_t sr_read_end_tick,
                             uint32_t ir_read_start_tick,
                             uint32_t ir_read_end_tick,
                             uint32_t rx_rsr_read_start_tick,
                             uint32_t rx_rsr_read_end_tick) {
  ++g_w5500_http_trace_seq;
  g_w5500_http_trace_active = 1u;
  g_w5500_http_trace_mutex_wait_start_tick = g_w5500_http_last_mutex_wait_start_tick;
  g_w5500_http_trace_mutex_wait_end_tick = g_w5500_http_last_mutex_wait_end_tick;
  g_w5500_http_trace_mutex_wait_ms = g_w5500_http_last_mutex_wait_ms;
  g_w5500_http_trace_rx_ready_tick = HAL_GetTick();
  g_w5500_http_trace_rx_size = rx_size;
  g_w5500_http_trace_handle_enter_tick = 0u;
  g_w5500_http_trace_record_tick = 0u;
  g_w5500_http_trace_handler_return_tick = 0u;
  g_w5500_http_trace_handler_result = 0xffffffffu;
  g_w5500_http_trace_disconnect_start_tick = 0u;
  g_w5500_http_trace_disconnect_end_tick = 0u;
  g_w5500_http_trace_handle_wait_count = 0u;
  g_w5500_http_trace_handle_wait_first_tick = 0u;
  g_w5500_http_trace_handle_wait_last_rx_size = 0u;
  g_w5500_http_trace_poll_enter_tick = poll_enter_tick;
  g_w5500_http_trace_poll_gap_ms = poll_gap_ms;
  g_w5500_http_trace_sr_read_start_tick = sr_read_start_tick;
  g_w5500_http_trace_sr_read_end_tick = sr_read_end_tick;
  g_w5500_http_trace_ir_read_start_tick = ir_read_start_tick;
  g_w5500_http_trace_ir_read_end_tick = ir_read_end_tick;
  g_w5500_http_trace_rx_rsr_read_start_tick = rx_rsr_read_start_tick;
  g_w5500_http_trace_rx_rsr_read_end_tick = rx_rsr_read_end_tick;
  g_w5500_http_trace_request_read_start_tick = 0u;
  g_w5500_http_trace_request_read_end_tick = 0u;
  g_w5500_http_trace_rx_consume_start_tick = 0u;
  g_w5500_http_trace_rx_consume_end_tick = 0u;
  g_w5500_http_trace_status_body_start_tick = 0u;
  g_w5500_http_trace_status_body_end_tick = 0u;
  g_w5500_http_trace_header_send_enter_tick = 0u;
  g_w5500_http_trace_header_send_issued_tick = 0u;
  g_w5500_http_trace_header_sendok_tick = 0u;
  g_w5500_http_trace_send_count = 0u;
  g_w5500_http_trace_last_send_len = 0u;
  g_w5500_http_trace_tx_total = 0u;
  g_w5500_http_trace_post_sendok_tx_fsr_result = 0u;
  g_w5500_http_trace_post_sendok_tx_fsr_value = 0u;
}

static void http_trace_finish(void) {
  if (g_w5500_http_trace_active != 0u) {
    if (g_w5500_http_trace_disconnect_start_tick != 0u) {
      g_w5500_http_trace_disconnect_end_tick = HAL_GetTick();
    }
    g_w5500_http_trace_active = 0u;
  }
}

static void http_pretrace_latch(uint8_t sr,
                                uint8_t ir,
                                W5500Result rx_rsr_result,
                                uint16_t rx_rsr,
                                uint32_t poll_tick,
                                uint32_t poll_gap_ms) {
  if (g_w5500_http_pretrace_seq != 0u) {
    return;
  }
  g_w5500_http_pretrace_sr = sr;
  g_w5500_http_pretrace_ir = ir;
  g_w5500_http_pretrace_rx_rsr_result = (uint32_t)rx_rsr_result;
  g_w5500_http_pretrace_rx_rsr = rx_rsr;
  g_w5500_http_pretrace_poll_tick = poll_tick;
  g_w5500_http_pretrace_poll_gap_ms = poll_gap_ms;
  g_w5500_http_pretrace_mutex_wait_ms = g_w5500_http_last_mutex_wait_ms;
  ++g_w5500_http_pretrace_seq;
}

static uint32_t http_connection_sr_seen_bit(uint8_t sr) {
  if (sr == W5500_S0_SR_LISTEN) return W5500_HTTP_SR_SEEN_LISTEN;
  if (sr == W5500_S0_SR_SYNRECV) return W5500_HTTP_SR_SEEN_SYNRECV;
  if (sr == W5500_S0_SR_ESTABLISHED) return W5500_HTTP_SR_SEEN_ESTABLISHED;
  if (sr == W5500_S0_SR_CLOSE_WAIT) return W5500_HTTP_SR_SEEN_CLOSE_WAIT;
  return W5500_HTTP_SR_SEEN_OTHER;
}

static void http_idle_connection_reset(void) {
  g_w5500_http_idle_connection_pending = 0u;
  g_w5500_http_idle_connection_start_tick = 0u;
  g_w5500_http_idle_connection_elapsed_ms = 0u;
}

static void http_connection_reset(void) {
  http_idle_connection_reset();
  g_w5500_http_connection_sr_seen_mask = W5500_HTTP_SR_SEEN_LISTEN;
  g_w5500_http_connection_last_sr = W5500_S0_SR_LISTEN;
  g_w5500_http_connection_last_ir_result = 0xffffffffu;
  g_w5500_http_connection_last_ir = 0xffffffffu;
  g_w5500_http_connection_last_rx_rsr_result = 0xffffffffu;
  g_w5500_http_connection_last_rx_rsr = 0xffffffffu;
  g_w5500_http_connection_last_poll_gap_ms = 0u;
  g_w5500_http_connection_trace_started = 0u;
  g_w5500_http_no_trace_frozen = 0u;
}

static void http_connection_observe(uint8_t sr,
                                    W5500Result ir_result,
                                    uint8_t ir,
                                    uint32_t poll_gap_ms) {
  if (sr == W5500_S0_SR_LISTEN) {
    http_connection_reset();
    return;
  }
  g_w5500_http_connection_sr_seen_mask |= http_connection_sr_seen_bit(sr);
  g_w5500_http_connection_last_sr = sr;
  g_w5500_http_connection_last_ir_result = (uint32_t)ir_result;
  g_w5500_http_connection_last_ir = ir_result == W5500_OK ? ir : 0xffffffffu;
  g_w5500_http_connection_last_poll_gap_ms = poll_gap_ms;
}

static void http_no_trace_freeze(void) {
  if (g_w5500_http_no_trace_frozen != 0u) {
    return;
  }
  g_w5500_http_no_trace_sr_seen_mask = g_w5500_http_connection_sr_seen_mask;
  g_w5500_http_no_trace_last_sr = g_w5500_http_connection_last_sr;
  g_w5500_http_no_trace_last_ir_result = g_w5500_http_connection_last_ir_result;
  g_w5500_http_no_trace_last_ir = g_w5500_http_connection_last_ir;
  g_w5500_http_no_trace_last_rx_rsr_result = g_w5500_http_connection_last_rx_rsr_result;
  g_w5500_http_no_trace_last_rx_rsr = g_w5500_http_connection_last_rx_rsr;
  g_w5500_http_no_trace_last_poll_gap_ms = g_w5500_http_connection_last_poll_gap_ms;
  g_w5500_http_no_trace_frozen = 1u;
  ++g_w5500_http_no_trace_seq;
}

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

static W5500Result s0_read_u16_stable(uint16_t address, uint16_t *value) {
  for (uint32_t pair = 0u; pair < 4u; ++pair) {
    uint16_t first = 0u;
    uint16_t second = 0u;
    if (s0_read_u16(address, &first) != W5500_OK || s0_read_u16(address, &second) != W5500_OK) {
      return W5500_ERROR;
    }
    if (first == second) {
      *value = first;
      return W5500_OK;
    }
  }
  return W5500_ERROR;
}

static W5500Result s0_command(uint8_t command) {
  if (s0_write_u8(W5500_S0_CR, command) != W5500_OK) {
    return W5500_ERROR;
  }
  if (command == W5500_S0_CR_SEND && g_w5500_http_trace_active != 0u &&
      g_w5500_http_trace_header_send_active != 0u) {
    g_w5500_http_trace_header_send_issued_tick = HAL_GetTick();
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

static bool http_upload_sink_begin(void *context,
                                   uint64_t generation,
                                   uint32_t content_length) {
  (void)context;
  return stm32h750_tf_large_dbc_upload_begin(generation, content_length) == 0;
}

static bool http_upload_sink_write(void *context,
                                   const uint8_t *data,
                                   uint32_t size) {
  (void)context;
  return stm32h750_tf_large_dbc_upload_write(data, size) == 0;
}

static bool http_upload_sink_finalize(void *context) {
  (void)context;
  return stm32h750_tf_large_dbc_upload_finalize() == 0;
}

static void http_upload_sink_abort(void *context) {
  (void)context;
  stm32h750_tf_large_dbc_upload_abort();
}

static void http_upload_update_diagnostics(void) {
  g_w5500_http_dbc_upload_phase = (uint32_t)g_http_dbc_upload.phase;
  g_w5500_http_dbc_upload_result = (uint32_t)g_http_dbc_upload.status;
  g_w5500_http_dbc_upload_expected_bytes = g_http_dbc_upload.expected_size;
  g_w5500_http_dbc_upload_bytes = g_http_dbc_upload.received_size;
  g_w5500_http_dbc_upload_generation_hi =
    (uint32_t)(g_http_dbc_upload.generation >> 32u);
  g_w5500_http_dbc_upload_generation_lo =
    (uint32_t)g_http_dbc_upload.generation;
}

static int http_upload_reset_state(void) {
  if (g_http_dbc_upload.phase == DBC_UPLOAD_STATE_RECEIVING) {
    (void)dbc_upload_cancel(&g_http_dbc_upload);
    ++g_w5500_http_dbc_upload_abort_count;
    candidate_mutation_end();
  }
  const DbcUploadSink sink = {
    .context = NULL,
    .begin = http_upload_sink_begin,
    .write = http_upload_sink_write,
    .finalize = http_upload_sink_finalize,
    .abort = http_upload_sink_abort
  };
  const DbcUploadStatus status = dbc_upload_init(&g_http_dbc_upload, &sink);
  http_upload_update_diagnostics();
  return status == DBC_UPLOAD_STATUS_OK ? 0 : 1;
}

static int http_close_socket(uint32_t source) {
  uint8_t sr = 0u;
  if (s0_read_u8(W5500_S0_SR, &sr) != W5500_OK) {
    g_w5500_http_last_nonclosed_close = 0x80000000u | source;
  } else if (sr != W5500_S0_SR_CLOSED) {
    g_w5500_http_last_nonclosed_close = (source << 8) | sr;
  }
  g_w5500_http_disconnect_pending = 0u;
  g_w5500_http_disconnect_pending_start_tick = 0u;
  g_w5500_http_ack_wait_pending = 0u;
  g_w5500_http_ack_wait_start_tick = 0u;
  http_idle_connection_reset();
  (void)http_upload_reset_state();
  const W5500Result close_result = s0_command(W5500_S0_CR_CLOSE);
  (void)s0_write_u8(W5500_S0_IR, 0x1fu);
  if (close_result != W5500_OK) {
    http_trace_finish();
    return 1;
  }
  for (uint32_t i = 0u; i < 1000u; ++i) {
    if (s0_read_u8(W5500_S0_SR, &sr) != W5500_OK) {
      http_trace_finish();
      return 1;
    }
    if (sr == W5500_S0_SR_CLOSED) {
      http_trace_finish();
      return 0;
    }
    if ((i % 100u) == 0u) {
      g_w5500_port.ops->delay_ms(g_w5500_port.ctx, 1u);
    }
  }
  http_trace_finish();
  return 1;
}

static int http_begin_graceful_disconnect(void) {
  if (g_w5500_http_disconnect_pending != 0u) {
    return 0;
  }
  http_idle_connection_reset();
  if (g_w5500_http_trace_active != 0u) {
    g_w5500_http_trace_disconnect_start_tick = HAL_GetTick();
  }
  if (s0_command(W5500_S0_CR_DISCON) != W5500_OK) {
    return 1;
  }
  g_w5500_http_disconnect_pending_start_tick = HAL_GetTick();
  g_w5500_http_disconnect_pending = 1u;
  return 0;
}

static void http_begin_ack_wait(void) {
  const uint32_t initial_fsr =
    g_w5500_http_trace_post_sendok_tx_fsr_result == (uint32_t)W5500_OK
      ? g_w5500_http_trace_post_sendok_tx_fsr_value
      : 0xffffffffu;
  g_w5500_http_ack_wait_start_tick = HAL_GetTick();
  g_w5500_http_ack_wait_pending = 1u;
  ++g_w5500_http_ack_wait_count;
  g_w5500_http_ack_wait_initial_fsr = initial_fsr;
  g_w5500_http_ack_wait_final_fsr = initial_fsr;
  g_w5500_http_ack_wait_elapsed_ms = 0u;
}

static int http_finish_response_send(void) {
  if (g_w5500_http_trace_send_count == 0u) {
    return 1;
  }
  if (g_w5500_http_trace_post_sendok_tx_fsr_result == (uint32_t)W5500_OK &&
      g_w5500_http_trace_post_sendok_tx_fsr_value == W5500_SOCKET_BUFFER_SIZE) {
    return http_begin_graceful_disconnect();
  }
  /* SEND_OK only proves that W5500 accepted the SEND command. Keep the
   * response connection alive until TCP ACKs release the complete TX buffer;
   * the poll path remains bounded by the existing 500 ms recovery timeout. */
  http_begin_ack_wait();
  return 0;
}

static int http_ensure_network_config(void) {
  bool repaired = false;
  const W5500Result result =
    w5500_port_ensure_network_config(&g_w5500_port, &g_w5500_config, &repaired);
  g_w5500_http_network_repair_result = (uint32_t)result;
  if (result != W5500_OK) {
    ++g_w5500_http_network_repair_failure_count;
    return 1;
  }
  if (repaired) {
    ++g_w5500_http_network_repair_count;
  }
  return 0;
}

static int http_open_listener(void) {
  uint8_t sr = 0u;
  if (s0_read_u8(W5500_S0_SR, &sr) == W5500_OK &&
      (sr == W5500_S0_SR_LISTEN || sr == W5500_S0_SR_SYNRECV ||
       sr == W5500_S0_SR_ESTABLISHED)) {
    if (http_ensure_network_config() != 0) {
      g_w5500_http_status = 2u;
      ++g_w5500_http_error_count;
      (void)http_close_socket(4u);
      return 1;
    }
    g_w5500_http_socket_sr = sr;
    g_w5500_http_status = 0u;
    return 0;
  }
  if (http_close_socket(1u) != 0 ||
      http_ensure_network_config() != 0 ||
      s0_write_u8(W5500_S0_MR, W5500_S0_MR_TCP) != W5500_OK ||
      s0_write_u16(W5500_S0_PORT, W5500_HTTP_PORT) != W5500_OK ||
      s0_command(W5500_S0_CR_OPEN) != W5500_OK ||
      s0_read_u8(W5500_S0_SR, &sr) != W5500_OK ||
      sr != W5500_S0_SR_INIT ||
      s0_command(W5500_S0_CR_LISTEN) != W5500_OK ||
      s0_read_u8(W5500_S0_SR, &sr) != W5500_OK ||
      sr != W5500_S0_SR_LISTEN ||
      http_ensure_network_config() != 0) {
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
    case 201u:
      return "Created";
    case 202u:
      return "Accepted";
    case 400u:
      return "Bad Request";
    case 408u:
      return "Request Timeout";
    case 404u:
      return "Not Found";
    case 405u:
      return "Method Not Allowed";
    case 409u:
      return "Conflict";
    case 411u:
      return "Length Required";
    case 413u:
      return "Payload Too Large";
    case 415u:
      return "Unsupported Media Type";
    case 422u:
      return "Unprocessable Content";
    case 500u:
      return "Internal Server Error";
    case 504u:
      return "Gateway Timeout";
    case 507u:
      return "Insufficient Storage";
    default:
      return "Error";
  }
}

static size_t build_status_body(char *body, size_t len) {
  if (g_w5500_http_trace_active != 0u) {
    g_w5500_http_trace_status_body_start_tick = HAL_GetTick();
  }
  const size_t result = (size_t)snprintf(body,
                          len,
                          "{\"ok\":true,\"data\":{\"rtos\":{\"started\":%lu,\"ready\":%lu,\"loop\":%lu},"
                          "\"w5500\":{\"status\":%d,\"link\":%lu,\"version\":%lu,\"phycfgr\":%lu,\"lastNonclosedClose\":%lu,\"socketIr\":%lu,"
                          "\"httpTrace\":{\"seq\":%lu,\"active\":%lu,\"mutexWaitStartTick\":%lu,\"mutexWaitEndTick\":%lu,\"mutexWaitMs\":%lu,\"rxReadyTick\":%lu,\"rxSize\":%lu,\"handleEnterTick\":%lu,\"recordTick\":%lu,\"handlerReturnTick\":%lu,\"handlerResult\":%lu,\"disconnectStartTick\":%lu,\"disconnectEndTick\":%lu,\"handleWaitCount\":%lu,\"handleWaitFirstTick\":%lu,\"handleWaitLastRxSize\":%lu,\"sendCount\":%lu,\"lastSendLen\":%lu,\"txTotal\":%lu,\"postSendokTxFsrResult\":%lu,\"postSendokTxFsrValue\":%lu,"
                          "\"t\":{\"p\":%lu,\"g\":%lu,\"ss\":%lu,\"se\":%lu,\"is\":%lu,\"ie\":%lu,\"rs\":%lu,\"re\":%lu,\"bs\":%lu,\"be\":%lu,\"cs\":%lu,\"ce\":%lu,\"us\":%lu,\"ue\":%lu,\"hs\":%lu,\"hi\":%lu,\"hk\":%lu}}},"
                          "\"tf\":{\"status\":%d},\"qspi\":{\"status\":%d,\"jedec\":%lu}}}",
                          (unsigned long)g_freertos_task_started,
                          (unsigned long)g_freertos_bringup_complete,
                          (unsigned long)g_freertos_loop_count,
                          g_w5500_bringup_status,
                          (unsigned long)g_w5500_link_up,
                          (unsigned long)g_w5500_version,
                          (unsigned long)g_w5500_phycfgr,
                          (unsigned long)g_w5500_http_last_nonclosed_close,
                          (unsigned long)g_w5500_http_socket_ir,
                          (unsigned long)g_w5500_http_trace_seq,
                          (unsigned long)g_w5500_http_trace_active,
                          (unsigned long)g_w5500_http_trace_mutex_wait_start_tick,
                          (unsigned long)g_w5500_http_trace_mutex_wait_end_tick,
                          (unsigned long)g_w5500_http_trace_mutex_wait_ms,
                          (unsigned long)g_w5500_http_trace_rx_ready_tick,
                          (unsigned long)g_w5500_http_trace_rx_size,
                          (unsigned long)g_w5500_http_trace_handle_enter_tick,
                          (unsigned long)g_w5500_http_trace_record_tick,
                          (unsigned long)g_w5500_http_trace_handler_return_tick,
                          (unsigned long)g_w5500_http_trace_handler_result,
                          (unsigned long)g_w5500_http_trace_disconnect_start_tick,
                          (unsigned long)g_w5500_http_trace_disconnect_end_tick,
                          (unsigned long)g_w5500_http_trace_handle_wait_count,
                          (unsigned long)g_w5500_http_trace_handle_wait_first_tick,
                          (unsigned long)g_w5500_http_trace_handle_wait_last_rx_size,
                          (unsigned long)g_w5500_http_trace_send_count,
                          (unsigned long)g_w5500_http_trace_last_send_len,
                          (unsigned long)g_w5500_http_trace_tx_total,
                          (unsigned long)g_w5500_http_trace_post_sendok_tx_fsr_result,
                          (unsigned long)g_w5500_http_trace_post_sendok_tx_fsr_value,
                          (unsigned long)g_w5500_http_trace_poll_enter_tick,
                          (unsigned long)g_w5500_http_trace_poll_gap_ms,
                          (unsigned long)g_w5500_http_trace_sr_read_start_tick,
                          (unsigned long)g_w5500_http_trace_sr_read_end_tick,
                          (unsigned long)g_w5500_http_trace_ir_read_start_tick,
                          (unsigned long)g_w5500_http_trace_ir_read_end_tick,
                          (unsigned long)g_w5500_http_trace_rx_rsr_read_start_tick,
                          (unsigned long)g_w5500_http_trace_rx_rsr_read_end_tick,
                          (unsigned long)g_w5500_http_trace_request_read_start_tick,
                          (unsigned long)g_w5500_http_trace_request_read_end_tick,
                          (unsigned long)g_w5500_http_trace_rx_consume_start_tick,
                          (unsigned long)g_w5500_http_trace_rx_consume_end_tick,
                          (unsigned long)g_w5500_http_trace_status_body_start_tick,
                          (unsigned long)g_w5500_http_trace_status_body_end_tick,
                          (unsigned long)g_w5500_http_trace_header_send_enter_tick,
                          (unsigned long)g_w5500_http_trace_header_send_issued_tick,
                          (unsigned long)g_w5500_http_trace_header_sendok_tick,
                          g_tf_card_bringup_status,
                          g_w25q128_bringup_status,
                          (unsigned long)g_w25q128_jedec_id);
  if (g_w5500_http_trace_active != 0u) {
    g_w5500_http_trace_status_body_end_tick = HAL_GetTick();
  }
  return result;
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

static size_t build_can_tx_body(char *body, size_t len) {
  CanTxControlState state;

  can2_tx_control_snapshot(&state);
  return (size_t)snprintf(body,
                          len,
                          "{\"ok\":true,\"data\":{\"enabled\":%s,\"id\":%u,\"dlc\":%u,"
                          "\"data\":[%u,%u,%u,%u,%u,%u,%u,%u],\"periodMs\":%lu,"
                          "\"requestSeq\":%lu,\"appliedSeq\":%lu,\"lastResult\":%lu}}",
                          state.config.enabled ? "true" : "false",
                          (unsigned)state.config.standard_id,
                          (unsigned)state.config.dlc,
                          (unsigned)state.config.data[0],
                          (unsigned)state.config.data[1],
                          (unsigned)state.config.data[2],
                          (unsigned)state.config.data[3],
                          (unsigned)state.config.data[4],
                          (unsigned)state.config.data[5],
                          (unsigned)state.config.data[6],
                          (unsigned)state.config.data[7],
                          (unsigned long)state.config.period_ms,
                          (unsigned long)state.request_seq,
                          (unsigned long)state.applied_seq,
                          (unsigned long)state.last_result);
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

static size_t build_dbc_active_body(char *body, size_t len, const DbcUploadReport *report) {
  return (size_t)snprintf(body,
                          len,
                          "{\"ok\":true,\"data\":{\"candidate\":\"%s\",\"active\":\"%s\","
                          "\"activeBackup\":\"%s\",\"bytes\":%lu,\"lines\":%lu,"
                          "\"messages\":%lu,\"signals\":%lu,\"skipped\":%lu,"
                          "\"errors\":%lu,\"valid\":%s,\"activated\":true,"
                          "\"runtimeGeneration\":%lu}}",
                          W5500_HTTP_DBC_CANDIDATE_PATH,
                          W5500_HTTP_DBC_ACTIVE_PATH,
                          W5500_HTTP_DBC_ACTIVE_BACKUP_PATH,
                          (unsigned long)report->bytes,
                          (unsigned long)report->lines,
                          (unsigned long)report->messages,
                          (unsigned long)report->signals,
                          (unsigned long)report->skipped,
                          (unsigned long)report->errors,
                          report->errors == 0u ? "true" : "false",
                          (unsigned long)g_w5500_http_dbc_runtime_generation);
}

static size_t build_dbc_runtime_body(char *body, size_t len) {
  return (size_t)snprintf(body,
                          len,
                          "{\"ok\":true,\"data\":{\"active\":\"%s\",\"loaded\":%s,"
                          "\"generation\":%lu,\"activeGeneration\":\"%08lX%08lX\","
                          "\"candidateGeneration\":\"%08lX%08lX\","
                          "\"selectionCrc32\":\"%08lX\",\"selectedOnly\":%s,"
                          "\"activeSlot\":%lu,\"lastResult\":%lu,"
                          "\"bytes\":%lu,\"lines\":%lu,\"messages\":%lu,"
                          "\"signals\":%lu,\"skipped\":%lu,\"errors\":%lu}}",
                          W5500_HTTP_DBC_ACTIVE_PATH,
                          g_w5500_http_dbc_runtime_valid != 0u ? "true" : "false",
                          (unsigned long)g_w5500_http_dbc_runtime_generation,
                          (unsigned long)g_w5500_http_active_generation_hi,
                          (unsigned long)g_w5500_http_active_generation_lo,
                          (unsigned long)g_w5500_http_active_candidate_generation_hi,
                          (unsigned long)g_w5500_http_active_candidate_generation_lo,
                          (unsigned long)g_w5500_http_active_selection_crc32,
                          w5500_http_selected_runtime_available() != 0 ? "true" : "false",
                          (unsigned long)g_w5500_http_dbc_runtime_active_slot,
                          (unsigned long)g_w5500_http_dbc_runtime_result,
                          (unsigned long)g_w5500_http_dbc_runtime_bytes,
                          (unsigned long)g_w5500_http_dbc_runtime_lines,
                          (unsigned long)g_w5500_http_dbc_runtime_messages,
                          (unsigned long)g_w5500_http_dbc_runtime_signals,
                          (unsigned long)g_w5500_http_dbc_runtime_skipped,
                          (unsigned long)g_w5500_http_dbc_runtime_errors);
}

static size_t build_can_tx_signals_body(char *body, size_t len) {
  static const char empty[] =
    "{\"ok\":true,\"data\":{\"items\":[],\"count\":0}}";
  if (body == NULL || sizeof(empty) > len) {
    return len;
  }
  memcpy(body, empty, sizeof(empty));
  return sizeof(empty) - 1u;
}

static size_t build_manual_relay_body(char *body, size_t len) {
  uint32_t enabled = 0u;
  uint32_t relay1 = 0u;
  uint32_t relay2 = 0u;
  uint32_t request_seq = 0u;
  uint32_t applied_seq = 0u;
  uint32_t relay1_output = 0u;
  uint32_t relay2_output = 0u;

  rule_task_manual_override_snapshot(&enabled,
                                     &relay1,
                                     &relay2,
                                     &request_seq,
                                     &applied_seq,
                                     &relay1_output,
                                     &relay2_output);
  return (size_t)snprintf(body,
                          len,
                          "{\"ok\":true,\"data\":{\"enabled\":%lu,\"relay1\":%lu,\"relay2\":%lu,"
                          "\"requestSeq\":%lu,\"appliedSeq\":%lu,\"relay1Output\":%lu,\"relay2Output\":%lu}}",
                          (unsigned long)enabled,
                          (unsigned long)relay1,
                          (unsigned long)relay2,
                          (unsigned long)request_seq,
                          (unsigned long)applied_seq,
                          (unsigned long)relay1_output,
                          (unsigned long)relay2_output);
}

static size_t build_rules_body(char *body, size_t len, int slot) {
  const RuleFileV5Slot *first = &g_rule_file_v5_current.slots[0];
  const RuleFileV5Slot *second = &g_rule_file_v5_current.slots[1];
  const char *source = g_rule_file_v5_load_result == 0u ? "v5" :
    g_rule_file_v5_load_result == 1u ? "none" :
    g_rule_file_v5_load_result == 7u ? "legacy-incompatible" :
    "v5-invalid";
  const uint64_t first_hash = first->definition_hash;
  const uint64_t second_hash = second->definition_hash;
  char first_threshold[32];
  char second_threshold[32];

  if (rule_file_format_decimal(first->threshold, first_threshold, sizeof(first_threshold)) == 0u ||
      rule_file_format_decimal(second->threshold, second_threshold, sizeof(second_threshold)) == 0u) {
    return len;
  }
  if (slot >= 0 && slot < 2) {
    const RuleFileV5Slot *rule = &g_rule_file_v5_current.slots[slot];
    char threshold[32];
    if (rule_file_format_decimal(rule->threshold, threshold, sizeof(threshold)) == 0u) {
      return len;
    }
    return (size_t)snprintf(body, len,
      "{\"ok\":true,\"data\":{\"source\":\"%s\",\"version\":%u,\"slot\":%d,\"enabled\":%s,\"relay\":%u,\"signalKey\":\"%s\",\"definitionHash\":\"%08lX%08lX\",\"threshold\":%s,\"action\":\"%s\",\"delayMs\":%lu,\"timeoutMs\":%lu,\"safeState\":\"%s\",\"priority\":%u}}",
      source, 5u, slot, rule->enabled ? "true" : "false", (unsigned)rule->relay,
      rule->signal_key,
      (unsigned long)((slot == 0 ? first_hash : second_hash) >> 32u),
      (unsigned long)(uint32_t)(slot == 0 ? first_hash : second_hash),
      threshold, rule->action_state == RELAY_STATE_ON ? "on" : "off",
      (unsigned long)rule->delay_ms, (unsigned long)rule->timeout_ms,
      rule->safe_state == RELAY_STATE_ON ? "on" : "off", (unsigned)rule->priority);
  }
  return (size_t)snprintf(body, len,
    "{\"ok\":true,\"data\":{\"source\":\"%s\",\"version\":%u,\"rules\":[{\"slot\":0,\"enabled\":%s,\"relay\":%u,\"signalKey\":\"%s\",\"definitionHash\":\"%08lX%08lX\",\"threshold\":%s,\"action\":\"%s\",\"delayMs\":%lu,\"timeoutMs\":%lu,\"safeState\":\"%s\",\"priority\":%u},{\"slot\":1,\"enabled\":%s,\"relay\":%u,\"signalKey\":\"%s\",\"definitionHash\":\"%08lX%08lX\",\"threshold\":%s,\"action\":\"%s\",\"delayMs\":%lu,\"timeoutMs\":%lu,\"safeState\":\"%s\",\"priority\":%u}]}}",
    source, 5u, first->enabled ? "true" : "false", (unsigned)first->relay, first->signal_key,
    (unsigned long)(first_hash >> 32u), (unsigned long)(uint32_t)first_hash, first_threshold,
    first->action_state == RELAY_STATE_ON ? "on" : "off", (unsigned long)first->delay_ms, (unsigned long)first->timeout_ms, first->safe_state == RELAY_STATE_ON ? "on" : "off", (unsigned)first->priority,
    second->enabled ? "true" : "false", (unsigned)second->relay, second->signal_key,
    (unsigned long)(second_hash >> 32u), (unsigned long)(uint32_t)second_hash, second_threshold,
    second->action_state == RELAY_STATE_ON ? "on" : "off", (unsigned long)second->delay_ms, (unsigned long)second->timeout_ms, second->safe_state == RELAY_STATE_ON ? "on" : "off", (unsigned)second->priority);
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
    const size_t digit = (size_t)(*text - '0');
    if (result > (SIZE_MAX - digit) / 10u) {
      return false;
    }
    result = (result * 10u) + digit;
    has_digit = true;
    ++text;
  }
  text = skip_http_space(text);
  if (!has_digit || text != end) {
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

static bool http_span_equals_case(const char *text, size_t text_len,
                                  const char *expected) {
  const size_t expected_len = strlen(expected);
  if (text_len != expected_len) {
    return false;
  }
  for (size_t i = 0u; i < text_len; ++i) {
    uint8_t left = (uint8_t)text[i];
    uint8_t right = (uint8_t)expected[i];
    if (left >= (uint8_t)'A' && left <= (uint8_t)'Z') left += 32u;
    if (right >= (uint8_t)'A' && right <= (uint8_t)'Z') right += 32u;
    if (left != right) return false;
  }
  return true;
}

static bool http_content_type_is_selection_form(const char *request,
                                                const char *header_end) {
  const char *line = strstr(request, "\r\n");
  bool found = false;
  if (line == NULL || line >= header_end) return false;
  line += 2u;
  while (line < header_end) {
    const char *line_end = strstr(line, "\r\n");
    if (line_end == NULL || line_end > header_end) line_end = header_end;
    const char *colon = memchr(line, ':', (size_t)(line_end - line));
    if (colon != NULL &&
        http_span_equals_case(line, (size_t)(colon - line), "Content-Type")) {
      if (found) return false;
      const char *value = skip_http_space(colon + 1u);
      const char *value_end = line_end;
      while (value_end > value &&
             (value_end[-1] == ' ' || value_end[-1] == '\t')) --value_end;
      if (!http_span_equals_case(value, (size_t)(value_end - value),
                                 "application/x-www-form-urlencoded")) {
        return false;
      }
      found = true;
    }
    if (line_end == header_end) break;
    line = line_end + 2u;
  }
  return found;
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

static void dbc_record_runtime_report(const DbcUploadReport *report, uint32_t valid) {
  g_w5500_http_dbc_runtime_bytes = (uint32_t)report->bytes;
  g_w5500_http_dbc_runtime_lines = (uint32_t)report->lines;
  g_w5500_http_dbc_runtime_messages = (uint32_t)report->messages;
  g_w5500_http_dbc_runtime_signals = (uint32_t)report->signals;
  g_w5500_http_dbc_runtime_skipped = (uint32_t)report->skipped;
  g_w5500_http_dbc_runtime_errors = (uint32_t)report->errors;
  g_w5500_http_dbc_runtime_valid = valid;
}

static void dbc_record_runtime_failure(const DbcUploadReport *report) {
  if (g_http_dbc_runtime_active_db == NULL) {
    dbc_record_runtime_report(report, 0u);
  }
}

static int w5500_http_load_active_dbc_unlocked(void) {
  DbcUploadReport report;
  size_t read_len = 0u;
  size_t line_count = 0u;
  memset(&report, 0, sizeof(report));

  const uint32_t current_slot = g_w5500_http_dbc_runtime_active_slot == 0u ? 0u : 1u;
  const uint32_t next_slot = current_slot == 0u ? 1u : 0u;
  DbcDatabase *next_db = &g_http_dbc_runtime_db[next_slot];

  const int read_result = stm32h750_tf_read_file_locked(W5500_HTTP_DBC_ACTIVE_PATH,
                                                        (uint8_t *)g_http_dbc_candidate_buffer,
                                                        sizeof(g_http_dbc_candidate_buffer),
                                                        &read_len);
  if (read_result != 0) {
    g_w5500_http_dbc_runtime_result = (uint32_t)read_result;
    dbc_record_runtime_failure(&report);
    return 1;
  }
  if (read_len > W5500_HTTP_UPLOAD_BODY_MAX) {
    report.bytes = read_len;
    report.errors = 1u;
    g_w5500_http_dbc_runtime_result = 2u;
    dbc_record_runtime_failure(&report);
    return 2;
  }

  g_http_dbc_candidate_buffer[read_len] = '\0';
  const uint32_t valid =
    dbc_parse_text(next_db, g_http_dbc_candidate_buffer, read_len, &line_count) ? 1u : 0u;
  report.bytes = read_len;
  report.lines = line_count;
  report.messages = next_db->message_count;
  report.signals = next_db->signal_count;
  report.skipped = next_db->skipped_lines;
  report.errors = next_db->error_lines;
  if (valid == 0u) {
    g_w5500_http_dbc_runtime_result = 3u;
    dbc_record_runtime_failure(&report);
    return 3;
  }

  g_http_dbc_runtime_active_db = next_db;
  g_w5500_http_dbc_runtime_active_slot = next_slot;
  g_w5500_http_dbc_runtime_generation++;
  g_w5500_http_dbc_runtime_result = 0u;
  g_w5500_http_dbc_runtime_load_count++;
  dbc_record_runtime_report(&report, 1u);
  return 0;
}

int w5500_http_dbc_lock(void) {
  if (g_w5500_dbc_mutex == NULL) {
    return 1;
  }
  return xSemaphoreTake(g_w5500_dbc_mutex, portMAX_DELAY) == pdTRUE ? 0 : 1;
}

void w5500_http_dbc_unlock(void) {
  if (g_w5500_dbc_mutex != NULL) {
    (void)xSemaphoreGive(g_w5500_dbc_mutex);
  }
}

int w5500_http_load_active_dbc(void) {
  if (w5500_http_dbc_lock() != 0) {
    return 1;
  }
  const int result = w5500_http_load_active_dbc_unlocked();
  w5500_http_dbc_unlock();
  return result;
}

#define DBC_WORK_COMMAND_RELOAD 1u
#define DBC_WORK_COMMAND_CANDIDATE 2u
#define DBC_WORK_COMMAND_CANDIDATE_QUERY 3u
#define DBC_WORK_COMMAND_CANDIDATE_SELECTION 4u
#define DBC_WORK_COMMAND_ACTIVE 5u

static void candidate_record_snapshot_unlocked(
  const DbcCandidateDescriptor *candidate) {
  if (candidate == NULL) {
    return;
  }
  g_w5500_http_candidate_generation_hi =
    (uint32_t)(candidate->generation >> 32u);
  g_w5500_http_candidate_generation_lo = (uint32_t)candidate->generation;
  g_w5500_http_candidate_source_size = candidate->source_size;
  g_w5500_http_candidate_source_crc32 = candidate->source_crc32;
  g_w5500_http_candidate_index_size = candidate->index_size;
  g_w5500_http_candidate_index_crc32 = candidate->index_crc32;
  g_w5500_http_candidate_selection_crc32 = candidate->selection_crc32;
  g_w5500_http_candidate_catalog_messages = candidate->catalog_message_count;
  g_w5500_http_candidate_catalog_signals = candidate->catalog_signal_count;
  g_w5500_http_candidate_selected_count = candidate->selected_count;
  g_w5500_http_candidate_selected_messages = candidate->selected_message_count;
}

static void candidate_publish_snapshot(const DbcCandidateDescriptor *candidate,
                                       bool available) {
  taskENTER_CRITICAL();
  if (candidate != NULL && available) {
    g_http_candidate_snapshot = *candidate;
    candidate_record_snapshot_unlocked(candidate);
  } else {
    memset(&g_http_candidate_snapshot, 0, sizeof(g_http_candidate_snapshot));
  }
  g_w5500_http_candidate_available = available ? 1u : 0u;
  taskEXIT_CRITICAL();
}

static bool candidate_read_snapshot(DbcCandidateDescriptor *candidate) {
  bool available;
  taskENTER_CRITICAL();
  available = g_w5500_http_candidate_available != 0u;
  if (available && candidate != NULL) {
    *candidate = g_http_candidate_snapshot;
  }
  taskEXIT_CRITICAL();
  return available;
}

typedef enum {
  HTTP_CANDIDATE_MUTATION_OK = 0,
  HTTP_CANDIDATE_MUTATION_LOGGING,
  HTTP_CANDIDATE_MUTATION_BUSY
} HttpCandidateMutationGate;

static HttpCandidateMutationGate candidate_mutation_begin(void) {
  HttpCandidateMutationGate result = HTTP_CANDIDATE_MUTATION_OK;
  taskENTER_CRITICAL();
  if (g_signal_log_control.enabled) {
    result = HTTP_CANDIDATE_MUTATION_LOGGING;
  } else if (g_http_candidate_mutation_busy != 0u ||
             g_http_candidate_query_pending != 0u ||
             g_http_candidate_query_active != 0u) {
    result = HTTP_CANDIDATE_MUTATION_BUSY;
  } else {
    g_http_candidate_mutation_busy = 1u;
  }
  taskEXIT_CRITICAL();
  return result;
}

static void candidate_mutation_end(void) {
  taskENTER_CRITICAL();
  g_http_candidate_mutation_busy = 0u;
  taskEXIT_CRITICAL();
}

static void candidate_progress(void *context,
                               LargeDbcCandidateIoOperation operation,
                               uint32_t transferred_bytes,
                               int io_result) {
  (void)context;
  ++g_w5500_http_candidate_progress_count;
  g_w5500_http_candidate_progress_bytes += transferred_bytes;
  g_w5500_http_candidate_last_io_operation = (uint32_t)operation;
  g_w5500_http_candidate_last_io_result = (uint32_t)io_result;
  if ((g_w5500_http_candidate_progress_count & 31u) == 0u) {
    vTaskDelay(pdMS_TO_TICKS(1u));
  }
}

static DbcSelectedRuntimeStatus active_publish_runtime(
  void *context,
  DbcSelectedRuntimeSnapshot *snapshot,
  const DbcActiveDescriptor *active,
  uint8_t prepared_slot) {
  (void)context;
  DbcSelectedRuntimeStatus status;
  taskENTER_CRITICAL();
  status = dbc_selected_runtime_publish_prepared_checked(
    snapshot, active->active_generation, prepared_slot);
  if (status == DBC_SELECTED_RUNTIME_OK) {
    g_http_active_descriptor = *active;
    g_w5500_http_active_generation_hi =
      (uint32_t)(active->active_generation >> 32u);
    g_w5500_http_active_generation_lo = (uint32_t)active->active_generation;
    g_w5500_http_active_candidate_generation_hi =
      (uint32_t)(active->candidate_generation >> 32u);
    g_w5500_http_active_candidate_generation_lo =
      (uint32_t)active->candidate_generation;
    g_w5500_http_active_selection_crc32 = active->selection_crc32;
    g_w5500_http_dbc_runtime_generation =
      (uint32_t)active->active_generation;
    g_w5500_http_dbc_runtime_active_slot = snapshot->active_slot;
    g_w5500_http_dbc_runtime_bytes = active->source_size;
    g_w5500_http_dbc_runtime_messages = active->selected_message_count;
    g_w5500_http_dbc_runtime_signals = active->selected_count;
    g_w5500_http_dbc_runtime_lines = 0u;
    g_w5500_http_dbc_runtime_skipped = 0u;
    g_w5500_http_dbc_runtime_errors = 0u;
    g_w5500_http_dbc_runtime_valid = 1u;
    g_w5500_http_dbc_runtime_result = 0u;
    ++g_w5500_http_dbc_runtime_load_count;
  }
  taskEXIT_CRITICAL();
  return status;
}

static int active_copy_rule_requirements(
  DbcSelectedRuleRequirement *requirements,
  size_t *rule_count) {
  if (requirements == NULL || rule_count == NULL) {
    return 1;
  }
  *rule_count = 0u;
  if (g_rule_file_v5_load_result == 0u) {
    for (size_t slot = 0u; slot < RULE_FILE_V2_RULE_COUNT; ++slot) {
      const RuleFileV5Slot *rule = &g_rule_file_v5_current.slots[slot];
      if (!rule->enabled) {
        continue;
      }
      memcpy(g_http_active_rule_keys[*rule_count], rule->signal_key,
             sizeof(g_http_active_rule_keys[*rule_count]));
      requirements[*rule_count] = (DbcSelectedRuleRequirement){
        .enabled = true,
        .key = g_http_active_rule_keys[*rule_count],
        .definition_hash = rule->definition_hash
      };
      ++*rule_count;
    }
    return 0;
  }
  if (g_rule_file_v5_load_result == 1u) {
    return 0;
  }
  return g_rule_file_v5_load_result == 7u ? 2 : 1;
}

int w5500_http_recover_large_dbc_active(void) {
  size_t rule_count = 0u;
  bool available = false;
  uint64_t next_generation = 0u;
  memset(&g_http_active_backend_result, 0,
         sizeof(g_http_active_backend_result));
  if (active_copy_rule_requirements(g_http_active_rule_requirements,
                                    &rule_count) != 0) {
    g_w5500_http_active_recovery_result =
      LARGE_DBC_ACTIVE_STM32_RULE_DEFINITION_CONFLICT;
    return 1;
  }
  const LargeDbcActiveStm32Status status =
    stm32h750_large_dbc_active_recover(
      g_http_active_rule_requirements, rule_count, &g_http_selected_runtime,
      active_publish_runtime, NULL, candidate_progress, NULL, &available,
      &g_http_active_backend_result, &next_generation);
  g_w5500_http_active_recovery_result = (uint32_t)status;
  if (status != LARGE_DBC_ACTIVE_STM32_OK) {
    return 1;
  }
  taskENTER_CRITICAL();
  g_http_active_next_generation = next_generation;
  taskEXIT_CRITICAL();
  return available ? 0 : 1;
}

int w5500_http_recover_large_dbc_candidate(void) {
  bool available = false;
  uint64_t next_generation = 0u;
  DbcCandidateDescriptor recovered;
  memset(&recovered, 0, sizeof(recovered));
  const LargeDbcCandidateStm32Status status =
    stm32h750_large_dbc_candidate_recover(candidate_progress,
                                           NULL,
                                           &available,
                                           &recovered,
                                           &next_generation);
  g_w5500_http_candidate_recovery_result = (uint32_t)status;
  if (status != LARGE_DBC_CANDIDATE_STM32_OK || next_generation == 0u) {
    taskENTER_CRITICAL();
    g_http_dbc_upload_next_generation = 0u;
    taskEXIT_CRITICAL();
    candidate_publish_snapshot(NULL, false);
    return 1;
  }
  taskENTER_CRITICAL();
  g_http_dbc_upload_next_generation = next_generation;
  taskEXIT_CRITICAL();
  candidate_publish_snapshot(available ? &recovered : NULL, available);
  return 0;
}

void w5500_http_request_dbc_reload(void) {
  const uint8_t command = DBC_WORK_COMMAND_RELOAD;

  g_w5500_http_dbc_reload_result = 0xffffffffu;
  g_w5500_http_dbc_reload_complete = 0u;
  if (g_w5500_dbc_reload_queue == NULL ||
      xQueueSend(g_w5500_dbc_reload_queue, &command, 0u) != pdPASS) {
    ++g_w5500_http_dbc_reload_queue_drop_count;
    g_w5500_http_dbc_reload_result = 1u;
    return;
  }
  ++g_w5500_http_dbc_reload_enqueue_count;
}

static int w5500_http_request_candidate_build(const DbcUploadResult *upload) {
  const uint8_t command = DBC_WORK_COMMAND_CANDIDATE;
  uint64_t next_generation;
  taskENTER_CRITICAL();
  next_generation = g_http_dbc_upload_next_generation;
  taskEXIT_CRITICAL();
  if (upload == NULL || g_w5500_dbc_reload_queue == NULL ||
      g_w5500_http_candidate_pending != 0u ||
      g_w5500_http_candidate_active != 0u ||
      upload->generation != next_generation) {
    return 1;
  }
  g_http_candidate_request.generation = upload->generation;
  g_http_candidate_request.source_size = upload->source_size;
  g_http_candidate_request.source_crc32 = upload->source_crc32;
  g_w5500_http_candidate_complete = 0u;
  g_w5500_http_candidate_result = 0xffffffffu;
  g_w5500_http_candidate_pending = 1u;
  if (xQueueSend(g_w5500_dbc_reload_queue, &command, 0u) != pdPASS) {
    g_w5500_http_candidate_pending = 0u;
    ++g_w5500_http_dbc_reload_queue_drop_count;
    return 1;
  }
  ++g_w5500_http_dbc_reload_enqueue_count;
  return 0;
}

static int w5500_http_request_candidate_query(
  const DbcCandidateCatalogQuery *query) {
  const uint8_t command = DBC_WORK_COMMAND_CANDIDATE_QUERY;
  if (query == NULL || query->query_length > LARGE_DBC_QUERY_MAX_BYTES ||
      (query->query_length != 0u && query->query == NULL) ||
      g_w5500_dbc_reload_queue == NULL) {
    return 1;
  }
  taskENTER_CRITICAL();
  if (g_http_candidate_mutation_busy != 0u ||
      g_http_candidate_query_pending != 0u ||
      g_http_candidate_query_active != 0u) {
    taskEXIT_CRITICAL();
    return 1;
  }
  if (query->query_length != 0u) {
    memcpy(g_http_candidate_query_text, query->query, query->query_length);
  }
  g_http_candidate_query_text[query->query_length] = '\0';
  g_http_candidate_query_request = *query;
  g_http_candidate_query_request.query = g_http_candidate_query_text;
  g_http_candidate_query_complete = 0u;
  g_http_candidate_query_result = 0xffffffffu;
  g_http_candidate_query_pending = 1u;
  taskEXIT_CRITICAL();
  if (xQueueSend(g_w5500_dbc_reload_queue, &command, 0u) != pdPASS) {
    taskENTER_CRITICAL();
    g_http_candidate_query_pending = 0u;
    taskEXIT_CRITICAL();
    ++g_w5500_http_dbc_reload_queue_drop_count;
    return 1;
  }
  ++g_w5500_http_dbc_reload_enqueue_count;
  return 0;
}

static int w5500_http_request_candidate_selection(
  const DbcCandidateSelectionMutation *mutation) {
  const uint8_t command = DBC_WORK_COMMAND_CANDIDATE_SELECTION;
  if (mutation == NULL || mutation->candidate_token == NULL ||
      mutation->set_count + mutation->clear_count == 0u ||
      mutation->set_count + mutation->clear_count >
        LARGE_DBC_SELECTION_UPDATE_MAX_ORDINALS ||
      g_w5500_dbc_reload_queue == NULL) {
    return 1;
  }
  taskENTER_CRITICAL();
  if (g_http_candidate_mutation_busy == 0u ||
      g_http_candidate_selection_pending != 0u) {
    taskEXIT_CRITICAL();
    return 1;
  }
  memcpy(g_http_candidate_selection_token, mutation->candidate_token,
         sizeof(g_http_candidate_selection_token));
  if (mutation->set_count != 0u) {
    memcpy(g_http_candidate_selection_set, mutation->set_ordinals,
           mutation->set_count * sizeof(g_http_candidate_selection_set[0]));
  }
  if (mutation->clear_count != 0u) {
    memcpy(g_http_candidate_selection_clear, mutation->clear_ordinals,
           mutation->clear_count * sizeof(g_http_candidate_selection_clear[0]));
  }
  g_http_candidate_selection_request = *mutation;
  g_http_candidate_selection_request.candidate_token =
    g_http_candidate_selection_token;
  g_http_candidate_selection_request.set_ordinals =
    g_http_candidate_selection_set;
  g_http_candidate_selection_request.clear_ordinals =
    g_http_candidate_selection_clear;
  g_http_candidate_selection_request.writes_blocked = false;
  g_http_candidate_selection_complete = 0u;
  g_http_candidate_selection_result = 0xffffffffu;
  g_http_candidate_selection_pending = 1u;
  taskEXIT_CRITICAL();
  if (xQueueSend(g_w5500_dbc_reload_queue, &command, 0u) != pdPASS) {
    taskENTER_CRITICAL();
    g_http_candidate_selection_pending = 0u;
    taskEXIT_CRITICAL();
    ++g_w5500_http_dbc_reload_queue_drop_count;
    return 1;
  }
  ++g_w5500_http_dbc_reload_enqueue_count;
  return 0;
}

static int w5500_http_request_active_commit(void) {
  const uint8_t command = DBC_WORK_COMMAND_ACTIVE;
  size_t rule_count = 0u;
  if (g_w5500_dbc_reload_queue == NULL || g_w5500_http_active_pending != 0u ||
      active_copy_rule_requirements(g_http_active_rule_requirements,
                                    &rule_count) != 0) {
    return 1;
  }
  taskENTER_CRITICAL();
  g_http_active_request = (LargeDbcActiveRequest){
    .active_generation = g_http_active_next_generation,
    .writes_blocked = g_signal_log_control.enabled,
    .rules = g_http_active_rule_requirements,
    .rule_count = rule_count,
    .runtime_snapshot = &g_http_selected_runtime,
    .publish = active_publish_runtime,
    .publish_context = NULL
  };
  g_w5500_http_active_complete = 0u;
  g_w5500_http_active_transaction_result = 0xffffffffu;
  g_w5500_http_active_pending = 1u;
  taskEXIT_CRITICAL();
  if (g_http_active_request.active_generation == 0u ||
      xQueueSend(g_w5500_dbc_reload_queue, &command, 0u) != pdPASS) {
    g_w5500_http_active_pending = 0u;
    ++g_w5500_http_dbc_reload_queue_drop_count;
    return 1;
  }
  ++g_w5500_http_dbc_reload_enqueue_count;
  return 0;
}

int w5500_http_dbc_reload_requested(void) {
  return g_w5500_dbc_reload_queue != NULL &&
                 uxQueueMessagesWaiting(g_w5500_dbc_reload_queue) != 0u
             ? 1
             : 0;
}

int w5500_http_process_dbc_reload(void) {
  uint8_t command = 0u;
  if (g_w5500_dbc_reload_queue == NULL ||
      xQueueReceive(g_w5500_dbc_reload_queue, &command, 0u) != pdPASS) {
    return 1;
  }
  if (command == DBC_WORK_COMMAND_CANDIDATE) {
    DbcCandidateDescriptor published;
    g_w5500_http_candidate_pending = 0u;
    g_w5500_http_candidate_active = 1u;
    const LargeDbcCandidateStm32Status status =
      stm32h750_large_dbc_candidate_commit(&g_http_candidate_request,
                                            candidate_progress,
                                            NULL,
                                            &published);
    g_w5500_http_candidate_active = 0u;
    g_w5500_http_candidate_result = (uint32_t)status;
    g_w5500_http_candidate_complete = 1u;
    if (status != LARGE_DBC_CANDIDATE_STM32_OK) {
      candidate_mutation_end();
      return 2;
    }
    candidate_publish_snapshot(&published, true);
    taskENTER_CRITICAL();
    if (published.generation == UINT64_MAX) {
      g_http_dbc_upload_next_generation = 0u;
    } else {
      g_http_dbc_upload_next_generation = published.generation + 1u;
    }
    taskEXIT_CRITICAL();
    candidate_mutation_end();
    return 0;
  }
  if (command == DBC_WORK_COMMAND_CANDIDATE_QUERY) {
    taskENTER_CRITICAL();
    g_http_candidate_query_pending = 0u;
    g_http_candidate_query_active = 1u;
    taskEXIT_CRITICAL();
    const LargeDbcCandidateStm32Status status =
      stm32h750_large_dbc_candidate_query(&g_http_candidate_query_request,
                                           candidate_progress,
                                           NULL,
                                           &g_http_candidate_query_backend_result);
    taskENTER_CRITICAL();
    g_http_candidate_query_result = (uint32_t)status;
    if (status == LARGE_DBC_CANDIDATE_STM32_OK) {
      g_http_candidate_query_page = g_http_candidate_query_backend_result.page;
      g_http_candidate_query_snapshot =
        g_http_candidate_query_backend_result.snapshot.candidate;
      memcpy(g_http_candidate_query_token,
             g_http_candidate_query_backend_result.snapshot.candidate_token,
             sizeof(g_http_candidate_query_token));
    }
    taskEXIT_CRITICAL();
    if (status == LARGE_DBC_CANDIDATE_STM32_OK) {
      candidate_publish_snapshot(
        &g_http_candidate_query_backend_result.snapshot.candidate, true);
    }
    taskENTER_CRITICAL();
    g_http_candidate_query_active = 0u;
    g_http_candidate_query_complete = 1u;
    taskEXIT_CRITICAL();
    return status == LARGE_DBC_CANDIDATE_STM32_OK ? 0 : 3;
  }
  if (command == DBC_WORK_COMMAND_CANDIDATE_SELECTION) {
    taskENTER_CRITICAL();
    g_http_candidate_selection_pending = 0u;
    taskEXIT_CRITICAL();
    const LargeDbcCandidateStm32Status status =
      stm32h750_large_dbc_candidate_update_selection(
        &g_http_candidate_selection_request,
        candidate_progress,
        NULL,
        &g_http_candidate_selection_backend_result);
    taskENTER_CRITICAL();
    g_http_candidate_selection_result = (uint32_t)status;
    if (status == LARGE_DBC_CANDIDATE_STM32_OK) {
      g_http_candidate_selection_snapshot =
        g_http_candidate_selection_backend_result.candidate;
    }
    taskEXIT_CRITICAL();
    if (status == LARGE_DBC_CANDIDATE_STM32_OK) {
      candidate_publish_snapshot(
        &g_http_candidate_selection_backend_result.candidate, true);
      taskENTER_CRITICAL();
      if (g_http_candidate_selection_backend_result.candidate.generation ==
          UINT64_MAX) {
        g_http_dbc_upload_next_generation = 0u;
      } else {
        g_http_dbc_upload_next_generation =
          g_http_candidate_selection_backend_result.candidate.generation + 1u;
      }
      taskEXIT_CRITICAL();
    }
    candidate_mutation_end();
    taskENTER_CRITICAL();
    g_http_candidate_selection_complete = 1u;
    taskEXIT_CRITICAL();
    return status == LARGE_DBC_CANDIDATE_STM32_OK ? 0 : 4;
  }
  if (command == DBC_WORK_COMMAND_ACTIVE) {
    taskENTER_CRITICAL();
    g_w5500_http_active_pending = 0u;
    g_http_active_request.writes_blocked = g_signal_log_control.enabled;
    taskEXIT_CRITICAL();
    const LargeDbcActiveStm32Status status =
      stm32h750_large_dbc_active_commit(&g_http_active_request,
                                        candidate_progress, NULL,
                                        &g_http_active_backend_result);
    taskENTER_CRITICAL();
    g_w5500_http_active_transaction_result = (uint32_t)status;
    if (status == LARGE_DBC_ACTIVE_STM32_OK) {
      if (g_http_active_backend_result.active.active_generation == UINT64_MAX) {
        g_http_active_next_generation = 0u;
      } else {
        g_http_active_next_generation =
          g_http_active_backend_result.active.active_generation + 1u;
      }
    }
    g_w5500_http_active_complete = 1u;
    taskEXIT_CRITICAL();
    candidate_mutation_end();
    return status == LARGE_DBC_ACTIVE_STM32_OK ? 0 : 5;
  }
  return 1;
}

int w5500_http_dbc_reload_queue_init(void) {
  g_w5500_dbc_reload_queue = xQueueCreate(1u, sizeof(uint8_t));
  g_w5500_http_dbc_reload_queue_ready = g_w5500_dbc_reload_queue != NULL ? 1u : 0u;
  return g_w5500_dbc_reload_queue != NULL ? 0 : 1;
}

int w5500_http_dbc_reload_complete(void) {
  return g_w5500_http_dbc_reload_complete != 0u ? 1 : 0;
}

int w5500_http_dbc_reload_result(void) {
  return (int)g_w5500_http_dbc_reload_result;
}

const DbcDatabase *w5500_http_active_dbc_snapshot(void) {
  return g_w5500_http_dbc_runtime_valid != 0u ? g_http_dbc_runtime_active_db : NULL;
}

int w5500_http_selected_runtime_available(void) {
  return dbc_selected_runtime_active(&g_http_selected_runtime) != NULL ? 1 : 0;
}

size_t w5500_http_decode_selected_frame(const CanFrame *frame,
                                        uint32_t now_ms) {
  return dbc_decode_frame_to_selected_values(&g_http_selected_runtime, frame,
                                             now_ms);
}

size_t w5500_http_selected_rule_snapshots(
  const RuleEngine *engine,
  SignalSnapshot *out_signals,
  size_t out_capacity) {
  const DbcSelectedRuntime *runtime =
    dbc_selected_runtime_active(&g_http_selected_runtime);
  if (engine == NULL || out_signals == NULL || runtime == NULL) {
    return 0u;
  }
  size_t count = 0u;
  for (size_t rule_index = 0u;
       rule_index < engine->rule_count && count < out_capacity;
       ++rule_index) {
    const Rule *rule = &engine->rules[rule_index];
    if (!rule->enabled) {
      continue;
    }
    bool duplicate = false;
    for (size_t existing = 0u; existing < count; ++existing) {
      if (strcmp(out_signals[existing].key, rule->signal_key) == 0) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) {
      continue;
    }
    const DbcSelectedRuntimeSignal *signal =
      dbc_selected_runtime_find_signal(runtime, rule->signal_key);
    SignalValueSnapshot value;
    if (signal == NULL ||
        !dbc_selected_runtime_copy_active_value(
          &g_http_selected_runtime, signal->value_state_index, &value)) {
      continue;
    }
    memset(&out_signals[count], 0, sizeof(out_signals[count]));
    memcpy(out_signals[count].key, signal->key,
           sizeof(out_signals[count].key));
    out_signals[count].value = value.value;
    out_signals[count].updated_ms = value.updated_ms;
    out_signals[count].valid = value.quality == SIGNAL_VALUE_QUALITY_GOOD;
    ++count;
  }
  return count;
}

int w5500_http_selected_signal_definition(const char *key,
                                           uint64_t *definition_hash) {
  const DbcSelectedRuntime *runtime =
    dbc_selected_runtime_active(&g_http_selected_runtime);
  const DbcSelectedRuntimeSignal *signal =
    dbc_selected_runtime_find_signal(runtime, key);
  if (signal == NULL || definition_hash == NULL) {
    return 1;
  }
  *definition_hash = signal->definition_hash;
  return 0;
}

int w5500_http_selected_log_signal(
  uint64_t active_generation,
  uint32_t selection_crc32,
  uint16_t signal_index,
  DbcSelectedRuntimeSignal *signal,
  SignalValueSnapshot *value) {
  const DbcSelectedRuntime *runtime =
    dbc_selected_runtime_active(&g_http_selected_runtime);
  if (runtime == NULL || signal == NULL || value == NULL ||
      runtime->runtime_generation != active_generation ||
      runtime->selection_crc32 != selection_crc32 ||
      signal_index >= runtime->signal_count) {
    return 1;
  }
  *signal = runtime->signals[signal_index];
  if (!dbc_selected_runtime_copy_active_value(
        &g_http_selected_runtime, signal->value_state_index, value)) {
    return 2;
  }
  const DbcSelectedRuntime *current =
    dbc_selected_runtime_active(&g_http_selected_runtime);
  return current == runtime &&
         current->runtime_generation == active_generation &&
         current->selection_crc32 == selection_crc32 ? 0 : 3;
}

static int http_send_bytes(const uint8_t *data, size_t len) {
  uint16_t tx_free = 0u;
  uint16_t tx_wr = 0u;
  if (data == NULL || len == 0u || len > W5500_SOCKET_BUFFER_SIZE) {
    return 1;
  }

  for (uint32_t i = 0u; i < 1000u; ++i) {
    if (s0_read_u16_stable(W5500_S0_TX_FSR, &tx_free) != W5500_OK) {
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
      s0_write_u8(W5500_S0_IR, W5500_S0_IR_SENDOK | W5500_S0_IR_TIMEOUT) != W5500_OK ||
      s0_command(W5500_S0_CR_SEND) != W5500_OK) {
    return 1;
  }

  for (uint32_t i = 0u; i < 2000u; ++i) {
    uint8_t ir = 0u;
    if (s0_read_u8(W5500_S0_IR, &ir) != W5500_OK) {
      return 1;
    }
    if ((ir & W5500_S0_IR_SENDOK) != 0u) {
      if (g_w5500_http_trace_active != 0u && g_w5500_http_trace_header_send_active != 0u) {
        g_w5500_http_trace_header_sendok_tick = HAL_GetTick();
      }
      if (s0_write_u8(W5500_S0_IR, W5500_S0_IR_SENDOK) != W5500_OK) {
        return 1;
      }
      uint16_t post_sendok_tx_fsr = 0u;
      const W5500Result post_sendok_tx_fsr_result =
        s0_read_u16_stable(W5500_S0_TX_FSR, &post_sendok_tx_fsr);
      ++g_w5500_http_trace_send_count;
      g_w5500_http_trace_last_send_len = (uint32_t)len;
      g_w5500_http_trace_tx_total += (uint32_t)len;
      g_w5500_http_trace_post_sendok_tx_fsr_result = (uint32_t)post_sendok_tx_fsr_result;
      g_w5500_http_trace_post_sendok_tx_fsr_value =
        post_sendok_tx_fsr_result == W5500_OK ? post_sendok_tx_fsr : 0xffffffffu;
      g_w5500_http_last_tx_size += (uint32_t)len;
      return 0;
    }
    if ((ir & W5500_S0_IR_TIMEOUT) != 0u) {
      if (s0_write_u8(W5500_S0_IR, W5500_S0_IR_TIMEOUT) != W5500_OK) {
        return 1;
      }
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
  int result;
  if (g_w5500_http_trace_active != 0u) {
    g_w5500_http_trace_header_send_enter_tick = HAL_GetTick();
    g_w5500_http_trace_header_send_active = 1u;
  }
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
    g_w5500_http_trace_header_send_active = 0u;
    return 1;
  }
  result = http_send_bytes((const uint8_t *)header, (size_t)header_len);
  g_w5500_http_trace_header_send_active = 0u;
  return result;
}

static int http_send_response(uint16_t code, const char *content_type, const char *body, size_t body_len) {
  size_t offset = 0u;
  g_w5500_http_last_tx_size = 0u;
  if (http_send_header(code, content_type, body_len) != 0) {
    return 1;
  }
  while (offset < body_len) {
    const size_t remaining = body_len - offset;
    const size_t chunk = remaining < LARGE_DBC_HTTP_RESPONSE_SEGMENT_BYTES ?
      remaining : LARGE_DBC_HTTP_RESPONSE_SEGMENT_BYTES;
    if (http_send_bytes((const uint8_t *)&body[offset], chunk) != 0) {
      return 1;
    }
    offset += chunk;
  }
  return 0;
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
  if (g_w5500_http_trace_active != 0u) {
    g_w5500_http_trace_rx_consume_start_tick = HAL_GetTick();
  }
  const int result = s0_write_u16(W5500_S0_RX_RD, (uint16_t)(rx_rd + rx_size)) == W5500_OK &&
                     s0_command(W5500_S0_CR_RECV) == W5500_OK
                       ? 0
                       : 1;
  if (g_w5500_http_trace_active != 0u) {
    g_w5500_http_trace_rx_consume_end_tick = HAL_GetTick();
  }
  return result;
}

static int http_send_json_error(uint16_t code, const char *error_code, const char *message) {
  char body[192];
  const size_t body_len = build_error_body(body, sizeof(body), error_code, message);
  if (body_len >= sizeof(body)) {
    return 1;
  }
  return http_send_response(code, "application/json", body, body_len);
}

static void http_record_request(uint32_t path_code, uint16_t code);

static int http_candidate_error(uint32_t path_code, uint16_t code,
                                const char *error_code,
                                const char *message) {
  (void)message;
  http_record_request(path_code, code);
  return http_send_json_error(code, error_code, error_code);
}

static bool http_wait_candidate_flag(volatile uint32_t *flag,
                                     uint32_t timeout_ms) {
  for (uint32_t waited = 0u; waited < timeout_ms; ++waited) {
    bool complete;
    taskENTER_CRITICAL();
    complete = *flag != 0u;
    taskEXIT_CRITICAL();
    if (complete) return true;
    vTaskDelay(pdMS_TO_TICKS(1u));
  }
  return false;
}

static int http_handle_candidate_query(const char *request) {
  const char *target = request + sizeof("GET ") - 1u;
  const char *target_end = strchr(target, ' ');
  DbcCandidateCatalogQuery query;
  char query_storage[DBC_CANDIDATE_HTTP_QUERY_STORAGE_BYTES];
  DbcCandidateDescriptor available_candidate;
  if (target_end == NULL ||
      dbc_candidate_http_parse_get_target(
        target, (size_t)(target_end - target), &query, query_storage) !=
        DBC_CANDIDATE_HTTP_OK) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_CANDIDATE_SIGNALS,
                                400u, "invalid_candidate_query",
                                "invalid candidate query");
  }
  if (!candidate_read_snapshot(&available_candidate)) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_CANDIDATE_SIGNALS,
                                404u, "candidate_unavailable",
                                "candidate unavailable");
  }
  if (w5500_http_request_candidate_query(&query) != 0) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_CANDIDATE_SIGNALS,
                                409u, "candidate_busy",
                                "candidate operation in progress");
  }
  if (!http_wait_candidate_flag(&g_http_candidate_query_complete, 30000u)) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_CANDIDATE_SIGNALS,
                                504u, "candidate_query_timeout",
                                "candidate query completion not observed");
  }
  LargeDbcCandidateStm32Status status;
  taskENTER_CRITICAL();
  status = (LargeDbcCandidateStm32Status)g_http_candidate_query_result;
  taskEXIT_CRITICAL();
  if (status == LARGE_DBC_CANDIDATE_STM32_NOT_FOUND) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_CANDIDATE_SIGNALS,
                                404u, "candidate_unavailable",
                                "candidate unavailable");
  }
  if (status == LARGE_DBC_CANDIDATE_STM32_INVALID_ARGUMENT) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_CANDIDATE_SIGNALS,
                                400u, "invalid_candidate_query",
                                "candidate query rejected");
  }
  if (status != LARGE_DBC_CANDIDATE_STM32_OK) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_CANDIDATE_SIGNALS,
                                500u, "candidate_query_failed",
                                "candidate query or verification failed");
  }
  size_t response_len = 0u;
  if (dbc_candidate_http_serialize_catalog_json(
        g_http_candidate_query_token, &g_http_candidate_query_snapshot,
        &g_http_candidate_query_page, g_http_response_body,
        sizeof(g_http_response_body), &response_len) !=
      DBC_CANDIDATE_HTTP_OK) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_CANDIDATE_SIGNALS,
                                500u, "candidate_response_too_large",
                                "candidate response serialization failed");
  }
  http_record_request(W5500_HTTP_PATH_DBC_CANDIDATE_SIGNALS, 200u);
  return http_send_response(200u, "application/json", g_http_response_body,
                            response_len);
}

static int http_handle_selected_signals(const char *request) {
  const char *target = request + sizeof("GET ") - 1u;
  const char *target_end = strchr(target, ' ');
  SignalApiQuery query;
  SignalApiPage page;
  size_t response_len = 0u;
  SignalApiStatus status;

  if (target_end == NULL) {
    return http_candidate_error(W5500_HTTP_PATH_SIGNALS, 400u,
                                "invalid_signals_query",
                                "invalid signals query");
  }
  status = signal_api_parse_get_target(
    target, (size_t)(target_end - target), &query);
  if (status != SIGNAL_API_OK) {
    return http_candidate_error(W5500_HTTP_PATH_SIGNALS, 400u,
                                signal_api_status_string(status),
                                "invalid signals query");
  }
  status = signal_api_build_selected_page(
    &g_http_selected_runtime, &query, HAL_GetTick(),
    LARGE_DBC_SIGNAL_STALE_AFTER_MS, &page);
  if (status == SIGNAL_API_INVALID_STATE) {
    return http_candidate_error(W5500_HTTP_PATH_SIGNALS, 503u,
                                "selected_runtime_unavailable",
                                "selected runtime unavailable");
  }
  if (status == SIGNAL_API_SNAPSHOT_BUSY) {
    return http_candidate_error(W5500_HTTP_PATH_SIGNALS, 409u,
                                "snapshot_busy",
                                "selected runtime changed");
  }
  if (status != SIGNAL_API_OK) {
    return http_candidate_error(W5500_HTTP_PATH_SIGNALS, 500u,
                                signal_api_status_string(status),
                                "signals snapshot failed");
  }
  status = signal_api_serialize_selected_page(
    &page, g_http_response_body, sizeof(g_http_response_body), &response_len);
  if (status != SIGNAL_API_OK) {
    return http_candidate_error(W5500_HTTP_PATH_SIGNALS, 500u,
                                signal_api_status_string(status),
                                "signals response failed");
  }
  g_w5500_http_signals_count = page.item_count;
  http_record_request(W5500_HTTP_PATH_SIGNALS, 200u);
  return http_send_response(200u, "application/json", g_http_response_body,
                            response_len);
}

static bool http_candidate_token_matches(
  const char *token, const DbcCandidateDescriptor *candidate) {
  uint64_t generation = 0u;
  uint32_t source_size = 0u;
  uint32_t source_crc32 = 0u;
  return dbc_candidate_token_parse(token, &generation, &source_size,
                                   &source_crc32) ==
           DBC_CANDIDATE_FORMAT_OK &&
         generation == candidate->generation &&
         source_size == candidate->source_size &&
         source_crc32 == candidate->source_crc32;
}

static int http_handle_candidate_selection(const char *body,
                                           size_t body_len) {
  DbcCandidateHttpSelectionForm form;
  DbcCandidateDescriptor candidate;
  const DbcCandidateHttpStatus parse_status =
    dbc_candidate_http_parse_selection_form(body, body_len, &form);
  if (parse_status != DBC_CANDIDATE_HTTP_OK) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_SELECTION, 400u,
                                "invalid_selection",
                                "invalid selection form");
  }
  if (!candidate_read_snapshot(&candidate)) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_SELECTION, 404u,
                                "candidate_unavailable",
                                "candidate unavailable");
  }
  if (!http_candidate_token_matches(form.candidate_token, &candidate)) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_SELECTION, 409u,
                                "stale_candidate_token",
                                "candidate token changed");
  }
  for (uint8_t i = 0u; i < form.set_count; ++i) {
    if (form.set_ordinals[i] >= candidate.catalog_signal_count) {
      return http_candidate_error(W5500_HTTP_PATH_DBC_SELECTION, 400u,
                                  "ordinal_out_of_range",
                                  "selection ordinal out of range");
    }
  }
  for (uint8_t i = 0u; i < form.clear_count; ++i) {
    if (form.clear_ordinals[i] >= candidate.catalog_signal_count) {
      return http_candidate_error(W5500_HTTP_PATH_DBC_SELECTION, 400u,
                                  "ordinal_out_of_range",
                                  "selection ordinal out of range");
    }
  }
  const HttpCandidateMutationGate gate = candidate_mutation_begin();
  if (gate != HTTP_CANDIDATE_MUTATION_OK) {
    return http_candidate_error(
      W5500_HTTP_PATH_DBC_SELECTION, 409u,
      gate == HTTP_CANDIDATE_MUTATION_LOGGING ?
        "logging_active" : "candidate_busy",
      gate == HTTP_CANDIDATE_MUTATION_LOGGING ?
        "selection rejected while logging" :
        "candidate mutation in progress");
  }
  const DbcCandidateSelectionMutation mutation = {
    .candidate_token = form.candidate_token,
    .set_ordinals = form.set_ordinals,
    .set_count = form.set_count,
    .clear_ordinals = form.clear_ordinals,
    .clear_count = form.clear_count,
    .writes_blocked = false
  };
  if (w5500_http_request_candidate_selection(&mutation) != 0) {
    candidate_mutation_end();
    return http_candidate_error(W5500_HTTP_PATH_DBC_SELECTION, 409u,
                                "candidate_busy",
                                "candidate selection queue unavailable");
  }
  if (!http_wait_candidate_flag(&g_http_candidate_selection_complete,
                                120000u)) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_SELECTION, 504u,
                                "selection_completion_timeout",
                                "selection completion not observed; query candidate state");
  }
  LargeDbcCandidateStm32Status status;
  taskENTER_CRITICAL();
  status = (LargeDbcCandidateStm32Status)g_http_candidate_selection_result;
  candidate = g_http_candidate_selection_snapshot;
  taskEXIT_CRITICAL();
  if (status == LARGE_DBC_CANDIDATE_STM32_TOKEN_MISMATCH) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_SELECTION, 409u,
                                "stale_candidate_token",
                                "candidate token changed");
  }
  if (status == LARGE_DBC_CANDIDATE_STM32_WRITE_BLOCKED) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_SELECTION, 409u,
                                "logging_active",
                                "selection rejected while logging");
  }
  if (status == LARGE_DBC_CANDIDATE_STM32_GENERATION_EXHAUSTED) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_SELECTION, 409u,
                                "generation_exhausted",
                                "candidate generation exhausted");
  }
  if (status == LARGE_DBC_CANDIDATE_STM32_NOT_FOUND) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_SELECTION, 404u,
                                "candidate_unavailable",
                                "candidate unavailable");
  }
  if (status == LARGE_DBC_CANDIDATE_STM32_SELECTION_FAILED) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_SELECTION, 422u,
                                "selection_limit",
                                "selection exceeds signal or message limit");
  }
  if (status == LARGE_DBC_CANDIDATE_STM32_INVALID_ARGUMENT) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_SELECTION, 400u,
                                "invalid_selection",
                                "selection rejected");
  }
  if (status != LARGE_DBC_CANDIDATE_STM32_OK) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_SELECTION, 507u,
                                "selection_persist_failed",
                                "selection transaction failed");
  }
  char candidate_token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES];
  size_t response_len = 0u;
  if (dbc_candidate_token_format(candidate.generation, candidate.source_size,
                                 candidate.source_crc32, candidate_token) !=
        DBC_CANDIDATE_FORMAT_OK ||
      dbc_candidate_http_serialize_selection_json(
        candidate_token, &candidate, g_http_response_body,
        sizeof(g_http_response_body), &response_len) !=
        DBC_CANDIDATE_HTTP_OK) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_SELECTION, 500u,
                                "selection_response_failed",
                                "selection committed but response serialization failed");
  }
  http_record_request(W5500_HTTP_PATH_DBC_SELECTION, 200u);
  return http_send_response(200u, "application/json", g_http_response_body,
                            response_len);
}

static int http_handle_active_commit(void) {
  size_t ignored_rule_count = 0u;
  const int rule_source = active_copy_rule_requirements(
    g_http_active_rule_requirements, &ignored_rule_count);
  if (rule_source == 2) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_ACTIVE, 409u,
                                "rule_definition_unbound",
                                "enabled legacy rule requires confirmation");
  }
  if (rule_source != 0) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_ACTIVE, 500u,
                                "rule_source_invalid",
                                "rule source invalid");
  }
  const HttpCandidateMutationGate gate = candidate_mutation_begin();
  if (gate != HTTP_CANDIDATE_MUTATION_OK) {
    return http_candidate_error(
      W5500_HTTP_PATH_DBC_ACTIVE, 409u,
      gate == HTTP_CANDIDATE_MUTATION_LOGGING ?
        "logging_active" : "candidate_busy",
      "active commit blocked");
  }
  if (w5500_http_request_active_commit() != 0) {
    candidate_mutation_end();
    return http_candidate_error(W5500_HTTP_PATH_DBC_ACTIVE, 409u,
                                "active_busy",
                                "active queue unavailable");
  }
  if (!http_wait_candidate_flag(&g_w5500_http_active_complete, 120000u)) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_ACTIVE, 504u,
                                "active_completion_timeout",
                                "query runtime state");
  }
  LargeDbcActiveStm32Status status;
  LargeDbcActiveResult active;
  taskENTER_CRITICAL();
  status = (LargeDbcActiveStm32Status)
    g_w5500_http_active_transaction_result;
  active = g_http_active_backend_result;
  taskEXIT_CRITICAL();
  if (status == LARGE_DBC_ACTIVE_STM32_LOGGING_ACTIVE) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_ACTIVE, 409u,
                                "logging_active",
                                "active rejected while logging");
  }
  if (status == LARGE_DBC_ACTIVE_STM32_RULE_KEY_MISSING) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_ACTIVE, 409u,
                                "rule_key_missing",
                                "enabled rule key not selected");
  }
  if (status == LARGE_DBC_ACTIVE_STM32_RULE_DEFINITION_CONFLICT) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_ACTIVE, 409u,
                                "rule_definition_conflict",
                                "enabled rule definition changed");
  }
  if (status == LARGE_DBC_ACTIVE_STM32_NOT_FOUND) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_ACTIVE, 404u,
                                "candidate_unavailable",
                                "candidate unavailable");
  }
  if (status == LARGE_DBC_ACTIVE_STM32_GENERATION_EXHAUSTED) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_ACTIVE, 409u,
                                "generation_exhausted",
                                "active generation exhausted");
  }
  if (status == LARGE_DBC_ACTIVE_STM32_VERIFY_FAILED ||
      status == LARGE_DBC_ACTIVE_STM32_RUNTIME_FAILED) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_ACTIVE, 422u,
                                "active_validation_failed",
                                "candidate cannot form runtime");
  }
  if (status != LARGE_DBC_ACTIVE_STM32_OK) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_ACTIVE, 507u,
                                "active_persist_failed",
                                "active transaction failed");
  }
  const int written = snprintf(
    g_http_response_body, sizeof(g_http_response_body),
    "{\"ok\":true,\"data\":{\"activeGeneration\":\"%08lX%08lX\","
    "\"candidateGeneration\":\"%08lX%08lX\",\"selectedCount\":%u,"
    "\"selectedMessageCount\":%u,\"selectionCrc32\":\"%08lX\","
    "\"runtimeSlot\":%u}}",
    (unsigned long)(active.active.active_generation >> 32u),
    (unsigned long)(uint32_t)active.active.active_generation,
    (unsigned long)(active.active.candidate_generation >> 32u),
    (unsigned long)(uint32_t)active.active.candidate_generation,
    (unsigned)active.active.selected_count,
    (unsigned)active.active.selected_message_count,
    (unsigned long)active.active.selection_crc32,
    (unsigned)active.runtime_slot);
  if (written < 0 || (size_t)written >= sizeof(g_http_response_body)) {
    return http_candidate_error(W5500_HTTP_PATH_DBC_ACTIVE, 500u,
                                "active_response_failed",
                                "active committed; query runtime");
  }
  http_record_request(W5500_HTTP_PATH_DBC_ACTIVE, 200u);
  return http_send_response(200u, "application/json", g_http_response_body,
                            (size_t)written);
}

enum {
  HTTP_RULES_POST = 1u,
  HTTP_RULES_PUT = 2u,
  HTTP_RULES_DELETE = 3u,
  HTTP_RULE_FIELD_SLOT = 1u << 0,
  HTTP_RULE_FIELD_ENABLED = 1u << 1,
  HTTP_RULE_FIELD_RELAY = 1u << 2,
  HTTP_RULE_FIELD_SIGNAL_KEY = 1u << 3,
  HTTP_RULE_FIELD_THRESHOLD = 1u << 4,
  HTTP_RULE_FIELD_ACTION = 1u << 5,
  HTTP_RULE_FIELD_DELAY = 1u << 6,
  HTTP_RULE_FIELD_TIMEOUT = 1u << 7,
  HTTP_RULE_FIELD_SAFE_STATE = 1u << 8,
  HTTP_RULE_FIELD_PRIORITY = 1u << 9,
  HTTP_RULE_FIELD_ALL = HTTP_RULE_FIELD_ENABLED | HTTP_RULE_FIELD_RELAY | HTTP_RULE_FIELD_SIGNAL_KEY |
                        HTTP_RULE_FIELD_THRESHOLD | HTTP_RULE_FIELD_ACTION |
                        HTTP_RULE_FIELD_DELAY | HTTP_RULE_FIELD_TIMEOUT |
                        HTTP_RULE_FIELD_SAFE_STATE | HTTP_RULE_FIELD_PRIORITY,
};

static bool http_form_span_is(const char *text, size_t len, const char *expected) {
  const size_t expected_len = strlen(expected);
  return len == expected_len && memcmp(text, expected, len) == 0;
}

static bool http_form_parse_u32(const char *text, size_t len, uint32_t *value) {
  uint32_t result = 0u;

  if (len == 0u || value == NULL) {
    return false;
  }
  for (size_t i = 0u; i < len; ++i) {
    const uint8_t ch = (uint8_t)text[i];
    if (ch < (uint8_t)'0' || ch > (uint8_t)'9' ||
        result > (UINT32_MAX - (uint32_t)(ch - (uint8_t)'0')) / 10u) {
      return false;
    }
    result = (result * 10u) + (uint32_t)(ch - (uint8_t)'0');
  }
  *value = result;
  return true;
}

static bool http_form_parse_u64(const char *text, size_t len, uint64_t *value) {
  uint64_t result = 0u;

  if (len == 0u || value == NULL) {
    return false;
  }
  for (size_t i = 0u; i < len; ++i) {
    const uint8_t ch = (uint8_t)text[i];
    if (ch < (uint8_t)'0' || ch > (uint8_t)'9' ||
        result > (UINT64_MAX - (uint64_t)(ch - (uint8_t)'0')) / 10u) {
      return false;
    }
    result = (result * 10u) + (uint64_t)(ch - (uint8_t)'0');
  }
  *value = result;
  return true;
}

static bool http_form_parse_i32(const char *text, size_t len, int32_t *value) {
  uint32_t magnitude;
  bool negative = false;

  if (text == NULL || value == NULL || len == 0u) return false;
  if (text[0] == '-') {
    negative = true;
    ++text;
    --len;
  }
  if (!http_form_parse_u32(text, len, &magnitude) ||
      (!negative && magnitude > INT32_MAX) ||
      (negative && magnitude > (uint32_t)INT32_MAX + 1u)) {
    return false;
  }
  *value = negative ? (magnitude == (uint32_t)INT32_MAX + 1u ? INT32_MIN : -(int32_t)magnitude)
                    : (int32_t)magnitude;
  return true;
}

static bool http_form_parse_log_control(const char *body,
                                        size_t body_len,
                                        bool *enabled,
                                        uint32_t *sample_period_ms,
                                        bool *has_unix_ms,
                                        uint64_t *unix_ms,
                                        int32_t *utc_offset_min) {
  uint32_t fields = 0u;
  size_t offset = 0u;

  if (body == NULL || enabled == NULL || sample_period_ms == NULL || has_unix_ms == NULL ||
      unix_ms == NULL || utc_offset_min == NULL || body_len == 0u || body[body_len - 1u] == '&') {
    return false;
  }
  *has_unix_ms = false;
  *utc_offset_min = 0;
  while (offset < body_len) {
    size_t equals = offset;
    size_t entry_end;
    while (equals < body_len && body[equals] != '=' && body[equals] != '&') ++equals;
    if (equals == offset || equals == body_len || body[equals] != '=') return false;
    entry_end = equals + 1u;
    while (entry_end < body_len && body[entry_end] != '&') ++entry_end;
    if (http_form_span_is(&body[offset], equals - offset, "enabled")) {
      if ((fields & 1u) != 0u || entry_end != equals + 2u ||
          (body[equals + 1u] != '0' && body[equals + 1u] != '1')) return false;
      *enabled = body[equals + 1u] == '1';
      fields |= 1u;
    } else if (http_form_span_is(&body[offset], equals - offset, "samplePeriodMs")) {
      if ((fields & 2u) != 0u ||
          !http_form_parse_u32(&body[equals + 1u], entry_end - equals - 1u, sample_period_ms)) return false;
      fields |= 2u;
    } else if (http_form_span_is(&body[offset], equals - offset, "unixMs")) {
      if (*has_unix_ms ||
          !http_form_parse_u64(&body[equals + 1u], entry_end - equals - 1u, unix_ms)) return false;
      *has_unix_ms = true;
    } else if (http_form_span_is(&body[offset], equals - offset, "utcOffsetMin")) {
      if ((fields & 4u) != 0u ||
          !http_form_parse_i32(&body[equals + 1u], entry_end - equals - 1u, utc_offset_min) ||
          !signal_log_time_offset_is_valid(*utc_offset_min)) return false;
      fields |= 4u;
    } else {
      return false;
    }
    offset = entry_end + (entry_end < body_len ? 1u : 0u);
  }
  return (fields & 3u) == 3u;
}

static bool http_form_parse_time_sync(const char *body,
                                      size_t body_len,
                                      uint64_t *unix_ms,
                                      int32_t *utc_offset_min) {
  uint32_t fields = 0u;
  size_t offset = 0u;

  if (body == NULL || unix_ms == NULL || utc_offset_min == NULL || body_len == 0u ||
      body[body_len - 1u] == '&') return false;
  *utc_offset_min = 0;
  while (offset < body_len) {
    size_t equals = offset;
    size_t entry_end;
    while (equals < body_len && body[equals] != '=' && body[equals] != '&') ++equals;
    if (equals == offset || equals == body_len || body[equals] != '=') return false;
    entry_end = equals + 1u;
    while (entry_end < body_len && body[entry_end] != '&') ++entry_end;
    if (http_form_span_is(&body[offset], equals - offset, "unixMs")) {
      if ((fields & 1u) != 0u ||
          !http_form_parse_u64(&body[equals + 1u], entry_end - equals - 1u, unix_ms)) return false;
      fields |= 1u;
    } else if (http_form_span_is(&body[offset], equals - offset, "utcOffsetMin")) {
      if ((fields & 2u) != 0u ||
          !http_form_parse_i32(&body[equals + 1u], entry_end - equals - 1u, utc_offset_min) ||
          !signal_log_time_offset_is_valid(*utc_offset_min)) return false;
      fields |= 2u;
    } else {
      return false;
    }
    offset = entry_end + (entry_end < body_len ? 1u : 0u);
  }
  return (fields & 1u) != 0u;
}

static size_t http_append_u64_decimal(char *body, size_t len, size_t used, uint64_t value) {
  char digits[20];
  size_t count = 0u;

  if (body == NULL || used >= len) {
    return len;
  }
  do {
    digits[count++] = (char)('0' + value % 10u);
    value /= 10u;
  } while (value != 0u);
  if (count >= len - used) {
    return len;
  }
  while (count > 0u) {
    body[used++] = digits[--count];
  }
  body[used] = '\0';
  return used;
}

static const char *selected_log_state_name(uint8_t state) {
  switch ((SelectedSignalLogState)state) {
    case SELECTED_SIGNAL_LOG_STOPPED: return "STOPPED";
    case SELECTED_SIGNAL_LOG_STARTING: return "STARTING";
    case SELECTED_SIGNAL_LOG_ACTIVE: return "ACTIVE";
    case SELECTED_SIGNAL_LOG_STOPPING: return "STOPPING";
    case SELECTED_SIGNAL_LOG_FAILED: return "FAILED";
    default: return "INVALID";
  }
}

static size_t build_log_control_body(char *body, size_t len) {
  SignalLogControl control;
  SelectedSignalLogSession session;
  uint64_t unix_ms = 0u;
  size_t used;
  int written;
  const uint32_t now_ms = HAL_GetTick();

  taskENTER_CRITICAL();
  control = g_signal_log_control;
  session = g_selected_signal_log_session;
  taskEXIT_CRITICAL();
  (void)signal_log_control_unix_ms(&control, now_ms, &unix_ms);
  written = snprintf(body,
                     len,
                     "{\"ok\":true,\"data\":{\"enabled\":%s,\"samplePeriodMs\":%lu,\"timeSynced\":%s,\"unixMs\":",
                     control.enabled ? "true" : "false",
                     (unsigned long)control.sample_period_ms,
                     control.time_synced ? "true" : "false");
  if (written < 0 || (size_t)written >= len) {
    return len;
  }
  used = http_append_u64_decimal(body, len, (size_t)written, unix_ms);
  if (used >= len) {
    return len;
  }
  written = snprintf(
    body + used, len - used,
    ",\"utcOffsetMin\":%d,\"path\":\"%s\","
    "\"state\":\"%s\",\"activeGeneration\":\"%08lX%08lX\","
    "\"selectionCrc32\":\"%08lX\",\"selectedCount\":%u}}",
    (int)control.utc_offset_min,
    session.identity.csv_path,
    selected_log_state_name(session.state),
    (unsigned long)(session.identity.active_generation >> 32u),
    (unsigned long)session.identity.active_generation,
    (unsigned long)session.identity.selection_crc32,
    (unsigned)session.identity.selected_count);
  if (written < 0 || (size_t)written >= len - used) {
    return len;
  }
  return used + (size_t)written;
}

static int http_handle_time_sync(const char *body, size_t body_len) {
  uint64_t unix_ms;
  int32_t utc_offset_min;

  if (!http_form_parse_time_sync(body, body_len, &unix_ms, &utc_offset_min)) {
    http_record_request(W5500_HTTP_PATH_TIME_SYNC, 400u);
    return http_send_json_error(400u, "invalid_time_sync", "unixMs is required");
  }
  taskENTER_CRITICAL();
  (void)signal_log_control_sync_time(&g_signal_log_control, unix_ms, HAL_GetTick(), utc_offset_min);
  taskEXIT_CRITICAL();
  {
    const size_t response_len = build_log_control_body(g_http_response_body, sizeof(g_http_response_body));
    if (response_len >= sizeof(g_http_response_body)) return 1;
    http_record_request(W5500_HTTP_PATH_TIME_SYNC, 200u);
    return http_send_response(200u, "application/json", g_http_response_body, response_len);
  }
}

static int http_handle_log_control(const char *body, size_t body_len) {
  bool enabled = false;
  bool has_unix_ms;
  uint32_t sample_period_ms;
  uint64_t unix_ms = 0u;
  int32_t utc_offset_min;
  SignalLogControl control;
  SelectedSignalLogSession session;

  if (!http_form_parse_log_control(body, body_len, &enabled, &sample_period_ms, &has_unix_ms, &unix_ms,
                                   &utc_offset_min)) {
    http_record_request(W5500_HTTP_PATH_LOG_CONTROL, 400u);
    return http_send_json_error(400u, "invalid_log_control", "enabled and samplePeriodMs required");
  }
  if (sample_period_ms < SIGNAL_LOG_SAMPLE_PERIOD_MIN_MS ||
      sample_period_ms > SIGNAL_LOG_SAMPLE_PERIOD_MAX_MS) {
    http_record_request(W5500_HTTP_PATH_LOG_CONTROL, 400u);
    return http_send_json_error(400u, "invalid_log_control", "samplePeriodMs must be 100..10000");
  }
  taskENTER_CRITICAL();
  control = g_signal_log_control;
  session = g_selected_signal_log_session;
  taskEXIT_CRITICAL();

  if (!enabled) {
    if (session.state == SELECTED_SIGNAL_LOG_STARTING ||
        session.state == SELECTED_SIGNAL_LOG_STOPPING) {
      return http_candidate_error(W5500_HTTP_PATH_LOG_CONTROL, 409u,
                                  "log_transition_busy", "");
    }
    if (session.state == SELECTED_SIGNAL_LOG_ACTIVE) {
      if (selected_signal_log_session_transition(
            &session, SELECTED_SIGNAL_LOG_STOPPING) !=
          SELECTED_SIGNAL_LOG_OK) {
        return http_candidate_error(W5500_HTTP_PATH_LOG_CONTROL, 409u,
                                    "invalid_log_state", "");
      }
    } else if (session.state == SELECTED_SIGNAL_LOG_FAILED) {
      (void)selected_signal_log_session_transition(
        &session, SELECTED_SIGNAL_LOG_STOPPED);
      control.enabled = false;
    } else {
      control.enabled = false;
    }
    taskENTER_CRITICAL();
    g_selected_signal_log_session = session;
    g_signal_log_control = control;
    taskEXIT_CRITICAL();
  } else {
    if (session.state == SELECTED_SIGNAL_LOG_FAILED) {
      return http_candidate_error(W5500_HTTP_PATH_LOG_CONTROL, 409u,
                                  "log_failed_needs_reset", "");
    }
    const HttpCandidateMutationGate gate = candidate_mutation_begin();
    if (gate != HTTP_CANDIDATE_MUTATION_OK) {
      return http_candidate_error(
        W5500_HTTP_PATH_LOG_CONTROL, 409u,
        gate == HTTP_CANDIDATE_MUTATION_LOGGING ?
          "logging_active" : "candidate_busy",
        "");
    }
    const uint32_t now_ms = HAL_GetTick();
    if (!signal_log_control_set(&control, true, sample_period_ms,
                                has_unix_ms, unix_ms, now_ms,
                                utc_offset_min)) {
      candidate_mutation_end();
      return http_candidate_error(W5500_HTTP_PATH_LOG_CONTROL, 400u,
                                  "time_not_synced", "");
    }
    const DbcSelectedRuntime *runtime =
      dbc_selected_runtime_active(&g_http_selected_runtime);
    if (runtime == NULL) {
      candidate_mutation_end();
      return http_candidate_error(W5500_HTTP_PATH_LOG_CONTROL, 503u,
                                  "selected_runtime_unavailable", "");
    }
    if (!selected_signal_log_admissible(runtime->signal_count,
                                        sample_period_ms)) {
      candidate_mutation_end();
      return http_candidate_error(W5500_HTTP_PATH_LOG_CONTROL, 422u,
                                  "rate_limit", "");
    }
    SelectedSignalLogIdentity identity;
    memset(&identity, 0, sizeof(identity));
    identity.meta_format_version = SELECTED_SIGNAL_LOG_META_FORMAT_VERSION;
    identity.csv_format_version = SELECTED_SIGNAL_LOG_CSV_FORMAT_VERSION;
    identity.selected_count = runtime->signal_count;
    identity.active_generation = runtime->runtime_generation;
    identity.candidate_generation = runtime->candidate_generation;
    identity.source_size = runtime->source_size;
    identity.source_crc32 = runtime->source_crc32;
    identity.selection_crc32 = runtime->selection_crc32;
    identity.sample_period_ms = sample_period_ms;
    identity.start_unix_ms = control.session_start_unix_ms;
    memcpy(identity.firmware_build_id, "large-dbc-g-v1",
           sizeof("large-dbc-g-v1"));
    SelectedSignalLogStatus log_status = selected_signal_log_format_paths(
      identity.start_unix_ms, control.session_utc_offset_min,
      identity.csv_path, sizeof(identity.csv_path),
      identity.meta_path, sizeof(identity.meta_path));
    if (log_status != SELECTED_SIGNAL_LOG_OK) {
      candidate_mutation_end();
      return http_candidate_error(W5500_HTTP_PATH_LOG_CONTROL, 500u,
                                  "log_path_failed", "");
    }
    size_t ignored_size = 0u;
    const int csv_size_result = stm32h750_tf_file_size_locked(
      identity.csv_path, &ignored_size);
    ignored_size = 0u;
    const int meta_size_result = stm32h750_tf_file_size_locked(
      identity.meta_path, &ignored_size);
    if ((csv_size_result != 0 && csv_size_result != FR_NO_FILE) ||
        (meta_size_result != 0 && meta_size_result != FR_NO_FILE)) {
      candidate_mutation_end();
      return http_candidate_error(W5500_HTTP_PATH_LOG_CONTROL, 500u,
                                  "log_path_check_failed", "");
    }
    selected_signal_log_session_init(&session);
    log_status = selected_signal_log_session_prepare(
      &session, &identity, csv_size_result == 0, meta_size_result == 0);
    if (log_status != SELECTED_SIGNAL_LOG_OK) {
      candidate_mutation_end();
      const uint16_t code =
        log_status == SELECTED_SIGNAL_LOG_RATE_LIMIT ? 422u : 409u;
      return http_candidate_error(
        W5500_HTTP_PATH_LOG_CONTROL, code,
        selected_signal_log_status_string(log_status), "");
    }
    taskENTER_CRITICAL();
    g_signal_log_control = control;
    g_selected_signal_log_session = session;
    taskEXIT_CRITICAL();
    candidate_mutation_end();
  }
  {
    const size_t response_len = build_log_control_body(g_http_response_body, sizeof(g_http_response_body));
    if (response_len >= sizeof(g_http_response_body)) return 1;
    http_record_request(W5500_HTTP_PATH_LOG_CONTROL, 200u);
    return http_send_response(200u, "application/json", g_http_response_body, response_len);
  }
}

enum {
  HTTP_MANUAL_FIELD_ENABLED = 1u << 0,
  HTTP_MANUAL_FIELD_RELAY1 = 1u << 1,
  HTTP_MANUAL_FIELD_RELAY2 = 1u << 2,
  HTTP_MANUAL_FIELD_ALL = HTTP_MANUAL_FIELD_ENABLED | HTTP_MANUAL_FIELD_RELAY1 |
                          HTTP_MANUAL_FIELD_RELAY2,
};

static bool http_form_parse_manual_override(const char *body,
                                            size_t body_len,
                                            uint32_t *enabled,
                                            uint32_t *relay1,
                                            uint32_t *relay2) {
  uint32_t fields = 0u;
  size_t offset = 0u;

  if (body == NULL || enabled == NULL || relay1 == NULL || relay2 == NULL || body_len == 0u ||
      body[body_len - 1u] == '&') {
    return false;
  }
  while (offset < body_len) {
    size_t equals = offset;
    size_t entry_end = offset;
    while (equals < body_len && body[equals] != '=' && body[equals] != '&') ++equals;
    if (equals == offset || equals == body_len || body[equals] != '=') return false;
    entry_end = equals + 1u;
    while (entry_end < body_len && body[entry_end] != '&') ++entry_end;
    if (entry_end != equals + 2u ||
        (body[equals + 1u] != '0' && body[equals + 1u] != '1')) {
      return false;
    }
    const uint32_t value = (uint32_t)(body[equals + 1u] - '0');
    if (http_form_span_is(&body[offset], equals - offset, "enabled")) {
      if ((fields & HTTP_MANUAL_FIELD_ENABLED) != 0u) return false;
      *enabled = value;
      fields |= HTTP_MANUAL_FIELD_ENABLED;
    } else if (http_form_span_is(&body[offset], equals - offset, "relay1")) {
      if ((fields & HTTP_MANUAL_FIELD_RELAY1) != 0u) return false;
      *relay1 = value;
      fields |= HTTP_MANUAL_FIELD_RELAY1;
    } else if (http_form_span_is(&body[offset], equals - offset, "relay2")) {
      if ((fields & HTTP_MANUAL_FIELD_RELAY2) != 0u) return false;
      *relay2 = value;
      fields |= HTTP_MANUAL_FIELD_RELAY2;
    } else {
      return false;
    }
    offset = entry_end + (entry_end < body_len ? 1u : 0u);
  }
  return fields == HTTP_MANUAL_FIELD_ALL;
}

static int http_handle_manual_override(const char *body, size_t body_len) {
  uint32_t enabled = 0u;
  uint32_t relay1 = 0u;
  uint32_t relay2 = 0u;
  uint32_t request_seq = 0u;
  uint32_t applied_seq = 0u;
  const uint32_t start_tick = HAL_GetTick();

  if (!http_form_parse_manual_override(body, body_len, &enabled, &relay1, &relay2)) {
    http_record_request(W5500_HTTP_PATH_RELAY_MANUAL, 400u);
    return http_send_json_error(400u, "invalid_manual_override", "enabled relay1 relay2 must be 0 or 1");
  }
  if (rule_task_manual_override_submit(enabled, relay1, relay2, &request_seq) != 0) {
    http_record_request(W5500_HTTP_PATH_RELAY_MANUAL, 500u);
    return http_send_json_error(500u, "manual_override_submit_failed", "manual override submit failed");
  }
  do {
    rule_task_manual_override_snapshot(NULL, NULL, NULL, NULL, &applied_seq, NULL, NULL);
    if (applied_seq == request_seq) {
      const size_t response_len = build_manual_relay_body(g_http_response_body,
                                                          sizeof(g_http_response_body));
      if (response_len >= sizeof(g_http_response_body)) return 1;
      http_record_request(W5500_HTTP_PATH_RELAY_MANUAL, 200u);
      return http_send_response(200u, "application/json", g_http_response_body, response_len);
    }
    vTaskDelay(pdMS_TO_TICKS(1u));
  } while ((uint32_t)(HAL_GetTick() - start_tick) < 100u);

  http_record_request(W5500_HTTP_PATH_RELAY_MANUAL, 500u);
  return http_send_json_error(500u, "manual_override_timeout", "rule task did not apply manual override");
}

static int http_handle_can_tx(const char *body, size_t body_len) {
  CanTxControlConfig config;
  uint32_t request_seq;
  size_t response_len;

  if (!can_tx_control_parse_form(body, body_len, &config)) {
    http_record_request(W5500_HTTP_PATH_CAN_TX, 400u);
    return http_send_json_error(400u, "invalid_can_tx", "enabled id dlc data periodMs required");
  }
  if (can2_tx_control_submit(&config, &request_seq) != 0) {
    http_record_request(W5500_HTTP_PATH_CAN_TX, 500u);
    return http_send_json_error(500u, "can_tx_submit_failed", "can tx submit failed");
  }
  (void)request_seq;
  response_len = build_can_tx_body(g_http_response_body, sizeof(g_http_response_body));
  if (response_len >= sizeof(g_http_response_body)) return 1;
  http_record_request(W5500_HTTP_PATH_CAN_TX, 200u);
  return http_send_response(200u, "application/json", g_http_response_body, response_len);
}

static bool http_form_parse_rule(const char *body,
                                 size_t body_len,
                                 bool include_slot,
                                 uint32_t *slot,
                                 RuleFileV4Slot *rule) {
  uint32_t fields = 0u;
  size_t offset = 0u;

  if (body == NULL || rule == NULL || (include_slot && slot == NULL) || body_len == 0u ||
      body[body_len - 1u] == '&') {
    return false;
  }
  while (offset < body_len) {
    size_t entry_end = offset;
    size_t equals = offset;
    bool has_equals = false;
    uint32_t value;

    while (entry_end < body_len && body[entry_end] != '&') {
      if (body[entry_end] == '=' && !has_equals) {
        equals = entry_end;
        has_equals = true;
      } else if (body[entry_end] == '=' || body[entry_end] == '\0') {
        return false;
      }
      ++entry_end;
    }
    if (!has_equals || equals == offset || equals + 1u >= entry_end) {
      return false;
    }
    const char *key = &body[offset];
    const size_t key_len = equals - offset;
    const char *value_text = &body[equals + 1u];
    const size_t value_len = entry_end - equals - 1u;

    if (include_slot && http_form_span_is(key, key_len, "slot")) {
      if ((fields & HTTP_RULE_FIELD_SLOT) != 0u || !http_form_parse_u32(value_text, value_len, &value)) {
        return false;
      }
      *slot = value;
      fields |= HTTP_RULE_FIELD_SLOT;
    } else if (http_form_span_is(key, key_len, "enabled")) {
      if ((fields & HTTP_RULE_FIELD_ENABLED) != 0u || !http_form_parse_u32(value_text, value_len, &value) || value > 1u) return false;
      rule->enabled = value != 0u;
      fields |= HTTP_RULE_FIELD_ENABLED;
    } else if (http_form_span_is(key, key_len, "relay")) {
      if ((fields & HTTP_RULE_FIELD_RELAY) != 0u || !http_form_parse_u32(value_text, value_len, &value) || value >= RULE_RELAY_COUNT) return false;
      rule->relay = (uint8_t)value;
      fields |= HTTP_RULE_FIELD_RELAY;
    } else if (http_form_span_is(key, key_len, "signalKey")) {
      if ((fields & HTTP_RULE_FIELD_SIGNAL_KEY) != 0u || value_len >= sizeof(rule->signal_key)) return false;
      memcpy(rule->signal_key, value_text, value_len);
      rule->signal_key[value_len] = '\0';
      fields |= HTTP_RULE_FIELD_SIGNAL_KEY;
    } else if (http_form_span_is(key, key_len, "threshold")) {
      if ((fields & HTTP_RULE_FIELD_THRESHOLD) != 0u ||
          !rule_file_parse_decimal(value_text, value_len, &rule->threshold)) return false;
      fields |= HTTP_RULE_FIELD_THRESHOLD;
    } else if (http_form_span_is(key, key_len, "action")) {
      if ((fields & HTTP_RULE_FIELD_ACTION) != 0u) return false;
      if (http_form_span_is(value_text, value_len, "on")) rule->action_state = RELAY_STATE_ON;
      else if (http_form_span_is(value_text, value_len, "off")) rule->action_state = RELAY_STATE_OFF;
      else return false;
      fields |= HTTP_RULE_FIELD_ACTION;
    } else if (http_form_span_is(key, key_len, "delayMs")) {
      if ((fields & HTTP_RULE_FIELD_DELAY) != 0u || !http_form_parse_u32(value_text, value_len, &rule->delay_ms)) return false;
      fields |= HTTP_RULE_FIELD_DELAY;
    } else if (http_form_span_is(key, key_len, "timeoutMs")) {
      if ((fields & HTTP_RULE_FIELD_TIMEOUT) != 0u || !http_form_parse_u32(value_text, value_len, &rule->timeout_ms)) return false;
      fields |= HTTP_RULE_FIELD_TIMEOUT;
    } else if (http_form_span_is(key, key_len, "safeState")) {
      if ((fields & HTTP_RULE_FIELD_SAFE_STATE) != 0u) return false;
      if (http_form_span_is(value_text, value_len, "on")) rule->safe_state = RELAY_STATE_ON;
      else if (http_form_span_is(value_text, value_len, "off")) rule->safe_state = RELAY_STATE_OFF;
      else return false;
      fields |= HTTP_RULE_FIELD_SAFE_STATE;
    } else if (http_form_span_is(key, key_len, "priority")) {
      if ((fields & HTTP_RULE_FIELD_PRIORITY) != 0u || !http_form_parse_u32(value_text, value_len, &value) || value > UINT8_MAX) return false;
      rule->priority = (uint8_t)value;
      fields |= HTTP_RULE_FIELD_PRIORITY;
    } else {
      return false;
    }
    offset = entry_end + (entry_end < body_len ? 1u : 0u);
  }
  return fields == (HTTP_RULE_FIELD_ALL | (include_slot ? HTTP_RULE_FIELD_SLOT : 0u));
}

static int http_validate_rule_signal_key(const char *signal_key) {
  uint64_t definition_hash = 0u;
  if (w5500_http_selected_runtime_available() == 0) {
    return -1;
  }
  return w5500_http_selected_signal_definition(signal_key,
                                                &definition_hash) == 0 ? 0 : 1;
}

static void rule_v5_slot_from_v4(const RuleFileV4Slot *source,
                                 RuleFileV5Slot *target) {
  const uint64_t definition_hash = target->definition_hash;
  memset(target, 0, sizeof(*target));
  target->enabled = source->enabled;
  target->relay = source->relay;
  memcpy(target->signal_key, source->signal_key,
         sizeof(target->signal_key));
  target->threshold = source->threshold;
  target->action_state = source->action_state;
  target->delay_ms = source->delay_ms;
  target->timeout_ms = source->timeout_ms;
  target->safe_state = source->safe_state;
  target->priority = source->priority;
  target->definition_hash = definition_hash;
}

static void rule_v4_slot_from_v5(const RuleFileV5Slot *source,
                                 RuleFileV4Slot *target) {
  memset(target, 0, sizeof(*target));
  target->enabled = source->enabled;
  target->relay = source->relay;
  memcpy(target->signal_key, source->signal_key,
         sizeof(target->signal_key));
  target->threshold = source->threshold;
  target->action_state = source->action_state;
  target->delay_ms = source->delay_ms;
  target->timeout_ms = source->timeout_ms;
  target->safe_state = source->safe_state;
  target->priority = source->priority;
}

static bool rule_v5_seed_disabled_keys(RuleFileV5 *rules) {
  if (rules == NULL) {
    return false;
  }
  const DbcSelectedRuntime *runtime =
    dbc_selected_runtime_active(&g_http_selected_runtime);
  for (size_t slot = 0u; slot < RULE_FILE_V2_RULE_COUNT; ++slot) {
    if (rules->slots[slot].signal_key[0] != '\0') {
      continue;
    }
    if (rules->slots[slot].enabled) {
      return false;
    }
    if (runtime == NULL || runtime->signal_count == 0u) {
      return false;
    }
    const DbcSelectedRuntimeSignal *signal =
      &runtime->signals[slot < runtime->signal_count ? slot : 0u];
    memcpy(rules->slots[slot].signal_key, signal->key,
           sizeof(rules->slots[slot].signal_key));
    rules->slots[slot].definition_hash = signal->definition_hash;
  }
  return true;
}

static int http_handle_rules_write(uint32_t operation, int path_slot, const char *body, size_t body_len) {
  RuleFileV4 candidate;
  RuleFileV5 candidate_v5 = {0};
  RuleFileV4Slot replacement = {0};
  uint32_t slot = path_slot >= 0 ? (uint32_t)path_slot : 0u;
  const uint32_t generation = g_rule_task_config_generation;
  uint16_t code = 200u;

  if (g_signal_log_control.enabled) {
    return http_candidate_error(W5500_HTTP_PATH_RULES, 409u,
                                "logging_active", "");
  }
  if (g_rule_file_v5_load_result != 0u &&
      g_rule_file_v5_load_result != 1u &&
      g_rule_file_v5_load_result != 7u) {
    return http_candidate_error(W5500_HTTP_PATH_RULES, 409u,
                                "v5_rule_file_invalid", "");
  }
  if (g_rule_file_v5_load_result != 0u && operation != HTTP_RULES_POST) {
    return http_candidate_error(W5500_HTTP_PATH_RULES, 409u,
                                "v5_rule_confirmation_required", "");
  }

  memset(&candidate, 0, sizeof(candidate));
  candidate_v5 = g_rule_file_v5_current;
  for (size_t candidate_slot = 0u;
       candidate_slot < RULE_FILE_V2_RULE_COUNT;
       ++candidate_slot) {
    rule_v4_slot_from_v5(&candidate_v5.slots[candidate_slot],
                         &candidate.slots[candidate_slot]);
  }
  if (operation == HTTP_RULES_DELETE) {
    if (body_len != 0u) {
      http_record_request(W5500_HTTP_PATH_RULES, 400u);
      return http_send_json_error(400u, "invalid_rule", "delete body unsupported");
    }
    if (!candidate.slots[slot].enabled) {
      http_record_request(W5500_HTTP_PATH_RULES, 409u);
      return http_send_json_error(409u, "rule_disabled", "rule already disabled");
    }
    candidate.slots[slot].enabled = false;
  } else {
    if (!http_form_parse_rule(body, body_len, operation == HTTP_RULES_POST, &slot, &replacement) ||
        replacement.delay_ms > replacement.timeout_ms) {
      http_record_request(W5500_HTTP_PATH_RULES, 400u);
      return http_send_json_error(400u, "invalid_rule", "invalid rule form");
    }
    if (slot >= RULE_FILE_V2_RULE_COUNT) {
      http_record_request(W5500_HTTP_PATH_RULES, 404u);
      return http_send_json_error(404u, "not_found", "rule slot not found");
    }
    const int signal_key_result = http_validate_rule_signal_key(replacement.signal_key);
    if (signal_key_result < 0) {
      http_record_request(W5500_HTTP_PATH_RULES, 409u);
      return http_send_json_error(409u, "dbc_unavailable", "active dbc required");
    }
    if (signal_key_result != 0) {
      http_record_request(W5500_HTTP_PATH_RULES, 400u);
      return http_send_json_error(400u, "invalid_signal_key", "signal key not in active dbc");
    }
    if (operation == HTTP_RULES_POST) {
      if (replacement.enabled == false) {
        http_record_request(W5500_HTTP_PATH_RULES, 400u);
        return http_send_json_error(400u, "invalid_rule", "post must enable rule");
      }
      if (candidate.slots[slot].enabled) {
        http_record_request(W5500_HTTP_PATH_RULES, 409u);
        return http_send_json_error(409u, "rule_exists", "rule already enabled");
      }
      code = 201u;
    }
    candidate.slots[slot] = replacement;
  }
  if (candidate.slots[0].priority == candidate.slots[1].priority ||
      candidate.slots[0].delay_ms > candidate.slots[0].timeout_ms ||
      candidate.slots[1].delay_ms > candidate.slots[1].timeout_ms) {
    http_record_request(W5500_HTTP_PATH_RULES, 400u);
    return http_send_json_error(400u, "invalid_rule", "invalid rule set");
  }
  for (size_t candidate_slot = 0u;
       candidate_slot < RULE_FILE_V2_RULE_COUNT;
       ++candidate_slot) {
    rule_v5_slot_from_v4(&candidate.slots[candidate_slot],
                         &candidate_v5.slots[candidate_slot]);
    if (candidate_v5.slots[candidate_slot].enabled) {
      uint64_t definition_hash = 0u;
      if (w5500_http_selected_signal_definition(
            candidate_v5.slots[candidate_slot].signal_key,
            &definition_hash) != 0) {
        http_record_request(W5500_HTTP_PATH_RULES, 409u);
        return http_send_json_error(409u, "rule_key_missing",
                                    "enabled rule key is not selected");
      }
      candidate_v5.slots[candidate_slot].definition_hash = definition_hash;
    }
  }
  if (!rule_v5_seed_disabled_keys(&candidate_v5)) {
    http_record_request(W5500_HTTP_PATH_RULES, 409u);
    return http_send_json_error(409u, "dbc_unavailable",
                                "selected runtime required");
  }

  g_rule_file_v5_pending = candidate_v5;
  g_rule_file_v5_save_result = 0xffffffffu;
  g_rule_file_v5_save_request = 1u;
  for (uint32_t wait_ms = 0u; wait_ms < 2000u; ++wait_ms) {
    if (g_rule_file_v5_save_request == 0u &&
        g_rule_file_v5_save_result != 0xffffffffu) break;
    vTaskDelay(pdMS_TO_TICKS(1u));
  }
  if (g_rule_file_v5_save_request != 0u || g_rule_file_v5_save_result != 0u) {
    http_record_request(W5500_HTTP_PATH_RULES, 500u);
    return http_send_json_error(500u, "rule_save_failed", "rule file save failed");
  }
  for (uint32_t wait_ms = 0u; wait_ms < 2000u; ++wait_ms) {
    if (g_rule_task_config_generation != generation && g_rule_task_engine_reload == 0u &&
        g_rule_task_config_result == 0u) break;
    vTaskDelay(pdMS_TO_TICKS(1u));
  }
  if (g_rule_task_config_generation == generation || g_rule_task_engine_reload != 0u ||
      g_rule_task_config_result != 0u) {
    http_record_request(W5500_HTTP_PATH_RULES, 500u);
    return http_send_json_error(500u, "rule_reload_failed", "rule reload failed");
  }
  const size_t response_len = build_rules_body(g_http_response_body,
                                               sizeof(g_http_response_body),
                                               (int)slot);
  if (response_len >= sizeof(g_http_response_body)) return 1;
  http_record_request(W5500_HTTP_PATH_RULES, code);
  return http_send_response(code, "application/json", g_http_response_body, response_len);
}

static void http_record_request(uint32_t path_code, uint16_t code) {
  if (g_w5500_http_trace_active != 0u) {
    g_w5500_http_trace_record_tick = HAL_GetTick();
  }
  g_w5500_http_request_count++;
  g_w5500_http_last_path = path_code;
  g_w5500_http_last_code = code;
}

static int http_handle_dbc_upload_complete(const DbcUploadResult *result) {
  if (w5500_http_request_candidate_build(result) != 0) {
    candidate_mutation_end();
    return http_upload_error_response(409u,
                                      "candidate_busy",
                                      "candidate build unavailable");
  }
  const int response_len = snprintf(
    g_http_response_body,
    sizeof(g_http_response_body),
    "{\"ok\":true,\"data\":{\"stagedOnly\":true,\"candidateQueued\":true,\"generation\":\"%08lX%08lX\",\"sourceSize\":%lu,\"sourceCrc32\":\"%08lX\",\"tmpPath\":\"%s\"}}",
    (unsigned long)(result->generation >> 32u),
    (unsigned long)(uint32_t)result->generation,
    (unsigned long)result->source_size,
    (unsigned long)result->source_crc32,
    stm32h750_tf_large_dbc_upload_path());
  if (response_len <= 0 || (size_t)response_len >= sizeof(g_http_response_body)) {
    return 1;
  }
  g_w5500_http_dbc_upload_crc32 = result->source_crc32;
  g_w5500_http_dbc_upload_lines = 0u;
  g_w5500_http_dbc_upload_messages = 0u;
  g_w5500_http_dbc_upload_signals = 0u;
  g_w5500_http_dbc_upload_skipped = 0u;
  g_w5500_http_dbc_upload_errors = 0u;
  ++g_w5500_http_dbc_upload_count;
  http_record_request(W5500_HTTP_PATH_DBC_UPLOAD, 202u);
  return http_send_response(202u,
                            "application/json",
                            g_http_response_body,
                            (size_t)response_len);
}

static int http_upload_error_response(uint16_t code,
                                      const char *error_code,
                                      const char *message) {
  http_upload_update_diagnostics();
  http_record_request(W5500_HTTP_PATH_DBC_UPLOAD, code);
  return http_send_json_error(code, error_code, message) == 0 ?
    W5500_HTTP_HANDLE_OK : W5500_HTTP_HANDLE_ERROR;
}

static int http_upload_finish_if_complete(uint32_t now_ms) {
  if (g_http_dbc_upload.received_size != g_http_dbc_upload.expected_size) {
    http_upload_update_diagnostics();
    return W5500_HTTP_HANDLE_WAIT;
  }
  const DbcUploadStatus status =
    dbc_upload_finish(&g_http_dbc_upload, now_ms, &g_http_dbc_upload_completed);
  http_upload_update_diagnostics();
  if (status != DBC_UPLOAD_STATUS_OK) {
    candidate_mutation_end();
    return http_upload_error_response(507u,
                                      "upload_finalize_failed",
                                      "dbc upload sync or close failed");
  }
  return http_handle_dbc_upload_complete(&g_http_dbc_upload_completed) == 0 ?
    W5500_HTTP_HANDLE_OK : W5500_HTTP_HANDLE_ERROR;
}

static int http_upload_start_request(const uint8_t *request,
                                     size_t read_len,
                                     uint16_t rx_rd,
                                     uint16_t rx_size) {
  DbcUploadHttpHeader header;
  uint64_t next_generation;
  const DbcUploadHttpStatus header_status =
    dbc_upload_http_parse_header(request, read_len, &header);
  if (header_status == DBC_UPLOAD_HTTP_INCOMPLETE &&
      read_len + 1u < W5500_HTTP_REQUEST_BUFFER_SIZE) {
    return W5500_HTTP_HANDLE_WAIT;
  }
  if (header_status != DBC_UPLOAD_HTTP_OK) {
    if (http_consume_rx(rx_rd, rx_size) != 0) {
      return W5500_HTTP_HANDLE_ERROR;
    }
    if (header_status == DBC_UPLOAD_HTTP_CONTENT_LENGTH_LIMIT) {
      return http_upload_error_response(413u,
                                        "payload_too_large",
                                        "dbc upload exceeds 262144 bytes");
    }
    if (header_status == DBC_UPLOAD_HTTP_CONTENT_LENGTH_MISSING) {
      return http_upload_error_response(411u,
                                        "length_required",
                                        "Content-Length required");
    }
    if (header_status == DBC_UPLOAD_HTTP_CONTENT_TYPE_MISSING ||
        header_status == DBC_UPLOAD_HTTP_CONTENT_TYPE_REJECTED ||
        header_status == DBC_UPLOAD_HTTP_TRANSFER_ENCODING_REJECTED) {
      return http_upload_error_response(415u,
                                        "unsupported_upload_encoding",
                                        "text/plain with Content-Length required");
    }
    return http_upload_error_response(400u,
                                      dbc_upload_http_status_string(header_status),
                                      "invalid dbc upload request");
  }
  if ((size_t)rx_size > header.header_bytes + header.content_length) {
    if (http_consume_rx(rx_rd, rx_size) != 0) {
      return W5500_HTTP_HANDLE_ERROR;
    }
    return http_upload_error_response(400u,
                                      "body_overrun",
                                      "request exceeds Content-Length");
  }
  if (g_w5500_http_candidate_pending != 0u ||
      g_w5500_http_candidate_active != 0u) {
    if (http_consume_rx(rx_rd, rx_size) != 0) {
      return W5500_HTTP_HANDLE_ERROR;
    }
    return http_upload_error_response(409u,
                                      "candidate_busy",
                                      "candidate build in progress");
  }
  taskENTER_CRITICAL();
  next_generation = g_http_dbc_upload_next_generation;
  taskEXIT_CRITICAL();
  if (next_generation == 0u) {
    if (http_consume_rx(rx_rd, rx_size) != 0) {
      return W5500_HTTP_HANDLE_ERROR;
    }
    return http_upload_error_response(409u,
                                      "generation_exhausted",
                                      "upload generation exhausted");
  }
  const HttpCandidateMutationGate mutation_gate = candidate_mutation_begin();
  if (mutation_gate != HTTP_CANDIDATE_MUTATION_OK) {
    if (http_consume_rx(rx_rd, rx_size) != 0) {
      return W5500_HTTP_HANDLE_ERROR;
    }
    return http_upload_error_response(
      409u,
      mutation_gate == HTTP_CANDIDATE_MUTATION_LOGGING ?
        "logging_active" : "candidate_busy",
      mutation_gate == HTTP_CANDIDATE_MUTATION_LOGGING ?
        "dbc upload rejected while logging" :
        "candidate mutation in progress");
  }
  if (http_upload_reset_state() != 0) {
    candidate_mutation_end();
    return http_upload_error_response(500u,
                                      "upload_state_failed",
                                      "upload state unavailable");
  }
  DbcUploadStatus status = dbc_upload_start(&g_http_dbc_upload,
                                            next_generation,
                                            header.content_length,
                                            HAL_GetTick());
  http_upload_update_diagnostics();
  if (status != DBC_UPLOAD_STATUS_OK) {
    candidate_mutation_end();
    return http_upload_error_response(507u,
                                      "upload_open_failed",
                                      "dbc upload tmp open failed");
  }
  size_t first_body = header.body_bytes;
  if (first_body > LARGE_DBC_UPLOAD_CHUNK_BYTES) {
    first_body = LARGE_DBC_UPLOAD_CHUNK_BYTES;
  }
  if (first_body != 0u) {
    status = dbc_upload_feed(&g_http_dbc_upload,
                             request + header.header_bytes,
                             first_body,
                             HAL_GetTick());
    http_upload_update_diagnostics();
    if (status != DBC_UPLOAD_STATUS_OK) {
      candidate_mutation_end();
      return http_upload_error_response(507u,
                                        "upload_write_failed",
                                        "dbc upload tmp write failed");
    }
  }
  const size_t consumed = header.header_bytes + first_body;
  if (consumed > UINT16_MAX ||
      http_consume_rx(rx_rd, (uint16_t)consumed) != 0) {
    (void)dbc_upload_cancel(&g_http_dbc_upload);
    ++g_w5500_http_dbc_upload_abort_count;
    http_upload_update_diagnostics();
    candidate_mutation_end();
    return W5500_HTTP_HANDLE_ERROR;
  }
  return http_upload_finish_if_complete(HAL_GetTick());
}

static int http_upload_process_body(uint16_t rx_size) {
  const uint32_t now_ms = HAL_GetTick();
  if (g_http_dbc_upload.phase != DBC_UPLOAD_STATE_RECEIVING) {
    return W5500_HTTP_HANDLE_ERROR;
  }
  if (rx_size == 0u) {
    const DbcUploadStatus timeout =
      dbc_upload_check_timeout(&g_http_dbc_upload, now_ms);
    if (timeout == DBC_UPLOAD_STATUS_OK) {
      http_upload_update_diagnostics();
      return W5500_HTTP_HANDLE_WAIT;
    }
    if (timeout == DBC_UPLOAD_STATUS_IDLE_TIMEOUT) {
      ++g_w5500_http_dbc_upload_idle_timeout_count;
    } else if (timeout == DBC_UPLOAD_STATUS_TOTAL_TIMEOUT) {
      ++g_w5500_http_dbc_upload_total_timeout_count;
    }
    ++g_w5500_http_dbc_upload_abort_count;
    candidate_mutation_end();
    return http_upload_error_response(408u,
                                      "upload_timeout",
                                      "dbc upload timed out");
  }
  const uint32_t remaining =
    g_http_dbc_upload.expected_size - g_http_dbc_upload.received_size;
  if ((uint32_t)rx_size > remaining) {
    (void)dbc_upload_cancel(&g_http_dbc_upload);
    ++g_w5500_http_dbc_upload_abort_count;
    candidate_mutation_end();
    return http_upload_error_response(400u,
                                      "body_overrun",
                                      "request exceeds Content-Length");
  }
  uint16_t amount = rx_size;
  if (amount > LARGE_DBC_UPLOAD_CHUNK_BYTES) {
    amount = LARGE_DBC_UPLOAD_CHUNK_BYTES;
  }
  uint16_t rx_rd = 0u;
  if (s0_read_u16(W5500_S0_RX_RD, &rx_rd) != W5500_OK ||
      socket_buffer_read(W5500_S0_RX_BLOCK,
                         rx_rd,
                         g_http_static_chunk,
                         amount) != W5500_OK) {
    (void)dbc_upload_cancel(&g_http_dbc_upload);
    ++g_w5500_http_dbc_upload_abort_count;
    http_upload_update_diagnostics();
    candidate_mutation_end();
    return W5500_HTTP_HANDLE_ERROR;
  }
  const DbcUploadStatus status =
    dbc_upload_feed(&g_http_dbc_upload,
                    g_http_static_chunk,
                    amount,
                    now_ms);
  http_upload_update_diagnostics();
  if (status != DBC_UPLOAD_STATUS_OK) {
    ++g_w5500_http_dbc_upload_abort_count;
    candidate_mutation_end();
    return http_upload_error_response(507u,
                                      "upload_write_failed",
                                      "dbc upload tmp write failed");
  }
  if (http_consume_rx(rx_rd, amount) != 0) {
    (void)dbc_upload_cancel(&g_http_dbc_upload);
    ++g_w5500_http_dbc_upload_abort_count;
    http_upload_update_diagnostics();
    candidate_mutation_end();
    return W5500_HTTP_HANDLE_ERROR;
  }
  return http_upload_finish_if_complete(HAL_GetTick());
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

static __attribute__((unused)) int http_handle_dbc_active(void) {
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
  w5500_http_request_dbc_reload();
  for (uint32_t wait_ms = 0u; wait_ms < 100u && !w5500_http_dbc_reload_complete(); ++wait_ms) {
    vTaskDelay(pdMS_TO_TICKS(1u));
  }
  if (!w5500_http_dbc_reload_complete() || w5500_http_dbc_reload_result() != 0) {
    dbc_record_active_report(&report, 0u);
    http_record_request(W5500_HTTP_PATH_DBC_ACTIVE, 500u);
    return http_send_json_error(500u, "runtime_load_failed", "dbc runtime load failed");
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
  if (g_w5500_http_trace_active != 0u) {
    g_w5500_http_trace_request_read_start_tick = HAL_GetTick();
  }
  if (s0_read_u16(W5500_S0_RX_RD, &rx_rd) != W5500_OK ||
      socket_buffer_read(W5500_S0_RX_BLOCK, rx_rd, (uint8_t *)request, read_len) != W5500_OK) {
    return 1;
  }
  if (g_w5500_http_trace_active != 0u) {
    g_w5500_http_trace_request_read_end_tick = HAL_GetTick();
  }
  request[read_len] = '\0';
  g_w5500_http_last_rx_size = rx_size;

  const char *header_end = strstr(request, "\r\n\r\n");
  uint32_t rules_operation = 0u;
  int rules_slot = -1;
  if (request_path_is(request, "POST", "/api/rules")) {
    rules_operation = HTTP_RULES_POST;
  } else if (request_path_is(request, "PUT", "/api/rules/0")) {
    rules_operation = HTTP_RULES_PUT;
    rules_slot = 0;
  } else if (request_path_is(request, "PUT", "/api/rules/1")) {
    rules_operation = HTTP_RULES_PUT;
    rules_slot = 1;
  } else if (request_path_is(request, "DELETE", "/api/rules/0")) {
    rules_operation = HTTP_RULES_DELETE;
    rules_slot = 0;
  } else if (request_path_is(request, "DELETE", "/api/rules/1")) {
    rules_operation = HTTP_RULES_DELETE;
    rules_slot = 1;
  }
  if (rules_operation != 0u) {
    size_t content_length = 0u;
    bool has_content_length;
    size_t body_offset;

    if (header_end == NULL) {
      if (read_len + 1u < W5500_HTTP_REQUEST_BUFFER_SIZE) return W5500_HTTP_HANDLE_WAIT;
      if (http_consume_rx(rx_rd, rx_size) != 0) return W5500_HTTP_HANDLE_ERROR;
      http_record_request(W5500_HTTP_PATH_RULES, 400u);
      return http_send_json_error(400u, "bad_request", "rules header too large");
    }
    body_offset = (size_t)((header_end + 4u) - request);
    if (body_offset > read_len) return W5500_HTTP_HANDLE_WAIT;
    const bool body_optional = rules_operation == HTTP_RULES_DELETE;
    has_content_length = http_parse_content_length(request, header_end, &content_length);
    if ((!has_content_length && !body_optional) ||
        content_length > W5500_HTTP_RULES_BODY_MAX ||
        (has_content_length && (size_t)rx_size < body_offset + content_length)) {
      if ((has_content_length && (size_t)rx_size < body_offset + content_length) &&
          read_len + 1u < W5500_HTTP_REQUEST_BUFFER_SIZE) return W5500_HTTP_HANDLE_WAIT;
      if (http_consume_rx(rx_rd, rx_size) != 0) return W5500_HTTP_HANDLE_ERROR;
      http_record_request(W5500_HTTP_PATH_RULES, 400u);
      return http_send_json_error(400u, "bad_request", "invalid rules body");
    }
    if ((!body_optional && content_length == 0u) ||
        body_offset + content_length > read_len ||
        (size_t)rx_size != body_offset + content_length) {
      if (http_consume_rx(rx_rd, rx_size) != 0) return W5500_HTTP_HANDLE_ERROR;
      http_record_request(W5500_HTTP_PATH_RULES, 400u);
      return http_send_json_error(400u, "bad_request", "invalid rules body");
    }
    if (http_consume_rx(rx_rd, rx_size) != 0) return W5500_HTTP_HANDLE_ERROR;
    request[body_offset + content_length] = '\0';
    return http_handle_rules_write(rules_operation, rules_slot, &request[body_offset], content_length) == 0
             ? W5500_HTTP_HANDLE_OK
             : W5500_HTTP_HANDLE_ERROR;
  }
  if (request_path_is(request, "POST", "/api/time/sync")) {
    size_t content_length = 0u;
    size_t body_offset;

    if (header_end == NULL) {
      if (read_len + 1u < W5500_HTTP_REQUEST_BUFFER_SIZE) return W5500_HTTP_HANDLE_WAIT;
      if (http_consume_rx(rx_rd, rx_size) != 0) return W5500_HTTP_HANDLE_ERROR;
      http_record_request(W5500_HTTP_PATH_TIME_SYNC, 400u);
      return http_send_json_error(400u, "bad_request", "time sync header too large");
    }
    body_offset = (size_t)((header_end + 4u) - request);
    if (body_offset > read_len) return W5500_HTTP_HANDLE_WAIT;
    if (!http_parse_content_length(request, header_end, &content_length) || content_length == 0u ||
        content_length > W5500_HTTP_TIME_SYNC_BODY_MAX || (size_t)rx_size < body_offset + content_length) {
      if ((size_t)rx_size < body_offset + content_length && read_len + 1u < W5500_HTTP_REQUEST_BUFFER_SIZE) {
        return W5500_HTTP_HANDLE_WAIT;
      }
      if (http_consume_rx(rx_rd, rx_size) != 0) return W5500_HTTP_HANDLE_ERROR;
      http_record_request(W5500_HTTP_PATH_TIME_SYNC, 400u);
      return http_send_json_error(400u, "bad_request", "invalid time sync body");
    }
    if (body_offset + content_length > read_len || (size_t)rx_size != body_offset + content_length) {
      if (http_consume_rx(rx_rd, rx_size) != 0) return W5500_HTTP_HANDLE_ERROR;
      http_record_request(W5500_HTTP_PATH_TIME_SYNC, 400u);
      return http_send_json_error(400u, "bad_request", "invalid time sync body");
    }
    if (http_consume_rx(rx_rd, rx_size) != 0) return W5500_HTTP_HANDLE_ERROR;
    request[body_offset + content_length] = '\0';
    return http_handle_time_sync(&request[body_offset], content_length) == 0
             ? W5500_HTTP_HANDLE_OK
             : W5500_HTTP_HANDLE_ERROR;
  }
  if (request_path_is(request, "POST", "/api/log/control")) {
    size_t content_length = 0u;
    size_t body_offset;

    if (header_end == NULL) {
      if (read_len + 1u < W5500_HTTP_REQUEST_BUFFER_SIZE) return W5500_HTTP_HANDLE_WAIT;
      if (http_consume_rx(rx_rd, rx_size) != 0) return W5500_HTTP_HANDLE_ERROR;
      http_record_request(W5500_HTTP_PATH_LOG_CONTROL, 400u);
      return http_send_json_error(400u, "bad_request", "log control header too large");
    }
    body_offset = (size_t)((header_end + 4u) - request);
    if (body_offset > read_len) return W5500_HTTP_HANDLE_WAIT;
    if (!http_parse_content_length(request, header_end, &content_length) || content_length == 0u ||
        content_length > W5500_HTTP_LOG_CONTROL_BODY_MAX || (size_t)rx_size < body_offset + content_length) {
      if ((size_t)rx_size < body_offset + content_length && read_len + 1u < W5500_HTTP_REQUEST_BUFFER_SIZE) {
        return W5500_HTTP_HANDLE_WAIT;
      }
      if (http_consume_rx(rx_rd, rx_size) != 0) return W5500_HTTP_HANDLE_ERROR;
      http_record_request(W5500_HTTP_PATH_LOG_CONTROL, 400u);
      return http_send_json_error(400u, "bad_request", "invalid log control body");
    }
    if (body_offset + content_length > read_len || (size_t)rx_size != body_offset + content_length) {
      if (http_consume_rx(rx_rd, rx_size) != 0) return W5500_HTTP_HANDLE_ERROR;
      http_record_request(W5500_HTTP_PATH_LOG_CONTROL, 400u);
      return http_send_json_error(400u, "bad_request", "invalid log control body");
    }
    if (http_consume_rx(rx_rd, rx_size) != 0) return W5500_HTTP_HANDLE_ERROR;
    request[body_offset + content_length] = '\0';
    return http_handle_log_control(&request[body_offset], content_length) == 0
             ? W5500_HTTP_HANDLE_OK
             : W5500_HTTP_HANDLE_ERROR;
  }
  if (request_path_is(request, "POST", "/api/can/tx")) {
    size_t content_length = 0u;
    size_t body_offset;

    if (header_end == NULL) {
      if (read_len + 1u < W5500_HTTP_REQUEST_BUFFER_SIZE) return W5500_HTTP_HANDLE_WAIT;
      if (http_consume_rx(rx_rd, rx_size) != 0) return W5500_HTTP_HANDLE_ERROR;
      http_record_request(W5500_HTTP_PATH_CAN_TX, 400u);
      return http_send_json_error(400u, "bad_request", "can tx header too large");
    }
    body_offset = (size_t)((header_end + 4u) - request);
    if (body_offset > read_len) return W5500_HTTP_HANDLE_WAIT;
    if (!http_parse_content_length(request, header_end, &content_length) || content_length == 0u ||
        content_length > W5500_HTTP_CAN_TX_BODY_MAX || (size_t)rx_size < body_offset + content_length) {
      if ((size_t)rx_size < body_offset + content_length && read_len + 1u < W5500_HTTP_REQUEST_BUFFER_SIZE) {
        return W5500_HTTP_HANDLE_WAIT;
      }
      if (http_consume_rx(rx_rd, rx_size) != 0) return W5500_HTTP_HANDLE_ERROR;
      http_record_request(W5500_HTTP_PATH_CAN_TX, 400u);
      return http_send_json_error(400u, "bad_request", "invalid can tx body");
    }
    if (body_offset + content_length > read_len || (size_t)rx_size != body_offset + content_length) {
      if (http_consume_rx(rx_rd, rx_size) != 0) return W5500_HTTP_HANDLE_ERROR;
      http_record_request(W5500_HTTP_PATH_CAN_TX, 400u);
      return http_send_json_error(400u, "bad_request", "invalid can tx body");
    }
    if (http_consume_rx(rx_rd, rx_size) != 0) return W5500_HTTP_HANDLE_ERROR;
    request[body_offset + content_length] = '\0';
    return http_handle_can_tx(&request[body_offset], content_length) == 0
             ? W5500_HTTP_HANDLE_OK
             : W5500_HTTP_HANDLE_ERROR;
  }
  if (request_path_is(request, "POST", "/api/relay/manual")) {
    size_t content_length = 0u;
    size_t body_offset;

    if (header_end == NULL) {
      if (read_len + 1u < W5500_HTTP_REQUEST_BUFFER_SIZE) return W5500_HTTP_HANDLE_WAIT;
      if (http_consume_rx(rx_rd, rx_size) != 0) return W5500_HTTP_HANDLE_ERROR;
      http_record_request(W5500_HTTP_PATH_RELAY_MANUAL, 400u);
      return http_send_json_error(400u, "bad_request", "manual override header too large");
    }
    body_offset = (size_t)((header_end + 4u) - request);
    if (body_offset > read_len) return W5500_HTTP_HANDLE_WAIT;
    if (!http_parse_content_length(request, header_end, &content_length) || content_length == 0u ||
        content_length > W5500_HTTP_MANUAL_BODY_MAX || (size_t)rx_size < body_offset + content_length) {
      if ((size_t)rx_size < body_offset + content_length && read_len + 1u < W5500_HTTP_REQUEST_BUFFER_SIZE) {
        return W5500_HTTP_HANDLE_WAIT;
      }
      if (http_consume_rx(rx_rd, rx_size) != 0) return W5500_HTTP_HANDLE_ERROR;
      http_record_request(W5500_HTTP_PATH_RELAY_MANUAL, 400u);
      return http_send_json_error(400u, "bad_request", "invalid manual override body");
    }
    if (body_offset + content_length > read_len || (size_t)rx_size != body_offset + content_length) {
      if (http_consume_rx(rx_rd, rx_size) != 0) return W5500_HTTP_HANDLE_ERROR;
      http_record_request(W5500_HTTP_PATH_RELAY_MANUAL, 400u);
      return http_send_json_error(400u, "bad_request", "invalid manual override body");
    }
    if (http_consume_rx(rx_rd, rx_size) != 0) return W5500_HTTP_HANDLE_ERROR;
    request[body_offset + content_length] = '\0';
    return http_handle_manual_override(&request[body_offset], content_length) == 0
             ? W5500_HTTP_HANDLE_OK
             : W5500_HTTP_HANDLE_ERROR;
  }
  if (request_path_is(request, "POST", "/api/dbc/selection")) {
    size_t content_length = 0u;
    size_t body_offset;
    if (header_end == NULL) {
      if (read_len + 1u < W5500_HTTP_REQUEST_BUFFER_SIZE) {
        return W5500_HTTP_HANDLE_WAIT;
      }
      if (http_consume_rx(rx_rd, rx_size) != 0) {
        return W5500_HTTP_HANDLE_ERROR;
      }
      return http_candidate_error(W5500_HTTP_PATH_DBC_SELECTION, 400u,
                                  "bad_request", "selection header too large") == 0 ?
        W5500_HTTP_HANDLE_OK : W5500_HTTP_HANDLE_ERROR;
    }
    body_offset = (size_t)((header_end + 4u) - request);
    if (body_offset > read_len) return W5500_HTTP_HANDLE_WAIT;
    if (!http_parse_content_length(request, header_end, &content_length)) {
      if (http_consume_rx(rx_rd, rx_size) != 0) {
        return W5500_HTTP_HANDLE_ERROR;
      }
      return http_candidate_error(W5500_HTTP_PATH_DBC_SELECTION, 411u,
                                  "length_required",
                                  "selection Content-Length required") == 0 ?
        W5500_HTTP_HANDLE_OK : W5500_HTTP_HANDLE_ERROR;
    }
    if (content_length > LARGE_DBC_SELECTION_REQUEST_BODY_BYTES) {
      if (http_consume_rx(rx_rd, rx_size) != 0) {
        return W5500_HTTP_HANDLE_ERROR;
      }
      return http_candidate_error(W5500_HTTP_PATH_DBC_SELECTION, 413u,
                                  "payload_too_large",
                                  "selection body exceeds 512 bytes") == 0 ?
        W5500_HTTP_HANDLE_OK : W5500_HTTP_HANDLE_ERROR;
    }
    if (!http_content_type_is_selection_form(request, header_end)) {
      if (http_consume_rx(rx_rd, rx_size) != 0) {
        return W5500_HTTP_HANDLE_ERROR;
      }
      return http_candidate_error(
        W5500_HTTP_PATH_DBC_SELECTION, 415u, "unsupported_content_type",
        "application/x-www-form-urlencoded required") == 0 ?
          W5500_HTTP_HANDLE_OK : W5500_HTTP_HANDLE_ERROR;
    }
    if ((size_t)rx_size < body_offset + content_length) {
      if (read_len + 1u < W5500_HTTP_REQUEST_BUFFER_SIZE) {
        return W5500_HTTP_HANDLE_WAIT;
      }
    }
    if (content_length == 0u || body_offset + content_length > read_len ||
        (size_t)rx_size != body_offset + content_length) {
      if (http_consume_rx(rx_rd, rx_size) != 0) {
        return W5500_HTTP_HANDLE_ERROR;
      }
      return http_candidate_error(W5500_HTTP_PATH_DBC_SELECTION, 400u,
                                  "bad_request",
                                  "invalid selection body framing") == 0 ?
        W5500_HTTP_HANDLE_OK : W5500_HTTP_HANDLE_ERROR;
    }
    if (http_consume_rx(rx_rd, rx_size) != 0) {
      return W5500_HTTP_HANDLE_ERROR;
    }
    request[body_offset + content_length] = '\0';
    return http_handle_candidate_selection(&request[body_offset],
                                           content_length) == 0 ?
      W5500_HTTP_HANDLE_OK : W5500_HTTP_HANDLE_ERROR;
  }
  if (request_path_is(request, "POST", "/api/dbc/upload")) {
    return http_upload_start_request((const uint8_t *)request,
                                     read_len,
                                     rx_rd,
                                     rx_size);
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
    return http_handle_active_commit() == 0 ?
      W5500_HTTP_HANDLE_OK : W5500_HTTP_HANDLE_ERROR;
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
  } else if (request_path_is(request, "GET", "/api/log/control")) {
    code = 200u;
    path_code = W5500_HTTP_PATH_LOG_CONTROL;
    body_len = build_log_control_body(body, sizeof(g_http_response_body));
  } else if (request_path_is(request, "GET", "/api/can/tx")) {
    code = 200u;
    path_code = W5500_HTTP_PATH_CAN_TX;
    body_len = build_can_tx_body(body, sizeof(g_http_response_body));
  } else if (request_path_is(request, "GET", "/api/can/tx/signals")) {
    code = 200u;
    path_code = W5500_HTTP_PATH_CAN_TX_SIGNALS;
    body_len = build_can_tx_signals_body(body, sizeof(g_http_response_body));
  } else if (request_path_is(request, "GET", "/api/dbc/runtime")) {
    code = 200u;
    path_code = W5500_HTTP_PATH_DBC_RUNTIME;
    body_len = build_dbc_runtime_body(body, sizeof(g_http_response_body));
  } else if (request_path_is(request, "GET",
                             "/api/dbc/candidate/signals")) {
    return http_handle_candidate_query(request) == 0 ?
      W5500_HTTP_HANDLE_OK : W5500_HTTP_HANDLE_ERROR;
  } else if (request_path_is(request, "GET", "/api/signals")) {
    return http_handle_selected_signals(request) == 0 ?
      W5500_HTTP_HANDLE_OK : W5500_HTTP_HANDLE_ERROR;
  } else if (request_path_is(request, "GET", "/api/rules")) {
    code = 200u;
    path_code = W5500_HTTP_PATH_RULES;
    body_len = build_rules_body(body, sizeof(g_http_response_body), -1);
  } else if (request_path_is(request, "GET", "/api/rules/0")) {
    code = 200u;
    path_code = W5500_HTTP_PATH_RULES;
    body_len = build_rules_body(body, sizeof(g_http_response_body), 0);
  } else if (request_path_is(request, "GET", "/api/rules/1")) {
    code = 200u;
    path_code = W5500_HTTP_PATH_RULES;
    body_len = build_rules_body(body, sizeof(g_http_response_body), 1);
  } else if (request_path_is(request, "GET", "/api/relay/manual")) {
    code = 200u;
    path_code = W5500_HTTP_PATH_RELAY_MANUAL;
    body_len = build_manual_relay_body(body, sizeof(g_http_response_body));
  } else if (request_path_is(request, "GET", "/") || request_path_is(request, "GET", "/index.html")) {
    const int static_result = http_send_static_index();
    if (static_result == W5500_HTTP_STATIC_OK) {
      http_record_request(W5500_HTTP_PATH_INDEX, 200u);
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
  http_record_request(path_code, code);
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
  stm32h750_w5500_bind(&g_w5500_port,
                       &g_w5500_ctx,
                       &hspi2,
                       W5500_CS_GPIO_Port,
                       W5500_CS_Pin,
                       W5500_RST_GPIO_Port,
                       W5500_RST_Pin);
  g_w5500_bound = 1u;

  const W5500Result result = w5500_port_init(&g_w5500_port, &g_w5500_config);
  g_w5500_init_result = (uint32_t)result;
  if (g_w5500_dbc_mutex == NULL) {
    g_w5500_dbc_mutex = xSemaphoreCreateMutex();
  }
  if (g_w5500_dbc_mutex == NULL) {
    g_w5500_init_result = 1u;
    return 1;
  }
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
  const uint32_t poll_enter_tick = HAL_GetTick();
  const uint32_t poll_gap_ms = g_w5500_http_last_poll_enter_tick == 0u
                                 ? 0u
                                 : poll_enter_tick - g_w5500_http_last_poll_enter_tick;
  g_w5500_http_last_poll_enter_tick = poll_enter_tick;
  if (g_w5500_bound == 0u || g_w5500_network_configured == 0u) {
    g_w5500_http_status = 1u;
    return 1;
  }

  uint8_t sr = 0u;
  const uint32_t sr_read_start_tick = HAL_GetTick();
  if (s0_read_u8(W5500_S0_SR, &sr) != W5500_OK) {
    g_w5500_http_status = 3u;
    g_w5500_http_error_count++;
    return 1;
  }
  const uint32_t sr_read_end_tick = HAL_GetTick();
  g_w5500_http_socket_sr = sr;
  uint8_t ir = 0u;
  const uint32_t ir_read_start_tick = HAL_GetTick();
  const W5500Result ir_result = s0_read_u8(W5500_S0_IR, &ir);
  const uint32_t ir_read_end_tick = HAL_GetTick();
  if (ir_result == W5500_OK) {
    g_w5500_http_socket_ir = ir;
  } else {
    g_w5500_http_socket_ir = 0xffffffffu;
  }
  http_connection_observe(sr, ir_result, ir, poll_gap_ms);

  if (g_w5500_http_ack_wait_pending != 0u) {
    const uint32_t elapsed_ms = HAL_GetTick() - g_w5500_http_ack_wait_start_tick;
    g_w5500_http_ack_wait_elapsed_ms = elapsed_ms;
    if (sr == W5500_S0_SR_LISTEN) {
      g_w5500_http_ack_wait_pending = 0u;
      g_w5500_http_ack_wait_start_tick = 0u;
      http_trace_finish();
      g_w5500_http_status = 0u;
      return 0;
    }
    if (sr == W5500_S0_SR_CLOSE_WAIT) {
      g_w5500_http_ack_wait_pending = 0u;
      g_w5500_http_ack_wait_start_tick = 0u;
      if (http_begin_graceful_disconnect() != 0) {
        g_w5500_http_status = 5u;
        ++g_w5500_http_error_count;
        (void)http_close_socket(2u);
        return 1;
      }
      return 0;
    }
    if (sr == W5500_S0_SR_CLOSED || sr == W5500_S0_SR_INIT) {
      g_w5500_http_ack_wait_pending = 0u;
      g_w5500_http_ack_wait_start_tick = 0u;
      http_trace_finish();
      return http_open_listener();
    }

    uint16_t tx_free = 0u;
    const W5500Result tx_fsr_result = s0_read_u16_stable(W5500_S0_TX_FSR, &tx_free);
    g_w5500_http_ack_wait_final_fsr =
      tx_fsr_result == W5500_OK ? tx_free : 0xffffffffu;
    if (tx_fsr_result == W5500_OK && tx_free == W5500_SOCKET_BUFFER_SIZE) {
      g_w5500_http_ack_wait_pending = 0u;
      g_w5500_http_ack_wait_start_tick = 0u;
      if (http_begin_graceful_disconnect() != 0) {
        g_w5500_http_status = 5u;
        ++g_w5500_http_error_count;
        (void)http_close_socket(2u);
        return 1;
      }
      return 0;
    }
    if (elapsed_ms >= W5500_HTTP_DISCONNECT_RECOVERY_TIMEOUT_MS) {
      ++g_w5500_http_ack_wait_timeout_count;
      g_w5500_http_ack_wait_pending = 0u;
      g_w5500_http_ack_wait_start_tick = 0u;
      if (http_begin_graceful_disconnect() != 0) {
        g_w5500_http_status = 5u;
        ++g_w5500_http_error_count;
        (void)http_close_socket(2u);
        return 1;
      }
      return 0;
    }
    g_w5500_http_status = 0u;
    return 0;
  }

  if (g_w5500_http_disconnect_pending != 0u) {
    if (sr == W5500_S0_SR_LISTEN) {
      g_w5500_http_disconnect_pending = 0u;
      g_w5500_http_disconnect_pending_start_tick = 0u;
      http_trace_finish();
      g_w5500_http_status = 0u;
      return 0;
    }
    if (sr == W5500_S0_SR_CLOSE_WAIT) {
      g_w5500_http_disconnect_pending = 0u;
      g_w5500_http_disconnect_pending_start_tick = 0u;
      if (http_begin_graceful_disconnect() != 0) {
        g_w5500_http_status = 5u;
        ++g_w5500_http_error_count;
        (void)http_close_socket(2u);
        return 1;
      }
      return 0;
    }
    if (sr == W5500_S0_SR_CLOSED || sr == W5500_S0_SR_INIT) {
      g_w5500_http_disconnect_pending = 0u;
      g_w5500_http_disconnect_pending_start_tick = 0u;
      http_trace_finish();
      return http_open_listener();
    }
    const uint32_t elapsed_ms = HAL_GetTick() - g_w5500_http_disconnect_pending_start_tick;
    if (elapsed_ms >= W5500_HTTP_DISCONNECT_RECOVERY_TIMEOUT_MS) {
      g_w5500_http_recovery_last_sr = sr;
      ++g_w5500_http_recovery_count;
      g_w5500_http_disconnect_pending = 0u;
      g_w5500_http_disconnect_pending_start_tick = 0u;
      http_trace_finish();
      if (w5500_bringup_run() != 0) {
        g_w5500_http_status = 6u;
        ++g_w5500_http_error_count;
        return 1;
      }
      return http_open_listener();
    }
    g_w5500_http_status = 0u;
    return 0;
  }

  if (sr == W5500_S0_SR_CLOSED || sr == W5500_S0_SR_INIT) {
    return http_open_listener();
  }
  if (sr == W5500_S0_SR_LISTEN) {
    /* A listener can remain locally healthy while the common network
     * registers are corrupted between connections. Check the frozen
     * configuration before accepting the next SYN; the port helper writes
     * nothing when every field already matches. */
    if (http_ensure_network_config() != 0) {
      g_w5500_http_status = 2u;
      ++g_w5500_http_error_count;
      return 1;
    }
    g_w5500_http_status = 0u;
    return 0;
  }
  if (sr == W5500_S0_SR_SYNRECV) {
    g_w5500_http_status = 0u;
    return 0;
  }
  if (sr == W5500_S0_SR_ESTABLISHED || sr == W5500_S0_SR_CLOSE_WAIT) {
    uint16_t rx_size = 0u;
    const uint32_t rx_rsr_read_start_tick = HAL_GetTick();
    const W5500Result rx_rsr_result = s0_read_u16_stable(W5500_S0_RX_RSR, &rx_size);
    g_w5500_http_connection_last_rx_rsr_result = (uint32_t)rx_rsr_result;
    g_w5500_http_connection_last_rx_rsr = rx_rsr_result == W5500_OK ? rx_size : 0xffffffffu;
    if (ir_result == W5500_OK && (ir & W5500_S0_IR_RECV) != 0u &&
        g_w5500_http_trace_active == 0u &&
        (rx_rsr_result != W5500_OK || rx_size == 0u)) {
      http_pretrace_latch(sr, ir, rx_rsr_result, rx_size, poll_enter_tick, poll_gap_ms);
    }
    if (rx_rsr_result != W5500_OK) {
      if (sr == W5500_S0_SR_CLOSE_WAIT && g_w5500_http_connection_trace_started == 0u) {
        http_no_trace_freeze();
      }
      g_w5500_http_status = 4u;
      g_w5500_http_error_count++;
      return 1;
    }
    const uint32_t rx_rsr_read_end_tick = HAL_GetTick();
    if (g_http_dbc_upload.phase == DBC_UPLOAD_STATE_RECEIVING) {
      if (sr == W5500_S0_SR_CLOSE_WAIT && rx_size == 0u) {
        (void)dbc_upload_cancel(&g_http_dbc_upload);
        ++g_w5500_http_dbc_upload_abort_count;
        http_upload_update_diagnostics();
        candidate_mutation_end();
        if (http_begin_graceful_disconnect() != 0) {
          g_w5500_http_status = 5u;
          ++g_w5500_http_error_count;
          (void)http_close_socket(2u);
          return 1;
        }
        return 0;
      }
      g_w5500_http_trace_handle_enter_tick = HAL_GetTick();
      const int upload_result = http_upload_process_body(rx_size);
      g_w5500_http_trace_handler_return_tick = HAL_GetTick();
      g_w5500_http_trace_handler_result = (uint32_t)upload_result;
      if (upload_result == W5500_HTTP_HANDLE_WAIT) {
        ++g_w5500_http_trace_handle_wait_count;
        if (g_w5500_http_trace_handle_wait_first_tick == 0u) {
          g_w5500_http_trace_handle_wait_first_tick = HAL_GetTick();
        }
        g_w5500_http_trace_handle_wait_last_rx_size = rx_size;
        if (sr == W5500_S0_SR_CLOSE_WAIT) {
          (void)dbc_upload_cancel(&g_http_dbc_upload);
          ++g_w5500_http_dbc_upload_abort_count;
          http_upload_update_diagnostics();
          candidate_mutation_end();
          if (http_begin_graceful_disconnect() != 0) {
            g_w5500_http_status = 5u;
            ++g_w5500_http_error_count;
            (void)http_close_socket(2u);
            return 1;
          }
        }
        return 0;
      }
      if (upload_result != W5500_HTTP_HANDLE_OK) {
        g_w5500_http_status = 5u;
        ++g_w5500_http_error_count;
        (void)http_close_socket(2u);
        return 1;
      }
      if (http_finish_response_send() != 0) {
        g_w5500_http_status = 5u;
        ++g_w5500_http_error_count;
        (void)http_close_socket(2u);
        return 1;
      }
      return 0;
    }
    if (rx_size > 0u) {
      http_idle_connection_reset();
      if (g_w5500_http_trace_active == 0u) {
        http_trace_begin(rx_size,
                         poll_enter_tick,
                         poll_gap_ms,
                         sr_read_start_tick,
                         sr_read_end_tick,
                         ir_read_start_tick,
                         ir_read_end_tick,
                         rx_rsr_read_start_tick,
                         rx_rsr_read_end_tick);
      }
      g_w5500_http_connection_trace_started = 1u;
      g_w5500_http_trace_handle_enter_tick = HAL_GetTick();
      const int request_result = http_handle_request(rx_size);
      g_w5500_http_trace_handler_return_tick = HAL_GetTick();
      g_w5500_http_trace_handler_result = (uint32_t)request_result;
      if (request_result == W5500_HTTP_HANDLE_WAIT) {
        ++g_w5500_http_trace_handle_wait_count;
        if (g_w5500_http_trace_handle_wait_first_tick == 0u) {
          g_w5500_http_trace_handle_wait_first_tick = HAL_GetTick();
        }
        g_w5500_http_trace_handle_wait_last_rx_size = rx_size;
        if (sr == W5500_S0_SR_CLOSE_WAIT) {
          if (http_begin_graceful_disconnect() != 0) {
            g_w5500_http_status = 5u;
            ++g_w5500_http_error_count;
            (void)http_close_socket(2u);
            return 1;
          }
        }
        return 0;
      }
      if (request_result != W5500_HTTP_HANDLE_OK) {
        g_w5500_http_status = 5u;
        g_w5500_http_error_count++;
        (void)http_close_socket(2u);
        return 1;
      }
      if (http_finish_response_send() != 0) {
        g_w5500_http_status = 5u;
        g_w5500_http_error_count++;
        (void)http_close_socket(2u);
        return 1;
      }
      return 0;
    }
    if (sr == W5500_S0_SR_CLOSE_WAIT) {
      if (g_w5500_http_connection_trace_started == 0u) {
        http_no_trace_freeze();
      }
      if (http_begin_graceful_disconnect() != 0) {
        g_w5500_http_status = 5u;
        g_w5500_http_error_count++;
        (void)http_close_socket(2u);
        return 1;
      }
    }
    if (sr == W5500_S0_SR_ESTABLISHED) {
      const uint32_t now = HAL_GetTick();
      if (g_w5500_http_idle_connection_pending == 0u) {
        g_w5500_http_idle_connection_start_tick = now;
        g_w5500_http_idle_connection_pending = 1u;
        g_w5500_http_idle_connection_elapsed_ms = 0u;
        return 0;
      }
      const uint32_t elapsed_ms = now - g_w5500_http_idle_connection_start_tick;
      g_w5500_http_idle_connection_elapsed_ms = elapsed_ms;
      if (elapsed_ms >= W5500_HTTP_IDLE_CONNECTION_TIMEOUT_MS) {
        ++g_w5500_http_idle_connection_timeout_count;
        g_w5500_http_idle_connection_last_timeout_ms = elapsed_ms;
        if (http_begin_graceful_disconnect() != 0) {
          g_w5500_http_status = 5u;
          ++g_w5500_http_error_count;
          (void)http_close_socket(2u);
          return 1;
        }
      }
    }
    return 0;
  }

  (void)http_close_socket(3u);
  return 0;
}

#endif
