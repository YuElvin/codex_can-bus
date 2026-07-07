# Feature 与 ADR

## Feature 索引

| ID | 主题 | 状态 | 当前判断 |
| --- | --- | --- | --- |
| F-001 | W5500 SPI 网络 bring-up | [客观已验证] | 作为当前网络主路径，替代 LAN8720/RMII |
| F-002 | FDCAN2 外部 CAN 收发 | [客观已验证] | `PB5/PB6` + MCP2562FD + USBCAN-2E-U 是当前外部 CAN 主通道 |
| F-003 | TF 卡 FatFs 存储 | [客观已验证] | 当前 smoke test 通过，后续承载 Web/DBC/日志/配置 |
| F-004 | W25Q128 QSPI | [客观已验证] | 当前验证通过；正式使用前处理测试扇区擦写问题 |
| F-005 | FreeRTOS 单任务迁移 | [客观已验证] | 已烧录复核，调度器运行且 W5500/CAN/TF/W25Q128 状态保持通过 |
| F-006 | FreeRTOS 多任务拆分 | [部分客观已验证] | CAN2 周期任务、W5500 轮询任务和状态打印任务已上板复核；完整队列/mutex 架构待实现 |
| F-007 | W5500 HTTP/API | [待实现] | 先做状态接口，再扩展静态文件和 DBC/日志接口 |
| F-008 | DBC 解析和信号缓存 | [待实现] | portable core 已有测试基础，后续接入固件任务 |
| F-009 | 日志和规则引擎 | [待实现] | 依赖 TF 稳定写入、信号缓存和任务拆分 |

## ADR 索引

| ADR | 决策 | 状态 |
| --- | --- | --- |
| ADR-001 | 停止 LAN8720/RMII/lwIP 主线，改用 W5500/SPI2 | 已接受 |
| ADR-002 | 外部 CAN 主通道采用 FDCAN2 `PB5/PB6` | 已接受 |
| ADR-003 | FreeRTOS 先手动最小接入，不立即 CubeMX 重生成 | 已接受 |
| ADR-004 | 单 `bringup` 任务硬件复核通过后，按低风险路径逐步拆任务 | 已接受 |
| ADR-005 | 项目治理采用 `01` 到 `05` 文档加 `CONVERSATION_SUMMARY.md` | 已接受 |

## 决策记录摘要

### ADR-001：网络路线切换到 W5500

旧 LAN8720 路径出现 MDIO/HAL/bit-bang 均读不到有效 PHY 的硬件阻断。W5500 已完成 SPI 寄存器读写、静态 IP 和 ping 验证，因此当前网络主线切换为 W5500。

### ADR-003：FreeRTOS 先手动接入

为了避免 CubeMX 重新生成覆盖已验证 bring-up 代码，当前先在 CMake 固件中手动接入 FreeRTOS Kernel，并通过编译和反汇编确认入口。待上板验证后，再决定是否补齐 `.ioc` 或重新生成。

### ADR-004：FreeRTOS 逐步拆任务

FreeRTOS 单任务版本已经上板验证通过。本轮先只把运行态周期逻辑拆成 CAN2 周期任务、W5500 轮询任务和状态打印任务，暂不把 TF/FatFs、QSPI、HTTP 或配置保存并发化，避免在基础任务调度验证前引入共享资源写入风险。
