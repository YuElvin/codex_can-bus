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
#include "rule_config.h"
#include "rule_engine.h"
#include "signal_log_buffer.h"
#include "semphr.h"
#include "task.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

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
extern volatile uint32_t g_w5500_http_dbc_reload_queue_ready;
extern volatile uint32_t g_w5500_http_dbc_reload_enqueue_count;
extern volatile uint32_t g_w5500_http_dbc_reload_queue_drop_count;
volatile uint32_t g_monitor_task_started;
volatile uint32_t g_monitor_task_loop_count;
volatile uint32_t g_config_task_started;
volatile uint32_t g_config_task_loop_count;
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
volatile uint32_t g_rule_task_condition_since_ms;
volatile uint32_t g_rule_task_delay_pending;
volatile uint32_t g_rule_task_hysteresis_latched;
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
volatile uint32_t g_rule_task_config_save_request;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void bringup_print_status(const char *phase);
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
  char line[1080];
  (void)snprintf(line,
                 sizeof(line),
                 "[bringup] %s rtos=%lu rtc=%lu rdy=%lu ctsk=%lu ctlp=%lu c2dts=%lu c2dtl=%lu c2qr=%lu c2qe=%lu c2qd=%lu c2qdrop=%lu c2tqr=%lu c2tqe=%lu c2tqd=%lu c2tqdrop=%lu wtsk=%lu wtlp=%lu htsk=%lu htlp=%lu wm=%lu dtsk=%lu dtlp=%lu dreq=%lu dcmp=%lu dr=%lu dq=%lu denq=%lu ddrop=%lu ttsk=%lu tdone=%lu tres=%lu mtsk=%lu mtlp=%lu can=%d ctx=%lu crx=%lu ce=%lu cbo=%lu ctec=%lu crec=%lu cid=%08lx cdl=%lu cd0=%02lx cext=%d extx=%lu exrx=%lu exe=%lu exbo=%lu extec=%lu exrec=%lu exid=%08lx exdl=%lu exd0=%02lx can2=%d c2tx=%lu c2rx=%lu c2e=%lu c2bo=%lu c2tec=%lu c2rec=%lu c2id=%08lx c2dl=%lu c2d0=%02lx c2sr=%lu c2pc=%lu qspi=%d qid=%06lx qsr=%02lx qaddr=%06lx qmi=%lu qe=%02lx qa=%02lx qhs=%lu tf=%d fsm=%lu fsl=%lu www=%lu wwwl=%lu w=%d wir=%lu wv=%02lx wp=%02lx wl=%lu wn=%lu http=%lu hsr=%02lx hreq=%lu hpath=%lu hcode=%lu hstatic=%lu hsrd=%lu herr=%lu sdh=%lu sde=%08lx sds=%08lx sdc=%lu\r\n",
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
                 (unsigned long)g_w5500_http_request_count,
                 (unsigned long)g_w5500_http_last_path,
                 (unsigned long)g_w5500_http_last_code,
                 (unsigned long)g_w5500_http_static_count,
                 (unsigned long)g_w5500_http_static_read_result,
                 (unsigned long)g_w5500_http_error_count,
                 (unsigned long)g_tf_sd_last_hal_status,
                 (unsigned long)g_tf_sd_last_error,
                 (unsigned long)g_tf_sd_last_sta,
                 (unsigned long)g_tf_sd_last_dcount);
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
    const RelayState manual_relays[RULE_RELAY_COUNT] = {
      g_rule_task_manual_relay1 != 0u ? RELAY_STATE_ON : RELAY_STATE_OFF,
      g_rule_task_manual_relay2 != 0u ? RELAY_STATE_ON : RELAY_STATE_OFF,
    };
    const bool manual_enabled = g_rule_task_manual_enabled != 0u;
    const uint32_t now_ms = HAL_GetTick();
    const size_t count = can2_signal_cache_export_rule_snapshots(signals, 2u);
    bool marker_safe = true;

    if (g_rule_task_config_reload != 0u) {
      g_rule_task_config_reload = 0u;
      (void)rule_task_load_config(&engine);
    }

    for (size_t i = 0u; i < count; ++i) {
      if (strcmp(signals[i].key, engine.rules[0].signal_key) == 0) {
        marker_safe = !signals[i].valid || now_ms - signals[i].updated_ms > engine.rules[0].timeout_ms;
        break;
      }
    }
    g_rule_task_input_count = (uint32_t)count;
    rule_engine_set_manual(&engine, manual_enabled, manual_relays);
    rule_engine_evaluate(&engine, signals, count, now_ms, relays);
    rule_apply_relays(relays);
    g_rule_task_rule_matched = g_rule_task_relay1_output == (uint32_t)RELAY_STATE_ON ? 1u : 0u;
    g_rule_task_condition_since_ms = engine.rules[0].condition_since_ms;
    g_rule_task_delay_pending = engine.rules[0].condition_since_ms != 0u &&
                                  g_rule_task_relay1_output == (uint32_t)RELAY_STATE_OFF ? 1u : 0u;
    g_rule_task_hysteresis_latched = engine.rules[0].latched_state ? 1u : 0u;
    g_rule_task_safe_active = marker_safe ? 1u : 0u;
    g_rule_task_manual_active = manual_enabled ? 1u : 0u;
    ++g_rule_task_evaluation_count;
    ++g_rule_task_loop_count;
    vTaskDelay(pdMS_TO_TICKS(50u));
  }
}

static void signal_log_task(void *argument)
{
  enum {
    LOG_SAMPLE_MS = 1000u,
    LOG_FLUSH_MS = 5000u,
    LOG_FLUSH_THRESHOLD = 512u,
  };
  static char storage[768];
  static const char default_path[] = "/log/signal.csv";
  static const char recovery_path[] = "/log/signal-recovery.csv";
  SignalLogBuffer buffer;
  const char *active_path;
  uint32_t last_sample_ms;
  uint32_t last_flush_ms;
  size_t file_size = 0u;
  const int default_file_size_result = stm32h750_tf_file_size_locked(default_path, &file_size);
  const SignalLogPathMode path_mode = signal_log_select_path(default_file_size_result);

  (void)argument;
  active_path = path_mode == SIGNAL_LOG_PATH_RECOVERY ? recovery_path : default_path;
  if (path_mode == SIGNAL_LOG_PATH_RECOVERY ||
      default_file_size_result == SIGNAL_LOG_FILE_NOT_FOUND) {
    file_size = 0u;
  }
  g_log_path_mode = (uint32_t)path_mode;
  if (path_mode == SIGNAL_LOG_PATH_RECOVERY) {
    ++g_log_path_switch_count;
  }
  g_log_active_file_size = (uint32_t)file_size;
  g_tf_csv_file_size = (uint32_t)file_size;
  signal_log_buffer_init(&buffer, storage, sizeof(storage));
  last_sample_ms = HAL_GetTick();
  last_flush_ms = last_sample_ms;
  g_log_task_started = 1u;

  for (;;) {
    const uint32_t now_ms = HAL_GetTick();
    if ((uint32_t)(now_ms - last_sample_ms) >= LOG_SAMPLE_MS) {
      SignalCacheEntry entries[2];
      const size_t count = can2_signal_cache_copy(entries, 2u);
      last_sample_ms = now_ms;
      ++g_log_sample_count;
      if (count == 0u) {
        ++g_log_drop_count;
      } else {
        const SignalLogBufferResult result = signal_log_buffer_append_snapshot(&buffer,
                                                                                 entries,
                                                                                 count,
                                                                                 file_size == 0u && buffer.length == 0u);
        if (result != SIGNAL_LOG_BUFFER_OK) {
          if (result == SIGNAL_LOG_BUFFER_SERIALIZE_ERROR) {
            ++g_log_failure_count;
          }
          ++g_log_drop_count;
          g_log_last_result = (uint32_t)result + 2u;
        } else {
          ++g_log_buffer_samples;
          g_log_buffer_len = (uint32_t)buffer.length;
        }
      }
    }

    if (signal_log_buffer_should_flush(&buffer,
                                       LOG_FLUSH_THRESHOLD,
                                       last_flush_ms,
                                       now_ms,
                                       LOG_FLUSH_MS)) {
      ++g_log_flush_count;
      g_tf_csv_write_len = (uint32_t)buffer.length;
      g_tf_csv_write_result = (uint32_t)stm32h750_tf_append_file_locked(active_path,
                                                                          (const uint8_t *)buffer.data,
                                                                          buffer.length,
                                                                          &file_size);
      g_tf_csv_file_size = (uint32_t)file_size;
      g_log_active_file_size = (uint32_t)file_size;
      g_log_last_result = g_tf_csv_write_result;
      last_flush_ms = now_ms;
      if (g_tf_csv_write_result == 0u) {
        ++g_tf_csv_write_count;
        ++g_log_write_count;
      } else {
        ++g_log_failure_count;
        g_log_drop_count += g_log_buffer_samples;
      }
      signal_log_buffer_clear(&buffer);
      g_log_buffer_len = 0u;
      g_log_buffer_samples = 0u;
    }

    ++g_log_task_loop_count;
    vTaskDelay(pdMS_TO_TICKS(100u));
  }
}

static void can2_periodic_task(void *argument)
{
  uint32_t poll_ticks = 0u;

  (void)argument;

  g_can_task_started = 1u;
  for (;;) {
    if (poll_ticks == 0u) {
      (void)can2_analyzer_poll();
    } else {
      (void)can2_analyzer_receive();
    }
    ++poll_ticks;
    if (poll_ticks >= 20u) {
      poll_ticks = 0u;
    }
    g_can_task_loop_count++;
    vTaskDelay(pdMS_TO_TICKS(50u));
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

  g_http_task_started = 1u;
  for (;;) {
    w5500_mutex_take();
    (void)w5500_http_status_poll();
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
    ++g_freertos_loop_count;
    ++g_monitor_task_loop_count;
    bringup_print_status("run");
  }
}

static void config_task(void *argument)
{
  (void)argument;

  g_config_task_started = 1u;
  for (;;) {
    if (g_w25q128_diagnostic_request != 0u) {
      g_w25q128_diagnostic_request = 0u;
      g_w25q128_diagnostic_result = (uint32_t)w25q128_diagnostic_run();
      ++g_w25q128_diagnostic_count;
      bringup_print_status("w25q128_diag");
    }
    if (g_rule_task_config_save_request != 0u) {
      const uint32_t on_threshold = g_rule_task_config_pending_on_threshold;
      const uint32_t off_threshold = g_rule_task_config_pending_off_threshold;
      const uint32_t delay_ms = g_rule_task_config_pending_delay_ms;
      const uint32_t timeout_ms = g_rule_task_config_pending_timeout_ms;
      int save_result;

      g_rule_task_config_save_request = 0u;
      save_result = w25q128_rule_config_save(on_threshold,
                                              off_threshold,
                                              delay_ms,
                                              timeout_ms);
      if (save_result == 0) {
        g_rule_task_config_on_threshold = on_threshold;
        g_rule_task_config_off_threshold = off_threshold;
        g_rule_task_config_delay_ms = delay_ms;
        g_rule_task_config_timeout_ms = timeout_ms;
        g_rule_task_config_reload = 1u;
      }
      bringup_print_status("rule_config_save");
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
  (void)w5500_http_load_active_dbc();
  bringup_print_status("init");
  g_freertos_bringup_complete = 1u;

  if (w5500_http_dbc_reload_queue_init() != 0) {
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
                  NULL) != pdPASS) {
    g_can_task_started = 0xffffffffu;
    Error_Handler();
  }
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
