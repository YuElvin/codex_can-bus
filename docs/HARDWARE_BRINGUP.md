# STM32H750 Hardware Bring-up

This repository now separates hardware validation into three independent modules: FDCAN1, TF card/FatFs, and LAN8720/lwIP. The host CMake build verifies the common port layer. Real hardware builds still require a CubeMX or STM32CubeIDE project that provides HAL startup code, clocks, linker script, peripheral handles, and middleware configuration.

## Common Integration

In the STM32 project, define:

```c
#define CAN_BUS_USE_STM32_HAL
```

Add the shared include path and these common sources:

```text
include/
src/ports/can_port.c
src/ports/tf_card_port.c
src/ports/lan8720_port.c
```

Then add only the adapter and bring-up file for the module under test. In `main.c`, call the selected function after `HAL_Init()`, clock setup, GPIO, and the relevant `MX_*_Init()` calls:

```c
int rc = can_bringup_run();      // or tf_card_bringup_run(), lan8720_bringup_run()
while (1) {
  (void)rc;                      // inspect with debugger, UART, or LED pattern
}
```

## FDCAN1 Validation

Enable FDCAN1 in CubeMX. For first validation, use internal loopback or connect a known-good CAN transceiver and external CAN analyzer. Add:

```text
src/platform/stm32h750/fdcan_port_stm32.c
firmware/bringup/can_bringup.c
```

Expected CubeMX symbol: `FDCAN_HandleTypeDef hfdcan1`.

Pass condition: `can_bringup_run()` returns `0` after sending frame `0x123` and receiving it back. Nonzero return codes identify configure/start/send/receive failures.

## TF Card Validation

Enable SDMMC1 and FatFs. Confirm DMA/cache settings are correct for STM32H7, and mount path matches the generated FatFs setup. Add:

```text
src/platform/stm32h750/tf_card_fatfs_stm32.c
firmware/bringup/tf_card_bringup.c
```

Expected CubeMX symbol: `FATFS SDFatFS`.

Pass condition: `tf_card_bringup_run()` returns `0`, creates `/www`, `/dbc`, `/log`, `/config`, `/sys`, writes `/sys/smoke.txt`, then reads it back. Override weak `stm32h750_tf_card_detect()` if the board has a real card-detect GPIO such as PA8.

## LAN8720 Validation

Enable ETH RMII and lwIP. Configure PHY address, RMII reference clock, MDC/MDIO pins, and LAN8720 reset timing according to the board. Add:

```text
src/platform/stm32h750/lan8720_lwip_stm32.c
firmware/bringup/lan8720_bringup.c
```

Expected CubeMX symbol: `struct netif gnetif`.

Pass condition: `lan8720_bringup_run()` returns `0` after link-up with static IP `192.168.1.88`. From a PC on the same subnet, verify:

```sh
ping 192.168.1.88
```

If link stays down, check PHY address, RMII 50 MHz clock, reset pin timing, magnetics, and cable.
