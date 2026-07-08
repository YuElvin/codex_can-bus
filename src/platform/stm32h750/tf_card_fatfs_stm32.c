#if defined(CAN_BUS_USE_STM32_HAL) || defined(STM32H750xx)

#include "platform/stm32h750_bringup.h"

#include "bsp_driver_sd.h"

#include <string.h>

extern SD_HandleTypeDef hsd1;

#define TF_CARD_SD_CLOCK_DIV 16u
#define TF_CARD_SD_OP_TIMEOUT_MS 1000u

volatile uint32_t g_tf_mount_result;
volatile uint32_t g_tf_mkdir_result;
volatile uint32_t g_tf_write_attempts;
volatile uint32_t g_tf_write_open_result;
volatile uint32_t g_tf_write_result;
volatile uint32_t g_tf_write_close_result;
volatile uint32_t g_tf_write_len;
volatile uint32_t g_tf_read_open_result;
volatile uint32_t g_tf_read_result;
volatile uint32_t g_tf_read_close_result;
volatile uint32_t g_tf_read_len;
volatile uint32_t g_tf_sd_init_count;
volatile uint32_t g_tf_sd_last_hal_status;
volatile uint32_t g_tf_sd_last_error;
volatile uint32_t g_tf_sd_last_sta;
volatile uint32_t g_tf_sd_last_dcount;
volatile uint32_t g_tf_sd_last_clkcr;

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
  hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
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

  status = HAL_SD_Init(&hsd1);
  tf_sd_record_diag(status);
  return status == HAL_OK ? MSD_OK : MSD_ERROR;
}

uint8_t BSP_SD_ReadBlocks_DMA(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks) {
  HAL_StatusTypeDef status = HAL_SD_ReadBlocks(&hsd1,
                                               (uint8_t *)pData,
                                               ReadAddr,
                                               NumOfBlocks,
                                               TF_CARD_SD_OP_TIMEOUT_MS);
  tf_sd_record_diag(status);
  if (status != HAL_OK) {
    return MSD_ERROR;
  }
  BSP_SD_ReadCpltCallback();
  return MSD_OK;
}

uint8_t BSP_SD_WriteBlocks_DMA(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks) {
  HAL_StatusTypeDef status = HAL_SD_WriteBlocks(&hsd1,
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
  const FRESULT result = f_mount(tf->fs, tf->logical_drive, 1u);
  g_tf_mount_result = result;
  return result == FR_OK ? TF_CARD_OK : TF_CARD_ERROR;
}

static TfCardResult fatfs_mkdir(void *ctx, const char *path) {
  char full_path[64];
  if (build_fatfs_path((const Stm32TfCardContext *)ctx, path, full_path, sizeof(full_path)) != TF_CARD_OK) {
    return TF_CARD_ERROR;
  }
  const FRESULT result = f_mkdir(full_path);
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
    g_tf_write_open_result = f_open(&file, full_path, FA_CREATE_ALWAYS | FA_WRITE);
    if (g_tf_write_open_result != FR_OK) {
      HAL_Delay(20u);
      continue;
    }
    g_tf_write_result = f_write(&file, data, (UINT)len, &written);
    g_tf_write_close_result = f_close(&file);
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
  g_tf_read_open_result = f_open(&file, full_path, FA_READ);
  if (g_tf_read_open_result != FR_OK) {
    return TF_CARD_ERROR;
  }
  const FRESULT result = f_read(&file, data, (UINT)len, &read);
  const FRESULT close_result = f_close(&file);
  g_tf_read_result = result;
  g_tf_read_close_result = close_result;
  g_tf_read_len = read;
  if (read_len != NULL) {
    *read_len = read;
  }
  return result == FR_OK && close_result == FR_OK ? TF_CARD_OK : TF_CARD_ERROR;
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
