# Feature 与 ADR

## Feature 索引

| ID | 主题 | 状态 | 当前判断 |
| --- | --- | --- | --- |
| F-001 | W5500 SPI 网络 bring-up | [客观已验证] | 作为当前网络主路径，替代 LAN8720/RMII |
| F-002 | FDCAN2 外部 CAN 收发 | [客观已验证] | `PB5/PB6` + MCP2562FD + USBCAN-2E-U 是当前外部 CAN 主通道 |
| F-003 | TF 卡 FatFs 存储 | [客观已验证] | 当前 smoke test 通过；`/www/index.html` 默认静态页已可通过 W5500 HTTP 读取，HTTP 静态页路径已使用 FatFs mutex 下的分块读取 |
| F-004 | W25Q128 QSPI | [客观已验证] | 默认启动仅完成 JEDEC 检查；`0x00FFF000` 为显式诊断区，单规则配置 v2 在 `0x00FFE000`/`0x00FFD000` 双槽交替保存、读回与复位加载已烧录验证 |
| F-005 | FreeRTOS 单任务迁移 | [客观已验证] | 已烧录复核，调度器运行且 W5500/CAN/TF/W25Q128 状态保持通过 |
| F-006 | FreeRTOS 多任务拆分 | [部分客观已验证] | MonitorTask、CAN2、LogTask、RuleTask、ConfigTask、DbcTask、TfTask 已并行/分阶段运行；TfTask 接管一次性 TF 初始化；W5500 状态轮询与 HTTP socket0 轮询由 mutex 串行化；完整队列与通用配置保存待实现 |
| F-007 | W5500 HTTP/API | [部分客观已验证] | `/api/status`、`/api/can/status`、`/api/signals`、`/`、`/index.html`、`POST /api/dbc/upload`、`POST /api/dbc/active` 和 `GET /api/dbc/runtime` 已烧录验证 |
| F-008 | DBC 解析和信号缓存 | [部分客观已验证] | 已烧录验证 runtime active DBC 双槽快照、DBC mutex、CAN2 轮询解码、`SignalCache` 更新和最多两项的 `/api/signals` 快照；本轮 TX self-test 无解码错误，外部 CANtest RX 未验证，self-test 缓存仍与外部消费缓存隔离 |
| F-009 | 日志和规则引擎 | [部分客观已验证] | 独立 LogTask 默认路径批量写已客观验证；默认大小读取失败时选择 recovery 的源码/单测已完成但本次现场未触发；完整规则与通用配置仍待实现 |
| F-010 | 最小 RuleTask/继电器 | [客观已验证] | 已烧录 50 ms RuleTask、短临界区外部 RX 快照和 PE7/PE8 集中输出；固定延时/超时/手动优先级/高滞回均已实测，ST-Link pending 候选仅在 QSPI 保存读回成功后提交并自动 reload，失败保留旧运行态配置 |

## ADR 索引

| ADR | 决策 | 状态 |
| --- | --- | --- |
| ADR-001 | 停止 LAN8720/RMII/lwIP 主线，改用 W5500/SPI2 | 已接受 |
| ADR-002 | 外部 CAN 主通道采用 FDCAN2 `PB5/PB6` | 已接受 |
| ADR-003 | FreeRTOS 先手动最小接入，不立即 CubeMX 重生成 | 已接受 |
| ADR-004 | 单 `bringup` 任务硬件复核通过后，按低风险路径逐步拆任务 | 已接受 |
| ADR-005 | 项目治理采用 `01` 到 `05` 文档加 `CONVERSATION_SUMMARY.md` | 已接受 |
| ADR-007 | CSV 首步复用监控循环、SignalCache 快照和 FatFs mutex，不先创建 LogTask/队列 | 已接受 |
| ADR-008 | 用独立最小 LogTask 替换监控循环直接 CSV 写入，并在启动时一次性选择日志路径 | 已接受，recovery 现场验收待做 |
| ADR-009 | RuleTask 复用 portable rule_engine，仅从短临界区外部 RX SignalCache 快照集中驱动 PE7/PE8 | 已接受，最小 marker、固定延时/高滞回、手动优先级与单规则 reload 已上板验证 |
| ADR-010 | 最小 ConfigTask 仅串行执行既有 W25Q128 显式诊断请求 | 已接受，默认零擦写、显式请求擦写读回与任务运行均已上板验证 |
| ADR-011 | 单规则配置使用 QSPI 双槽做最小持久化 | 已接受，v1 兼容、交替保存、读回校验、损坏最新槽回退、两槽无效默认保留和复位加载均已上板验证 |

## 决策记录摘要

### ADR-001：网络路线切换到 W5500

旧 LAN8720 路径出现 MDIO/HAL/bit-bang 均读不到有效 PHY 的硬件阻断。W5500 已完成 SPI 寄存器读写、静态 IP 和 ping 验证，因此当前网络主线切换为 W5500。

### ADR-003：FreeRTOS 先手动接入

为了避免 CubeMX 重新生成覆盖已验证 bring-up 代码，当前先在 CMake 固件中手动接入 FreeRTOS Kernel，并通过编译和反汇编确认入口。待上板验证后，再决定是否补齐 `.ioc` 或重新生成。

### ADR-004：FreeRTOS 逐步拆任务

FreeRTOS 单任务版本已经上板验证通过。当前 CAN2 任务在不改变 1 s `0x321` 诊断发送节奏的前提下，每 50 ms 清空 FIFO 并更新外部 RX `SignalCache`；低优先级 MonitorTask 已接管原 bring-up 任务的 1 s 状态打印，W5500 轮询和最小 ConfigTask 保持既有边界。暂不把 TF/FatFs、QSPI、HTTP 或配置保存并发化，避免在基础任务调度验证前引入共享资源写入风险。

### ADR-006：最小解码先复用 CAN2 轮询，区分 TX self-test 与外部 RX

在完整 `CanRxTask`/`DbcDecodeTask`/queue 之前，先让 CAN2 周期轮询取得 active DBC 快照并调用 portable 解码器更新单个 `SignalCache`。成功发送的 `0x321` 周期帧进入同一函数仅用于板端 TX self-test；外部 FIFO 接收帧仍走同一函数但必须由 `g_can2_dbc_rx_frame_count` 单独证明。TX self-test 不得作为外部 RX 验证结论。

### ADR-007：最小 CSV 复用既有监控循环

CSV 首步只在 `bringup_default_task` 的约 1 秒监控循环中复制固定两项 `SignalCache` 快照，序列化为 `updated_ms,key,value,raw,unit,quality` 行并在 FatFs mutex 下追加 `/log/signal.csv`。这样可验证 TF 写入、缓存快照和 CAN 同时工作；本步不引入队列、LogTask、文件轮换、下载 API、配置或规则。

### ADR-008：最小 LogTask 行缓冲与失败丢弃

`LogTask` 每 100 ms 运行、每 1 秒复制最多两项 `SignalCache`，使用 768 B 内存行缓冲；缓冲达到 512 B 或距上次 flush 5 秒时，复用 `fs_mutex` 下的单批追加。初始化仅探测一次默认路径：大小读取成功或 `FR_NO_FILE` 选 `/log/signal.csv`，其他返回选 `/log/signal-recovery.csv` 并递增切换计数；之后整次运行固定该路径。失败不加入重试、轮换、下载 API、HTTP 配置或通用队列。临时 probe 已从最终产品移除；默认路径已上板验证，recovery 分支待真实错误触发。

### ADR-009：最小 RuleTask 复用已有引擎与安全快照

不重写 portable `rule_engine` 的延时、滞回、超时和手动优先级语义。CAN2 外部 RX 是供 HTTP、日志和规则消费的 `SignalCache` 唯一写者；TX self-test 复用解码器但写入独立缓存，不能刷新执行规则的输入。导出函数只在短 FreeRTOS 临界区内把外部缓存转换为 `SignalSnapshot`；50 ms RuleTask 不直接访问缓存，调用已有引擎后由唯一 `rule_apply_relays()` 写 PE7/PE8。当前固定高滞回为 `Can2Data.marker on=42434/off=42432`：Relay1 高、Relay2 固定低、1000 ms 连续匹配延时、1500 ms 无效/缺失输入安全低。另有默认关闭、仅供 ST-Link 诊断/验收写入的手动覆盖和单规则配置槽；reload 先将候选装入独立 engine，只有阈值/延时校验成功才替换当前 engine，失败保留旧有效规则。实机已验证 42434 置位、42433 保持、42432 释放，以及 reload 失配生效、恢复默认后延时高态、非法候选保留旧高态；不增加 HTTP、文件保存、多规则或持久化。

### ADR-010：最小 ConfigTask 串行化 QSPI 诊断

默认启动的 W25Q128 bring-up 只读 JEDEC ID。既有 `g_w25q128_diagnostic_request` 非零时，50 ms ConfigTask 清除请求、独占执行保留区 `0x00FFF000` 的擦写读回诊断并更新结果与次数；bringup 任务不再直接执行该写路径。本步不定义配置数据、备份地址、文件来源、HTTP、队列或持久化，后续正式配置保存必须另行确定地址和事务边界。

### ADR-011：单规则配置的最小 QSPI 持久化与安全生效

为直接推进配置备份目标，`0x00FFE000` 主槽和 `0x00FFD000` 备用槽只保存一条规则，禁止与 `0x00FFF000` 诊断扇区混用。v2 记录含 magic、version、四个整数规则参数、递增 sequence 与 XOR checksum；启动只读选择有效记录中 sequence 最新的一份，保存只擦写另一槽并读回比较。v1 主槽记录仍可加载，首次 v2 保存写入备用槽。目标板已通过仅在验证固件中临时擦除槽的方式确认：最新槽无效时加载较旧有效槽，两槽都无效时保留编译默认配置并报告 load result=2；测试入口已移除。ST-Link 只写 pending 候选参数，ConfigTask 收到请求后快照候选，只有保存函数成功时才提交运行态参数并置位既有 RuleTask reload；保存失败保留旧 engine 和旧运行态参数。它不增加 HTTP、TF 文件、CRUD、多规则、队列或通用恢复策略；后续扩展必须定义多记录模型和正式命令来源。
### ADR-012：运行态 DBC 加载与解码共用 mutex

在引入独立 `DbcTask`/队列前，active DBC 双槽加载和 CAN2 解码先共用 `w5500_http_dbc_lock()`。加载函数持锁完成 TF 读回、解析、非活动槽写入和 active 指针切换；解码函数持锁完成查表和 `SignalCache` 更新。该边界不改变 HTTP/API、DBC 格式或任务周期；本轮 active reload `generation/load=2/2`、TX self-test decode errors=0，外部 RX 未验证。

### ADR-013：DbcTask 只承载 active DBC reload 窄命令

为推进阶段 7 的任务边界，新增独立 `DbcTask`，仅消费由现有 `POST /api/dbc/active` 提交的一次性 reload 请求；任务调用既有 `w5500_http_load_active_dbc()`，继续使用 DBC mutex。HTTP 等待最多 100 ms 后沿用原有成功/失败语义；不引入通用消息总线、配置系统、多规则或 API 变化。已实机验证 `request=1/complete=1/result=0`、`runtimeGeneration=2`。

### ADR-014：TfTask 只承载一次性 TF 初始化

为推进阶段 7 的任务边界，新增一次性 `TfTask`，复用既有 `tf_card_bringup_run()`、默认页面确保函数和 `fs_mutex`。bring-up 创建任务后以 `vTaskDelay(1)` 等待完成信号，最多等待 5000 ms；成功或失败均通过 `g_tf_task_complete/g_tf_task_last_result` 明确交付，超时进入 `Error_Handler()`。本轮不引入队列、不改变 FatFs/recovery/API 语义，已烧录验证 `started=1/complete=1/result=0`。
