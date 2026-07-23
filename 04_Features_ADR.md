# Feature 与 ADR

## Feature 索引

| ID | 主题 | 状态 | 当前判断 |
| --- | --- | --- | --- |
| F-001 | W5500 SPI 网络 bring-up | [客观已验证] | 作为当前网络主路径，替代 LAN8720/RMII |
| F-002 | FDCAN2 外部 CAN 收发 | [客观已验证] | `PB5/PB6` + MCP2562FD + USBCAN-2E-U 是当前外部 CAN 主通道 |
| F-003 | TF 卡 FatFs 存储 | [客观已验证] | 当前 smoke test 通过；`/www/index.html` 默认静态页已可通过 W5500 HTTP 读取，HTTP 静态页路径已使用 FatFs mutex 下的分块读取 |
| F-004 | W25Q128 QSPI | [客观已验证] | 默认启动仅完成 JEDEC 检查；`0x00FFF000` 为显式诊断区，单规则配置 v2 在 `0x00FFE000`/`0x00FFD000` 双槽交替保存、读回与复位加载已烧录验证 |
| F-005 | FreeRTOS 单任务迁移 | [客观已验证] | 已烧录复核，调度器运行且 W5500/CAN/TF/W25Q128 状态保持通过 |
| F-006 | FreeRTOS 多任务拆分 | [一期边界客观已验证] | MonitorTask、CAN2、CanDecodeTask、LogTask、RuleTask、ConfigTask、DbcTask、TfTask及固定深度RX/TX/DBC/config队列均已运行验证；W5500与FatFs共享资源由mutex串行化。通用消息总线和通用配置服务为非目标 |
| F-007 | W5500 HTTP/API | [一期边界客观已验证] | 状态、CAN、signals、静态页、DBC、规则和manual受限API均已烧录/现场验证；F-76完整响应与双向FIN、G-1的200/400/404和G-2的500均通过。保持单socket非并发边界 |
| F-008 | DBC 解析和信号缓存 | [一期边界客观已验证] | runtime active DBC双槽快照、mutex、外部CANtest RX解码、独立TX self-test缓存和最多两项`/api/signals`均已验证；G-1外部marker/sequence及DBC RX增长且decode error=0 |
| F-009 | 日志和规则引擎 | [一期边界客观已验证] | LogTask长跑、实体CSV、冷启动恢复及G-1同映像增长通过；QSPI单规则与TF固定两槽v2/v3、优先级、manual、timeout/safeState、HTTP CRUD和GPIO均有现场证据。热插拔、无界规则和通用配置为非目标 |
| F-010 | 最小 RuleTask/继电器 | [客观已验证] | 已烧录 50 ms RuleTask、短临界区外部 RX 快照和 PE7/PE8 集中输出；固定延时/超时/手动优先级/高滞回均已实测，ST-Link pending 候选仅在 QSPI 保存读回成功后提交并自动 reload，失败保留旧运行态配置 |
| F-011 | TF RuleFile v1 单规则启动加载 | [客观已验证；非法板端输入未注入] | `/config/rule.conf` 固定 256 字节上限；有效 v1 已在板端覆盖非默认 QSPI 参数，缺失文件已创建且不覆盖；非法文件由主机纯解析测试覆盖，板端未注入；仅表达已有单规则四参数 |
| F-012 | 网页 CAN 发送控制 | [客观通过] | classic CAN窄合同：`GET/POST /api/can/tx`和`GET /api/can/tx/signals`，标准ID、DLC、8字节HEX、`100..10000 ms`；TX self-test/RX缓存独立。用户已部署网页上电，浏览器实测状态灯、TX/RX累计、默认折叠和CANoe式TX/RX DBC表；最终`0x321`/DLC4/`C2 A5 34 12 00 00 00 00`/1000ms已应用result=0，自动刷新和两次reload均无连接拒绝。CANtest外部输入由用户确认且RX表增长；未直接读取CANtest接收显示，不能声称外部接收器逐帧确认新TX帧 |
| F-013 | 网页手动 TX 与 DBC `signalKey` 两槽规则 | [最终现场验收完成] | 根目录新网页下自动刷新 TX/RX=`55/364→576/5540`，两次刷新期间编辑均保留并提交，最终 TX `sequence=256`，warn/error为空。候选 DBC获用户授权激活，runtime=`loaded=true/generation=1/bytes=151/messages=1/signals=2`；TX/RX为 marker=`42434`、sequence=`256/4660`。slot1 V4回读`Can2Data.sequence/4660/priority20/action off`，外部 RX sequence=`4660`时manual `relay1Output=0`与高优先级off一致；此前构建、反汇编、烧录已实际完成 |

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
| ADR-029 | 网页 CAN 发送控制保持经典 CAN 窄合同与 TX/RX 证据分离 | 已接受并完成本轮网页现场验收；外部接收器逐帧读回仍非本轮证据 |
| ADR-031 | 规则选择绑定活动 DBC `signalKey`，但保留两槽与旧 v3 `marker` 回退 | 已接受并完成最终现场验收 |

## 决策记录摘要

### ADR-001：网络路线切换到 W5500

旧 LAN8720 路径出现 MDIO/HAL/bit-bang 均读不到有效 PHY 的硬件阻断。W5500 已完成 SPI 寄存器读写、静态 IP 和 ping 验证，因此当前网络主线切换为 W5500。

### ADR-003：FreeRTOS 先手动接入

为了避免 CubeMX 重新生成覆盖已验证 bring-up 代码，当前先在 CMake 固件中手动接入 FreeRTOS Kernel，并通过编译和反汇编确认入口。待上板验证后，再决定是否补齐 `.ioc` 或重新生成。

### ADR-004：FreeRTOS 逐步拆任务

FreeRTOS 单任务版本已经上板验证通过。当前 CAN2 任务在不改变 1 s `0x321` 诊断发送节奏的前提下，每 50 ms 清空 FIFO 并更新外部 RX `SignalCache`；低优先级 MonitorTask 已接管原 bring-up 任务的 1 s 状态打印，W5500 轮询和最小 ConfigTask 保持既有边界。暂不把 TF/FatFs、QSPI、HTTP 或配置保存并发化，避免在基础任务调度验证前引入共享资源写入风险。

### ADR-031：两槽规则选择活动 DBC 信号

新阶段只修正已确认的两个边界：网页全局 FIFO 使手动 TX 等待多个历史请求，以及 v3 `marker` 规则无法按活动 DBC 选择信号。网络仍使用 W5500 单 socket、非并发模型；手动 TX 只等待当前一个在途请求。规则数量保持恰好两槽，每槽从活动 DBC 选择一个 `signalKey`，RuleTask 只导出两条已配置的信号；无活动 DBC 时规则写入必须在任何持久化或运行态更新前拒绝。旧 v3 不扩展迁移格式，仅在新配置加载失败时回退 `marker` 兼容语义。

前端 dirty-state 修复已在`www`。RuleFile V4 threshold 曾因 nano `printf`缺少浮点链接支持而格式化为`0`；CMake STM32链接选项现增加`-Wl,-u,_printf_float`。`./scripts/verify.sh` host tests=`17/17`通过，STM32 firmware构建成功，最终ELF `text/data/bss=106748/764/243688`；`nm`/map确认`_printf_float`、`_dtoa_r`、`_vfiprintf_r`，`objdump`确认`rule_file_format_decimal`调用`sniprintf`，2026-07-23候选HEX已由OpenOCD完成`Programming Finished/Verified OK/Resetting Target`。

最终现场中，正确部署根目录网页后自动刷新 TX/RX=`55/364→576/5540`且 warn/error为空。两次编辑 TX 在`1800 ms`、`1300 ms`刷新后仍保留并提交，最终 TX DBC `sequence=256`。候选 DBC经用户授权激活，runtime=`loaded=true/generation=1/bytes=151/messages=1/signals=2`；TX/RX表解析 marker=`42434`，sequence=`256/4660`。两槽目录均含 marker/sequence；slot1 V4回读`Can2Data.sequence`、threshold=`4660`、priority=`20`、action=`off`。外部 RX sequence=`4660`时manual `relay1Output=0`，符合高优先级off规则。该验收不把 TX self-test 冒充为外部接收器读回。

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

### ADR-023：首错先以板级冷启动快照定界，不据此直接改传输策略

F-5 的 MCU reset 后读数已处于连续失败，不能作为首错。F-6 因此固定要求 TF 保持插入并执行板级断电至少 10 秒；首个 failure 在约 32 秒时出现，request 为 `LBA=3826/blocks=1`，调用前 `ErrorCode/STA/DCOUNT=0/0/0`，调用后为 `0x20/0x29000/448`，stage/op=`2/2`。这只把事实定界为默认 append `f_open` 的 polling read RX FIFO overrun；在没有 IRQ、缓存维护或时序证据前，不允许直接改 DMA、timeout、重试、remount 或热插拔策略。后续先做只读实现审计。

### ADR-024：先隔离任务切换假设，不改 SD 传输参数

F-7 证实项目的 `BSP_SD_ReadBlocks_DMA` 实际调用 `HAL_SD_ReadBlocks(..., 1000ms)`；故障时 `MASK=0`，HAL polling 循环由当前任务读取 FIFO，未使用 SDMMC IRQ、DMA/IDMA 或 DMA cache maintenance。FDCAN2 亦无 NVIC RX IRQ。下一验证只允许在这一既有 HAL 调用外加临时 FreeRTOS 临界区，并以 F-5 既有 request/寄存器快照在同样断电冷启动下比较首错；这是判定任务切换假设的实验，不是长期架构方案。

### ADR-025：关中断临界区会破坏 HAL polling-read 的超时基准

F-8 已实际证明 `taskENTER_CRITICAL()` 不适用于包裹 `HAL_SD_ReadBlocks`：冷启动后任务计数连续不变，暂停读取 PC 位于 `HAL_SD_ReadBlocks`。该临界区掩蔽 tick，而 HAL 的超时依赖 tick，故实验本身造成停滞，不能用于支持或反驳任务切换假设；代码已撤回并重新烧录正式 F-5 路径。若继续验证，只能使用 `vTaskSuspendAll/xTaskResumeAll`，保留 SysTick 和 HAL timeout。

### ADR-026：保留 tick 的调度挂起也不能消除 SD 传输故障

F-9 只以 `vTaskSuspendAll/xTaskResumeAll` 包住原 read，保持所有中断、HAL tick、1000 ms timeout、F-5 快照和调用参数。外部 CANtest `0x321` 使 LogTask 实际连续 append；成功读调用越过 F-6 的 `call=102` 后，仍在 call=`120` 见到 read failure=`5`、LogTask failure=`6`，最新 read 为 `LBA=3826/blocks=1`、`HAL_TIMEOUT`。故任务切换不是足以解释故障的原因；实验代码已撤回，下一步先审计 SD clock/bus/flow-control/polling 配置。

### ADR-027：先单独验证 SDMMC 硬件流控

F-10 只读审计确认最终生效配置为 `ClockDiv=16`、1-bit、上升沿、PowerSave/HWFC 均关闭，实际数据 CK≈3.125MHz；F-6 `CLKCR=0x10` 和读 `DCTRL=0x90→0x92` 与源码/HAL polling 机制一致。低速1-bit已仍出现 RX FIFO overrun，故不回退 CubeMX 4-bit/25MHz。下一实验仅将 `HardwareFlowControl` 置 ENABLE，预期 CK 不变而 `CLKCR` 增加 bit17=`0x20000`；结果只能说明 FIFO 节流是否缓解当前路径，不能单独证明根因。

F-11 已按上述单字段执行：烧录后冷启动实测 `CLKCR=0x20010`，外部 `0x321` 持续输入下 read call `31→150`、LogTask write/flush `1/1→25/25` 且 failure=0，并完成 ping/API/CAN 回归。决定暂时保留 HWFC=`ENABLE` 进入固定 F-12 30 分钟耐久；该短时证据不等价于根因已证实或阶段 F 已完成。若 F-12 出现 failure，必须以新快照记录失败，不得自动叠加降速、重试或恢复策略。

F-12 已达到该耐久门槛：当前已烧录 HWFC 固件在30分17秒内 read failure/log failure 均为0，read、write、flush与文件大小持续增长，结束网络/CAN/外部 SignalCache 回归通过。因此 HWFC=`ENABLE` 保留为当前正式配置；决定依据仅为当前路径的实测稳定性，不把它提升为 RX overrun 的根因结论，也不扩大为热插拔、断网或 bus-off 恢复策略。

### ADR-028：socket0 成功响应采用异步优雅断开

F-16 的实板异常显示 `SENDOK`、`last_code=200` 与 socket监听状态不能保证主机收到响应。审计确认正常请求路径在同一轮 `DISCON` 后立即 `CLOSE`，可能中断 TCP 关闭并形成 RST。正式修复只为成功响应/CLOSE_WAIT 引入私有 pending 标志：首次仅发 `DISCON`，后续HTTP轮询读到 `CLOSED/INIT` 后才重新监听；请求处理错误和未知状态仍强制关闭。该决定保持单 socket、50ms轮询的最小HTTP模型，不承诺并发或零间隔短连接；现场连续验收以250ms关闭窗口为边界。

### ADR-029：W5500 动态长度寄存器采用稳定双读

Socket0 的 `Sn_RX_RSR` 与 `Sn_TX_FSR` 是异步16位动态寄存器。当前实现只对这两项最多四组连续双读、两值相等才使用；读取失败或持续不等沿用既有错误路径。不得把此 helper 扩展到RX/TX指针、端口或其它静态寄存器。F65 已在移除会改变栈帧和响应长度的临时Socket0快照诊断后，完成五个独立20次压力轮、压力后五API与ping回归；它只接受该短连接压力边界。G-1的空闲首请求曾超时，F-67的成功pcap又显示GET到HTTP header约2.20秒，因此当前不把间歇延迟的根因、空闲后可用性或并发HTTP能力表述为已证明。

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

### ADR-023：bus-off 仅在真实 BO 状态下限流 Stop/Start 恢复

F-25 在既有 `capture_can2_status()` 读取到真实 `bus_off` 时，先逐位 Abort `TXBRP` 中的挂起请求，再调用 `HAL_FDCAN_Stop()`；只有 Stop 成功时才 `HAL_FDCAN_Start()`。私有 latch/tick 使首次 BO 立即尝试、持续 BO 每1000ms最多一次，正常状态清 latch；公开 attempt/result 仅供诊断。不得调用 DeInit/Init，不改位率、过滤器、任务周期、队列或正常状态路径。错误250k形成 BO 后该路径已实测恢复，恢复500k后外部 RX、SignalCache 与 TEC=0 均复原。

### ADR-024：CSV 内容验收保持下电取卡的只读边界

最终 CSV 内容复查不增加 `/log/*`、下载 API、路径参数化或并发文件服务。先以外部 CAN 与 LogTask write/flush/size 增长证明运行态写入正常，再完全下电取卡并在主机只读检查当前日志文件的单表头、marker/sequence 行、字段数、`quality=ok` 和字节数。人工断电与板端读数不能原子同步，故文件大小以“不小于最近一次板端读数，新增部分以完整成对行结束”为准，不能强行要求绝对相等。该证据只覆盖下电关闭前内容，不定义热插拔、在线读取或 recovery 运行时恢复。F-26 实读 `971532 B/16975` 行、唯一表头、零字段错误和7024组目标同时间戳记录，已通过。

### ADR-025：一期Web控制台只复用既有单socket API

一期新增TF驻留的单页原生HTML/CSS/JS控制台，入口为`/www/index.html`。它呈现并串行调用已客观存在的`/api/status`、`/api/can/status`、`/api/dbc/runtime`、`/api/signals`、`/api/rules`、DBC upload/active和两槽规则CRUD；用户要求页面可见时CAN状态和signals每1000 ms顺序刷新，每个完整响应后留至少250 ms重监听窗口；任何在途请求、失败或隐藏页面都不得继续发起下一轮。

为满足继电器操作，F-75允许新增且仅新增`GET/POST /api/relay/manual`：POST要求完整`enabled/relay1/relay2`的`0|1`表单；固件只在短临界区更新现有RuleTask手动覆盖快照并递增请求序号，RuleTask应用快照后回写应用序号，HTTP最多等待100 ms确认应用并返回实际两路输出，超时返回500；RuleTask仍是唯一继电器GPIO写入者，`enabled=0`恢复规则执行。页面资产必须在仓库中可追溯，最终通过下电取TF卡更新、插回上电后由浏览器、pcap、API回读和GPIO读数验收；默认启动仅在文件缺失时创建的旧占位页不能被误称为新页面部署。不得引入框架、CDN、外部资源、鉴权、TLS、WebSocket、CAN发送、日志下载、系统设置或无界规则管理。F-75的HTTP稳定性前置条件是F-74异常诊断先获得有效唯一会话分类；前端页面不能用于掩盖该未关闭的故障。

F-75过程记录（已被后文最终状态替代）：当时仅达到源码与本地构建边界，尚无烧录或现场证据。

F-75过程记录（已被后文最终状态替代）：严格固件已完成host CTest=15/15、关键反汇编、OpenOCD `Verified OK`和顺序API验证；当时TF页面联合验收尚未完成。

F-75最终按用户明确的一期三项核心完成客观验收：板端服务页面为`11143 B`且SHA-256=`2ed23b7fe6d1047b897d62bb8b6aa6376e4c1e6d90c5c7d4ff11918fc99117da`；独立pcap为1次首页、14次CAN状态、14次signals全部200，首页最终ACK至首API SYN=`303.503 ms`、RST=0；浏览器规则slot1 threshold完成`42435→42436→42435`保存/回读；浏览器手动覆盖`relay1=0/relay2=1`得到RuleTask序号`1/1`和GPIOE ODR=`0x100`，关闭覆盖后`2/2`且ODR=`0x80`恢复自动规则。第一次手动页面提交仍曾出现服务端记录200而浏览器`Failed to fetch`、主机网络暂时不可达，复位后第二次才完整通过，故本ADR只确认三项功能，不确认HTTP长期稳定。DBC区域保留并复用既有API；F-75现场未重新通过浏览器执行upload/active，因此只是不把该次操作列为F-75三项证据，不影响G-1已验收的DBC上传/激活/API与页面能力。

### ADR-026：HTTP响应完成以TCP ACK和优雅关闭为准

F-76确认W5500 `SENDOK`只表示芯片接受SEND命令，不能证明对端已确认TCP响应。所有成功响应发送后使用稳定`Sn_TX_FSR`非阻塞门控：FSR回到2048才开始既有graceful `DISCON`；未释放时每个HttpTask周期只读一次，500 ms超时才进入既有DISCON和完整W5500恢复。若等待期间socket先进入`CLOSE_WAIT`，只清ACK内部状态并调用既有graceful `DISCON`，禁止通过`http_open_listener()`硬CLOSE旧连接。

最终固件host CTest=`15/15`，FLASH/RAM_D1=`92628/242792 B`，ELF/HEX SHA-256=`26b63222463e9c0cd31d55e5dc40b0ad1c86e2d2ca6d10a8f9b3d0f276b34bc5`/`d1ef3838757beb8478aea6413eb3b12c5f9fdf41f5196b96bfd2d7e8ddb7968c`，已反汇编并烧录验证。最终pcap SHA-256=`77a1ed949fb374641968b5d38e9a744e75057bfc7c9c7fa647520d01e418ad6e`：63包/5连接，2条manual POST与3条顺序GET全部HTTP200且Content-Length匹配，全部请求/响应被ACK，5/5双方FIN最终确认，HTTP数据重传=0、RST=0。该决定不提供并发HTTP能力，也不扩大Web/API或业务语义。

### ADR-027：阶段G复用高成本故障证据并执行同映像联合烟雾

F-76只改变HTTP发送完成和关闭状态机，未触及CAN/DBC、SD/FatFs/LogTask、QSPI、RuleTask业务语义或W5500 PHY轮询。因此G阶段复用F-12长跑、F-14拔线、F-19冷启动、F-25真实bus-off、F-26实体CSV等高成本现场证据，不重复故障注入；但必须用同一最终ELF/HEX做正常联合烟雾覆盖各域共同运行。

G-1复位后统一窗口完成19个严格串行HTTP连接：200/400/404状态与Content-Length全部正确；DBC 151 B上传/激活、signals、Web页面哈希、v3规则`42435→42436→42435`可逆保存及manual安全态均通过。Snapshot A→B中LogTask write/flush=`20/20→56/56`、文件=`15326272→15346678 B`，CAN TX/RX=`103/1017→284/2811`；所有日志、TF read、CAN、DBC decode和队列错误为0。HTTP request=`0→19`，socket最终LISTEN，HTTP error、ACK timeout、W5500 recovery为0。G-1通过；HTTP 500仍必须另用安全、可回退、非破坏协议补证。

### ADR-028：HTTP 500只用规则来源前置RAM故障标志补证

当前所有自然500都依赖队列超时、TF保存/读取、运行态reload或manual交接，直接制造会扩大为持久化或任务故障。G-2只使用`http_handle_rules_write()`最前置的`rules_source_unavailable`：该判断位于candidate复制、body解析、pending/save_request和任何TF/QSPI操作之前。现场实际v3/v2加载结果为`0/0xffffffff`，仅临时把v3 RAM诊断值改为1；请求体故意使用已知非法`enabled=true`，所以注入失效时只会400而不会保存。

实测500为98 B完整JSON，随后立即把v3恢复为0；同一请求恢复400，规则正文前后哈希一致，save request/result=`0/0`、generation=4，HTTP error/ACK timeout/recovery为0。该方法只证明现有500响应语义和恢复后的顺序服务，不把RAM注入冒充TF/QSPI真实失败，也不新增生产故障接口。

### ADR-029：网页 CAN 发送控制保持经典 CAN 窄合同与 TX/RX 证据分离

用户已确认此候选只覆盖 classic CAN。新增路由限定为`GET/POST /api/can/tx`和`GET /api/can/tx/signals`，请求只表达标准 ID、DLC、最多8字节HEX与`100..10000 ms`周期；不引入 CAN-FD、扩展 ID、可变长数据、通用周期管理或其他 CAN API。网页候选仅显示状态灯、TX/RX区、折叠区和 TX/RX DBC 表，仍受单 socket 串行请求约束。

候选构建与烧录证据为 CTest=`16/16`、ELF/HEX SHA-256 前缀=`f589...`/`37b9...`、`text/data/bss=93976/384/242448`、50 ms poll/队列/解析反汇编以及 OpenOCD `Programming Finished/Verified OK/Resetting Target`。修复后的网页已由用户写入TF并上电：浏览器绿灯为`status-lamp ok`、四个details默认折叠；可逆关闭再恢复后`0x321`/DLC4/`C2 A5 34 12 00 00 00 00`/1000ms的request/applied相等、result=0。TX self-test和用户确认CANtest发送后的外部RX DBC表均为`42434/4660/ok`，自动刷新`120/273→134/417`，两次reload为`145/527`、`162/694`，无连接拒绝或控制台warn/error。TX self-test与外部RX仍是不同证据，且未直接读取CANtest接收显示，不能将本轮记录为外部接收器逐帧确认新控制帧。

### ADR-030：网页自动刷新保留末请求收尾窗口

首轮页面曾在重入后端口80连续5次失败；GDB当时为socket0=`0x17(ESTABLISHED)`、HTTP status/error=`0/0`、requestCount=`403`、RX_RSR=`0`、HTTP任务tick增长。该历史失败不单独证明根因。

最小修复恢复`www/index.html`第4个串行请求后的250ms收尾等待。用户已部署该网页并上电，严格串行自动刷新和两次reload重入均无连接拒绝；样本TX/RX=`120/273→134/417`、`145/527`、`162/694`。因此此修复在本验证窗口内通过，原先ESTABLISHED无RX不再是当前阻断，也不被扩写为既定状态机根因。
