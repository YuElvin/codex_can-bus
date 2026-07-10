# Feature 与 ADR

## Feature 索引

| ID | 主题 | 状态 | 当前判断 |
| --- | --- | --- | --- |
| F-001 | W5500 SPI 网络 bring-up | [客观已验证] | 作为当前网络主路径，替代 LAN8720/RMII |
| F-002 | FDCAN2 外部 CAN 收发 | [客观已验证] | `PB5/PB6` + MCP2562FD + USBCAN-2E-U 是当前外部 CAN 主通道 |
| F-003 | TF 卡 FatFs 存储 | [客观已验证] | 当前 smoke test 通过；`/www/index.html` 默认静态页已可通过 W5500 HTTP 读取，HTTP 静态页路径已使用 FatFs mutex 下的分块读取 |
| F-004 | W25Q128 QSPI | [客观已验证] | 当前验证通过；正式使用前处理测试扇区擦写问题 |
| F-005 | FreeRTOS 单任务迁移 | [客观已验证] | 已烧录复核，调度器运行且 W5500/CAN/TF/W25Q128 状态保持通过 |
| F-006 | FreeRTOS 多任务拆分 | [部分客观已验证] | CAN2 周期任务、W5500 轮询任务和状态打印任务已上板复核；完整队列/mutex 架构待实现 |
| F-007 | W5500 HTTP/API | [部分客观已验证] | `/api/status`、`/api/can/status`、`/api/signals`、`/`、`/index.html`、`POST /api/dbc/upload`、`POST /api/dbc/active` 和 `GET /api/dbc/runtime` 已烧录验证 |
| F-008 | DBC 解析和信号缓存 | [部分客观已验证] | 已烧录验证 runtime active DBC 双槽快照、CAN2 轮询解码、`SignalCache` 更新和最多两项的 `/api/signals` 快照；`0x321` TX self-test 和外部 CANtest RX 均无解码错误。日志、规则和配置生效仍待实现 |
| F-009 | 日志和规则引擎 | [部分客观已验证] | 最小 CSV 已在既有监控循环中从 SignalCache 落盘到 `/log/signal.csv`；专用日志任务、规则和配置仍待实现 |

## ADR 索引

| ADR | 决策 | 状态 |
| --- | --- | --- |
| ADR-001 | 停止 LAN8720/RMII/lwIP 主线，改用 W5500/SPI2 | 已接受 |
| ADR-002 | 外部 CAN 主通道采用 FDCAN2 `PB5/PB6` | 已接受 |
| ADR-003 | FreeRTOS 先手动最小接入，不立即 CubeMX 重生成 | 已接受 |
| ADR-004 | 单 `bringup` 任务硬件复核通过后，按低风险路径逐步拆任务 | 已接受 |
| ADR-005 | 项目治理采用 `01` 到 `05` 文档加 `CONVERSATION_SUMMARY.md` | 已接受 |
| ADR-007 | CSV 首步复用监控循环、SignalCache 快照和 FatFs mutex，不先创建 LogTask/队列 | 已接受 |

## 决策记录摘要

### ADR-001：网络路线切换到 W5500

旧 LAN8720 路径出现 MDIO/HAL/bit-bang 均读不到有效 PHY 的硬件阻断。W5500 已完成 SPI 寄存器读写、静态 IP 和 ping 验证，因此当前网络主线切换为 W5500。

### ADR-003：FreeRTOS 先手动接入

为了避免 CubeMX 重新生成覆盖已验证 bring-up 代码，当前先在 CMake 固件中手动接入 FreeRTOS Kernel，并通过编译和反汇编确认入口。待上板验证后，再决定是否补齐 `.ioc` 或重新生成。

### ADR-004：FreeRTOS 逐步拆任务

FreeRTOS 单任务版本已经上板验证通过。本轮先只把运行态周期逻辑拆成 CAN2 周期任务、W5500 轮询任务和状态打印任务，暂不把 TF/FatFs、QSPI、HTTP 或配置保存并发化，避免在基础任务调度验证前引入共享资源写入风险。

### ADR-006：最小解码先复用 CAN2 轮询，区分 TX self-test 与外部 RX

在完整 `CanRxTask`/`DbcDecodeTask`/queue 之前，先让 CAN2 周期轮询取得 active DBC 快照并调用 portable 解码器更新单个 `SignalCache`。成功发送的 `0x321` 周期帧进入同一函数仅用于板端 TX self-test；外部 FIFO 接收帧仍走同一函数但必须由 `g_can2_dbc_rx_frame_count` 单独证明。TX self-test 不得作为外部 RX 验证结论。

### ADR-007：最小 CSV 复用既有监控循环

CSV 首步只在 `bringup_default_task` 的约 1 秒监控循环中复制固定两项 `SignalCache` 快照，序列化为 `updated_ms,key,value,raw,unit,quality` 行并在 FatFs mutex 下追加 `/log/signal.csv`。这样可验证 TF 写入、缓存快照和 CAN 同时工作；本步不引入队列、LogTask、文件轮换、下载 API、配置或规则。
