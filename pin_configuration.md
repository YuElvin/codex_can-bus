# STM32H750VBTx 引脚分配

本文件记录当前 `can_bus_W5500` 工程的实际引脚方案。当前方案已经从
LAN8720/RMII 切换为 W5500/SPI2；CAN 外部收发器实际验证链路使用
`FDCAN2 PB5/PB6`；TF 卡、W25Q128、W5500 和 CAN2 外部收发均已完成硬件验证。

## 当前硬件功能总览

| 模块 | 接口 | 状态 | 说明 |
| --- | --- | --- | --- |
| W5500 | SPI2 + GPIO | 已验证 | 固定 IP `192.168.1.88`，MAC `02:00:00:12:34:56`，主机 ping 已通过 |
| CAN 收发器 | FDCAN2 | 已验证 | `PB6_TX/PB5_RX` 经 MCP2562FD 接 USBCAN-2E-U，Windows CANtest 收发已通过 |
| TF 卡 | SDMMC1 | 已验证 | 固件跳过 PA8 检卡，使用保守 1-bit/低速初始化后 smoke test 通过；PA8 在实物插卡/拔卡时均读高，不能作为检测依据 |
| W25Q128 | QUADSPI | 已验证 | JEDEC ID `EF4018`，最后 4KB 扇区擦写读回通过 |
| USART2 | UART | 可用但 macOS 曾出现乱码 | 固件参数为 `115200 8N1`，Windows 侧读取正常 |
| FreeRTOS | SysTick/SVC/PendSV | 已接入源码并编译验证 | 不占用额外外设引脚 |

## W5500，SPI 以太网模块

LAN8720 已移除，不再使用 ETH/RMII、MDIO、MDC 或 LwIP/ethernetif 路径。
W5500 使用 SPI2 和独立 GPIO 控制脚。

| MCU 引脚 | CubeMX 信号 | 外部连接 | 方向 | 备注 |
| --- | --- | --- | --- | --- |
| PB13 | SPI2_SCK | W5500 SCK | MCU 输出 | SPI Mode 0，当前 `.ioc` 分频为 `SPI_BAUDRATEPRESCALER_16` |
| PB14 | SPI2_MISO | W5500 MISO | MCU 输入 | 已用于 W5500 寄存器读回 |
| PB15 | SPI2_MOSI | W5500 MOSI | MCU 输出 | 已用于 W5500 寄存器写入 |
| PB12 | GPIO_Output | W5500 CS | MCU 输出 | 默认高电平，软件片选 |
| PB11 | GPIO_Output | W5500 RST | MCU 输出 | 默认高电平，驱动中执行硬复位 |
| PA7 | GPIO_EXTI7 | W5500 INT | MCU 输入 | 上拉，下降沿 EXTI；当前 bring-up 主要轮询寄存器状态 |

## CAN / CAN-FD

当前工程同时保留 FDCAN1 和 FDCAN2。实际外部 CAN 收发器验证使用
`FDCAN2 PB5/PB6`，这是当前接线优先方案。

### FDCAN2，当前外部 CAN 分析仪验证通道

| MCU 引脚 | CubeMX 信号 | 外部连接 | 方向 | 备注 |
| --- | --- | --- | --- | --- |
| PB5 | FDCAN2_RX | MCP2562FD RXD / MCU RX | MCU 输入 | 用户实际接线；Windows CANtest 发帧回读已验证 |
| PB6 | FDCAN2_TX | MCP2562FD TXD / MCU TX | MCU 输出 | 周期发送标准帧 `0x321` 已被 USBCAN-2E-U 接收 |

当前 FDCAN2 验证配置：

| 项目 | 当前值 |
| --- | --- |
| 帧格式 | Classic CAN |
| 标称速率 | 500 kbit/s |
| 周期发送帧 ID | 标准帧 `0x321` |
| 周期发送数据 | `C2 A5 xx xx 02 03 04 05`，序号递增 |
| 外部工具 | USBCAN-2E-U + Windows CANtest |

### FDCAN1，保留/辅助验证通道

| MCU 引脚 | CubeMX 信号 | 外部连接 | 方向 | 备注 |
| --- | --- | --- | --- | --- |
| PD0 | FDCAN1_RX | 预留 CAN1 RX | MCU 输入 | 代码中用于 FDCAN1 internal/external loopback 自检 |
| PD1 | FDCAN1_TX | 预留 CAN1 TX | MCU 输出 | 若要外接第二路 CAN 收发器，需要确认板级接线 |

当前 FDCAN1 代码验证配置：

| 项目 | 当前值 |
| --- | --- |
| 内部自检 | 标准 CAN `0x123` + CAN-FD/BRS 扩展帧 `0x18FF50E5` |
| 标称速率 | 500 kbit/s |
| 数据相位速率 | 2 Mbit/s |
| 说明 | internal loopback 不证明外部收发器；external loopback 仅在 PD0/PD1 物理链路存在时有效 |

## TF 卡，SDMMC1

CubeMX 仍保留 PA8 检卡输入，但当前固件覆盖 `BSP_SD_IsDetected()`，实际跳过
PA8 检卡；原理图虽然将其经 R5 接 `SWITCH`，但实物插卡/拔卡均读高，故检测脚当前不可用。

| MCU 引脚 | CubeMX 信号 | 外部连接 | 方向 | 备注 |
| --- | --- | --- | --- | --- |
| PC8 | SDMMC1_D0 | TF DAT0 | 双向 | 当前固件 bring-up 使用保守 1-bit 路径 |
| PC9 | SDMMC1_D1 | TF DAT1 | 双向 | CubeMX 配置为 4-bit，总线脚保留 |
| PC10 | SDMMC1_D2 | TF DAT2 | 双向 | CubeMX 配置为 4-bit，总线脚保留 |
| PC11 | SDMMC1_D3 | TF DAT3 | 双向 | CubeMX 配置为 4-bit，总线脚保留 |
| PC12 | SDMMC1_CK | TF CLK | MCU 输出 | 当前 `.ioc` `ClockDiv=2`，固件 bring-up 中改为更保守配置 |
| PD2 | SDMMC1_CMD | TF CMD | 双向 | 上拉 |
| PA8 | GPIO_Input | SD_DETECT | MCU 输入 | CubeMX 保留；当前固件跳过该检测。2026-07-13 插卡和用户确认拔卡后均实读为高电平，不能用于热插拔恢复 |

## W25Q128，板载 QSPI Flash

| MCU 引脚 | CubeMX 信号 | 外部连接 | 方向 | 备注 |
| --- | --- | --- | --- | --- |
| PB2 | QUADSPI_CLK | W25Q128 CLK | MCU 输出 | 当前 QSPI 时钟约 50 MHz |
| PB10 | QUADSPI_BK1_NCS | W25Q128 CS | MCU 输出 | 片选，上拉 |
| PD11 | QUADSPI_BK1_IO0 | W25Q128 IO0 / DI | 双向 | 单线命令/数据路径已验证 |
| PD12 | QUADSPI_BK1_IO1 | W25Q128 IO1 / DO | 双向 | JEDEC ID/读回已验证 |
| PE2 | QUADSPI_BK1_IO2 | W25Q128 IO2 / WP | 双向 | QSPI Bank1 IO2 |
| PD13 | QUADSPI_BK1_IO3 | W25Q128 IO3 / HOLD | 双向 | QSPI Bank1 IO3 |

当前 W25Q128 验证固件每次上电/复位会擦写最后一个 4KB 扇区
`0x00FFF000`，正式存储配置数据前需要改成按需触发，避免覆盖有效数据。

## 调试串口，USART2

| MCU 引脚 | CubeMX 信号 | 外部连接 | 参数 | 备注 |
| --- | --- | --- | --- | --- |
| PD5 | USART2_TX | USB-UART RX | 115200 8N1 | 输出 `[bringup]` 状态行 |
| PD6 | USART2_RX | USB-UART TX | 115200 8N1 | 当前 bring-up 主要未使用接收 |

## 继电器和调试 LED

| MCU 引脚 | CubeMX 信号 | 外部连接 | 默认状态 | 备注 |
| --- | --- | --- | --- | --- |
| PE7 | GPIO_Output | Relay1 | 低电平 | 高电平触发 |
| PE8 | GPIO_Output | Relay2 | 低电平 | 高电平触发 |
| PE10 | GPIO_Output | DBG_LED1 | 低电平 | 可用于心跳或阶段状态 |
| PE11 | GPIO_Output | DBG_LED2 | 低电平 | 预留调试 |

## SWD 调试

| MCU 引脚 | CubeMX 信号 | 外部连接 | 备注 |
| --- | --- | --- | --- |
| PA13 | DEBUG_JTMS-SWDIO | ST-Link SWDIO | 保留调试 |
| PA14 | DEBUG_JTCK-SWCLK | ST-Link SWCLK | 保留调试 |

## 已释放 / 不再使用的 LAN8720 引脚

以下 LAN8720/RMII 功能已经从当前方案移除，不应再按旧文档接线。
其中部分引脚已重新分配给 W5500。

| 旧引脚 | 旧功能 | 当前用途 |
| --- | --- | --- |
| PA1 | ETH_RMII_REF_CLK | 未使用 |
| PA2 | ETH_MDIO | 未使用 |
| PA7 | ETH_RMII_CRS_DV | W5500 INT |
| PC1 | ETH_MDC | 未使用 |
| PC4 | ETH_RMII_RXD0 | 未使用 |
| PC5 | ETH_RMII_RXD1 | 未使用 |
| PB11 | ETH_RMII_TX_EN | W5500 RST |
| PB12 | ETH_RMII_TXD0 | W5500 CS |
| PB13 | ETH_RMII_TXD1 | SPI2_SCK |

## CubeMX / 固件维护注意事项

- 当前 `.ioc` 工程路径：`cube_mx/can_bus_gateway.ioc`。
- 当前不启用 ETH/LwIP/LAN8742；网络功能由 W5500 SPI 寄存器和后续 WIZnet/socket 路径承担。
- FreeRTOS 已手动接入 CMake 固件；FreeRTOS 本身不改变引脚分配。
- 如果后续用 CubeMX 重新生成代码，必须保留 `USER CODE` 中的 W5500、CAN、TF、W25Q128 和 FreeRTOS 任务入口。
- 文档和 `.ioc` 不一致时，以 `.ioc`、当前源码和已烧录验证记录共同复核，不能按旧 LAN8720 引脚表操作。
