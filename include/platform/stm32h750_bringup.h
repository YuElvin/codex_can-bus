#ifndef STM32H750_BRINGUP_H
#define STM32H750_BRINGUP_H

#if defined(CAN_BUS_USE_STM32_HAL) || defined(STM32H750xx)

#include "ports/can_port.h"
#include "can_tx_control.h"
#include "dbc_parser.h"
#include "ports/tf_card_port.h"
#include "ports/w5500_port.h"
#include "signal_cache.h"

#include "ff.h"
#include "stm32h7xx_hal.h"

typedef struct {
  FDCAN_HandleTypeDef *hfdcan;
} Stm32FdcanContext;

typedef struct {
  FATFS *fs;
  const char *logical_drive;
} Stm32TfCardContext;

typedef struct {
  SPI_HandleTypeDef *hspi;
  GPIO_TypeDef *cs_port;
  uint16_t cs_pin;
  GPIO_TypeDef *reset_port;
  uint16_t reset_pin;
  uint32_t spi_timeout_ms;
} Stm32W5500Context;

void stm32h750_fdcan_bind(CanPort *port, Stm32FdcanContext *ctx, FDCAN_HandleTypeDef *hfdcan);
void stm32h750_tf_card_bind(TfCardPort *port, Stm32TfCardContext *ctx, FATFS *fs, const char *logical_drive);
int stm32h750_fs_mutex_init(void);
int stm32h750_tf_read_file_locked(const char *path, uint8_t *data, size_t len, size_t *read_len);
int stm32h750_tf_file_size_locked(const char *path, size_t *file_size);
int stm32h750_tf_read_file_chunk_locked(const char *path,
                                        size_t offset,
                                        uint8_t *data,
                                        size_t len,
                                        size_t *read_len);
int stm32h750_tf_append_file_locked(const char *path,
                                    const uint8_t *data,
                                    size_t len,
                                    size_t *file_size);
int stm32h750_tf_replace_file_locked(const char *tmp_path,
                                     const char *final_path,
                                     const uint8_t *data,
                                     size_t len);
int stm32h750_tf_replace_file_with_backup_locked(const char *tmp_path,
                                                 const char *final_path,
                                                 const char *backup_path,
                                                 const uint8_t *data,
                                                 size_t len);
int stm32h750_tf_ensure_default_www(void);
int stm32h750_tf_ensure_default_rule_file(uint32_t on_threshold,
                                          uint32_t off_threshold,
                                          uint32_t delay_ms,
                                          uint32_t timeout_ms);
int stm32h750_tf_ensure_default_rule_file_v2(void);
void stm32h750_w5500_bind(W5500Port *port,
                          Stm32W5500Context *ctx,
                          SPI_HandleTypeDef *hspi,
                          GPIO_TypeDef *cs_port,
                          uint16_t cs_pin,
                          GPIO_TypeDef *reset_port,
                          uint16_t reset_pin);

int can_bringup_run(void);
int can_external_bringup_run(void);
int can2_analyzer_bringup_run(void);
int can2_analyzer_poll(void);
int can2_analyzer_receive(void);
int can2_analyzer_decode_pending(void);
int can2_analyzer_rx_queue_init(void);
int can2_analyzer_tx_queue_init(void);
void can2_analyzer_set_rx_task_handle(void *task_handle);
void can2_analyzer_rx_notify_from_isr(uint32_t events);
int can2_tx_control_submit(const CanTxControlConfig *config, uint32_t *request_seq);
void can2_tx_control_snapshot(CanTxControlState *state);
size_t can2_signal_cache_copy(SignalCacheEntry *out_entries, size_t out_capacity);
size_t can2_tx_signal_cache_copy(SignalCacheEntry *out_entries, size_t out_capacity);
size_t can2_signal_cache_mark_stale(uint32_t now_ms, uint32_t stale_after_ms);
size_t can2_signal_cache_export_rule_snapshots(SignalSnapshot *out_signals, size_t out_capacity);
size_t can2_signal_cache_export_rule_snapshots_for_engine(const RuleEngine *engine,
                                                          SignalSnapshot *out_signals,
                                                          size_t out_capacity);
int tf_card_bringup_run(void);
int w25q128_bringup_run(void);
int w25q128_diagnostic_run(void);
int w25q128_rule_config_load(uint32_t *on_threshold,
                              uint32_t *off_threshold,
                              uint32_t *delay_ms,
                              uint32_t *timeout_ms);
int w25q128_rule_config_save(uint32_t on_threshold,
                              uint32_t off_threshold,
                              uint32_t delay_ms,
                              uint32_t timeout_ms);
int w5500_bringup_run(void);
int w5500_bringup_poll(void);
int w5500_http_dbc_lock(void);
void w5500_http_dbc_unlock(void);
int w5500_http_load_active_dbc(void);
void w5500_http_request_dbc_reload(void);
int w5500_http_dbc_reload_queue_init(void);
int w5500_http_dbc_reload_requested(void);
int w5500_http_process_dbc_reload(void);
int w5500_http_dbc_reload_complete(void);
int w5500_http_dbc_reload_result(void);
const DbcDatabase *w5500_http_active_dbc_snapshot(void);
int w5500_http_status_poll(void);
void w5500_http_trace_mutex_wait(uint32_t start_tick, uint32_t end_tick);
int rule_task_manual_override_submit(uint32_t enabled,
                                     uint32_t relay1,
                                     uint32_t relay2,
                                     uint32_t *request_seq);
void rule_task_manual_override_snapshot(uint32_t *enabled,
                                        uint32_t *relay1,
                                        uint32_t *relay2,
                                        uint32_t *request_seq,
                                        uint32_t *applied_seq,
                                        uint32_t *relay1_output,
                                        uint32_t *relay2_output);

#endif

#endif
