#if defined(CAN_BUS_USE_STM32_HAL) || defined(STM32H750xx)

#include "platform/stm32h750_bringup.h"

#include "bsp_driver_sd.h"

#include <string.h>

extern SD_HandleTypeDef hsd1;

__attribute__((weak)) bool stm32h750_tf_card_detect(void) {
  return true;
}

uint8_t BSP_SD_IsDetected(void) {
  return SD_PRESENT;
}

uint8_t BSP_SD_ReadBlocks_DMA(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks) {
  if (HAL_SD_ReadBlocks(&hsd1, (uint8_t *)pData, ReadAddr, NumOfBlocks, SD_DATATIMEOUT) != HAL_OK) {
    return MSD_ERROR;
  }
  BSP_SD_ReadCpltCallback();
  return MSD_OK;
}

uint8_t BSP_SD_WriteBlocks_DMA(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks) {
  if (HAL_SD_WriteBlocks(&hsd1, (uint8_t *)pData, WriteAddr, NumOfBlocks, SD_DATATIMEOUT) != HAL_OK) {
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
  return f_mount(tf->fs, tf->logical_drive, 1u) == FR_OK ? TF_CARD_OK : TF_CARD_ERROR;
}

static TfCardResult fatfs_mkdir(void *ctx, const char *path) {
  char full_path[64];
  if (build_fatfs_path((const Stm32TfCardContext *)ctx, path, full_path, sizeof(full_path)) != TF_CARD_OK) {
    return TF_CARD_ERROR;
  }
  const FRESULT result = f_mkdir(full_path);
  return result == FR_OK || result == FR_EXIST ? TF_CARD_OK : TF_CARD_ERROR;
}

static TfCardResult fatfs_write(void *ctx, const char *path, const uint8_t *data, size_t len) {
  FIL file;
  UINT written = 0u;
  char full_path[64];
  if (build_fatfs_path((const Stm32TfCardContext *)ctx, path, full_path, sizeof(full_path)) != TF_CARD_OK) {
    return TF_CARD_ERROR;
  }
  if (f_open(&file, full_path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
    return TF_CARD_ERROR;
  }
  const FRESULT result = f_write(&file, data, (UINT)len, &written);
  const FRESULT close_result = f_close(&file);
  return result == FR_OK && close_result == FR_OK && written == len ? TF_CARD_OK : TF_CARD_ERROR;
}

static TfCardResult fatfs_read(void *ctx, const char *path, uint8_t *data, size_t len, size_t *read_len) {
  FIL file;
  UINT read = 0u;
  char full_path[64];
  if (build_fatfs_path((const Stm32TfCardContext *)ctx, path, full_path, sizeof(full_path)) != TF_CARD_OK) {
    return TF_CARD_ERROR;
  }
  if (f_open(&file, full_path, FA_READ) != FR_OK) {
    return TF_CARD_ERROR;
  }
  const FRESULT result = f_read(&file, data, (UINT)len, &read);
  const FRESULT close_result = f_close(&file);
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
