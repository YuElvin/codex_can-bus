# Feature 与 ADR

## Feature 索引

| ID | 主题 | 状态 | 当前判断 |
| --- | --- | --- | --- |
| F-001 | W5500 SPI 网络 bring-up | [客观已验证] | 作为当前网络主路径，替代 LAN8720/RMII |
| F-002 | FDCAN2 外部 CAN 收发 | [客观已验证] | `PB5/PB6` + MCP2562FD + USBCAN-2E-U 是当前外部 CAN 主通道 |
| F-003 | TF 卡 FatFs 存储 | [客观已验证] | 当前 smoke test 通过；`/www/index.html` 默认静态页已可通过 W5500 HTTP 读取，HTTP 静态页路径已使用 FatFs mutex 下的分块读取 |
| F-004 | W25Q128 QSPI | [客观已验证] | 默认启动仅完成 JEDEC 检查；`0x00FFF000` 为显式诊断区，单规则配置 v2 在 `0x00FFE000`/`0x00FFD000` 双槽交替保存、读回与复位加载已烧录验证 |
| F-005 | FreeRTOS 单任务迁移 | [客观已验证] | 已烧录复核，调度器运行且 W5500/CAN/TF/W25Q128 状态保持通过 |
| F-006 | FreeRTOS 多任务拆分 | [部分客观已验证] | MonitorTask、CAN2、CanDecodeTask、LogTask、RuleTask、ConfigTask、DbcTask、TfTask 已并行/分阶段运行；TfTask 接管一次性 TF 初始化；外部 CAN RX 已用深度 8 队列交给 CanDecodeTask，CAN TX 已用深度 1 队列交给现有 CanDecodeTask 发送；W5500 状态轮询与 HTTP socket0 轮询由 mutex 串行化；DbcTask reload 和 ConfigTask 两类命令均已有固定深度队列，完整配置服务仍待实现 |
| F-007 | W5500 HTTP/API | [部分客观已验证] | `/api/status`、`/api/can/status`、`/api/signals`、`/`、`/index.html`、`POST /api/dbc/upload`、`POST /api/dbc/active` 和 `GET /api/dbc/runtime` 已烧录验证 |
| F-008 | DBC 解析和信号缓存 | [部分客观已验证] | 已烧录验证 runtime active DBC 双槽快照、DBC mutex、CAN2 轮询解码、`SignalCache` 更新和最多两项的 `/api/signals` 快照；本轮 TX self-test 无解码错误，外部 CANtest RX 未验证，self-test 缓存仍与外部消费缓存隔离 |
| F-009 | 日志和规则引擎 | [部分客观已验证] | 独立 LogTask 默认路径批量写已客观验证；TF 卡定义为仅支持下电后插拔，运行中 hotplug/recovery 不属于交付范围。格式化、重新挂载、TF bring-up 与默认日志持续写入均已实测；完整规则与通用配置仍待实现 |
| F-010 | 最小 RuleTask/继电器 | [客观已验证] | 已烧录 50 ms RuleTask、短临界区外部 RX 快照和 PE7/PE8 集中输出；固定延时/超时/手动优先级/高滞回均已实测，ST-Link pending 候选仅在 QSPI 保存读回成功后提交并自动 reload，失败保留旧运行态配置 |
| F-011 | TF RuleFile v1 单规则启动加载 | [客观已验证；非法板端输入未注入] | `/config/rule.conf` 固定 256 字节上限；有效 v1 已在板端覆盖非默认 QSPI 参数，缺失文件已创建且不覆盖；非法文件由主机纯解析测试覆盖，板端未注入；仅表达已有单规则四参数 |

## ADR 索引

| ADR | 决策 | 状态 |
| --- | --- | --- |
| ADR-001 | 停止 LAN8720/RMII/lwIP 主线，改用 W5500/SPI2 | 已接受 |
| ADR-002 | 外部 CAN 主通道采用 FDCAN2 `PB5/PB6` | 已接受 |
| ADR-003 | FreeRTOS 先手动最小接入，不立即 CubeMX 重生成 | 已接受 |
| ADR-004 | 单 `bringup` 任务硬件复核通过后，按低风险路径逐步拆任务 | 已接受 |
| ADR-005 | 项目治理采用 `01` 到 `05` 文档加 `CONVERSATION_SUMMARY.md` | 已接受 |
| ADR-007 | CSV 首步复用监控循环、SignalCache 快照和 FatFs mutex，不先创建 LogTask/队列 | 已接受 |
| ADR-008 | 用独立最小 LogTask 替换监控循环直接 CSV 写入，并在启动时一次性选择日志路径 | 已接受；运行中热插拔不支持 |
| ADR-021 | TF 卡插拔只能在开发板下电状态进行 | 已接受 |
| ADR-022 | 默认日志失败只增加 SD 操作与 append 阶段诊断 | 已接受；不改变写入策略 |
| ADR-009 | RuleTask 复用 portable rule_engine，仅从短临界区外部 RX SignalCache 快照集中驱动 PE7/PE8 | 已接受，最小 marker、固定延时/高滞回、手动优先级与单规则 reload 已上板验证 |
| ADR-010 | 最小 ConfigTask 仅串行执行既有 W25Q128 显式诊断请求 | 已接受，默认零擦写、显式请求擦写读回与任务运行均已上板验证 |
| ADR-011 | 单规则配置使用 QSPI 双槽做最小持久化 | 已接受，v1 兼容、交替保存、读回校验、损坏最新槽回退、两槽无效默认保留和复位加载均已上板验证 |
| ADR-018 | ConfigTask 使用固定深度命令队列 | 已接受，诊断和单规则保存入口已烧录验证入队/出队；完整配置模型和 HTTP 来源不在本 ADR |
| ADR-020 | TF RuleFile v1 采用固定文本格式并通过 RuleTask reload 生效 | 已接受；有效/缺失路径板端验证，非法板端输入未注入 |

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

`LogTask` 每 100 ms 运行、每 1 秒复制最多两项 `SignalCache`，使用 768 B 内存行缓冲；缓冲达到 512 B 或距上次 flush 5 秒时，复用 `fs_mutex` 下的单批追加。初始化仅探测一次默认路径：大小读取成功或 `FR_NO_FILE` 选 `/log/signal.csv`，其他返回选 `/log/signal-recovery.csv` 并递增切换计数；之后整次运行固定该路径。失败不加入重试、轮换、下载 API、HTTP 配置或通用队列。硬件操作边界为 TF 只能在开发板下电状态插拔，运行中热插拔/recovery 不支持；临时 probe/重挂载/状态旁路代码已从正式产品移除。

### ADR-021：TF 下电插拔边界

板载简易 TF 卡座没有可用的运行时插卡检测，真实热插回后的 SDMMC/FatFs append 已多次返回 `FR_DISK_ERR`，而插卡冷启动后的默认日志路径可持续写入。系统因此明确要求：拔出或插入 TF 卡前必须先关闭开发板电源；重新插卡后再上电，TfTask 负责正常 mount 与默认文件初始化。运行中拔插、自动重挂载、recovery 文件持续写入不属于产品功能或验收承诺。

### ADR-022：默认日志失败只增加来源诊断

阶段 F 的默认插卡长跑中，LogTask 首次 `FR_DISK_ERR` 前后只有共享的“最近 SD 状态”，不能把底层错误严谨归属到 append 的具体步骤。为缩小诊断证据缺口，只新增 `g_tf_sd_last_operation`（1=init、2=read、3=write）和 `g_tf_append_stage`（1=lock、2=open、3=lseek、4=write、5=close、6=ok）。它们只在既有 HAL SD 调用前和 `stm32h750_tf_append_file_locked()` 原调用顺序中赋值，失败保留阶段；不改变 timeout、重试、挂载、缓存、文件策略或返回值。烧录后首次失败实际为 stage=2/operation=2/open result=1，故下一轮只审计 SD read 路径。

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

### ADR-015：DbcTask reload 使用深度 1 命令队列

为形成最小任务通信闭环，现有 HTTP active DBC reload 请求改为投递到深度 1 的 FreeRTOS 队列，由 `DbcTask` 单消费者取出后调用既有加锁加载函数。队列满或未初始化时记录丢弃并沿用 HTTP 有限等待失败语义；不引入通用消息总线，也不改变文件、运行态快照或 API 字段。已烧录验证 `queue_ready=1/enqueue=1/drop=0`、`request=1/complete=1/result=0`。

### ADR-016：外部 CAN RX 使用固定深度队列交给 CanDecodeTask

为完成阶段 7 的最小 CAN 任务通信边界，CAN2 周期任务只负责从 FDCAN2 FIFO 读取外部帧并投递到深度 8 的 `CanFrame` 队列；独立 `CanDecodeTask` 每 10 ms 消费队列并执行既有 DBC mutex 保护下的解码和外部 `SignalCache` 更新。TX self-test 仍在 CAN2 周期任务内使用独立缓存，不能替代外部 RX 证据。队列满时只计数丢弃，不阻塞 CAN2 任务；已烧录验证入队/出队 `413/413`、丢弃 `0`、外部 RX decode `413`、decode errors `0`。本 ADR 不引入 TX 队列、IRQ 接收或通用消息总线。

### ADR-017：CAN2 周期发送使用深度 1 TX 队列

为完成阶段 7 的最小发送边界，CAN2 周期任务只生成并投递固定 `0x321` 周期帧到深度 1 的 `CanFrame` TX 队列；复用现有 `CanDecodeTask` 消费该队列并调用 `can_port_send`，实际发送成功后才执行独立 TX self-test 解码。这样不新增任务栈，也不改变 1 s 发送节奏、外部 RX 队列、HTTP/API 或 DBC 语义。队列满时只计数丢弃并返回发送失败。已烧录验证 `ready=1`、入队/出队无丢弃，CAN2 `errors=0/sendResult=0`；本 ADR 不引入通用配置队列或 IRQ 发送。

### ADR-018：ConfigTask 使用固定深度命令队列

为完成阶段 7 的最小配置任务通信边界，保留既有 ST-Link 请求标志作为兼容入口，在 ConfigTask 内转换为 `ConfigCommand`，投递到深度 2 队列，再由同一 ConfigTask 单消费者执行 QSPI 诊断或单规则保存。命令包含规则候选快照，避免消费时读取变化中的 pending 字段；队列满只计数丢弃并保留原请求。规则保存已烧录验证。历史 `0xffffffff/erase_count=0` 只代表未完成诊断的初始化哨兵值；2026-07-13 当前正式 ELF 的精确地址单次请求已验证 `enqueue/dequeue=1/1`、`result=0`、`erase_count=1`、`test_addr=0x00FFF000`，且规则双槽 `0x00FFE000/0x00FFD000` 未受影响。该 ADR 不定义多记录配置文件、HTTP API、CRUD 或通用消息总线。

### ADR-019：单规则 HTTP 配置复用 ConfigTask 保存边界

新增 `GET /api/rule/config` 和 `POST /api/rule/config`，POST 只接受已有单规则的 `onThreshold`、`offThreshold`、`delayMs`、`timeoutMs` 四个整数。HTTP 任务写入 pending 候选并置位兼容保存请求，实际 QSPI 双槽保存仍由 ConfigTask 深度 2 队列执行，成功后由 RuleTask reload；HTTP 等待保存和 reload 完成后才返回 200，非法关系返回 400，失败返回 500。本 ADR 不宣称规则文件、多规则、CRUD 或通用配置事务；本轮已烧录验证 HTTP 读写、QSPI 保存、RuleTask generation 和复位加载。

### ADR-020：TF RuleFile v1 固定为单规则文本文件

阶段 A 的 RuleFile 路径固定为 `/config/rule.conf`，文件容量上限为 256 字节。文件使用 ASCII 文本，每行一个 `key=value`，允许空行和 LF/CRLF 行尾；不允许注释、未知字段、重复字段、等号两侧空白或其他空白字符。必须且只能出现以下五个字段各一次：

```text
version=1
onThreshold=<uint32 十进制>
offThreshold=<uint32 十进制>
delayMs=<uint32 十进制>
timeoutMs=<uint32 十进制>
```

`version` 必须为 `1`；四个数值必须是无符号十进制 `uint32_t`，范围为 `0..4294967295`；并且必须满足 `onThreshold > offThreshold`、`delayMs <= timeoutMs`。文件超过 256 字节、为空、缺字段、重复字段、非法数字、溢出或关系校验失败均为无效。

启动优先级固定为“有效 RuleFile v1 > 有效 W25Q128 单规则双槽记录 > 编译默认安全配置”。TF 缺失或读取失败不阻断启动，也不改变已经加载的 QSPI/编译默认值。若文件返回 `FR_NO_FILE`，现有启动流程在 `fs_mutex` 下确保 `/config` 目录存在，并以当前有效单规则参数创建一次最小文件；创建使用 `FA_CREATE_NEW`，绝不覆盖已有文件。有效文件解析成功后只更新已有四参数候选并置位现有 `RuleTask` reload 边界，等待 generation 完成后才视为加载成功；无效文件不更新候选、不请求 reload，不增加规则数量，不改变 W25Q128 地址或写入策略。

### ADR-021：TF RuleFile v2 固定为两条规则与 priority winner

阶段 B 新增唯一 TF 文件 `/config/rules-v2.conf`，容量上限 512 字节；保留 `/config/rule.conf` v1、W25Q128 单规则双槽地址/格式/保存语义和既有 HTTP 单规则 API。v2 必须包含 `version=2`、`ruleCount=2`，并为 `rule0`、`rule1` 各提供且只提供 `relay`、`threshold`、`action`、`delayMs`、`timeoutMs`、`safeState`、`priority` 七个字段。输入固定为 `Can2Data.marker`，比较固定为 `marker >= threshold`；relay 为 0/1，action/safeState 为 on/off，priority 为 0..255 且两条不得相同，delay 不得大于 timeout。允许空行和 LF/CRLF，拒绝未知、重复、缺失、非十进制、溢出、非法状态和超限输入；解析失败保持候选不变。

有效 v2 在启动时构造完整两规则 `RuleEngine` 候选，由 RuleTask 在临界区一次性复制并递增 generation；同一继电器只选择最大 priority 的匹配/超时候选，延时未到保持该规则 default state，未匹配不改变默认态，输入超时使用 winner 的 safeState，手动覆盖高于所有规则。v2 缺失时目录存在后只用 `FA_CREATE_NEW` 写入固定默认文本，`v2_created=1/load_result=1` 仅表示创建，当前启动继续 v1/QSPI，下一次复位才尝试加载。无效、超限、读取失败均不改变既有安全配置且继续 v1/QSPI fallback；不写入 QSPI 多规则，不新增 HTTP/CRUD/DSL。

本 ADR 的阶段 B 实际状态为“已客观验证”：marker=42435 的外部 priority winner、停帧超时 safeState、手动优先和 GPIO 结果均已完成；首次 v2 缺失创建板端未观察，保留为未观察边界。

### ADR-022：RuleFile v3 受限双槽 HTTP CRUD 与 TF 原子提交

阶段 C 固定 `/config/rules-v3.conf` 为两槽、640 B 上限的 `version=3/ruleCount=2` 文本。每槽必须且只含 `enabled`、`relay`、`threshold`、`action`、`delayMs`、`timeoutMs`、`safeState`、`priority`；允许空行与 LF/CRLF，严格拒绝未知、重复、缺失、非十进制、溢出、非法 relay/state、priority 冲突及 `delayMs>timeoutMs`。禁用槽保留完整字段但不加入运行 engine；启动优先级为有效 v3、v2、v1、QSPI，v3 不自动创建。

HTTP 固定为 socket0 顺序服务：GET 列表/详情；POST 仅启用 disabled 槽；PUT 完整替换指定槽；DELETE 仅禁用 enabled 槽。POST/PUT 使用完整 URL-encoded 表单，非法为 400、无槽为 404、状态冲突为 409、保存/reload 失败为 500。首次写入只由当前有效 v2/v3 形成候选；HTTP 不直接修改运行 engine。ConfigTask 快照候选后使用 TF tmp/prev 原子替换，成功才提交 current/pending 并请求 RuleTask reload；HTTP 等待 save 和 generation 变化后才返回成功。实测 CRUD、复位持久化、默认恢复及非法 PUT 不变性均通过。

实现中发现 `rule_file_v3_build_engine()` 的 3856 B 自动 `RuleEngine` 在 1024-word ConfigTask 栈上造成真实 TCB 覆盖和 FreeRTOS HardFault；函数现直接构造调用方提供的 engine，最终 ELF 栈帧 120 B。该 ADR 不增加第三槽、前端、鉴权、并发请求、通用配置事务或 QSPI 多规则。
