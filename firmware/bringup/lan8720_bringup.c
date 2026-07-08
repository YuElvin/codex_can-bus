#if defined(CAN_BUS_USE_STM32_HAL) || defined(STM32H750xx)

#include "platform/stm32h750_bringup.h"

extern struct netif gnetif;

int lan8720_bringup_run(void) {
  const uint32_t timeout_ms = 10000u;
  const uint32_t stable_ms = 1000u;
  uint32_t stable_since = 0u;
  bool stable_timer_running = false;

  Stm32Lan8720Context ctx;
  Lan8720Port lan;
  stm32h750_lan8720_bind(&lan, &ctx, &gnetif);

  const Lan8720Config config = {
    .mac = {0x02u, 0x00u, 0x00u, 0x12u, 0x34u, 0x56u},
    .ip = 0xc0a80158u,
    .netmask = 0xffffff00u,
    .gateway = 0xc0a80101u,
    .dhcp_enabled = false,
  };

  if (lan8720_port_init(&lan, &config) != LAN8720_OK) {
    return 1;
  }
  if (lan8720_port_start(&lan) != LAN8720_OK) {
    return 2;
  }

  const uint32_t started_at = HAL_GetTick();
  while (HAL_GetTick() - started_at < timeout_ms) {
    const Lan8720Result result = lan8720_port_poll(&lan);
    Lan8720Status status;
    (void)lan8720_port_get_status(&lan, &status);
    if (result == LAN8720_OK && status.link_up && status.ip != 0u) {
      if (!stable_timer_running) {
        stable_since = HAL_GetTick();
        stable_timer_running = true;
      }
      if (HAL_GetTick() - stable_since >= stable_ms) {
        return 0;
      }
    } else {
      stable_timer_running = false;
    }
    HAL_Delay(1u);
  }
  return 3;
}

#endif
