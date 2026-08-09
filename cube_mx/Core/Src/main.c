/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"
#include "fdcan.h"
#include "quadspi.h"
#include "sdmmc.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "platform/stm32h750_bringup.h"
#include "manual_relay.h"
#include "rule_config.h"
#include "rule_engine.h"
#include "rule_file.h"
#include "selected_signal_log.h"
#include "signal_log_buffer.h"
#include "signal_log_control.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
  CONFIG_COMMAND_DIAGNOSTIC = 1u,
  CONFIG_COMMAND_RULE_FILE_V5_SAVE = 4u
} ConfigCommandType;

typedef struct {
  uint32_t type;
  RuleFileV5 rule_file_v5;
} ConfigCommand;

typedef struct {
  uint32_t can_task;
  uint32_t decode_task;
  uint32_t w5500_task;
  uint32_t http_task;
  uint32_t dbc_task;
  uint32_t dbc_candidate_progress;
  uint32_t config_task;
  uint32_t log_task;
  uint32_t rule_task;
} WatchdogSnapshot;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile int g_tf_card_bringup_status = -1;
volatile int g_w5500_bringup_status = -1;
volatile int g_w25q128_bringup_status = -1;
volatile int g_can_bringup_status = -1;
volatile int g_can_external_bringup_status = -1;
volatile int g_can2_analyzer_bringup_status = -1;
volatile uint32_t g_freertos_task_started;
volatile uint32_t g_freertos_loop_count;
volatile uint32_t g_freertos_bringup_complete;
volatile uint32_t g_can_task_started;
volatile uint32_t g_can_task_loop_count;
extern volatile uint32_t g_can2_rx_queue_ready;
extern volatile uint32_t g_can2_rx_queue_enqueue_count;
extern volatile uint32_t g_can2_rx_queue_drop_count;
extern volatile uint32_t g_can2_rx_queue_dequeue_count;
extern volatile uint32_t g_can2_dbc_stale_mark_count;
extern volatile uint32_t g_can2_decode_task_started;
extern volatile uint32_t g_can2_decode_task_loop_count;
extern volatile uint32_t g_can2_tx_queue_ready;
extern volatile uint32_t g_can2_tx_queue_enqueue_count;
extern volatile uint32_t g_can2_tx_queue_drop_count;
extern volatile uint32_t g_can2_tx_queue_dequeue_count;
volatile uint32_t g_w5500_task_started;
volatile uint32_t g_w5500_task_loop_count;
volatile uint32_t g_http_task_started;
volatile uint32_t g_http_task_loop_count;
volatile uint32_t g_http_task_poll_gap_max_ms;
volatile uint32_t g_http_task_mutex_wait_max_ms;
volatile uint32_t g_http_task_poll_exec_max_ms;
volatile uint32_t g_w5500_mutex_ready;
volatile uint32_t g_dbc_task_started;
volatile uint32_t g_dbc_task_loop_count;
volatile uint32_t g_dbc_task_request_count;
volatile uint32_t g_dbc_task_complete_count;
volatile uint32_t g_dbc_task_last_result = 0xffffffffu;
volatile uint32_t g_tf_task_started;
volatile uint32_t g_tf_task_complete;
volatile uint32_t g_tf_task_last_result = 0xffffffffu;
static SemaphoreHandle_t g_w5500_mutex;
static TaskHandle_t g_can2_rx_task_handle;
#if defined(CAN_BUS_P0_FAULT_INJECTION)
volatile uint32_t g_p0_fault_inject_can_stall;
#endif
static WatchdogSnapshot g_watchdog_snapshot;
static uint32_t g_watchdog_baseline_ready;
volatile uint32_t g_watchdog_started;
volatile uint32_t g_watchdog_init_result = 0xffffffffu;
volatile uint32_t g_watchdog_refresh_count;
volatile uint32_t g_watchdog_unhealthy_mask;
volatile uint32_t g_watchdog_reset_flags;
extern volatile uint32_t g_w5500_http_dbc_reload_queue_ready;
extern volatile uint32_t g_w5500_http_dbc_reload_enqueue_count;
extern volatile uint32_t g_w5500_http_dbc_reload_queue_drop_count;
extern volatile uint32_t g_w5500_http_candidate_progress_count;
volatile uint32_t g_monitor_task_started;
volatile uint32_t g_monitor_task_loop_count;
volatile uint32_t g_config_task_started;
volatile uint32_t g_config_task_loop_count;
volatile uint32_t g_config_queue_ready;
volatile uint32_t g_config_queue_enqueue_count;
volatile uint32_t g_config_queue_dequeue_count;
volatile uint32_t g_config_queue_drop_count;
volatile uint32_t g_config_task_command_count;
volatile uint32_t g_config_task_last_command = 0xffffffffu;
volatile uint32_t g_tf_csv_write_count;
volatile uint32_t g_tf_csv_write_result = 0xffffffffu;
volatile uint32_t g_tf_csv_write_len;
volatile uint32_t g_tf_csv_file_size;
volatile uint32_t g_log_task_started;
volatile uint32_t g_log_task_loop_count;
volatile uint32_t g_log_sample_count;
volatile uint32_t g_log_flush_count;
volatile uint32_t g_log_write_count;
volatile uint32_t g_log_failure_count;
volatile uint32_t g_log_drop_count;
volatile uint32_t g_log_buffer_len;
volatile uint32_t g_log_buffer_samples;
volatile uint32_t g_log_last_result = 0xffffffffu;
volatile uint32_t g_log_path_mode = 0xffffffffu;
volatile uint32_t g_log_path_switch_count;
volatile uint32_t g_log_active_file_size;
SignalLogControl g_signal_log_control;
SelectedSignalLogSession g_selected_signal_log_session;
volatile uint32_t g_rule_task_started;
volatile uint32_t g_rule_task_loop_count;
volatile uint32_t g_rule_task_evaluation_count;
volatile uint32_t g_rule_task_input_count;
volatile uint32_t g_rule_task_relay1_output;
volatile uint32_t g_rule_task_relay2_output;
volatile uint32_t g_rule_task_gpioe_odr;
volatile uint32_t g_rule_task_rule_matched;
volatile uint32_t g_rule_task_safe_active;
volatile uint32_t g_rule_task_manual_enabled;
volatile uint32_t g_rule_task_manual_relay1;
volatile uint32_t g_rule_task_manual_relay2;
volatile uint32_t g_rule_task_manual_active;
volatile uint32_t g_rule_task_manual_request_seq;
volatile uint32_t g_rule_task_manual_applied_seq;
volatile uint32_t g_rule_task_condition_since_ms;
volatile uint32_t g_rule_task_delay_pending;
volatile uint32_t g_rule_task_hysteresis_latched;
volatile uint32_t g_rule_task_rule_count;
volatile uint32_t g_rule_task_winner_rule0 = UINT32_MAX;
volatile uint32_t g_rule_task_winner_rule1 = UINT32_MAX;
volatile uint32_t g_rule_task_config_on_threshold = 42434u;
volatile uint32_t g_rule_task_config_off_threshold = 42432u;
volatile uint32_t g_rule_task_config_delay_ms = 1000u;
volatile uint32_t g_rule_task_config_timeout_ms = 1500u;
volatile uint32_t g_rule_task_config_pending_on_threshold = 42434u;
volatile uint32_t g_rule_task_config_pending_off_threshold = 42432u;
volatile uint32_t g_rule_task_config_pending_delay_ms = 1000u;
volatile uint32_t g_rule_task_config_pending_timeout_ms = 1500u;
volatile uint32_t g_rule_task_config_reload;
volatile uint32_t g_rule_task_config_result = 0xffffffffu;
volatile uint32_t g_rule_task_config_load_count;
volatile uint32_t g_rule_task_config_generation;
volatile uint32_t g_rule_task_engine_reload;
static RuleEngine g_rule_task_pending_engine;
static QueueHandle_t g_config_command_queue;
volatile uint32_t g_rule_file_v5_load_result = 0xffffffffu;
volatile uint32_t g_rule_file_v5_size;
volatile uint32_t g_rule_file_v5_read_len;
volatile uint32_t g_rule_file_v5_rule_count;
volatile uint32_t g_rule_file_v5_save_request;
volatile uint32_t g_rule_file_v5_save_result = 0xffffffffu;
RuleFileV5 g_rule_file_v5_current;
RuleFileV5 g_rule_file_v5_pending;
static char g_rule_file_v5_text[RULE_FILE_V5_MAX_BYTES + 1u];
static RuleEngine g_rule_file_v5_candidate_engine;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void bringup_print_status(const char *phase);
static void bringup_print_http_trace(void);
static void bringup_uart_write(const char *text);
static void bringup_default_task(void *argument);
static void can2_periodic_task(void *argument);
static void can2_decode_task(void *argument);
static void w5500_periodic_task(void *argument);
static void http_periodic_task(void *argument);
static void dbc_task(void *argument);
static void tf_task(void *argument);
static void monitor_task(void *argument);
static void config_task(void *argument);
static void signal_log_task(void *argument);
static void rule_task(void *argument);
static void rule_task_request_engine_reload(const RuleEngine *candidate);
static int watchdog_start(void);
static void watchdog_service(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
extern volatile uint32_t g_tf_sd_last_hal_status;
extern volatile uint32_t g_tf_sd_last_error;
extern volatile uint32_t g_tf_sd_last_sta;
extern volatile uint32_t g_tf_sd_last_dcount;
extern volatile uint32_t g_tf_fs_mutex_ready;
extern volatile uint32_t g_tf_fs_lock_result;
extern volatile uint32_t g_tf_www_index_status;
extern volatile uint32_t g_tf_www_index_len;
extern volatile uint32_t g_w5500_init_result;
extern volatile uint32_t g_w5500_version;
extern volatile uint32_t g_w5500_phycfgr;
extern volatile uint32_t g_w5500_link_up;
extern volatile uint32_t g_w5500_network_configured;
extern volatile uint32_t g_w5500_http_status;
extern volatile uint32_t g_w5500_http_socket_sr;
extern volatile uint32_t g_w5500_http_request_count;
extern volatile uint32_t g_w5500_http_last_path;
extern volatile uint32_t g_w5500_http_last_code;
extern volatile uint32_t g_w5500_http_error_count;
extern volatile uint32_t g_w5500_http_last_nonclosed_close;
extern volatile uint32_t g_w5500_http_socket_ir;
extern volatile uint32_t g_w5500_http_trace_seq;
extern volatile uint32_t g_w5500_http_trace_active;
extern volatile uint32_t g_w5500_http_trace_mutex_wait_start_tick;
extern volatile uint32_t g_w5500_http_trace_mutex_wait_end_tick;
extern volatile uint32_t g_w5500_http_trace_mutex_wait_ms;
extern volatile uint32_t g_w5500_http_trace_rx_ready_tick;
extern volatile uint32_t g_w5500_http_trace_rx_size;
extern volatile uint32_t g_w5500_http_trace_handle_enter_tick;
extern volatile uint32_t g_w5500_http_trace_record_tick;
extern volatile uint32_t g_w5500_http_trace_handler_return_tick;
extern volatile uint32_t g_w5500_http_trace_handler_result;
extern volatile uint32_t g_w5500_http_trace_disconnect_start_tick;
extern volatile uint32_t g_w5500_http_trace_disconnect_end_tick;
extern volatile uint32_t g_w5500_http_trace_handle_wait_count;
extern volatile uint32_t g_w5500_http_trace_handle_wait_first_tick;
extern volatile uint32_t g_w5500_http_trace_handle_wait_last_rx_size;
extern volatile uint32_t g_w5500_http_trace_poll_enter_tick;
extern volatile uint32_t g_w5500_http_trace_poll_gap_ms;
extern volatile uint32_t g_w5500_http_trace_sr_read_start_tick;
extern volatile uint32_t g_w5500_http_trace_sr_read_end_tick;
extern volatile uint32_t g_w5500_http_trace_ir_read_start_tick;
extern volatile uint32_t g_w5500_http_trace_ir_read_end_tick;
extern volatile uint32_t g_w5500_http_trace_rx_rsr_read_start_tick;
extern volatile uint32_t g_w5500_http_trace_rx_rsr_read_end_tick;
extern volatile uint32_t g_w5500_http_trace_request_read_start_tick;
extern volatile uint32_t g_w5500_http_trace_request_read_end_tick;
extern volatile uint32_t g_w5500_http_trace_rx_consume_start_tick;
extern volatile uint32_t g_w5500_http_trace_rx_consume_end_tick;
extern volatile uint32_t g_w5500_http_trace_status_body_start_tick;
extern volatile uint32_t g_w5500_http_trace_status_body_end_tick;
extern volatile uint32_t g_w5500_http_trace_header_send_enter_tick;
extern volatile uint32_t g_w5500_http_trace_header_send_issued_tick;
extern volatile uint32_t g_w5500_http_trace_header_sendok_tick;
extern volatile uint32_t g_w5500_http_trace_send_count;
extern volatile uint32_t g_w5500_http_trace_last_send_len;
extern volatile uint32_t g_w5500_http_trace_tx_total;
extern volatile uint32_t g_w5500_http_trace_post_sendok_tx_fsr_result;
extern volatile uint32_t g_w5500_http_trace_post_sendok_tx_fsr_value;
extern volatile uint32_t g_w5500_http_pretrace_seq;
extern volatile uint32_t g_w5500_http_pretrace_sr;
extern volatile uint32_t g_w5500_http_pretrace_ir;
extern volatile uint32_t g_w5500_http_pretrace_rx_rsr_result;
extern volatile uint32_t g_w5500_http_pretrace_rx_rsr;
extern volatile uint32_t g_w5500_http_pretrace_poll_tick;
extern volatile uint32_t g_w5500_http_pretrace_poll_gap_ms;
extern volatile uint32_t g_w5500_http_pretrace_mutex_wait_ms;
extern volatile uint32_t g_w5500_http_no_trace_seq;
extern volatile uint32_t g_w5500_http_no_trace_sr_seen_mask;
extern volatile uint32_t g_w5500_http_no_trace_last_sr;
extern volatile uint32_t g_w5500_http_no_trace_last_ir_result;
extern volatile uint32_t g_w5500_http_no_trace_last_ir;
extern volatile uint32_t g_w5500_http_no_trace_last_rx_rsr_result;
extern volatile uint32_t g_w5500_http_no_trace_last_rx_rsr;
extern volatile uint32_t g_w5500_http_no_trace_last_poll_gap_ms;
extern volatile uint32_t g_w5500_http_static_count;
extern volatile uint32_t g_w5500_http_static_read_result;
extern volatile uint32_t g_w25q128_jedec_id;
extern volatile uint32_t g_w25q128_status_reg1;
extern volatile uint32_t g_w25q128_test_addr;
extern volatile uint32_t g_w25q128_mismatch_index;
extern volatile uint32_t g_w25q128_expected;
extern volatile uint32_t g_w25q128_actual;
extern volatile uint32_t g_w25q128_last_hal_status;
extern volatile uint32_t g_w25q128_diagnostic_request;
extern volatile uint32_t g_w25q128_diagnostic_result;
extern volatile uint32_t g_w25q128_diagnostic_count;
extern volatile uint32_t g_w25q128_erase_count;
extern volatile uint32_t g_w25q128_config_addr;
extern volatile uint32_t g_w25q128_config_load_result;
extern volatile uint32_t g_w25q128_config_save_result;
extern volatile uint32_t g_w25q128_config_load_count;
extern volatile uint32_t g_w25q128_config_save_count;
extern volatile uint32_t g_can_tx_count;
extern volatile uint32_t g_can_rx_count;
extern volatile uint32_t g_can_error_count;
extern volatile uint32_t g_can_bus_off;
extern volatile uint32_t g_can_tec;
extern volatile uint32_t g_can_rec;
extern volatile uint32_t g_can_rx_id;
extern volatile uint32_t g_can_rx_dlc;
extern volatile uint32_t g_can_rx_first_byte;
extern volatile uint32_t g_can_external_tx_count;
extern volatile uint32_t g_can_external_rx_count;
extern volatile uint32_t g_can_external_error_count;
extern volatile uint32_t g_can_external_bus_off;
extern volatile uint32_t g_can_external_tec;
extern volatile uint32_t g_can_external_rec;
extern volatile uint32_t g_can_external_rx_id;
extern volatile uint32_t g_can_external_rx_dlc;
extern volatile uint32_t g_can_external_rx_first_byte;
extern volatile uint32_t g_can2_tx_count;
extern volatile uint32_t g_can2_rx_count;
extern volatile uint32_t g_can2_error_count;
extern volatile uint32_t g_can2_bus_off;
extern volatile uint32_t g_can2_tec;
extern volatile uint32_t g_can2_rec;
extern volatile uint32_t g_can2_rx_id;
extern volatile uint32_t g_can2_rx_dlc;
extern volatile uint32_t g_can2_rx_first_byte;
extern volatile uint32_t g_can2_send_result;
extern volatile uint32_t g_can2_poll_count;

static void bringup_uart_write(const char *text)
{
  if (text == NULL) {
    return;
  }

  (void)HAL_UART_Transmit(&huart2, (uint8_t *)text, (uint16_t)strlen(text), 1000u);
}

static void bringup_print_status(const char *phase)
{
  static char line[2048];
  (void)snprintf(line,
                 sizeof(line),
                 "[bringup] %s rtos=%lu rtc=%lu rdy=%lu ctsk=%lu ctlp=%lu c2dts=%lu c2dtl=%lu c2qr=%lu c2qe=%lu c2qd=%lu c2qdrop=%lu c2tqr=%lu c2tqe=%lu c2tqd=%lu c2tqdrop=%lu wtsk=%lu wtlp=%lu htsk=%lu htlp=%lu hpmg=%lu hpmw=%lu hpme=%lu wm=%lu dtsk=%lu dtlp=%lu dreq=%lu dcmp=%lu dr=%lu dq=%lu denq=%lu ddrop=%lu ttsk=%lu tdone=%lu tres=%lu mtsk=%lu mtlp=%lu cfgqr=%lu cfgqe=%lu cfgqd=%lu cfgdrop=%lu cfgcmd=%lu can=%d ctx=%lu crx=%lu ce=%lu cbo=%lu ctec=%lu crec=%lu cid=%08lx cdl=%lu cd0=%02lx cext=%d extx=%lu exrx=%lu exe=%lu exbo=%lu extec=%lu exrec=%lu exid=%08lx exdl=%lu exd0=%02lx can2=%d c2tx=%lu c2rx=%lu c2e=%lu c2bo=%lu c2tec=%lu c2rec=%lu c2id=%08lx c2dl=%lu c2d0=%02lx c2sr=%lu c2pc=%lu qspi=%d qid=%06lx qsr=%02lx qaddr=%06lx qmi=%lu qe=%02lx qa=%02lx qhs=%lu tf=%d fsm=%lu fsl=%lu www=%lu wwwl=%lu w=%d wir=%lu wv=%02lx wp=%02lx wl=%lu wn=%lu http=%lu hsr=%02lx hir=%08lx hreq=%lu hpath=%lu hcode=%lu hstatic=%lu hsrd=%lu herr=%lu sdh=%lu sde=%08lx sds=%08lx sdc=%lu hclose=%08lx htseq=%lu htact=%lu hps=%lu hpsr=%02lx hpir=%08lx hprr=%08lx hpr=%lu hpp=%lu hpg=%lu hpwm=%lu hnseq=%lu hnmask=%08lx hnsr=%02lx hnirr=%08lx hnir=%08lx hnrr=%08lx hnr=%lu hngap=%lu htm0=%lu htm1=%lu htmm=%lu htrx=%lu htrxs=%lu hthe=%lu htrc=%lu htre=%lu htrs=%08lx htds=%lu htde=%lu htwc=%lu htwf=%lu htwl=%lu\r\n",
                 phase,
                 (unsigned long)g_freertos_task_started,
                 (unsigned long)g_freertos_loop_count,
                 (unsigned long)g_freertos_bringup_complete,
                 (unsigned long)g_can_task_started,
                 (unsigned long)g_can_task_loop_count,
                 (unsigned long)g_can2_decode_task_started,
                 (unsigned long)g_can2_decode_task_loop_count,
                 (unsigned long)g_can2_rx_queue_ready,
                 (unsigned long)g_can2_rx_queue_enqueue_count,
                 (unsigned long)g_can2_rx_queue_dequeue_count,
                 (unsigned long)g_can2_rx_queue_drop_count,
                 (unsigned long)g_can2_tx_queue_ready,
                 (unsigned long)g_can2_tx_queue_enqueue_count,
                 (unsigned long)g_can2_tx_queue_dequeue_count,
                 (unsigned long)g_can2_tx_queue_drop_count,
                 (unsigned long)g_w5500_task_started,
                 (unsigned long)g_w5500_task_loop_count,
                 (unsigned long)g_http_task_started,
                 (unsigned long)g_http_task_loop_count,
                 (unsigned long)g_http_task_poll_gap_max_ms,
                 (unsigned long)g_http_task_mutex_wait_max_ms,
                 (unsigned long)g_http_task_poll_exec_max_ms,
                 (unsigned long)g_w5500_mutex_ready,
                 (unsigned long)g_dbc_task_started,
                 (unsigned long)g_dbc_task_loop_count,
                 (unsigned long)g_dbc_task_request_count,
                 (unsigned long)g_dbc_task_complete_count,
                 (unsigned long)g_dbc_task_last_result,
                 (unsigned long)g_w5500_http_dbc_reload_queue_ready,
                 (unsigned long)g_w5500_http_dbc_reload_enqueue_count,
                 (unsigned long)g_w5500_http_dbc_reload_queue_drop_count,
                 (unsigned long)g_tf_task_started,
                 (unsigned long)g_tf_task_complete,
                 (unsigned long)g_tf_task_last_result,
                 (unsigned long)g_monitor_task_started,
                 (unsigned long)g_monitor_task_loop_count,
                 (unsigned long)g_config_queue_ready,
                 (unsigned long)g_config_queue_enqueue_count,
                 (unsigned long)g_config_queue_dequeue_count,
                 (unsigned long)g_config_queue_drop_count,
                 (unsigned long)g_config_task_command_count,
                 g_can_bringup_status,
                 (unsigned long)g_can_tx_count,
                 (unsigned long)g_can_rx_count,
                 (unsigned long)g_can_error_count,
                 (unsigned long)g_can_bus_off,
                 (unsigned long)g_can_tec,
                 (unsigned long)g_can_rec,
                 (unsigned long)g_can_rx_id,
                 (unsigned long)g_can_rx_dlc,
                 (unsigned long)g_can_rx_first_byte,
                 g_can_external_bringup_status,
                 (unsigned long)g_can_external_tx_count,
                 (unsigned long)g_can_external_rx_count,
                 (unsigned long)g_can_external_error_count,
                 (unsigned long)g_can_external_bus_off,
                 (unsigned long)g_can_external_tec,
                 (unsigned long)g_can_external_rec,
                 (unsigned long)g_can_external_rx_id,
                 (unsigned long)g_can_external_rx_dlc,
                 (unsigned long)g_can_external_rx_first_byte,
                 g_can2_analyzer_bringup_status,
                 (unsigned long)g_can2_tx_count,
                 (unsigned long)g_can2_rx_count,
                 (unsigned long)g_can2_error_count,
                 (unsigned long)g_can2_bus_off,
                 (unsigned long)g_can2_tec,
                 (unsigned long)g_can2_rec,
                 (unsigned long)g_can2_rx_id,
                 (unsigned long)g_can2_rx_dlc,
                 (unsigned long)g_can2_rx_first_byte,
                 (unsigned long)g_can2_send_result,
                 (unsigned long)g_can2_poll_count,
                 g_w25q128_bringup_status,
                 (unsigned long)g_w25q128_jedec_id,
                 (unsigned long)g_w25q128_status_reg1,
                 (unsigned long)g_w25q128_test_addr,
                 (unsigned long)g_w25q128_mismatch_index,
                 (unsigned long)g_w25q128_expected,
                 (unsigned long)g_w25q128_actual,
                 (unsigned long)g_w25q128_last_hal_status,
                 g_tf_card_bringup_status,
                 (unsigned long)g_tf_fs_mutex_ready,
                 (unsigned long)g_tf_fs_lock_result,
                 (unsigned long)g_tf_www_index_status,
                 (unsigned long)g_tf_www_index_len,
                 g_w5500_bringup_status,
                 (unsigned long)g_w5500_init_result,
                 (unsigned long)g_w5500_version,
                 (unsigned long)g_w5500_phycfgr,
                 (unsigned long)g_w5500_link_up,
                 (unsigned long)g_w5500_network_configured,
                 (unsigned long)g_w5500_http_status,
                 (unsigned long)g_w5500_http_socket_sr,
                 (unsigned long)g_w5500_http_socket_ir,
                 (unsigned long)g_w5500_http_request_count,
                 (unsigned long)g_w5500_http_last_path,
                 (unsigned long)g_w5500_http_last_code,
                 (unsigned long)g_w5500_http_static_count,
                 (unsigned long)g_w5500_http_static_read_result,
                 (unsigned long)g_w5500_http_error_count,
                 (unsigned long)g_tf_sd_last_hal_status,
                 (unsigned long)g_tf_sd_last_error,
                 (unsigned long)g_tf_sd_last_sta,
                 (unsigned long)g_tf_sd_last_dcount,
                 (unsigned long)g_w5500_http_last_nonclosed_close,
                 (unsigned long)g_w5500_http_trace_seq,
                 (unsigned long)g_w5500_http_trace_active,
                 (unsigned long)g_w5500_http_pretrace_seq,
                 (unsigned long)g_w5500_http_pretrace_sr,
                 (unsigned long)g_w5500_http_pretrace_ir,
                 (unsigned long)g_w5500_http_pretrace_rx_rsr_result,
                 (unsigned long)g_w5500_http_pretrace_rx_rsr,
                 (unsigned long)g_w5500_http_pretrace_poll_tick,
                 (unsigned long)g_w5500_http_pretrace_poll_gap_ms,
                 (unsigned long)g_w5500_http_pretrace_mutex_wait_ms,
                 (unsigned long)g_w5500_http_no_trace_seq,
                 (unsigned long)g_w5500_http_no_trace_sr_seen_mask,
                 (unsigned long)g_w5500_http_no_trace_last_sr,
                 (unsigned long)g_w5500_http_no_trace_last_ir_result,
                 (unsigned long)g_w5500_http_no_trace_last_ir,
                 (unsigned long)g_w5500_http_no_trace_last_rx_rsr_result,
                 (unsigned long)g_w5500_http_no_trace_last_rx_rsr,
                 (unsigned long)g_w5500_http_no_trace_last_poll_gap_ms,
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
                 (unsigned long)g_w5500_http_trace_handle_wait_last_rx_size);
  bringup_uart_write(line);
}

static void bringup_print_http_trace(void)
{
  static uint32_t printed_seq;
  char line[384];

  if (g_w5500_http_trace_seq == 0u ||
      g_w5500_http_trace_active != 0u ||
      g_w5500_http_trace_seq == printed_seq) {
    return;
  }
  (void)snprintf(line,
                 sizeof(line),
                 "[http-trace] seq=%lu p=%lu g=%lu ss=%lu se=%lu is=%lu ie=%lu rs=%lu re=%lu bs=%lu be=%lu cs=%lu ce=%lu us=%lu ue=%lu hs=%lu hi=%lu hk=%lu sc=%lu sl=%lu st=%lu fr=%lu fv=%lu\r\n",
                 (unsigned long)g_w5500_http_trace_seq,
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
                 (unsigned long)g_w5500_http_trace_send_count,
                 (unsigned long)g_w5500_http_trace_last_send_len,
                 (unsigned long)g_w5500_http_trace_tx_total,
                 (unsigned long)g_w5500_http_trace_post_sendok_tx_fsr_result,
                 (unsigned long)g_w5500_http_trace_post_sendok_tx_fsr_value);
  printed_seq = g_w5500_http_trace_seq;
  bringup_uart_write(line);
}

static void rule_apply_relays(const RelayState relays[RULE_RELAY_COUNT])
{
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_7, relays[0] == RELAY_STATE_ON ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, relays[1] == RELAY_STATE_ON ? GPIO_PIN_SET : GPIO_PIN_RESET);
  g_rule_task_relay1_output = (uint32_t)relays[0];
  g_rule_task_relay2_output = (uint32_t)relays[1];
  g_rule_task_gpioe_odr = GPIOE->ODR;
}

int rule_task_manual_override_submit(uint32_t enabled,
                                     uint32_t relay1,
                                     uint32_t relay2,
                                     uint32_t *request_seq)
{
  ManualRelayState state;
  int result = 1;

  taskENTER_CRITICAL();
  state.enabled = g_rule_task_manual_enabled;
  state.relay1 = g_rule_task_manual_relay1;
  state.relay2 = g_rule_task_manual_relay2;
  state.request_seq = g_rule_task_manual_request_seq;
  state.applied_seq = g_rule_task_manual_applied_seq;
  if (manual_relay_state_submit(&state, enabled, relay1, relay2)) {
    g_rule_task_manual_enabled = state.enabled;
    g_rule_task_manual_relay1 = state.relay1;
    g_rule_task_manual_relay2 = state.relay2;
    g_rule_task_manual_request_seq = state.request_seq;
    if (request_seq != NULL) {
      *request_seq = state.request_seq;
    }
    result = 0;
  }
  taskEXIT_CRITICAL();
  return result;
}

void rule_task_manual_override_snapshot(uint32_t *enabled,
                                        uint32_t *relay1,
                                        uint32_t *relay2,
                                        uint32_t *request_seq,
                                        uint32_t *applied_seq,
                                        uint32_t *relay1_output,
                                        uint32_t *relay2_output)
{
  taskENTER_CRITICAL();
  if (enabled != NULL) *enabled = g_rule_task_manual_enabled;
  if (relay1 != NULL) *relay1 = g_rule_task_manual_relay1;
  if (relay2 != NULL) *relay2 = g_rule_task_manual_relay2;
  if (request_seq != NULL) *request_seq = g_rule_task_manual_request_seq;
  if (applied_seq != NULL) *applied_seq = g_rule_task_manual_applied_seq;
  if (relay1_output != NULL) *relay1_output = g_rule_task_relay1_output;
  if (relay2_output != NULL) *relay2_output = g_rule_task_relay2_output;
  taskEXIT_CRITICAL();
}

static bool rule_task_load_config(RuleEngine *engine)
{
  static RuleEngine candidate;
  const RuleTaskConfig config = {
    .on_threshold = (double)g_rule_task_config_on_threshold,
    .off_threshold = (double)g_rule_task_config_off_threshold,
    .delay_ms = g_rule_task_config_delay_ms,
    .timeout_ms = g_rule_task_config_timeout_ms,
  };
  if (!rule_task_config_load(&candidate, &config)) {
    g_rule_task_config_result = 1u;
    return false;
  }

  *engine = candidate;
  g_rule_task_config_result = 0u;
  ++g_rule_task_config_load_count;
  ++g_rule_task_config_generation;
  return true;
}

static void rule_task_request_engine_reload(const RuleEngine *candidate)
{
  if (candidate == NULL) {
    return;
  }

  taskENTER_CRITICAL();
  g_rule_task_pending_engine = *candidate;
  g_rule_task_engine_reload = 1u;
  taskEXIT_CRITICAL();
}

static bool rule_file_v5_load_from_tf(void)
{
  size_t file_size = 0u;
  uint8_t file_data[RULE_FILE_V5_MAX_BYTES];
  size_t read_len = 0u;
  RuleFileV5 rules;
  RuleEngine candidate;
  const int size_result =
    stm32h750_tf_file_size_locked(RULE_FILE_V5_PATH, &file_size);

  g_rule_file_v5_size = (uint32_t)file_size;
  g_rule_file_v5_read_len = 0u;
  g_rule_file_v5_rule_count = 0u;
  if (g_tf_card_bringup_status != 0) {
    g_rule_file_v5_load_result = 5u;
    return false;
  }
  if (size_result == FR_NO_FILE) {
    g_rule_file_v5_load_result = 1u;
    return false;
  }
  if (size_result != 0 || file_size == 0u ||
      file_size > RULE_FILE_V5_MAX_BYTES ||
      stm32h750_tf_read_file_locked(RULE_FILE_V5_PATH, file_data, file_size,
                                     &read_len) != 0 ||
      read_len != file_size) {
    g_rule_file_v5_read_len = (uint32_t)read_len;
    g_rule_file_v5_load_result =
      size_result == 0 && file_size > RULE_FILE_V5_MAX_BYTES ? 3u : 2u;
    return false;
  }
  g_rule_file_v5_read_len = (uint32_t)read_len;
  if (!rule_file_parse_v5(file_data, read_len, &rules) ||
      !rule_file_v5_build_engine(&rules, &candidate)) {
    g_rule_file_v5_load_result = 4u;
    return false;
  }
  const uint32_t previous_generation = g_rule_task_config_generation;
  g_rule_file_v5_current = rules;
  g_rule_file_v5_pending = rules;
  g_rule_file_v5_rule_count = (uint32_t)candidate.rule_count;
  rule_task_request_engine_reload(&candidate);
  for (uint32_t wait_ms = 0u; wait_ms < 250u; ++wait_ms) {
    if (g_rule_task_config_generation != previous_generation &&
        g_rule_task_engine_reload == 0u && g_rule_task_config_result == 0u) {
      g_rule_file_v5_load_result = 0u;
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(1u));
  }
  g_rule_file_v5_load_result = 6u;
  return false;
}

static void rule_file_load_from_tf(void)
{
  if (!rule_file_v5_load_from_tf()) {
    if (g_rule_file_v5_load_result == 1u) {
      static const char *const legacy_paths[] = {
        RULE_FILE_PATH,
        RULE_FILE_V2_PATH,
        RULE_FILE_V3_PATH,
        RULE_FILE_V4_PATH,
      };
      bool legacy_exists = false;
      bool stat_failed = false;
      for (size_t i = 0u; i < sizeof(legacy_paths) / sizeof(legacy_paths[0]);
           ++i) {
        size_t legacy_size = 0u;
        const int result =
          stm32h750_tf_file_size_locked(legacy_paths[i], &legacy_size);
        if (result == 0) {
          legacy_exists = true;
        } else if (result != FR_NO_FILE) {
          stat_failed = true;
        }
      }
      if (legacy_exists) {
        g_rule_file_v5_load_result = 7u;
      } else if (stat_failed) {
        g_rule_file_v5_load_result = 2u;
      }
    }
    rule_engine_init(&g_rule_file_v5_candidate_engine);
    rule_task_request_engine_reload(&g_rule_file_v5_candidate_engine);
  }
}

static void rule_task(void *argument)
{
  static RuleEngine engine;
  RelayState relays[RULE_RELAY_COUNT] = {RELAY_STATE_OFF, RELAY_STATE_OFF};

  (void)argument;
  if (!rule_task_load_config(&engine)) {
    g_rule_task_started = 0xffffffffu;
    Error_Handler();
  }
  rule_apply_relays(relays);
  g_rule_task_started = 1u;

  for (;;) {
    SignalSnapshot signals[2];
    uint32_t manual_request_seq;
    RelayState manual_relays[RULE_RELAY_COUNT];
    bool manual_enabled;

    taskENTER_CRITICAL();
    manual_relays[0] = g_rule_task_manual_relay1 != 0u ? RELAY_STATE_ON : RELAY_STATE_OFF;
    manual_relays[1] = g_rule_task_manual_relay2 != 0u ? RELAY_STATE_ON : RELAY_STATE_OFF;
    manual_enabled = g_rule_task_manual_enabled != 0u;
    manual_request_seq = g_rule_task_manual_request_seq;
    taskEXIT_CRITICAL();
    const uint32_t now_ms = HAL_GetTick();
    bool marker_safe = true;

    if (g_rule_task_engine_reload != 0u) {
      taskENTER_CRITICAL();
      engine = g_rule_task_pending_engine;
      g_rule_task_engine_reload = 0u;
      taskEXIT_CRITICAL();
      g_rule_task_config_result = 0u;
      ++g_rule_task_config_load_count;
      ++g_rule_task_config_generation;
    } else if (g_rule_task_config_reload != 0u) {
      g_rule_task_config_reload = 0u;
      (void)rule_task_load_config(&engine);
    }

    const size_t count =
      w5500_http_selected_rule_snapshots(&engine, signals, 2u);

    g_rule_task_input_count = (uint32_t)count;
    rule_engine_set_manual(&engine, manual_enabled, manual_relays);
    rule_engine_evaluate(&engine, signals, count, now_ms, relays);
    rule_apply_relays(relays);
    taskENTER_CRITICAL();
    {
      ManualRelayState state = {.applied_seq = g_rule_task_manual_applied_seq};
      manual_relay_state_mark_applied(&state, manual_request_seq);
      g_rule_task_manual_applied_seq = state.applied_seq;
    }
    taskEXIT_CRITICAL();
    g_rule_task_rule_matched = g_rule_task_relay1_output == (uint32_t)RELAY_STATE_ON ? 1u : 0u;
    g_rule_task_rule_count = (uint32_t)engine.rule_count;
    g_rule_task_winner_rule0 = engine.winner_rule[0];
    g_rule_task_winner_rule1 = engine.winner_rule[1];
    const uint8_t winner = engine.winner_rule[0];
    if (winner != UINT8_MAX && winner < engine.rule_count) {
      g_rule_task_condition_since_ms = engine.rules[winner].condition_since_ms;
      g_rule_task_delay_pending = engine.rules[winner].condition_since_ms != 0u &&
                                  g_rule_task_relay1_output == (uint32_t)RELAY_STATE_OFF ? 1u : 0u;
      g_rule_task_hysteresis_latched = engine.rules[winner].latched_state ? 1u : 0u;
      marker_safe = true;
      for (size_t i = 0u; i < count; ++i) {
        if (strcmp(signals[i].key, engine.rules[winner].signal_key) == 0) {
          marker_safe = !signals[i].valid ||
                        (engine.rules[winner].timeout_ms != 0u &&
                         now_ms - signals[i].updated_ms > engine.rules[winner].timeout_ms);
          break;
        }
      }
    } else {
      g_rule_task_condition_since_ms = 0u;
      g_rule_task_delay_pending = 0u;
      g_rule_task_hysteresis_latched = 0u;
      marker_safe = false;
    }
    g_rule_task_safe_active = marker_safe ? 1u : 0u;
    g_rule_task_manual_active = manual_enabled ? 1u : 0u;
    ++g_rule_task_evaluation_count;
    ++g_rule_task_loop_count;
    vTaskDelay(pdMS_TO_TICKS(50u));
  }
}

typedef struct {
  SignalLogBuffer buffer;
  SelectedSignalLogIdentity identity;
  SignalLogControl control;
  size_t csv_size;
  size_t meta_size;
  uint32_t pending_rows;
  uint32_t last_flush_ms;
} SelectedLogTaskContext;

static SelectedSignalLogState selected_log_state(void)
{
  SelectedSignalLogState state;
  taskENTER_CRITICAL();
  state = (SelectedSignalLogState)g_selected_signal_log_session.state;
  taskEXIT_CRITICAL();
  return state;
}

static void selected_log_start_snapshot(SelectedLogTaskContext *context)
{
  taskENTER_CRITICAL();
  context->identity = g_selected_signal_log_session.identity;
  context->control = g_signal_log_control;
  taskEXIT_CRITICAL();
}

static void selected_log_counter_snapshot(SelectedSignalLogCounters *counters)
{
  taskENTER_CRITICAL();
  *counters = g_selected_signal_log_session.counters;
  taskEXIT_CRITICAL();
}

static bool selected_log_transition(SelectedSignalLogState next_state)
{
  bool ok;
  taskENTER_CRITICAL();
  ok = selected_signal_log_session_transition(
         &g_selected_signal_log_session, next_state) ==
       SELECTED_SIGNAL_LOG_OK;
  taskEXIT_CRITICAL();
  return ok;
}

static void selected_log_disable_control(void)
{
  taskENTER_CRITICAL();
  g_signal_log_control.enabled = false;
  taskEXIT_CRITICAL();
}

static bool selected_log_flush_buffer(SelectedLogTaskContext *context,
                                      const char *path,
                                      uint32_t now_ms,
                                      bool csv_data)
{
  int result;
  SignalLogBuffer *buffer;
  size_t *file_size;
  if (context == NULL || path == NULL) {
    return false;
  }
  buffer = &context->buffer;
  file_size = csv_data ? &context->csv_size : &context->meta_size;
  if (buffer->length == 0u) {
    return true;
  }

  if (csv_data) {
    ++g_log_flush_count;
    g_tf_csv_write_len = (uint32_t)buffer->length;
  }
  result = stm32h750_tf_append_file_locked(
    path, (const uint8_t *)buffer->data, buffer->length, file_size);
  g_log_last_result = (uint32_t)result;
  if (csv_data) {
    taskENTER_CRITICAL();
    ++g_selected_signal_log_session.counters.flush_count;
    if (result == 0) {
      g_selected_signal_log_session.counters.rows_written +=
        context->pending_rows;
    }
    taskEXIT_CRITICAL();
    g_tf_csv_write_result = (uint32_t)result;
    g_tf_csv_file_size = (uint32_t)*file_size;
    g_log_active_file_size = (uint32_t)*file_size;
    context->last_flush_ms = now_ms;
  }
  if (result != 0) {
    return false;
  }

  if (csv_data) {
    ++g_tf_csv_write_count;
    ++g_log_write_count;
  }
  signal_log_buffer_clear(buffer);
  context->pending_rows = 0u;
  g_log_buffer_len = 0u;
  g_log_buffer_samples = 0u;
  return true;
}

static bool selected_log_append_fragment(SelectedLogTaskContext *context,
                                         const char *fragment,
                                         size_t fragment_length,
                                         const char *path,
                                         uint32_t now_ms,
                                         bool csv_data,
                                         bool is_row)
{
  SignalLogBuffer *buffer;
  if (context == NULL) {
    return false;
  }
  buffer = &context->buffer;
  if (buffer == NULL || fragment == NULL || fragment_length == 0u ||
      fragment_length >= buffer->capacity) {
    return false;
  }
  if (fragment_length >= buffer->capacity - buffer->length &&
      !selected_log_flush_buffer(context, path, now_ms, csv_data)) {
    return false;
  }
  if (fragment_length >= buffer->capacity - buffer->length) {
    return false;
  }
  memcpy(buffer->data + buffer->length, fragment, fragment_length);
  buffer->length += fragment_length;
  buffer->data[buffer->length] = '\0';
  if (is_row) {
    ++context->pending_rows;
  }
  g_log_buffer_len = (uint32_t)buffer->length;
  g_log_buffer_samples = context->pending_rows;
  return true;
}

static __attribute__((noinline)) bool selected_log_write_footer(
  const SelectedSignalLogIdentity *identity,
  uint64_t unix_ms,
  bool clean_close,
  SelectedLogTaskContext *context,
  char *scratch,
  uint32_t now_ms)
{
  SelectedSignalLogCounters counters;
  size_t footer_length = 0u;
  selected_log_counter_snapshot(&counters);
  return identity != NULL && identity->meta_path[0] != '\0' &&
         selected_signal_log_serialize_meta_footer(
           unix_ms, clean_close, &counters, scratch,
           SELECTED_SIGNAL_LOG_OUTPUT_MAX_BYTES, &footer_length) ==
           SELECTED_SIGNAL_LOG_OK &&
         selected_log_append_fragment(context, scratch, footer_length,
                                      identity->meta_path, now_ms, false,
                                      false) &&
         selected_log_flush_buffer(context, identity->meta_path, now_ms,
                                   false);
}

static void selected_log_fail_session(
  const SelectedSignalLogIdentity *identity,
  uint64_t unix_ms,
  SelectedLogTaskContext *context,
  char *scratch,
  uint32_t now_ms)
{
  const uint32_t dropped = context->pending_rows;
  signal_log_buffer_clear(&context->buffer);
  context->pending_rows = 0u;
  g_log_buffer_len = 0u;
  g_log_buffer_samples = 0u;
  ++g_log_failure_count;
  g_log_drop_count += dropped;
  taskENTER_CRITICAL();
  g_selected_signal_log_session.counters.rows_dropped += dropped;
  ++g_selected_signal_log_session.counters.write_failures;
  taskEXIT_CRITICAL();

  if (!selected_log_write_footer(identity, unix_ms, false, context, scratch,
                                 now_ms)) {
    ++g_log_failure_count;
    taskENTER_CRITICAL();
    ++g_selected_signal_log_session.counters.write_failures;
    taskEXIT_CRITICAL();
  }
  signal_log_buffer_clear(&context->buffer);
  g_log_buffer_len = 0u;
  (void)selected_log_transition(SELECTED_SIGNAL_LOG_FAILED);
  selected_log_disable_control();
}

static void signal_log_task(void *argument)
{
  enum {
    LOG_FLUSH_MS = 5000u,
    LOG_FLUSH_THRESHOLD = 512u,
  };
  static char storage[768];
  char scratch[SELECTED_SIGNAL_LOG_OUTPUT_MAX_BYTES];
  SelectedLogTaskContext context;
  uint32_t last_sample_ms;

  (void)argument;
  g_log_path_mode = 0xffffffffu;
  memset(&context, 0, sizeof(context));
  signal_log_buffer_init(&context.buffer, storage, sizeof(storage));
  last_sample_ms = HAL_GetTick();
  context.last_flush_ms = last_sample_ms;
  g_log_task_started = 1u;

  for (;;) {
    const uint32_t now_ms = HAL_GetTick();
    const SelectedSignalLogState state = selected_log_state();
    SelectedSignalLogIdentity *identity = &context.identity;
    uint64_t unix_ms = 0u;
    bool ok = true;

    if (state == SELECTED_SIGNAL_LOG_STOPPED ||
        state == SELECTED_SIGNAL_LOG_FAILED) {
      signal_log_buffer_clear(&context.buffer);
      context.pending_rows = 0u;
      g_log_buffer_len = 0u;
      g_log_buffer_samples = 0u;
      last_sample_ms = now_ms;
      context.last_flush_ms = now_ms;
      context.csv_size = 0u;
      context.meta_size = 0u;
    } else if (state == SELECTED_SIGNAL_LOG_STARTING) {
      selected_log_start_snapshot(&context);
      identity = &context.identity;
      if (!signal_log_control_unix_ms(&context.control, now_ms, &unix_ms)) {
        selected_log_fail_session(identity, 0u, &context, scratch, now_ms);
        ++g_log_task_loop_count;
        vTaskDelay(pdMS_TO_TICKS(100u));
        continue;
      }
      size_t existing_size = 0u;
      const int csv_size_result = stm32h750_tf_file_size_locked(
        identity->csv_path, &existing_size);
      const int meta_size_result = stm32h750_tf_file_size_locked(
        identity->meta_path, &existing_size);
      signal_log_buffer_clear(&context.buffer);
      context.pending_rows = 0u;
      context.csv_size = 0u;
      context.meta_size = 0u;
      g_log_path_mode = SIGNAL_LOG_PATH_DEFAULT;
      if (csv_size_result != SIGNAL_LOG_FILE_NOT_FOUND ||
          meta_size_result != SIGNAL_LOG_FILE_NOT_FOUND) {
        ok = false;
      }

      size_t fragment_length = 0u;
      if (ok && selected_signal_log_serialize_meta_header(
                  identity, scratch, sizeof(scratch), &fragment_length) ==
                SELECTED_SIGNAL_LOG_OK) {
        ok = selected_log_append_fragment(
          &context, scratch, fragment_length, identity->meta_path, now_ms,
          false, false);
      } else {
        ok = false;
      }
      for (uint16_t signal_index = 0u;
           ok && signal_index < identity->selected_count; ++signal_index) {
        DbcSelectedRuntimeSignal signal;
        SignalValueSnapshot value;
        if (w5500_http_selected_log_signal(
              identity->active_generation, identity->selection_crc32,
              signal_index, &signal, &value) != 0 ||
            selected_signal_log_serialize_meta_signal(
              &signal, scratch, sizeof(scratch), &fragment_length) !=
              SELECTED_SIGNAL_LOG_OK) {
          ok = false;
        } else {
          ok = selected_log_append_fragment(
            &context, scratch, fragment_length, identity->meta_path, now_ms,
            false, false);
        }
      }
      if (ok) {
        ok = selected_log_flush_buffer(&context, identity->meta_path, now_ms,
                                       false);
      }
      if (ok && selected_signal_log_serialize_csv_header(
                  scratch, sizeof(scratch), &fragment_length) ==
                SELECTED_SIGNAL_LOG_OK) {
        ok = selected_log_append_fragment(
          &context, scratch, fragment_length, identity->csv_path, now_ms,
          true, false) &&
             selected_log_flush_buffer(&context, identity->csv_path, now_ms,
                                       true);
      } else {
        ok = false;
      }
      if (ok && selected_log_transition(SELECTED_SIGNAL_LOG_ACTIVE)) {
        last_sample_ms = now_ms;
        context.last_flush_ms = now_ms;
        g_log_last_result = 0u;
      } else {
        selected_log_fail_session(identity, unix_ms, &context, scratch,
                                  now_ms);
      }
    } else if (!signal_log_control_unix_ms(&context.control, now_ms,
                                           &unix_ms)) {
      selected_log_fail_session(identity, 0u, &context, scratch, now_ms);
    } else if (state == SELECTED_SIGNAL_LOG_ACTIVE) {
      if ((uint32_t)(now_ms - last_sample_ms) >=
          identity->sample_period_ms) {
        size_t fragment_length = 0u;
        const uint32_t elapsed_ms = (uint32_t)(now_ms - last_sample_ms);
        const uint32_t elapsed_periods =
          elapsed_ms / identity->sample_period_ms;
        if (elapsed_periods > 1u) {
          taskENTER_CRITICAL();
          g_selected_signal_log_session.counters.late_samples +=
            (uint64_t)elapsed_periods - 1u;
          taskEXIT_CRITICAL();
        }
        last_sample_ms += elapsed_periods * identity->sample_period_ms;
        ++g_log_sample_count;
        for (uint16_t signal_index = 0u;
             ok && signal_index < identity->selected_count; ++signal_index) {
          DbcSelectedRuntimeSignal signal;
          SignalValueSnapshot value;
          if (w5500_http_selected_log_signal(
                identity->active_generation, identity->selection_crc32,
                signal_index, &signal, &value) != 0 ||
              selected_signal_log_serialize_csv_row(
                unix_ms, now_ms, &signal, &value, scratch, sizeof(scratch),
                &fragment_length) != SELECTED_SIGNAL_LOG_OK) {
            ok = false;
          } else {
            ok = selected_log_append_fragment(
              &context, scratch, fragment_length, identity->csv_path, now_ms,
              true, true);
          }
        }
      }
      if (ok && signal_log_buffer_should_flush(
                  &context.buffer, LOG_FLUSH_THRESHOLD,
                  context.last_flush_ms, now_ms,
                  LOG_FLUSH_MS)) {
        ok = selected_log_flush_buffer(&context, identity->csv_path, now_ms,
                                       true);
      }
      if (!ok) {
        selected_log_fail_session(identity, unix_ms, &context, scratch,
                                  now_ms);
      }
    } else if (state == SELECTED_SIGNAL_LOG_STOPPING) {
      ok = selected_log_flush_buffer(&context, identity->csv_path, now_ms,
                                     true);
      ok = ok && selected_log_write_footer(identity, unix_ms, true, &context,
                                           scratch, now_ms);
      if (ok && selected_log_transition(SELECTED_SIGNAL_LOG_STOPPED)) {
        selected_log_disable_control();
        g_log_last_result = 0u;
      } else {
        selected_log_fail_session(identity, unix_ms, &context, scratch,
                                  now_ms);
      }
    }

    ++g_log_task_loop_count;
    vTaskDelay(pdMS_TO_TICKS(100u));
  }
}

static void can2_periodic_task(void *argument)
{
  (void)argument;

  g_can_task_started = 1u;
  for (;;) {
#if defined(CAN_BUS_P0_FAULT_INJECTION)
    if (g_p0_fault_inject_can_stall != 0u) {
      vTaskDelay(pdMS_TO_TICKS(50u));
      continue;
    }
#endif
    (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50u));
    (void)can2_analyzer_poll();
    g_can_task_loop_count++;
  }
}

static void can2_decode_task(void *argument)
{
  (void)argument;

  g_can2_decode_task_started = 1u;
  for (;;) {
    (void)can2_analyzer_decode_pending();
    ++g_can2_decode_task_loop_count;
    vTaskDelay(pdMS_TO_TICKS(10u));
  }
}

static void w5500_mutex_take(void)
{
  if (g_w5500_mutex != NULL) {
    (void)xSemaphoreTake(g_w5500_mutex, portMAX_DELAY);
  }
}

static void w5500_mutex_give(void)
{
  if (g_w5500_mutex != NULL) {
    (void)xSemaphoreGive(g_w5500_mutex);
  }
}

static void w5500_periodic_task(void *argument)
{
  (void)argument;

  g_w5500_task_started = 1u;
  for (;;) {
    w5500_mutex_take();
    (void)w5500_bringup_poll();
    w5500_mutex_give();
    g_w5500_task_loop_count++;
    vTaskDelay(pdMS_TO_TICKS(50u));
  }
}

static void http_periodic_task(void *argument)
{
  (void)argument;
  TickType_t last_poll_start = 0u;

  g_http_task_started = 1u;
  for (;;) {
    const TickType_t mutex_wait_start = xTaskGetTickCount();
    if (last_poll_start != 0u) {
      const uint32_t poll_gap_ms = (uint32_t)(mutex_wait_start - last_poll_start) * portTICK_PERIOD_MS;
      if (poll_gap_ms > g_http_task_poll_gap_max_ms) {
        g_http_task_poll_gap_max_ms = poll_gap_ms;
      }
    }
    last_poll_start = mutex_wait_start;
    w5500_mutex_take();
    const TickType_t mutex_wait_end = xTaskGetTickCount();
    const uint32_t mutex_wait_ms = (uint32_t)(mutex_wait_end - mutex_wait_start) * portTICK_PERIOD_MS;
    if (mutex_wait_ms > g_http_task_mutex_wait_max_ms) {
      g_http_task_mutex_wait_max_ms = mutex_wait_ms;
    }
    w5500_http_trace_mutex_wait((uint32_t)mutex_wait_start, (uint32_t)mutex_wait_end);
    (void)w5500_http_status_poll();
    const uint32_t poll_exec_ms = (uint32_t)(xTaskGetTickCount() - mutex_wait_end) * portTICK_PERIOD_MS;
    if (poll_exec_ms > g_http_task_poll_exec_max_ms) {
      g_http_task_poll_exec_max_ms = poll_exec_ms;
    }
    w5500_mutex_give();
    g_http_task_loop_count++;
    vTaskDelay(pdMS_TO_TICKS(50u));
  }
}

static void dbc_task(void *argument)
{
  (void)argument;
  g_dbc_task_started = 1u;
  for (;;) {
    if (w5500_http_dbc_reload_requested()) {
      ++g_dbc_task_request_count;
      g_dbc_task_last_result = (uint32_t)w5500_http_process_dbc_reload();
      ++g_dbc_task_complete_count;
    }
    ++g_dbc_task_loop_count;
    vTaskDelay(pdMS_TO_TICKS(50u));
  }
}

static void tf_task(void *argument)
{
  (void)argument;
  g_tf_task_started = 1u;
  g_tf_task_last_result = (uint32_t)tf_card_bringup_run();
  g_tf_card_bringup_status = (int)g_tf_task_last_result;
  g_tf_task_complete = 1u;
  vTaskDelete(NULL);
}

static void monitor_task(void *argument)
{
  (void)argument;

  g_monitor_task_started = 1u;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000u));
    watchdog_service();
    ++g_freertos_loop_count;
    ++g_monitor_task_loop_count;
    bringup_print_status("run");
    bringup_print_http_trace();
  }
}

static int watchdog_start(void)
{
  uint32_t wait_started_ms;

  g_watchdog_reset_flags = RCC->RSR;
  RCC->RSR = RCC_RSR_RMVF;
  RCC->CSR |= RCC_CSR_LSION;
  wait_started_ms = HAL_GetTick();
  while ((RCC->CSR & RCC_CSR_LSIRDY) == 0u) {
    if ((uint32_t)(HAL_GetTick() - wait_started_ms) >= 100u) {
      g_watchdog_init_result = 1u;
      return 1;
    }
    vTaskDelay(pdMS_TO_TICKS(1u));
  }

  IWDG1->KR = 0xccccu;
  IWDG1->KR = 0x5555u;
  IWDG1->PR = 6u;
  IWDG1->RLR = 1000u;
  wait_started_ms = HAL_GetTick();
  while ((IWDG1->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0u) {
    if ((uint32_t)(HAL_GetTick() - wait_started_ms) >= 100u) {
      g_watchdog_init_result = 2u;
      return 1;
    }
    vTaskDelay(pdMS_TO_TICKS(1u));
  }

  IWDG1->KR = 0xaaaau;
  g_watchdog_baseline_ready = 0u;
  g_watchdog_unhealthy_mask = 0u;
  g_watchdog_init_result = 0u;
  g_watchdog_started = 1u;
  return 0;
}

static void watchdog_service(void)
{
  WatchdogSnapshot current = {
    .can_task = g_can_task_loop_count,
    .decode_task = g_can2_decode_task_loop_count,
    .w5500_task = g_w5500_task_loop_count,
    .http_task = g_http_task_loop_count,
    .dbc_task = g_dbc_task_loop_count,
    .dbc_candidate_progress = g_w5500_http_candidate_progress_count,
    .config_task = g_config_task_loop_count,
    .log_task = g_log_task_loop_count,
    .rule_task = g_rule_task_loop_count,
  };
  uint32_t unhealthy = 0u;

  if (g_watchdog_started == 0u) {
    return;
  }
  if (g_watchdog_baseline_ready == 0u) {
    g_watchdog_snapshot = current;
    g_watchdog_baseline_ready = 1u;
    return;
  }

  if (current.can_task == g_watchdog_snapshot.can_task) unhealthy |= 1u << 0;
  if (current.decode_task == g_watchdog_snapshot.decode_task) unhealthy |= 1u << 1;
  if (current.w5500_task == g_watchdog_snapshot.w5500_task &&
      current.dbc_candidate_progress ==
        g_watchdog_snapshot.dbc_candidate_progress) {
    unhealthy |= 1u << 2;
  }
  if (current.http_task == g_watchdog_snapshot.http_task &&
      current.dbc_candidate_progress ==
        g_watchdog_snapshot.dbc_candidate_progress) {
    unhealthy |= 1u << 3;
  }
  if (current.dbc_task == g_watchdog_snapshot.dbc_task &&
      current.dbc_candidate_progress ==
        g_watchdog_snapshot.dbc_candidate_progress) {
    unhealthy |= 1u << 4;
  }
  if (current.config_task == g_watchdog_snapshot.config_task) unhealthy |= 1u << 5;
  if (current.log_task == g_watchdog_snapshot.log_task) unhealthy |= 1u << 6;
  if (current.rule_task == g_watchdog_snapshot.rule_task) unhealthy |= 1u << 7;

  g_watchdog_snapshot = current;
  g_watchdog_unhealthy_mask = unhealthy;
  if (unhealthy == 0u) {
    IWDG1->KR = 0xaaaau;
    ++g_watchdog_refresh_count;
  }
}

static int config_queue_enqueue_pending(void)
{
  ConfigCommand command = {0};

  if (g_config_command_queue == NULL) {
    return 1;
  }

  if (g_w25q128_diagnostic_request != 0u) {
    command.type = CONFIG_COMMAND_DIAGNOSTIC;
    if (xQueueSend(g_config_command_queue, &command, 0u) != pdPASS) {
      ++g_config_queue_drop_count;
      return 1;
    }
    g_w25q128_diagnostic_request = 0u;
    ++g_config_queue_enqueue_count;
  }

  if (g_rule_file_v5_save_request != 0u) {
    command.type = CONFIG_COMMAND_RULE_FILE_V5_SAVE;
    command.rule_file_v5 = g_rule_file_v5_pending;
    if (xQueueSend(g_config_command_queue, &command, 0u) != pdPASS) {
      ++g_config_queue_drop_count;
      return 1;
    }
    ++g_config_queue_enqueue_count;
    g_rule_file_v5_save_request = 0u;
  }

  return 0;
}

static void config_task(void *argument)
{
  (void)argument;

  g_config_task_started = 1u;
  for (;;) {
    ConfigCommand command;

    (void)config_queue_enqueue_pending();
    if (xQueueReceive(g_config_command_queue, &command, 0u) == pdPASS) {
      ++g_config_queue_dequeue_count;
      ++g_config_task_command_count;
      g_config_task_last_command = command.type;
      if (command.type == CONFIG_COMMAND_DIAGNOSTIC) {
        g_w25q128_diagnostic_result = (uint32_t)w25q128_diagnostic_run();
        ++g_w25q128_diagnostic_count;
        bringup_print_status("w25q128_diag");
      } else if (command.type == CONFIG_COMMAND_RULE_FILE_V5_SAVE) {
        const size_t text_len = rule_file_format_v5(&command.rule_file_v5,
                                                     g_rule_file_v5_text,
                                                     sizeof(g_rule_file_v5_text));
        if (text_len == 0u ||
            !rule_file_v5_build_engine(&command.rule_file_v5,
                                       &g_rule_file_v5_candidate_engine)) {
          g_rule_file_v5_save_result = 1u;
        } else {
          g_rule_file_v5_save_result =
            (uint32_t)stm32h750_tf_replace_file_with_backup_locked(
              "/config/rules-v5.tmp", RULE_FILE_V5_PATH,
              "/config/rules-v5.prev",
              (const uint8_t *)g_rule_file_v5_text, text_len);
          if (g_rule_file_v5_save_result == 0u) {
            g_rule_file_v5_current = command.rule_file_v5;
            g_rule_file_v5_pending = command.rule_file_v5;
            g_rule_file_v5_load_result = 0u;
            g_rule_file_v5_size = (uint32_t)text_len;
            g_rule_file_v5_read_len = (uint32_t)text_len;
            g_rule_file_v5_rule_count =
              (uint32_t)g_rule_file_v5_candidate_engine.rule_count;
            rule_task_request_engine_reload(&g_rule_file_v5_candidate_engine);
          }
        }
        bringup_print_status("rule_file_v5_save");
      }
    }
    ++g_config_task_loop_count;
    vTaskDelay(pdMS_TO_TICKS(50u));
  }
}

static void bringup_default_task(void *argument)
{
  (void)argument;

  g_freertos_task_started = 1u;
  bringup_uart_write("\r\n[bringup] task start rtos=freertos\r\n");
  g_w25q128_bringup_status = w25q128_bringup_run();
  if (g_w25q128_bringup_status == 0) {
    uint32_t on_threshold;
    uint32_t off_threshold;
    uint32_t delay_ms;
    uint32_t timeout_ms;

    if (w25q128_rule_config_load(&on_threshold, &off_threshold, &delay_ms, &timeout_ms) == 0) {
      g_rule_task_config_on_threshold = on_threshold;
      g_rule_task_config_off_threshold = off_threshold;
      g_rule_task_config_delay_ms = delay_ms;
      g_rule_task_config_timeout_ms = timeout_ms;
      g_rule_task_config_pending_on_threshold = on_threshold;
      g_rule_task_config_pending_off_threshold = off_threshold;
      g_rule_task_config_pending_delay_ms = delay_ms;
      g_rule_task_config_pending_timeout_ms = timeout_ms;
    }
  }
  bringup_print_status("w25q128");
  g_can_bringup_status = can_bringup_run();
  bringup_print_status("can");
  g_can_external_bringup_status = can_external_bringup_run();
  bringup_print_status("can_ext");
  g_can2_analyzer_bringup_status = can2_analyzer_bringup_run();
  bringup_print_status("can2");
  if (can2_analyzer_rx_queue_init() != 0) {
    Error_Handler();
  }
  if (can2_analyzer_tx_queue_init() != 0) {
    Error_Handler();
  }
  g_w5500_bringup_status = w5500_bringup_run();
  bringup_print_status("w5500");
  if (xTaskCreate(tf_task,
                  "tf",
                  2048u,
                  NULL,
                  tskIDLE_PRIORITY + 2u,
                  NULL) != pdPASS) {
    g_tf_task_started = 0xffffffffu;
    Error_Handler();
  }
  for (uint32_t wait_ms = 0u; g_tf_task_complete == 0u && wait_ms < 5000u; ++wait_ms) {
    vTaskDelay(pdMS_TO_TICKS(1u));
  }
  if (g_tf_task_complete == 0u) {
    g_tf_task_last_result = 0xffffffffu;
    g_tf_card_bringup_status = -1;
    Error_Handler();
  }
  (void)w5500_http_recover_large_dbc_candidate();
  bringup_print_status("init");
  g_freertos_bringup_complete = 1u;

  if (w5500_http_dbc_reload_queue_init() != 0) {
    Error_Handler();
  }
  g_config_command_queue = xQueueCreate(2u, sizeof(ConfigCommand));
  g_config_queue_ready = g_config_command_queue != NULL ? 1u : 0u;
  if (g_config_command_queue == NULL) {
    Error_Handler();
  }

  g_w5500_mutex = xSemaphoreCreateMutex();
  if (g_w5500_mutex == NULL) {
    g_w5500_mutex_ready = 0u;
    Error_Handler();
  }
  g_w5500_mutex_ready = 1u;

  if (xTaskCreate(can2_periodic_task,
                  "can2",
                  1024u,
                  NULL,
                  tskIDLE_PRIORITY + 3u,
                  &g_can2_rx_task_handle) != pdPASS) {
    g_can_task_started = 0xffffffffu;
    Error_Handler();
  }
  can2_analyzer_set_rx_task_handle(g_can2_rx_task_handle);
  if (xTaskCreate(can2_decode_task,
                  "can2dec",
                  512u,
                  NULL,
                  tskIDLE_PRIORITY + 2u,
                  NULL) != pdPASS) {
    g_can2_decode_task_started = 0xffffffffu;
    Error_Handler();
  }
  if (xTaskCreate(w5500_periodic_task,
                  "w5500",
                  1024u,
                  NULL,
                  tskIDLE_PRIORITY + 2u,
                  NULL) != pdPASS) {
    g_w5500_task_started = 0xffffffffu;
    Error_Handler();
  }
  if (xTaskCreate(http_periodic_task,
                  "http",
                  1024u,
                  NULL,
                  tskIDLE_PRIORITY + 2u,
                  NULL) != pdPASS) {
    g_http_task_started = 0xffffffffu;
    Error_Handler();
  }
  if (xTaskCreate(dbc_task,
                  "dbc",
                  1024u,
                  NULL,
                  tskIDLE_PRIORITY + 2u,
                  NULL) != pdPASS) {
    g_dbc_task_started = 0xffffffffu;
    Error_Handler();
  }
  if (xTaskCreate(monitor_task,
                  "monitor",
                  1024u,
                  NULL,
                  tskIDLE_PRIORITY + 1u,
                  NULL) != pdPASS) {
    g_monitor_task_started = 0xffffffffu;
    Error_Handler();
  }
  if (xTaskCreate(config_task,
                  "config",
                  1024u,
                  NULL,
                  tskIDLE_PRIORITY + 1u,
                  NULL) != pdPASS) {
    g_config_task_started = 0xffffffffu;
    Error_Handler();
  }
  if (xTaskCreate(signal_log_task,
                  "log",
                  1024u,
                  NULL,
                  tskIDLE_PRIORITY + 1u,
                  NULL) != pdPASS) {
    g_log_task_started = 0xffffffffu;
    Error_Handler();
  }
  if (xTaskCreate(rule_task,
                  "rule",
                  1024u,
                  NULL,
                  tskIDLE_PRIORITY + 2u,
                  NULL) != pdPASS) {
    g_rule_task_started = 0xffffffffu;
    Error_Handler();
  }
  rule_file_load_from_tf();
  (void)w5500_http_recover_large_dbc_active();
  /* Candidate recovery can observe a transient TF state before the later
   * active/runtime recovery sequence. Retry after that sequence so a valid
   * candidate manifest is not left unavailable for the whole boot. */
  (void)w5500_http_recover_large_dbc_candidate();
  if (watchdog_start() != 0) {
    Error_Handler();
  }

  vTaskDelete(NULL);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_FDCAN1_Init();
  MX_FDCAN2_Init();
  MX_QUADSPI_Init();
  MX_USART2_UART_Init();
  MX_SDMMC1_SD_Init();
  MX_FATFS_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */
  (void)stm32h750_fs_mutex_init();
  signal_log_control_init(&g_signal_log_control);
  selected_signal_log_session_init(&g_selected_signal_log_session);
  bringup_uart_write("\r\n[bringup] boot stm32h750 rtos=freertos usart2=115200 sd_detect=skip lan=removed w5500=spi2 qspi=w25q128 can=fdcan1-loopback cext=fdcan1-external-loopback can2=pb5pb6-analyzer\r\n");
  if (xTaskCreate(bringup_default_task,
                  "bringup",
                  4096u,
                  NULL,
                  tskIDLE_PRIORITY + 1u,
                  NULL) != pdPASS) {
    g_freertos_task_started = 0xffffffffu;
    Error_Handler();
  }
  vTaskStartScheduler();
  Error_Handler();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 50;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 8;
  RCC_OscInitStruct.PLL.PLLR = 8;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
