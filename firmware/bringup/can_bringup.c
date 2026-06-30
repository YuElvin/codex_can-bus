#if defined(CAN_BUS_USE_STM32_HAL)

#include "platform/stm32h750_bringup.h"

extern FDCAN_HandleTypeDef hfdcan1;

int can_bringup_run(void) {
  Stm32FdcanContext ctx;
  CanPort can;
  stm32h750_fdcan_bind(&can, &ctx, &hfdcan1);

  const CanPortConfig config = {
    .nominal_bitrate = 500000u,
    .data_bitrate = 2000000u,
    .fd_enabled = true,
    .brs_enabled = true,
    .internal_loopback = true,
  };
  if (can_port_configure(&can, &config) != CAN_PORT_OK) {
    return 1;
  }
  if (can_port_start(&can) != CAN_PORT_OK) {
    return 2;
  }

  const CanFrame tx = {
    .id = 0x123u,
    .ide = CAN_ID_STANDARD,
    .fd = false,
    .brs = false,
    .dlc = 8u,
    .data = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u},
  };
  if (can_port_send(&can, &tx) != CAN_PORT_OK) {
    return 3;
  }

  for (uint32_t i = 0u; i < 1000000u; ++i) {
    CanFrame rx;
    if (can_port_receive(&can, &rx) == CAN_PORT_OK) {
      return rx.id == tx.id && rx.dlc == tx.dlc && rx.data[0] == tx.data[0] ? 0 : 4;
    }
  }
  return 5;
}

#endif
