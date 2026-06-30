# CubeMX Project

`can_bus_gateway.ioc` is the STM32CubeMX project generated from `../pin_configuration.txt`.

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
  /Users/elvin/Desktop/project/can_bus/cube_mx/can_bus_gateway.ioc
```

Then in STM32CubeMX:

1. Check Pinout & Configuration for warnings.
2. Set Project Manager output location as needed.
3. Click `GENERATE CODE`.
4. Open the generated STM32CubeIDE project.

## Configured Peripherals

- ETH RMII for LAN8720, using PA1 as external 50 MHz `ETH_REF_CLK`.
- QuadSPI single-bank pinout for W25Q128.
- SDMMC1 4-bit pins plus PA8 `SD_DETECT`.
- FDCAN1 on PD0/PD1; FDCAN2 pins PB5/PB6 reserved.
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
- FDCAN2 is reserved with a separate message RAM offset and classic 8-byte elements.
- SDMMC kernel clock: PLL1Q 100 MHz, `ClockDiv=2` for about 25 MHz card clock.
- QUADSPI clock: D1HCLK 200 MHz, prescaler 3 for 50 MHz serial clock.
- ETH RMII reference clock comes from the LAN8720 module on PA1, not from MCU MCO.

## Notes

CubeMX 6.18 successfully loads this `.ioc` on this machine. The command-line `-q` option is a script mode in this version; it does not directly generate code from an `.ioc` file.

The checked configuration keeps SWD explicitly reserved on PA13/PA14. The current generated tree also carries a user-code ETH MSP implementation because CubeMX did not emit `HAL_ETH_MspInit()` in the generated MSP file; keep that block when regenerating.
