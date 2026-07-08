#ifndef STM32H750_BRINGUP_H
#define STM32H750_BRINGUP_H

#if defined(CAN_BUS_USE_STM32_HAL) || defined(STM32H750xx)

#include "ports/can_port.h"
#include "ports/tf_card_port.h"
#include "ports/w5500_port.h"

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
int tf_card_bringup_run(void);
int w25q128_bringup_run(void);
int w5500_bringup_run(void);
int w5500_bringup_poll(void);
int w5500_http_status_poll(void);

#endif

#endif
