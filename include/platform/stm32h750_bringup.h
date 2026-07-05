#ifndef STM32H750_BRINGUP_H
#define STM32H750_BRINGUP_H

#if defined(CAN_BUS_USE_STM32_HAL) || defined(STM32H750xx)

#include "ports/can_port.h"
#include "ports/tf_card_port.h"

#include "ff.h"
#include "stm32h7xx_hal.h"

typedef struct {
  FDCAN_HandleTypeDef *hfdcan;
} Stm32FdcanContext;

typedef struct {
  FATFS *fs;
  const char *logical_drive;
} Stm32TfCardContext;

void stm32h750_fdcan_bind(CanPort *port, Stm32FdcanContext *ctx, FDCAN_HandleTypeDef *hfdcan);
void stm32h750_tf_card_bind(TfCardPort *port, Stm32TfCardContext *ctx, FATFS *fs, const char *logical_drive);

int can_bringup_run(void);
int tf_card_bringup_run(void);

#endif

#endif
