#if defined(CAN_BUS_USE_STM32_HAL) || defined(STM32H750xx)

#include "platform/stm32h750_bringup.h"

static void stm32_w5500_select(void *ctx) {
  Stm32W5500Context *w5500 = (Stm32W5500Context *)ctx;
  HAL_GPIO_WritePin(w5500->cs_port, w5500->cs_pin, GPIO_PIN_RESET);
}

static void stm32_w5500_deselect(void *ctx) {
  Stm32W5500Context *w5500 = (Stm32W5500Context *)ctx;
  HAL_GPIO_WritePin(w5500->cs_port, w5500->cs_pin, GPIO_PIN_SET);
}

static void stm32_w5500_reset_write(void *ctx, bool level_high) {
  Stm32W5500Context *w5500 = (Stm32W5500Context *)ctx;
  HAL_GPIO_WritePin(w5500->reset_port, w5500->reset_pin, level_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void stm32_w5500_delay_ms(void *ctx, uint32_t ms) {
  (void)ctx;
  HAL_Delay(ms);
}

static W5500Result stm32_w5500_transfer(void *ctx, uint8_t tx, uint8_t *rx) {
  Stm32W5500Context *w5500 = (Stm32W5500Context *)ctx;
  uint8_t received = 0u;
  if (HAL_SPI_TransmitReceive(w5500->hspi, &tx, &received, 1u, w5500->spi_timeout_ms) != HAL_OK) {
    return W5500_ERROR;
  }
  if (rx != NULL) {
    *rx = received;
  }
  return W5500_OK;
}

void stm32h750_w5500_bind(W5500Port *port,
                          Stm32W5500Context *ctx,
                          SPI_HandleTypeDef *hspi,
                          GPIO_TypeDef *cs_port,
                          uint16_t cs_pin,
                          GPIO_TypeDef *reset_port,
                          uint16_t reset_pin) {
  static const W5500PortOps ops = {
    .select = stm32_w5500_select,
    .deselect = stm32_w5500_deselect,
    .reset_write = stm32_w5500_reset_write,
    .delay_ms = stm32_w5500_delay_ms,
    .transfer = stm32_w5500_transfer,
  };

  ctx->hspi = hspi;
  ctx->cs_port = cs_port;
  ctx->cs_pin = cs_pin;
  ctx->reset_port = reset_port;
  ctx->reset_pin = reset_pin;
  ctx->spi_timeout_ms = 100u;
  w5500_port_bind(port, ctx, &ops);
}

#endif
