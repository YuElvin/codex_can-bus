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

Then add only the adapter and bring-up file for the module under test. The generated `cube_mx/Core/Src/main.c` already calls the TF card and LAN8720 validation functions after `HAL_Init()`, clock setup, GPIO, and the relevant `MX_*_Init()` calls:

```c
g_tf_card_bringup_status = tf_card_bringup_run();
g_lan8720_bringup_status = lan8720_bringup_run();
while (1) {
  MX_LWIP_Process();
}
```

Inspect `g_tf_card_bringup_status` and `g_lan8720_bringup_status` with a debugger. Both variables remain global and volatile so optimization will not hide the result.

## Command-line Firmware Build

Build the downloadable STM32H750 firmware with the project-local xPack toolchain:

```sh
. ./env.sh
cmake -S . -B build/stm32h750 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake \
  -DCAN_BUS_BUILD_TESTS=OFF \
  -DCAN_BUS_BUILD_STM32H750_FIRMWARE=ON
cmake --build build/stm32h750
```

Downloadable outputs:

```text
build/stm32h750/can_bus_gateway_stm32h750.elf
build/stm32h750/can_bus_gateway_stm32h750.hex
build/stm32h750/can_bus_gateway_stm32h750.bin
```

Use the `.hex` file with STM32CubeProgrammer, or flash the `.bin` at address `0x08000000`.

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

Expected CubeMX symbols: `FATFS SDFatFS` and `char SDPath[4]`.

Pass condition: `tf_card_bringup_run()` returns `0`, creates `/www`, `/dbc`, `/log`, `/config`, `/sys` on the CubeMX FatFs logical drive, writes `/sys/smoke.txt`, then reads it back. The STM32 adapter uses CubeMX `BSP_SD_IsDetected()`, which currently reads PA8 through `BSP_PlatformIsDetected()`.

## LAN8720 Validation

Enable ETH RMII and lwIP. Configure PHY address, RMII reference clock, MDC/MDIO pins, and LAN8720 reset timing according to the board. Add:

```text
src/platform/stm32h750/lan8720_lwip_stm32.c
firmware/bringup/lan8720_bringup.c
```

Expected CubeMX symbol: `struct netif gnetif`.

Pass condition: `lan8720_bringup_run()` returns `0` after link-up with static IP `192.168.1.88` and a continuous 1 second stable link window inside a 10 second timeout. The firmware must remain in the main loop calling `MX_LWIP_Process()` so ARP, ICMP, and link timeouts continue to run. From a PC on the same subnet, verify:

```sh
ping 192.168.1.88
ping -c 100 192.168.1.88
```

The expected stable result is 100 replies with 0% packet loss on a direct or same-switch connection. If link stays down or ping drops packets, check PHY address, RMII 50 MHz clock on PA1, reset pin timing, magnetics, cable, and whether the PC has an address in `192.168.1.0/24`.
