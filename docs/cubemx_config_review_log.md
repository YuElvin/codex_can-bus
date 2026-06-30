# CubeMX Configuration Review Log

## 2026-07-01 STM32H750VBTx bring-up review

This log records the CubeMX UI checks, configuration decisions, and follow-up code review after generating code for the CAN/CAN-FD gateway project.

### Target

- MCU: STM32H750VBTx, LQFP100.
- Clock: HSI 64 MHz to PLL1, SYSCLK 400 MHz, CPU 400 MHz, AXI/HCLK 200 MHz, APB clocks 100 MHz.
- ETH: RMII with LAN8720-compatible PHY wiring, static IPv4 `192.168.1.88/24`, gateway `192.168.1.1`.
- CAN: FDCAN1 as active CAN-FD channel, 500 kbit/s nominal, 2 Mbit/s data phase, 64-byte payloads.
- Storage: SDMMC1 4-bit TF card with FatFs, plus QSPI W25Q128 single-bank flash.
- Debug: SWD on PA13/PA14.

### UI Configuration Decisions

- RCC:
  - HSE disabled, LSE disabled.
  - Power source remains `PWR_LDO_SUPPLY`.
  - Voltage scale remains Scale 1.
  - Flash latency `2 WS` is correct for the selected 400 MHz CPU clock with AXI/HCLK at 200 MHz.

- Debug:
  - Set `Debug = Serial Wire`.
  - PA13 is `DEBUG_JTMS-SWDIO`.
  - PA14 is `DEBUG_JTCK-SWCLK`.
  - JTAG and trace remain disabled.

- PWR:
  - Wake-up pins, analog voltage detector, monitoring state, and PVD remain disabled.
  - Low-power wake-up features are not required for the current bring-up target.

- GPIO:
  - PA8 is SD card detect input with internal pull-up.
  - PE7/PE8 relay outputs remain push-pull, default low, no pull, low speed.
  - PE10/PE11 LED outputs remain push-pull, default low, no pull, low speed.

- SDMMC1:
  - Mode is `SD 4 bits Wide bus`.
  - PC8/PC9/PC10/PC11 data lines and PD2 CMD use pull-up.
  - PC12 CK uses no pull.
  - Speed is very high.
  - ClockDiv remains 2 with 100 MHz kernel clock, giving about 25 MHz SD clock.

- FatFs:
  - SD Card enabled on SDMMC1.
  - Long filename support enabled with static working buffer on BSS.
  - exFAT enabled.
  - SD DMA template enabled.
  - Generic SD BSP selected.

- ETH/LwIP:
  - ETH remains RMII.
  - RMII pins use no pull and very high speed.
  - LwIP uses static IP `192.168.1.88`, netmask `255.255.255.0`, gateway `192.168.1.1`.
  - DHCP disabled.
  - TCP, UDP, ICMP, DNS, netif link callback, and netif status callback enabled.
  - `MEM_SIZE` increased to 32768 bytes.
  - PHY driver selected as LAN8742, acceptable for LAN8720-compatible MDIO/basic link handling.
  - FreeRTOS remains disabled for first bring-up.

- FDCAN1:
  - Enabled in normal mode.
  - Frame format is CAN-FD with bit rate switching.
  - Auto retransmission enabled.
  - Nominal timing is 500 kbit/s.
  - Data timing is 2 Mbit/s.
  - RX FIFO0 has 16 elements, 64-byte data field.
  - TX FIFO queue has 8 elements, 64-byte data field.
  - PD0/PD1 use very high speed.

- QUADSPI:
  - Bank1 with Quad SPI lines.
  - Dual flash disabled.
  - Prescaler 3, flash size 23, CS high time 2 cycles, clock mode low.
  - GPIO speed changed to very high for CLK, NCS, and IO0-IO3.
  - PB10 NCS uses pull-up to keep flash deselected during reset/early init.

### Generated Code Review Against Previous Commit

- `cube_mx/Core/Src/main.c`:
  - CubeMX added FatFs include and `MX_FATFS_Init()`.
  - `MX_LWIP_Process()` in the main loop was preserved.
  - Flash latency changed from the previous manual `FLASH_LATENCY_4` to CubeMX-generated `FLASH_LATENCY_2`, which matches the current clock tree.

- `cube_mx/Core/Src/fdcan.c`:
  - Previous CAN-FD message RAM sizing was preserved.
  - FDCAN1 auto retransmission changed to enabled.
  - FDCAN1 GPIO speed changed to very high.

- `cube_mx/Core/Src/stm32h7xx_hal_msp.c`:
  - Previous manual ETH MSP functions would duplicate the new CubeMX-generated ETH MSP functions in `LWIP/Target/ethernetif.c`.
  - The duplicate manual ETH MSP functions were removed.

- `cube_mx/LWIP/Target/ethernetif.c`:
  - CubeMX generated LAN8742 PHY driver integration and ETH MSP functions.
  - Link state handling is now based on `LAN8742_GetLinkState()`.
  - ETH DMA descriptor and RX pool placement remain section-based for the linker script.

- `cube_mx/LWIP/Target/lwipopts.h`:
  - `MEM_SIZE` is now 32768 bytes.
  - Hardware checksum configuration remains enabled.

- `cube_mx/Core/Src/sdmmc.c`:
  - Data/CMD pull-ups and clock no-pull are reflected in generated GPIO init code.

- `cube_mx/FATFS/Target/sd_diskio.c`:
  - SD DMA template was generated.
  - Cache maintenance and scratch buffer support were enabled in USER CODE sections for STM32H7 DMA/cache safety.

- `cube_mx/Core/Src/quadspi.c`:
  - QSPI GPIO speed is now very high.
  - NCS has pull-up.

- `cube_mx/STM32H750VBTX_FLASH.ld`:
  - Minimum heap increased from `0x2000` to `0x4000`.
  - ETH descriptor/RX pool linker sections from the previous commit remain present.

### Remaining Bring-Up Notes

- Verify the actual PHY strap address on hardware. The generated LAN8742 driver scans MDIO addresses, but board documentation should still record the expected LAN8720 PHY address.
- SDMMC DMA now has cache maintenance enabled, but hardware validation should still test mount, read, write, and repeated writes with DCache enabled.
- FatFs exFAT support is enabled; use FAT32 or exFAT cards intentionally during tests.
- FreeRTOS is intentionally deferred until ETH, SDMMC/FatFs, and FDCAN are each proven in bare-metal bring-up.
