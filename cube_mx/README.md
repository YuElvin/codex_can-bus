# CubeMX Project

`can_bus_gateway.ioc` is the STM32CubeMX project tracked alongside
`../pin_configuration.md`.

## MCU Assumption

The pin file only says STM32H750, not the exact package. This project uses:

```text
STM32H750VBTx
LQFP100
```

If the board uses another STM32H750 package, open the `.ioc` in STM32CubeMX and migrate the MCU/package before generating code.

## Open and Generate

Open the project:

```sh
/Applications/STMicroelectronics/STM32CubeMX.app/Contents/MacOs/STM32CubeMX \
  /Users/elvin/Desktop/project/can_bus_W5500/cube_mx/can_bus_gateway.ioc
```

Then in STM32CubeMX:

1. Check Pinout & Configuration for warnings.
2. Set Project Manager output location as needed.
3. Click `GENERATE CODE`.
4. Open the generated STM32CubeIDE project.

## Configured Peripherals

- W5500 on SPI2: PB13 SCK, PB14 MISO, PB15 MOSI, PB12 CS, PB11 RST, PA7 INT.
- QuadSPI single-bank pinout for W25Q128.
- SDMMC1 4-bit pins plus PA8 `SD_DETECT`; firmware bring-up currently skips PA8 detection.
- FDCAN2 on PB5/PB6 is the verified external MCP2562FD/USBCAN-2E-U path.
- FDCAN1 on PD0/PD1 remains configured for internal/external loopback diagnostics.
- Relay GPIO outputs on PE7/PE8, default low in application code.
- Debug LEDs on PE10/PE11.
- USART2 debug UART on PD5/PD6, 115200 baud.
- SWD on PA13/PA14.

## Clock Tree

- Clock source: HSI 64 MHz into PLL1.
- PLL1: M=4, N=50, P=2, Q=8, R=8.
- SYSCLK: 400 MHz; HCLK/AXI/AHB: 200 MHz.
- APB1/APB2/APB3/APB4: 100 MHz.
- FDCAN kernel clock: PLL1Q 100 MHz.
- FDCAN1 timing: nominal 500 kbit/s, data phase 2 Mbit/s, 64-byte CAN-FD RX/TX elements.
- FDCAN2 timing: nominal 500 kbit/s classic CAN, separate message RAM offset and 8-byte elements.
- SDMMC kernel clock: PLL1Q 100 MHz, `ClockDiv=2` for about 25 MHz card clock.
- QUADSPI clock: D1HCLK 200 MHz, prescaler 3 for 50 MHz serial clock.
- SPI2 clock: APB1-derived, current `.ioc` calculates about 6.25 Mbit/s with prescaler 16.

## Notes

CubeMX 6.18 successfully loads this `.ioc` on this machine. The command-line `-q` option is a script mode in this version; it does not directly generate code from an `.ioc` file.

The checked configuration keeps SWD explicitly reserved on PA13/PA14. LAN8720,
ETH RMII, LwIP, and `ethernetif` are no longer part of the active W5500
firmware path.
