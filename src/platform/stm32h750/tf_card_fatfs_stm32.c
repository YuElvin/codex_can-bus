#if defined(CAN_BUS_USE_STM32_HAL)

#include "platform/stm32h750_bringup.h"

#include <string.h>

__attribute__((weak)) bool stm32h750_tf_card_detect(void) {
  return true;
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
  (void)ctx;
  const FRESULT result = f_mkdir(path);
  return result == FR_OK || result == FR_EXIST ? TF_CARD_OK : TF_CARD_ERROR;
}

static TfCardResult fatfs_write(void *ctx, const char *path, const uint8_t *data, size_t len) {
  (void)ctx;
  FIL file;
  UINT written = 0u;
  if (f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
    return TF_CARD_ERROR;
  }
  const FRESULT result = f_write(&file, data, (UINT)len, &written);
  const FRESULT close_result = f_close(&file);
  return result == FR_OK && close_result == FR_OK && written == len ? TF_CARD_OK : TF_CARD_ERROR;
}

static TfCardResult fatfs_read(void *ctx, const char *path, uint8_t *data, size_t len, size_t *read_len) {
  (void)ctx;
  FIL file;
  UINT read = 0u;
  if (f_open(&file, path, FA_READ) != FR_OK) {
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
  ctx->fs = fs;
  ctx->logical_drive = logical_drive;
  tf_card_port_bind(port, ctx, &ops);
}

#endif
