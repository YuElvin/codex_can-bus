#if defined(CAN_BUS_USE_STM32_HAL) || defined(STM32H750xx)

#include "platform/stm32h750_bringup.h"
#include "dbc_upload.h"
#include "large_dbc_contract.h"
#if defined(CAN_BUS_LARGE_DBC_H1_FAULT_INJECTION)
#include "platform/large_dbc_candidate_stm32.h"
#endif

#include "bsp_driver_sd.h"
#include "FreeRTOS.h"
#include "semphr.h"

#include <stdio.h>
#include <string.h>

extern SD_HandleTypeDef hsd1;
extern char SDPath[4];

#define TF_CARD_SD_CLOCK_DIV 16u
#define TF_CARD_SD_OP_TIMEOUT_MS 1000u

volatile uint32_t g_tf_mount_result;
volatile uint32_t g_tf_mkdir_result;
volatile uint32_t g_tf_write_attempts;
volatile uint32_t g_tf_write_open_result;
volatile uint32_t g_tf_write_result;
volatile uint32_t g_tf_write_sync_result;
volatile uint32_t g_tf_write_close_result;
volatile uint32_t g_tf_write_len;
volatile uint32_t g_tf_replace_unlink_result;
volatile uint32_t g_tf_replace_rename_result;
volatile uint32_t g_tf_replace_backup_rename_result;
volatile uint32_t g_tf_replace_restore_result;
volatile uint32_t g_tf_read_open_result;
volatile uint32_t g_tf_read_result;
volatile uint32_t g_tf_read_close_result;
volatile uint32_t g_tf_read_len;
volatile uint32_t g_tf_read_offset;
volatile uint32_t g_tf_read_file_size;
volatile uint32_t g_tf_sd_init_count;
volatile uint32_t g_tf_sd_last_hal_status;
volatile uint32_t g_tf_sd_last_error;
volatile uint32_t g_tf_sd_last_sta;
volatile uint32_t g_tf_sd_last_dcount;
volatile uint32_t g_tf_sd_last_clkcr;
volatile uint32_t g_tf_sd_last_operation;
volatile uint32_t g_tf_sd_read_call_count;
volatile uint32_t g_tf_sd_read_failure_count;
volatile uint32_t g_tf_sd_read_last_lba;
volatile uint32_t g_tf_sd_read_last_blocks;
volatile uint32_t g_tf_sd_read_before_state;
volatile uint32_t g_tf_sd_read_before_context;
volatile uint32_t g_tf_sd_read_before_error;
volatile uint32_t g_tf_sd_read_before_sta;
volatile uint32_t g_tf_sd_read_before_dcount;
volatile uint32_t g_tf_sd_read_before_mask;
volatile uint32_t g_tf_sd_read_before_dctrl;
volatile uint32_t g_tf_sd_read_before_clkcr;
volatile uint32_t g_tf_sd_read_after_state;
volatile uint32_t g_tf_sd_read_after_context;
volatile uint32_t g_tf_sd_read_after_error;
volatile uint32_t g_tf_sd_read_after_sta;
volatile uint32_t g_tf_sd_read_after_dcount;
volatile uint32_t g_tf_sd_read_after_mask;
volatile uint32_t g_tf_sd_read_after_dctrl;
volatile uint32_t g_tf_sd_read_after_clkcr;
volatile uint32_t g_tf_fs_mutex_ready;
volatile uint32_t g_tf_fs_lock_result;
volatile uint32_t g_tf_www_index_status = 0xffffffffu;
volatile uint32_t g_tf_www_index_len;
volatile uint32_t g_tf_append_stage;

static SemaphoreHandle_t g_tf_fs_mutex;
static FIL g_large_dbc_upload_file;
static char g_large_dbc_upload_path[40];
static uint32_t g_large_dbc_upload_expected_size;
static uint8_t g_large_dbc_upload_open;

volatile uint32_t g_large_dbc_upload_open_result = 0xffffffffu;
volatile uint32_t g_large_dbc_upload_write_result = 0xffffffffu;
volatile uint32_t g_large_dbc_upload_sync_result = 0xffffffffu;
volatile uint32_t g_large_dbc_upload_close_result = 0xffffffffu;
volatile uint32_t g_large_dbc_upload_unlink_result = 0xffffffffu;
volatile uint32_t g_large_dbc_upload_write_count;
volatile uint32_t g_large_dbc_upload_written_bytes;

static int tf_fs_lock(void) {
  if (g_tf_fs_mutex == NULL) {
    g_tf_fs_lock_result = 1u;
    return 1;
  }
  if (xSemaphoreTake(g_tf_fs_mutex, pdMS_TO_TICKS(1000u)) != pdTRUE) {
    g_tf_fs_lock_result = 2u;
    return 1;
  }
  g_tf_fs_lock_result = 0u;
  return 0;
}

static void tf_fs_unlock(void) {
  if (g_tf_fs_mutex != NULL) {
    (void)xSemaphoreGive(g_tf_fs_mutex);
  }
}

int stm32h750_tf_fs_lock(void) {
  return tf_fs_lock();
}

void stm32h750_tf_fs_unlock(void) {
  tf_fs_unlock();
}

int stm32h750_fs_mutex_init(void) {
  if (g_tf_fs_mutex == NULL) {
    g_tf_fs_mutex = xSemaphoreCreateMutex();
  }
  g_tf_fs_mutex_ready = g_tf_fs_mutex != NULL ? 1u : 0u;
  return g_tf_fs_mutex != NULL ? 0 : 1;
}

static void tf_sd_record_diag(HAL_StatusTypeDef status) {
  g_tf_sd_last_hal_status = (uint32_t)status;
  g_tf_sd_last_error = hsd1.ErrorCode;
  if (hsd1.Instance != NULL) {
    g_tf_sd_last_sta = hsd1.Instance->STA;
    g_tf_sd_last_dcount = hsd1.Instance->DCOUNT;
    g_tf_sd_last_clkcr = hsd1.Instance->CLKCR;
  }
}

static void tf_sd_apply_bringup_config(void) {
  hsd1.Instance = SDMMC1;
  hsd1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
  hsd1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
  hsd1.Init.BusWide = SDMMC_BUS_WIDE_1B;
  hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_ENABLE;
  hsd1.Init.ClockDiv = TF_CARD_SD_CLOCK_DIV;
}

__attribute__((weak)) bool stm32h750_tf_card_detect(void) {
  return true;
}

uint8_t BSP_SD_IsDetected(void) {
  return SD_PRESENT;
}

uint8_t BSP_SD_Init(void) {
  HAL_StatusTypeDef status;

  g_tf_sd_init_count++;
  if (BSP_SD_IsDetected() != SD_PRESENT) {
    return MSD_ERROR_SD_NOT_PRESENT;
  }

  if (hsd1.State != HAL_SD_STATE_RESET) {
    (void)HAL_SD_DeInit(&hsd1);
  }
  tf_sd_apply_bringup_config();

  g_tf_sd_last_operation = 1u;
  status = HAL_SD_Init(&hsd1);
  tf_sd_record_diag(status);
  return status == HAL_OK ? MSD_OK : MSD_ERROR;
}

uint8_t BSP_SD_ReadBlocks_DMA(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks) {
  HAL_StatusTypeDef status;

  g_tf_sd_last_operation = 2u;
  g_tf_sd_read_call_count++;
  g_tf_sd_read_last_lba = ReadAddr;
  g_tf_sd_read_last_blocks = NumOfBlocks;
  g_tf_sd_read_before_state = (uint32_t)hsd1.State;
  g_tf_sd_read_before_context = hsd1.Context;
  g_tf_sd_read_before_error = hsd1.ErrorCode;
  if (hsd1.Instance != NULL) {
    g_tf_sd_read_before_sta = hsd1.Instance->STA;
    g_tf_sd_read_before_dcount = hsd1.Instance->DCOUNT;
    g_tf_sd_read_before_mask = hsd1.Instance->MASK;
    g_tf_sd_read_before_dctrl = hsd1.Instance->DCTRL;
    g_tf_sd_read_before_clkcr = hsd1.Instance->CLKCR;
  } else {
    g_tf_sd_read_before_sta = 0u;
    g_tf_sd_read_before_dcount = 0u;
    g_tf_sd_read_before_mask = 0u;
    g_tf_sd_read_before_dctrl = 0u;
    g_tf_sd_read_before_clkcr = 0u;
  }
  status = HAL_SD_ReadBlocks(&hsd1,
                             (uint8_t *)pData,
                             ReadAddr,
                             NumOfBlocks,
                             TF_CARD_SD_OP_TIMEOUT_MS);
  g_tf_sd_read_after_state = (uint32_t)hsd1.State;
  g_tf_sd_read_after_context = hsd1.Context;
  g_tf_sd_read_after_error = hsd1.ErrorCode;
  if (hsd1.Instance != NULL) {
    g_tf_sd_read_after_sta = hsd1.Instance->STA;
    g_tf_sd_read_after_dcount = hsd1.Instance->DCOUNT;
    g_tf_sd_read_after_mask = hsd1.Instance->MASK;
    g_tf_sd_read_after_dctrl = hsd1.Instance->DCTRL;
    g_tf_sd_read_after_clkcr = hsd1.Instance->CLKCR;
  } else {
    g_tf_sd_read_after_sta = 0u;
    g_tf_sd_read_after_dcount = 0u;
    g_tf_sd_read_after_mask = 0u;
    g_tf_sd_read_after_dctrl = 0u;
    g_tf_sd_read_after_clkcr = 0u;
  }
  if (status != HAL_OK) {
    g_tf_sd_read_failure_count++;
  }
  tf_sd_record_diag(status);
  if (status != HAL_OK) {
    return MSD_ERROR;
  }
  BSP_SD_ReadCpltCallback();
  return MSD_OK;
}

uint8_t BSP_SD_WriteBlocks_DMA(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks) {
  HAL_StatusTypeDef status;

  g_tf_sd_last_operation = 3u;
  status = HAL_SD_WriteBlocks(&hsd1,
                              (uint8_t *)pData,
                              WriteAddr,
                              NumOfBlocks,
                              TF_CARD_SD_OP_TIMEOUT_MS);
  tf_sd_record_diag(status);
  if (status != HAL_OK) {
    return MSD_ERROR;
  }
  BSP_SD_WriteCpltCallback();
  return MSD_OK;
}

static TfCardResult build_fatfs_path(const Stm32TfCardContext *tf,
                                     const char *path,
                                     char *buffer,
                                     size_t buffer_len) {
  if (tf == NULL || path == NULL || buffer == NULL || buffer_len == 0u) {
    return TF_CARD_ERROR;
  }

  const char *drive = tf->logical_drive != NULL ? tf->logical_drive : "";
  const size_t drive_len = strlen(drive);
  const bool drive_has_slash = drive_len > 0u && drive[drive_len - 1u] == '/';
  const bool path_has_slash = path[0] == '/';
  const char *path_start = drive_has_slash && path_has_slash ? path + 1u : path;
  const size_t path_len = strlen(path_start);

  if (drive_len + path_len + 1u > buffer_len) {
    return TF_CARD_ERROR;
  }

  memcpy(buffer, drive, drive_len);
  memcpy(buffer + drive_len, path_start, path_len + 1u);
  return TF_CARD_OK;
}

static bool fatfs_present(void *ctx) {
  (void)ctx;
  return stm32h750_tf_card_detect();
}

static TfCardResult fatfs_mount(void *ctx) {
  Stm32TfCardContext *tf = (Stm32TfCardContext *)ctx;
  if (tf == NULL || tf->fs == NULL || tf->logical_drive == NULL) {
    return TF_CARD_ERROR;
  }
  if (tf_fs_lock() != 0) {
    return TF_CARD_ERROR;
  }
  const FRESULT result = f_mount(tf->fs, tf->logical_drive, 1u);
  tf_fs_unlock();
  g_tf_mount_result = result;
  return result == FR_OK ? TF_CARD_OK : TF_CARD_ERROR;
}

static TfCardResult fatfs_mkdir(void *ctx, const char *path) {
  char full_path[64];
  if (build_fatfs_path((const Stm32TfCardContext *)ctx, path, full_path, sizeof(full_path)) != TF_CARD_OK) {
    return TF_CARD_ERROR;
  }
  if (tf_fs_lock() != 0) {
    return TF_CARD_ERROR;
  }
  const FRESULT result = f_mkdir(full_path);
  tf_fs_unlock();
  g_tf_mkdir_result = result;
  return result == FR_OK || result == FR_EXIST ? TF_CARD_OK : TF_CARD_ERROR;
}

static TfCardResult fatfs_write(void *ctx, const char *path, const uint8_t *data, size_t len) {
  char full_path[64];
  if (build_fatfs_path((const Stm32TfCardContext *)ctx, path, full_path, sizeof(full_path)) != TF_CARD_OK) {
    return TF_CARD_ERROR;
  }

  for (uint32_t attempt = 0u; attempt < 3u; ++attempt) {
    FIL file;
    UINT written = 0u;
    g_tf_write_attempts = attempt + 1u;
    if (tf_fs_lock() != 0) {
      return TF_CARD_ERROR;
    }
    g_tf_write_open_result = f_open(&file, full_path, FA_CREATE_ALWAYS | FA_WRITE);
    if (g_tf_write_open_result != FR_OK) {
      tf_fs_unlock();
      HAL_Delay(20u);
      continue;
    }
    g_tf_write_result = f_write(&file, data, (UINT)len, &written);
    g_tf_write_close_result = f_close(&file);
    tf_fs_unlock();
    g_tf_write_len = written;
    if (g_tf_write_result == FR_OK && g_tf_write_close_result == FR_OK && written == len) {
      return TF_CARD_OK;
    }
    HAL_Delay(20u);
  }
  return TF_CARD_ERROR;
}

static TfCardResult fatfs_read(void *ctx, const char *path, uint8_t *data, size_t len, size_t *read_len) {
  FIL file;
  UINT read = 0u;
  char full_path[64];
  if (build_fatfs_path((const Stm32TfCardContext *)ctx, path, full_path, sizeof(full_path)) != TF_CARD_OK) {
    return TF_CARD_ERROR;
  }
  if (tf_fs_lock() != 0) {
    return TF_CARD_ERROR;
  }
  g_tf_read_open_result = f_open(&file, full_path, FA_READ);
  if (g_tf_read_open_result != FR_OK) {
    tf_fs_unlock();
    return TF_CARD_ERROR;
  }
  const FRESULT result = f_read(&file, data, (UINT)len, &read);
  const FRESULT close_result = f_close(&file);
  tf_fs_unlock();
  g_tf_read_result = result;
  g_tf_read_close_result = close_result;
  g_tf_read_len = read;
  if (read_len != NULL) {
    *read_len = read;
  }
  return result == FR_OK && close_result == FR_OK ? TF_CARD_OK : TF_CARD_ERROR;
}

int stm32h750_tf_read_file_locked(const char *path, uint8_t *data, size_t len, size_t *read_len) {
  Stm32TfCardContext ctx = {
    .fs = NULL,
    .logical_drive = SDPath,
  };
  return fatfs_read(&ctx, path, data, len, read_len) == TF_CARD_OK ? 0 : 1;
}

int stm32h750_tf_file_size_locked(const char *path, size_t *file_size) {
  FIL file;
  char full_path[64];
  const Stm32TfCardContext ctx = {
    .fs = NULL,
    .logical_drive = SDPath,
  };

  if (file_size == NULL ||
      build_fatfs_path(&ctx, path, full_path, sizeof(full_path)) != TF_CARD_OK ||
      tf_fs_lock() != 0) {
    return -1;
  }
  g_tf_read_open_result = f_open(&file, full_path, FA_READ);
  if (g_tf_read_open_result != FR_OK) {
    tf_fs_unlock();
    return (int)g_tf_read_open_result;
  }
  *file_size = (size_t)f_size(&file);
  g_tf_read_file_size = (uint32_t)*file_size;
  g_tf_read_close_result = f_close(&file);
  tf_fs_unlock();
  return g_tf_read_close_result == FR_OK ? 0 : (int)g_tf_read_close_result;
}

int stm32h750_tf_read_file_chunk_locked(const char *path,
                                        size_t offset,
                                        uint8_t *data,
                                        size_t len,
                                        size_t *read_len) {
  FIL file;
  UINT read = 0u;
  char full_path[64];
  const Stm32TfCardContext ctx = {
    .fs = NULL,
    .logical_drive = SDPath,
  };

  if (data == NULL || read_len == NULL ||
      build_fatfs_path(&ctx, path, full_path, sizeof(full_path)) != TF_CARD_OK ||
      tf_fs_lock() != 0) {
    return 1;
  }
  g_tf_read_offset = (uint32_t)offset;
  g_tf_read_open_result = f_open(&file, full_path, FA_READ);
  if (g_tf_read_open_result != FR_OK) {
    tf_fs_unlock();
    return 1;
  }
  g_tf_read_result = f_lseek(&file, (FSIZE_t)offset);
  if (g_tf_read_result == FR_OK) {
    g_tf_read_result = f_read(&file, data, (UINT)len, &read);
  }
  g_tf_read_close_result = f_close(&file);
  tf_fs_unlock();
  g_tf_read_len = read;
  *read_len = read;
  return g_tf_read_result == FR_OK && g_tf_read_close_result == FR_OK ? 0 : 1;
}

int stm32h750_tf_append_file_locked(const char *path,
                                    const uint8_t *data,
                                    size_t len,
                                    size_t *file_size) {
  FIL file;
  UINT written = 0u;
  uint32_t failure_stage = 0u;
  char full_path[64];
  const Stm32TfCardContext ctx = {
    .fs = NULL,
    .logical_drive = SDPath,
  };

  g_tf_append_stage = 1u;
  if (path == NULL || data == NULL || len == 0u || file_size == NULL ||
      build_fatfs_path(&ctx, path, full_path, sizeof(full_path)) != TF_CARD_OK ||
      tf_fs_lock() != 0) {
    return 1;
  }
  g_tf_append_stage = 2u;
  g_tf_write_open_result = f_open(&file, full_path, FA_OPEN_ALWAYS | FA_WRITE);
  if (g_tf_write_open_result != FR_OK) {
    tf_fs_unlock();
    return 1;
  }
  g_tf_append_stage = 3u;
  g_tf_write_sync_result = FR_OK;
  g_tf_write_result = f_lseek(&file, f_size(&file));
  if (g_tf_write_result != FR_OK) {
    failure_stage = 3u;
  } else {
    g_tf_append_stage = 4u;
    g_tf_write_result = f_write(&file, data, (UINT)len, &written);
    if (g_tf_write_result != FR_OK || written != len) {
      failure_stage = 4u;
    } else {
#if defined(CAN_BUS_LARGE_DBC_H1_FAULT_INJECTION)
      if (large_dbc_h1_fault_consume(LARGE_DBC_H1_FAULT_LOG_APPEND_SYNC,
                                      LARGE_DBC_CANDIDATE_IO_SYNC,
                                      (uint32_t)FR_DISK_ERR)) {
        g_tf_write_sync_result = FR_DISK_ERR;
      } else
#endif
      {
        g_tf_write_sync_result = f_sync(&file);
      }
      if (g_tf_write_sync_result != FR_OK) {
        failure_stage = 4u;
      }
    }
  }
  *file_size = (size_t)f_size(&file);
  g_tf_write_len = written;
  g_tf_append_stage = 5u;
  g_tf_write_close_result = f_close(&file);
  if (g_tf_write_close_result != FR_OK) {
    failure_stage = 5u;
  }
  tf_fs_unlock();
  if (g_tf_write_result == FR_OK && g_tf_write_sync_result == FR_OK &&
      g_tf_write_close_result == FR_OK && written == len) {
    g_tf_append_stage = 6u;
    return 0;
  }
  if (failure_stage != 0u) {
    g_tf_append_stage = failure_stage;
  }
  return 1;
}

int stm32h750_tf_large_dbc_upload_begin(uint64_t generation,
                                        uint32_t expected_size) {
  char full_path[64];
  const Stm32TfCardContext ctx = {
    .fs = NULL,
    .logical_drive = SDPath,
  };
  if (generation == 0u || expected_size == 0u ||
      !dbc_upload_format_tmp_path(g_large_dbc_upload_path,
                                  sizeof(g_large_dbc_upload_path),
                                  generation) ||
      build_fatfs_path(&ctx,
                       g_large_dbc_upload_path,
                       full_path,
                       sizeof(full_path)) != TF_CARD_OK ||
      tf_fs_lock() != 0) {
    return 1;
  }
  if (g_large_dbc_upload_open != 0u) {
    tf_fs_unlock();
    return 1;
  }
  g_large_dbc_upload_expected_size = expected_size;
  g_large_dbc_upload_write_count = 0u;
  g_large_dbc_upload_written_bytes = 0u;
  g_large_dbc_upload_write_result = 0xffffffffu;
  g_large_dbc_upload_sync_result = 0xffffffffu;
  g_large_dbc_upload_close_result = 0xffffffffu;
  g_large_dbc_upload_unlink_result = 0xffffffffu;
  g_large_dbc_upload_open_result =
    f_open(&g_large_dbc_upload_file, full_path, FA_CREATE_ALWAYS | FA_WRITE);
  if (g_large_dbc_upload_open_result == FR_OK) {
    g_large_dbc_upload_open = 1u;
  }
  tf_fs_unlock();
  return g_large_dbc_upload_open != 0u ? 0 : 1;
}

int stm32h750_tf_large_dbc_upload_write(const uint8_t *data, size_t len) {
  UINT written = 0u;
  if (data == NULL || len == 0u || len > LARGE_DBC_UPLOAD_CHUNK_BYTES ||
      g_large_dbc_upload_open == 0u ||
      g_large_dbc_upload_written_bytes > g_large_dbc_upload_expected_size ||
      (uint32_t)len >
        g_large_dbc_upload_expected_size - g_large_dbc_upload_written_bytes ||
      tf_fs_lock() != 0) {
    return 1;
  }
  if ((uint32_t)f_tell(&g_large_dbc_upload_file) !=
      g_large_dbc_upload_written_bytes) {
    tf_fs_unlock();
    return 1;
  }
  g_large_dbc_upload_write_result =
    f_write(&g_large_dbc_upload_file, data, (UINT)len, &written);
  if (g_large_dbc_upload_write_result == FR_OK && written == len) {
    ++g_large_dbc_upload_write_count;
    g_large_dbc_upload_written_bytes += written;
  }
  tf_fs_unlock();
  return g_large_dbc_upload_write_result == FR_OK && written == len ? 0 : 1;
}

int stm32h750_tf_large_dbc_upload_finalize(void) {
  if (g_large_dbc_upload_open == 0u || tf_fs_lock() != 0) {
    return 1;
  }
  if ((uint32_t)f_size(&g_large_dbc_upload_file) !=
      g_large_dbc_upload_expected_size) {
    g_large_dbc_upload_sync_result = FR_INT_ERR;
  } else {
    g_large_dbc_upload_sync_result = f_sync(&g_large_dbc_upload_file);
  }
  g_large_dbc_upload_close_result = f_close(&g_large_dbc_upload_file);
  g_large_dbc_upload_open = 0u;
  tf_fs_unlock();
  return g_large_dbc_upload_sync_result == FR_OK &&
         g_large_dbc_upload_close_result == FR_OK ? 0 : 1;
}

void stm32h750_tf_large_dbc_upload_abort(void) {
  char full_path[64];
  const Stm32TfCardContext ctx = {
    .fs = NULL,
    .logical_drive = SDPath,
  };
  if (g_large_dbc_upload_path[0] == '\0' ||
      build_fatfs_path(&ctx,
                       g_large_dbc_upload_path,
                       full_path,
                       sizeof(full_path)) != TF_CARD_OK ||
      tf_fs_lock() != 0) {
    return;
  }
  if (g_large_dbc_upload_open != 0u) {
    g_large_dbc_upload_close_result = f_close(&g_large_dbc_upload_file);
    g_large_dbc_upload_open = 0u;
  }
  g_large_dbc_upload_unlink_result = f_unlink(full_path);
  tf_fs_unlock();
}

const char *stm32h750_tf_large_dbc_upload_path(void) {
  return g_large_dbc_upload_path;
}

static int tf_replace_file_locked(const char *tmp_path,
                                  const char *final_path,
                                  const char *backup_path,
                                  const uint8_t *data,
                                  size_t len) {
  FIL file;
  UINT written = 0u;
  char tmp_full_path[64];
  char final_full_path[64];
  char backup_full_path[64];
  uint8_t backup_available = 0u;
  uint8_t final_was_backed_up = 0u;
  const Stm32TfCardContext ctx = {
    .fs = NULL,
    .logical_drive = SDPath,
  };

  if (tmp_path == NULL || final_path == NULL || data == NULL || len == 0u ||
      build_fatfs_path(&ctx, tmp_path, tmp_full_path, sizeof(tmp_full_path)) != TF_CARD_OK ||
      build_fatfs_path(&ctx, final_path, final_full_path, sizeof(final_full_path)) != TF_CARD_OK ||
      (backup_path != NULL &&
       build_fatfs_path(&ctx, backup_path, backup_full_path, sizeof(backup_full_path)) != TF_CARD_OK) ||
      tf_fs_lock() != 0) {
    return 1;
  }

  g_tf_replace_unlink_result = 0xffffffffu;
  g_tf_replace_rename_result = 0xffffffffu;
  g_tf_replace_backup_rename_result = 0xffffffffu;
  g_tf_replace_restore_result = 0xffffffffu;
  backup_available = backup_path != NULL ? 1u : 0u;

  g_tf_write_open_result = f_open(&file, tmp_full_path, FA_CREATE_ALWAYS | FA_WRITE);
  if (g_tf_write_open_result != FR_OK) {
    tf_fs_unlock();
    return 1;
  }
  g_tf_write_result = f_write(&file, data, (UINT)len, &written);
  g_tf_write_close_result = f_close(&file);
  g_tf_write_len = written;
  if (g_tf_write_result != FR_OK || g_tf_write_close_result != FR_OK || written != len) {
    (void)f_unlink(tmp_full_path);
    tf_fs_unlock();
    return 1;
  }

  if (backup_available != 0u) {
    g_tf_replace_unlink_result = f_unlink(backup_full_path);
    if (g_tf_replace_unlink_result != FR_OK && g_tf_replace_unlink_result != FR_NO_FILE) {
      (void)f_unlink(tmp_full_path);
      tf_fs_unlock();
      return 1;
    }

    g_tf_replace_backup_rename_result = f_rename(final_full_path, backup_full_path);
    if (g_tf_replace_backup_rename_result == FR_OK) {
      final_was_backed_up = 1u;
    } else if (g_tf_replace_backup_rename_result != FR_NO_FILE) {
      (void)f_unlink(tmp_full_path);
      tf_fs_unlock();
      return 1;
    }
  } else {
    g_tf_replace_unlink_result = f_unlink(final_full_path);
    if (g_tf_replace_unlink_result != FR_OK && g_tf_replace_unlink_result != FR_NO_FILE) {
      (void)f_unlink(tmp_full_path);
      tf_fs_unlock();
      return 1;
    }
  }

  g_tf_replace_rename_result = f_rename(tmp_full_path, final_full_path);
  if (g_tf_replace_rename_result == FR_OK) {
    tf_fs_unlock();
    return 0;
  }

  if (final_was_backed_up != 0u) {
    g_tf_replace_restore_result = f_rename(backup_full_path, final_full_path);
  }
  tf_fs_unlock();
  return 1;
}

int stm32h750_tf_replace_file_locked(const char *tmp_path,
                                     const char *final_path,
                                     const uint8_t *data,
                                     size_t len) {
  return tf_replace_file_locked(tmp_path, final_path, NULL, data, len);
}

int stm32h750_tf_replace_file_with_backup_locked(const char *tmp_path,
                                                 const char *final_path,
                                                 const char *backup_path,
                                                 const uint8_t *data,
                                                 size_t len) {
  return tf_replace_file_locked(tmp_path, final_path, backup_path, data, len);
}

int stm32h750_tf_ensure_default_www(void) {
  static const uint8_t index_html[] =
    "<!doctype html><html><head><meta charset=\"utf-8\"><title>CAN Bus Gateway</title></head>"
    "<body><h1>CAN Bus Gateway</h1><p>W5500 HTTP status API is running.</p></body></html>\n";
  FIL file;
  UINT written = 0u;
  char full_path[64];
  const Stm32TfCardContext ctx = {
    .fs = NULL,
    .logical_drive = SDPath,
  };

  if (build_fatfs_path(&ctx, "/www/index.html", full_path, sizeof(full_path)) != TF_CARD_OK) {
    g_tf_www_index_status = 1u;
    return 1;
  }
  if (tf_fs_lock() != 0) {
    g_tf_www_index_status = 2u;
    return 1;
  }
  FRESULT result = f_open(&file, full_path, FA_READ);
  if (result == FR_OK) {
    g_tf_www_index_len = f_size(&file);
    (void)f_close(&file);
    tf_fs_unlock();
    g_tf_www_index_status = 0u;
    return 0;
  }
  result = f_open(&file, full_path, FA_CREATE_NEW | FA_WRITE);
  if (result == FR_EXIST) {
    tf_fs_unlock();
    g_tf_www_index_status = 0u;
    return 0;
  }
  if (result != FR_OK) {
    tf_fs_unlock();
    g_tf_www_index_status = 3u;
    return 1;
  }
  result = f_write(&file, index_html, (UINT)(sizeof(index_html) - 1u), &written);
  const FRESULT close_result = f_close(&file);
  tf_fs_unlock();
  g_tf_www_index_len = written;
  if (result != FR_OK || close_result != FR_OK || written != sizeof(index_html) - 1u) {
    g_tf_www_index_status = 4u;
    return 1;
  }
  g_tf_www_index_status = 0u;
  return 0;
}

int stm32h750_tf_ensure_default_rule_file(uint32_t on_threshold,
                                          uint32_t off_threshold,
                                          uint32_t delay_ms,
                                          uint32_t timeout_ms) {
  char content[160];
  FIL file;
  UINT written = 0u;
  char full_path[64];
  char config_path[64];
  const Stm32TfCardContext ctx = {
    .fs = NULL,
    .logical_drive = SDPath,
  };
  const int content_len = snprintf(content,
                                   sizeof(content),
                                   "version=1\n"
                                   "onThreshold=%lu\n"
                                   "offThreshold=%lu\n"
                                   "delayMs=%lu\n"
                                   "timeoutMs=%lu\n",
                                   (unsigned long)on_threshold,
                                   (unsigned long)off_threshold,
                                   (unsigned long)delay_ms,
                                   (unsigned long)timeout_ms);

  if (content_len <= 0 || (size_t)content_len >= sizeof(content) ||
      on_threshold <= off_threshold || delay_ms > timeout_ms ||
      build_fatfs_path(&ctx, "/config/rule.conf", full_path, sizeof(full_path)) != TF_CARD_OK ||
      build_fatfs_path(&ctx, "/config", config_path, sizeof(config_path)) != TF_CARD_OK ||
      tf_fs_lock() != 0) {
    return 1;
  }

  g_tf_write_attempts = 1u;
  g_tf_mkdir_result = f_mkdir(config_path);
  if (g_tf_mkdir_result != FR_OK && g_tf_mkdir_result != FR_EXIST) {
    tf_fs_unlock();
    return 1;
  }
  g_tf_write_open_result = f_open(&file, full_path, FA_READ);
  if (g_tf_write_open_result == FR_OK) {
    (void)f_close(&file);
    tf_fs_unlock();
    return 0;
  }
  if (g_tf_write_open_result != FR_NO_FILE) {
    tf_fs_unlock();
    return 1;
  }

  g_tf_write_open_result = f_open(&file, full_path, FA_CREATE_NEW | FA_WRITE);
  if (g_tf_write_open_result == FR_EXIST) {
    tf_fs_unlock();
    return 0;
  }
  if (g_tf_write_open_result != FR_OK) {
    tf_fs_unlock();
    return 1;
  }
  g_tf_write_result = f_write(&file, content, (UINT)content_len, &written);
  g_tf_write_close_result = f_close(&file);
  g_tf_write_len = written;
  tf_fs_unlock();
  return g_tf_write_result == FR_OK && g_tf_write_close_result == FR_OK &&
                 written == (UINT)content_len ? 0 : 1;
}

int stm32h750_tf_ensure_default_rule_file_v2(void) {
  static const char content[] =
    "version=2\n"
    "ruleCount=2\n"
    "rule0.relay=0\n"
    "rule0.threshold=42434\n"
    "rule0.action=on\n"
    "rule0.delayMs=1000\n"
    "rule0.timeoutMs=1500\n"
    "rule0.safeState=off\n"
    "rule0.priority=10\n"
    "rule1.relay=0\n"
    "rule1.threshold=42435\n"
    "rule1.action=off\n"
    "rule1.delayMs=0\n"
    "rule1.timeoutMs=1500\n"
    "rule1.safeState=off\n"
    "rule1.priority=20\n";
  FIL file;
  UINT written = 0u;
  char full_path[64];
  char config_path[64];
  const Stm32TfCardContext ctx = {
    .fs = NULL,
    .logical_drive = SDPath,
  };

  if (build_fatfs_path(&ctx, "/config/rules-v2.conf", full_path, sizeof(full_path)) != TF_CARD_OK ||
      build_fatfs_path(&ctx, "/config", config_path, sizeof(config_path)) != TF_CARD_OK ||
      tf_fs_lock() != 0) {
    return 1;
  }

  g_tf_write_attempts = 1u;
  g_tf_mkdir_result = f_mkdir(config_path);
  if (g_tf_mkdir_result != FR_OK && g_tf_mkdir_result != FR_EXIST) {
    tf_fs_unlock();
    return 1;
  }
  g_tf_write_open_result = f_open(&file, full_path, FA_CREATE_NEW | FA_WRITE);
  if (g_tf_write_open_result == FR_EXIST) {
    tf_fs_unlock();
    return 0;
  }
  if (g_tf_write_open_result != FR_OK) {
    tf_fs_unlock();
    return 1;
  }
  g_tf_write_result = f_write(&file, content, (UINT)(sizeof(content) - 1u), &written);
  g_tf_write_close_result = f_close(&file);
  g_tf_write_len = written;
  tf_fs_unlock();
  return g_tf_write_result == FR_OK && g_tf_write_close_result == FR_OK &&
                 written == (UINT)(sizeof(content) - 1u) ? 0 : 1;
}

void stm32h750_tf_card_bind(TfCardPort *port, Stm32TfCardContext *ctx, FATFS *fs, const char *logical_drive) {
  static const TfCardPortOps ops = {
    .card_present = fatfs_present,
    .mount = fatfs_mount,
    .mkdir = fatfs_mkdir,
    .write_file = fatfs_write,
    .read_file = fatfs_read,
  };
  if (ctx == NULL) {
    tf_card_port_bind(port, NULL, &ops);
    return;
  }
  ctx->fs = fs;
  ctx->logical_drive = logical_drive;
  tf_card_port_bind(port, ctx, &ops);
}

#endif
