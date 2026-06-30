#if defined(CAN_BUS_USE_STM32_HAL)

#include "platform/stm32h750_bringup.h"

extern struct netif gnetif;

int lan8720_bringup_run(void) {
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

  for (uint32_t i = 0u; i < 5000u; ++i) {
    const Lan8720Result result = lan8720_port_poll(&lan);
    Lan8720Status status;
    (void)lan8720_port_get_status(&lan, &status);
    if (result == LAN8720_OK && status.link_up && status.ip != 0u) {
      return 0;
    }
    HAL_Delay(1u);
  }
  return 3;
}

#endif
