# Feature 与 ADR

## Feature 索引

| ID | 主题 | 状态 | 当前判断 |
| --- | --- | --- | --- |
| F-001 | W5500 SPI 网络 bring-up | [客观已验证] | 作为当前网络主路径，替代 LAN8720/RMII |
| F-002 | FDCAN2 外部 CAN 收发 | [客观已验证] | `PB5/PB6` + MCP2562FD + USBCAN-2E-U 是当前外部 CAN 主通道 |
| F-003 | TF 卡 FatFs 存储 | [P0物理断电与恢复通过] | 首次污染介质复核失败保留为历史；底层`CTRL_SYNC`和scratch写ready等待修复后，干净FAT32介质已完成持续写入物理断电、只读CSV完整性、`fsck_msdos -n=0`及重新上电续写验证 |
| F-004 | W25Q128 QSPI | [客观已验证] | 默认启动仅完成 JEDEC 检查；`0x00FFF000` 为显式诊断区，单规则配置 v2 在 `0x00FFE000`/`0x00FFD000` 双槽交替保存、读回与复位加载已烧录验证 |
| F-005 | FreeRTOS 单任务迁移 | [客观已验证] | 已烧录复核，调度器运行且 W5500/CAN/TF/W25Q128 状态保持通过 |
| F-006 | FreeRTOS 多任务拆分 | [一期边界客观已验证] | MonitorTask、CAN2、CanDecodeTask、LogTask、RuleTask、ConfigTask、DbcTask、TfTask及固定深度RX/TX/DBC/config队列均已运行验证；W5500与FatFs共享资源由mutex串行化。通用消息总线和通用配置服务为非目标 |
| F-007 | W5500 HTTP/API | [确认缺陷已修复并实板通过] | 已确定根因是普通`ESTABLISHED + RX_RSR=0`无超时；100 ms空连接计时复用既有graceful `DISCON`，不改ACK/recovery/API。最终CTest=`20/20`、反汇编/烧录通过，10轮空连接回收及后续HTTP、浏览器5次新页面/4次manual提交、最终9 API和ping均通过；单socket非并发边界保留 |
| F-008 | DBC 解析和信号缓存 | [一期边界客观已验证] | runtime active DBC双槽快照、mutex、外部CANtest RX解码、独立TX self-test缓存和最多两项`/api/signals`均已验证；G-1外部marker/sequence及DBC RX增长且decode error=0 |
| F-009 | 日志和规则引擎 | [一期边界客观已验证] | LogTask长跑、实体CSV、冷启动恢复及G-1同映像增长通过；QSPI单规则与TF固定两槽v2/v3、优先级、manual、timeout/safeState、HTTP CRUD和GPIO均有现场证据。热插拔、无界规则和通用配置为非目标 |
| F-010 | 最小 RuleTask/继电器 | [客观已验证] | 已烧录 50 ms RuleTask、短临界区外部 RX 快照和 PE7/PE8 集中输出；固定延时/超时/手动优先级/高滞回均已实测，ST-Link pending 候选仅在 QSPI 保存读回成功后提交并自动 reload，失败保留旧运行态配置 |
| F-011 | TF RuleFile v1 单规则启动加载 | [客观已验证；非法板端输入未注入] | `/config/rule.conf` 固定 256 字节上限；有效 v1 已在板端覆盖非默认 QSPI 参数，缺失文件已创建且不覆盖；非法文件由主机纯解析测试覆盖，板端未注入；仅表达已有单规则四参数 |
| F-012 | 网页 CAN 发送控制 | [客观通过] | classic CAN窄合同：`GET/POST /api/can/tx`和`GET /api/can/tx/signals`，标准ID、DLC、8字节HEX、`100..10000 ms`；TX self-test/RX缓存独立。用户已部署网页上电，浏览器实测状态灯、TX/RX累计、默认折叠和CANoe式TX/RX DBC表；最终`0x321`/DLC4/`C2 A5 34 12 00 00 00 00`/1000ms已应用result=0，自动刷新和两次reload均无连接拒绝。CANtest外部输入由用户确认且RX表增长；未直接读取CANtest接收显示，不能声称外部接收器逐帧确认新TX帧 |
| F-013 | 网页手动 TX 与 DBC `signalKey` 两槽规则 | [最终现场验收完成] | 根目录新网页下自动刷新 TX/RX=`55/364→576/5540`，两次刷新期间编辑均保留并提交，最终 TX `sequence=256`，warn/error为空。候选 DBC获用户授权激活，runtime=`loaded=true/generation=1/bytes=151/messages=1/signals=2`；TX/RX为 marker=`42434`、sequence=`256/4660`。slot1 V4回读`Can2Data.sequence/4660/priority20/action off`，外部 RX sequence=`4660`时manual `relay1Output=0`与高优先级off一致；此前构建、反汇编、烧录已实际完成 |
| F-014 | 大 DBC selected-only事务闭环 | [A0-F通过，G实板收口中] | active generation7为128项/16消息；最终G映像双ID输入下总RX+272、matched+136、updates+`136×8`，page0八项GOOD与page1 MISSING，证明只更新selected；停`0x100`保留`0x110`后RX仍增长且page0八项STALE/末值保持。v3会话已正常STOPPED；实体CSV/meta仍`[待确认]`，CAN-FD实板`[未验证]`。 |

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
| ADR-032 | 大 DBC active采用generation/manifest、prepared selected-only双槽与V5 definition hash | 已接受；A0-E实板门禁通过 |
| ADR-033 | selected实时质量与G日志合同 | 已接受；G实板收口中 |
| ADR-034 | 生产固件仅加载RuleFile V5并对legacy/损坏输入fail-closed | 已接受；用于满足A0 Flash预算，host保留V1-V4解析测试 |
| ADR-035 | active manifest轮转失败必须同步回滚旧current | 已接受；新候选已构建，待烧录回归 |

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

### ADR-032：大 DBC selected-only active事务

active不覆盖固定`active.dbc`后异步reload，而是验证candidate引用、用index只构造selected records到非活动runtime、检查V5规则key与固定`definitionHash`、写入并读回不可变active generation文件，最后提交current manifest并在同一TF mutex窗口内执行不可失败的短临界slot flip。启动独立验证current/previous，并只清理不可能被两者引用的next-generation tmp/formal；FAT32多文件rename不声明原子。所有失败显式discard prepared slot并保留previous、旧runtime/value slot和规则。实板已验证1/128项、rule key缺失、日志门禁和复位恢复；外部Classic RX属于F，CAN-FD实板仍`[未验证]`。

### ADR-035：active manifest轮转失败的立即回滚

`current→previous→current.tmp→current`不是FAT32的多文件原子操作。若旧current已移动而新current rename、读回或runtime短临界publish失败，平台必须在同一TF锁内删除本次current（如已生成）并把previous恢复为current，再读验旧manifest；绝不只依赖下次启动从previous兜底。若回滚I/O本身失败，previous保留且启动恢复以current/previous独立验证选择旧有效generation；不声称FAT32能在介质故障下提供原子回滚。

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

2026-07-26最新最终固件全功能回归推翻了上一段“当前无阻断”的时效性：在manual、日志、规则和DBC组合页面操作中再次观察到业务已应用但浏览器`Failed to fetch`，以及ping正常、socket0=`0x17(ESTABLISHED)`、80端口拒绝且关闭标签不能恢复，其中一次`lastNonclosedClose=0x11c`。复位后成功的低频串行窗口只证明恢复可用，不能覆盖失败样本；ADR-030的250ms网页收尾仍保留，但已不足以关闭当前HTTP稳定性问题，下一补丁必须等待同连接pcap/trace证据。

2026-07-27以原始TCP客户端“连接后不发送”稳定复现相同实时状态：socket0持续`0x17`、`RX_RSR=0`、请求计数固定、任务循环增长且后续连接失败；客户端关闭后立即恢复。源码确认普通`ESTABLISHED`零数据分支没有时限，因此只在该分支增加100 ms计时，超时调用既有graceful `DISCON`，任何请求字节、LISTEN、硬关闭或正常断开均清除计时；响应ACK和disconnect recovery仍为500 ms。最终10轮原始连接均回收、浏览器和API回归通过，关闭该确定性缺陷。浏览器偶发长尾的同步快照中socket已LISTEN且idle计数不变，因此不属于本ADR修复触发；不借此引入并发socket或修改半包`HANDLE_WAIT`。

### ADR-031：安全审查 P0 保持既有架构的可靠性防护

本轮不新增 HTTP、网页、规则动作或存储架构。FDCAN2 继续由高优先级 CAN 任务消费，但在原有最多 50 ms 轮询基础上启用 FIFO0 新帧、满和丢失通知；ISR 仅计数和 `vTaskNotifyGiveFromISR`，任务被通知后立即 drain FIFO，FIFO fill/满/丢失计数仅作诊断，不改变 CAN 业务语义。

独立 IWDG 仅在 CAN、解码、W5500、HTTP、DBC、配置、日志和规则任务自上一巡均有进展时刷新；首次巡检只建立基线。MemManage、BusFault、UsageFault 和 HardFault 保存堆栈寄存器、LR 与 SCB fault 状态到 RAM_D3 `.noinit` 的 68 B XOR 校验记录后系统复位。该记录用于复位后调试读取，不新增网络导出或持久化 API。

DBC 沿用当前 classic CAN 窄合同：仅标准 ID`0..0x7ff`、DLC`0..8`，每个信号必须在所属消息 DLC 位范围内且 factor 非零、minimum 不大于 maximum；不在本 ADR 中支持扩展帧。RuleTask 按当前启用规则的最大 timeout 维护 SignalCache `stale`质量，规则自身仍按各自 timeout 判定安全态。TF 追加写成功后执行`f_sync()`，底层`CTRL_SYNC`必须有界等待卡进入`SD_TRANSFER_OK`，非对齐scratch写分支在每扇区DMA完成后也必须等待ready；超时向FatFs返回错误。保持现有512 B/5 s批量策略，不把该修复表述为FAT32原子事务或绝对掉电保证。

IWDG启动额外显式等待LSI ready，并按`0xCCCC -> 0x5555 -> PR/RLR -> 等待SR -> 0xAAAA`顺序配置，避免硬件拒绝预启动配置。Fault record位于`RAM_D3+0x100`，复制后显式Clean D-Cache再复位，确保`.noinit`记录在软件复位后可按XOR校验读取。

本 ADR 的最终候选已完成CTest=`20/20`、STM32H750 ELF链接/关键反汇编、烧录、IWDG任务卡死复位、受控HardFault处理链路、外部CAN高负载和TF物理断电复测。HardFault受控跳转证明记录复制、D-Cache回写、复位及RAM_D3保持，不等价于真实硬件异常的堆栈根因采集。首次TF断电复核失败且存在测试前介质污染混杂；底层同步补丁修复后在测试前`fsck=0`的干净FAT32介质上复测，断电后目标CSV为399223 B/4159行、所有数据行可解析、无撕裂尾行，`fsck_msdos -n`退出0。重新上电后TF、网页、active DBC、网络、外部CAN均恢复，新日志file/write/flush继续增长且failure/drop=0。最终ELF/HEX SHA-256=`5f98bfa3e3af180219e0429734ff99d4c13a65645733356b387387eb17df7987`/`6f3e92ab95c91e79133a57710873c0dc9c20b3b8621bcab9f87aa4c4692144f8`。LAN写授权与TX白名单因缺少生产允许报文合同和授权来源，明确不在本ADR中擅自假定。

高负载暂停式GDB首测会在内核暂停期人为填满FIFO，故不能以该读数评估运行态。完整`32 x CanFrame`RX队列确因FreeRTOS heap耗尽导致rule任务创建失败，故不采用该方案。保持任务、队列和DecodeTask边界不变，RX队列改为`32 x Can2RxQueueFrame`（`id/IDE/DLC/8字节数据`，反汇编为`xQueueGenericCreate(32, 16)`），DecodeTask再重建既有`CanFrame`。在用户持续外部输入下，OpenOCD telnet `mdw`非停机C→D→E两个20秒窗口中FIFO full/lost和RX queue drop均无增长，而RX与DBC匹配持续增长；该候选的FDCAN高负载P0通过。

### ADR-032：大 DBC v1 使用 generation manifest 与 selected-only 合同

2026-07-31以`/Users/elvin/Desktop/project/data/BNE_CLASSIC_CAN_TEST_100KB.dbc`为真实输入启动阶段17 A0-H。实测输入为`100071 B/112 messages/896 signals`，标准ID`256..367`且全部DLC8；最长key为52 B，672个key超过旧47 B有效上限。旧计划中的23消息/922信号扩展CAN-FD样例和16项分页不再作为本轮验收基线。

A0合同以`docs/LARGE_DBC_P0_CONTRACT.md`和`include/large_dbc_contract.h`为权威来源。candidate和active均使用不可变generation文件组加64 B `current/previous` manifest，`current`为唯一提交点；统一称为“事务式、断电可恢复提交”，不得声称FAT32跨文件rename原子。candidate只有在upload二次读取、index、默认selection和三文件交叉验证全部成功后才提交manifest。active只有在规则`key + definition_hash`检查、非活动runtime构造/验证和active generation写入读回全部成功后，才提交磁盘manifest并在短临界区发布RAM manifest/runtime generation；任一失败保持旧active/runtime、规则、SignalValueState、继电器和日志。

目录上限与运行态上限分离：TF index最多2048消息/2048信号，允许当前112消息输入完整索引；RAM仅保留最多64个selected message和128个selected signal。selection是2048 bit单一正向位图，未选信号不得进入runtime、CAN decode、SignalValueState、`/api/signals`、新CSV或新规则候选。candidate token固定为`generation + source_size + source_crc32`，不是授权凭证。

格式v1全部按固定字段宽度、小端显式序列化并带size/CRC，禁止写packed struct。index header/message/signal record为`80/16/160 B`，selection为`64 B header + 256 B bitmap`，manifest为64 B；精确offset、CRC与FNV-1a-64 definition hash canonical stream见公共合同。parser支持LF/CRLF/无尾换行、标准/Vector扩展ID、Intel/Motorola、signed/unsigned、1..63 bit、科学计数法和DLC0..64；拒绝multiplex、`SG_MUL_VAL_`、重复消息/key、NaN/Inf/溢出和影响解码的未知语法。message/signal/key/unit有效内容上限为`31/31/63/31 B`。

Classic/FD合同为declared length `0..8 => CLASSIC_OR_FD`、`9..64 => FD_REQUIRED`，动态长度采用严格相等语义。当前硬件优先验收标准Classic CAN/DLC8；扩展CAN-FD模型保留但实板`[未验证]`，不得以源码、upload/index或TX self-test代替外部64 B RX证据。

API每页最多8项；最坏8项会超过2 KiB，因此JSON scratch为3072 B且最大正文3071 B，先完整有界序列化，再按最多512 B以固定Content-Length分段发送；这不是HTTP chunked transfer。容量或转义失败不得返回HTTP200截断body。日志准入固定为`selected_count * 1000 <= 20 * sample_period_ms`，会话锁定active generation与selection CRC并写同名`.meta`；20 rows/s的新路径硬件能力仍为`[未验证]`。最终固件至少保留4096 B FLASH；新selected runtime/value/static scratch总预算65536 B，不新增任务或常驻FreeRTOS heap分配，minimum-ever-free heap至少4096 B、受影响任务stack margin至少256 B。

授权边界冻结为隔离实验LAN和现场操作者显式写操作，仅供本轮lab验收；生产构建在统一生产写授权完成前必须fail closed。candidate token、测试帧和硬编码默认token均不得作为生产授权。A0通过前不修改HTTP、TF、FDCAN、网页或runtime业务，不烧录。

A0最终定向host合同测试`1/1`通过，统一`./scripts/verify.sh`为CTest=`21/21`；STM32目标`ninja: no work to do`，ELF `text/data/bss=112828/768/243948`，ELF/HEX SHA-256保持`742dbe264a5f6ea7282123fd151ff67aac30cd410ec5a41a0acb331092b2b92f`/`0e6396e22dfb8d85627e626314aa6d9fca61abf40efeab2fd01d8baf41c01025`。本阶段没有固件业务改动，因此未执行新反汇编或烧录；A0门禁通过，下一步仅解锁A1 pure core parser/index与host dump/verify。

A1已完成且不改变上述合同：parser仅持有512 B行scratch并按任意chunk消费，index builder使用TF/host可替换的random-access I/O与双spool，不把目录常驻RAM。真实输入用`1/137/512 B`三种chunk得到逐字节相同的`145232 B` index；header/message/signal区分别为`80/1792/143360 B`，source CRC32=`4B88D9CE`、record CRC32=`54431B55/90868936`。旧`151 B` active DBC得到`416 B/1 message/2 signals` index。index verifier独立检查CRC、offset/count、ordinal/parent、重复identity/key、definition hash、ASCII `message.signal` key、UTF-8 unit及control/padding；dump以binary64固定hex bits为权威输出，避免locale改变格式。

A1严格警告、ASan/UBSan、host build/verify/dump和统一`./scripts/verify.sh`均通过，统一CTest=`26/26`。新增源码虽参与STM32编译，但尚未被固件业务入口引用，最终ELF符号中不存在`dbc_stream*`/`dbc_catalog*`且map显示其section被GC；因此最终`text/data/bss=112828/768/243948`及ELF/HEX SHA-256仍为`742dbe264a5f6ea7282123fd151ff67aac30cd410ec5a41a0acb331092b2b92f`/`0e6396e22dfb8d85627e626314aa6d9fca61abf40efeab2fd01d8baf41c01025`。本阶段未改HTTP、TF、FDCAN、网页或runtime，未烧录；B阶段才允许接入流式upload tmp。

B阶段只发布暂存对象，不升级candidate语义：W5500 socket0顺序解析固定`POST /api/dbc/upload HTTP/1.1`，要求唯一Content-Length与`text/plain`，拒绝Transfer-Encoding、multipart、零长度和超过256 KiB；每次最多512 B，TF `f_write`成功后才更新W5500 `RX_RD`。状态机以增量IEEE CRC32记录source，5 s idle或120 s total、对端提前关闭、长度越界及任一TF错误都关闭并unlink当前tmp；成功仅校验size并`f_sync + f_close`，文件名固定为`/dbc/upload.<16位大写generation>.tmp`，不得提前替换candidate。

B门禁由host与实板共同关闭：portable测试覆盖1 B/256 KiB、任意分块、CRC golden、短/超写、sink失败、取消、tick wrap、5 s/120 s和HTTP拒绝矩阵；统一CTest=`27/27`。真实100071 B文件实板上传返回size=`100071`、CRC32=`4B88D9CE`和`/dbc/upload.0000000000000001.tmp`，板端为196次写、`f_write/f_sync/f_close=FR_OK`，active runtime前后完全一致；413/415/408和idle tmp unlink通过。nano printf不支持`%llX`曾实际破坏响应和路径，已改为两个固定32-bit字段并加入path host测试。C阶段必须把RAM-only upload序号纳入持久generation/manifest恢复，B结果本身不等于candidate有效。

C补充冻结：candidate与active各自维护独立单调generation；默认selection的`selection_generation == candidate_generation`，selection更新生成新candidate时两者再次相等。C工作文件固定为`candidate.<GEN16>.spool.tmp/.idx.tmp/.sel.tmp`和`candidate.current.tmp`，均不是提交点；正式三件套和current manifest仍按P0既定顺序发布。

C门禁已关闭：固件用DbcTask同步backend对upload二读并在TF上构造combined spool、index和默认selection，temp/formal/manifest引用均完整读回后才发布current。manifest发布后若最终读回失败会显式回滚本次current及本次rename的generation文件，旧candidate留在previous；next generation只在成功发布后推进。启动独立验证current和previous并选择current优先，candidate恢复不触碰active/runtime。真实输入最终发布generation2，复位后恢复generation2且旧active runtime仍为generation1。目录孤儿的全扫描清理未在C实现，保留H故障恢复范围，不影响current/previous有效性结论。

D阶段保持单socket和单请求串行。`GET /api/dbc/candidate/signals`只接受固定page、最多63 B解码搜索词和`selected=all|true|false`，每页固定8项；`POST /api/dbc/selection`要求完整candidate token，单请求set/clear各不超过32项且禁止交集。JSON先两遍计算和完整序列化，最大3071 B，容量不足、转义失败或索引错误均不得以200返回截断正文。selection修改复制旧generation的`.dbc/.idx`并创建新`.sel`，temp/formal/current manifest全部验证后才发布新candidate generation；不触碰active/runtime、规则、SignalCache或日志。

D后端与事务实板已通过：candidate由generation2/selected0逐批推进到generation7/selected128/messages16；第129项返回422、stale token返回409、非法参数返回400，日志进行中selection写入返回409；重启恢复generation7/128且旧active runtime仍为generation1。长TF查询期间HttpTask持W5500 mutex，故W5500Task loop可能停转；IWDG健康合同修正为W5500 loop或candidate progress任一增长，避免把同一受控长操作误判为死锁，最终查询19.67 s且`unhealthy=0`。这不提供并发HTTP能力。

D网页源码已加入256 KiB上传、固定高度8项目录、300 ms搜索防抖、selected筛选和set/clear；但板端TF仍是旧28989 B页面，仓库页面哈希尚未在板端出现。既有`stm32h750_tf_ensure_default_www()`只在文件不存在时创建极简页，固件没有更新已有网页的接口。按既有硬件边界，必须下电更新TF `/www/index.html`、插回上电后再做浏览器与console验收；完成前D不得关闭、E不得启动。

2026-08-01用户确认已覆盖新网页并插回上电，但板端事实未满足部署门禁：网络正常，TF状态为2，首页和candidate均404，active runtime也未加载。ST-Link读回`g_tf_mount_result=0x0D (FR_NO_FILESYSTEM)`，SD初始化和扇区读取本身无HAL错误或读失败；一次`reset run`后状态完全一致。因此本ADR不把人工文件复制确认升级为网页部署通过，也不把此前generation7恢复证据外推到当前不可挂载介质。处理顺序固定为下电、主机只读检查分区/FAT32、保留或恢复可审计数据、重新插卡冷启动，再核对网页哈希、candidate manifest和旧active；未经只读证据不得直接格式化或重建。

主机只读检查已把故障定位到卷引导区：`/dev/disk4`仍枚举为15.6 GB MBR，`disk4s1`类型为DOS_FAT_32且偏移2048扇区；但`diskutil verifyVolume`调用的`fsck_msdos -n`报告`Invalid BS_jmpBoot in boot block: 555342`并退出201，只读mount失败。当前设备保持unmounted，未执行repair或格式化。由于卡内可能含日志和此前candidate generations，是否尝试镜像/数据恢复属于用户数据保留决策；只有用户确认这些数据可丢弃时，才允许重建MBR/FAT32并从仓库网页、旧active资产和真实100KB DBC重新生成candidate/selection。无论选择哪条路径，旧generation7实板证据可保留为历史，但当前介质恢复后必须重新取得当前generation/selection和网页哈希，不能直接复用为新卡运行态。

用户随后明确授权格式化并要求加载全部TF必需文件。重建范围严格采用`deploy/tf/README.md`：`MBR + FAT32`卷、五个必需目录、新版`/www/index.html`、151 B `/dbc/active.dbc`及相同`/dbc/candidate.dbc`。日志、事务tmp、prev、large-DBC generation/manifest和V4规则均不是可猜测静态资产，不予伪造。格式化删除了旧卡数据且本轮未创建镜像。部署后网页与两DBC分别通过SHA-256和`cmp`；AppleDouble旁车清零，卸载后的FatFs三阶段只读检查exit0，随后只读复挂载再次核对文件并安全eject。该结果只证明主机部署介质有效；必须插回板端冷启动后再证明TF mount、页面服务、active load和新的candidate generation。
## ADR：阶段17 D网页门禁通过并冻结E边界（2026-08-01）

- 状态：D已验收，E进行中。
- 决策：重建TF后的candidate generation1只作为上传/索引结果；网页ordinal0..7 selection提交为新的generation2，当前权威token为`0000000000000002-000186E7-4B88D9CE`。后续E不得引用格式化前generation7。
- 证据：板端网页36254 B哈希与仓库一致；runtime仍为旧generation1/151 B；candidate total896；page1=ordinal8..15；`packvoltage`匹配112；`selected=true`精确8项；独立HTTP与浏览器DOM一致，console无warn/error。
- E合同：先完整验证candidate并构造非活动selected-only runtime，再进行规则`definition_hash`兼容检查；active generation持久化写入/读回成功后，最后短临界区提交active manifest和runtime generation。任何失败均不得破坏旧active/runtime、规则、SignalCache或日志。
- 非目标：不为一次短暂`Failed to fetch`引入多socket或并发请求；同页`requestQueue/exclusive()`已证明串行。多标签页/其他客户端是服务端单socket能力边界，不在D扩展。

## ADR：selected实时质量与G日志合同（ADR-033，2026-08-01）

- 状态：已接受，G实施中。
- F门禁：用户明确确认两个ID同时发送；时间分离样本总RX增量568、selected matched增量284、updates增量`284×8`，因此另284个外部未选帧没有进入selected runtime/value slots。标准Classic CAN正向八项GOOD值与混合流量反向隔离均通过；CAN-FD实板继续`[未验证]`。
- 实时质量：与规则timeout和日志周期解耦，固定`3000 ms` freshness阈值。`update_seq==0`为`MISSING`；收到且wrap-safe age不大于3000 ms为`GOOD`；超过为`STALE`并保留末值；内部错误为`ERROR`。API与CSV必须调用同一effective-quality逻辑。
- 实时API：`GET /api/signals?page=0&q=...`严格解析，ASCII不区分大小写搜索，固定最多8项；generation用16位hex字符串、selection CRC用8位hex字符串，item只含ordinal/key/value/raw/unit/quality/updatedMs。MISSING仍列出且value/raw为null。serializer必须两遍长度预检，失败清空body并返回非200。
- 日志：新增selected-only `_signal-v3.csv`和同basename `.meta`，旧v2文件不续写、不迁移。准入继续使用`selected_count*1000 <= 20*sample_period_ms`；session锁定active/candidate generation、source fingerprint、selection CRC、selected count和period。LogTask逐项短快照、512 B内单行scratch与既有静态batch；buffer不足必须先flush并重试当前行，禁止静默drop。stop必须先flush、写clean footer并同步，完成前继续阻止upload/selection/active。
- 资源与禁止范围：替换生产旧SignalCache/API/CSV引用，依靠GC回收，不并存两套大实现；不新增任务、队列、heap、多socket、chunked、WebSocket或目录常驻RAM。最终仍须满足Flash上限126976 B、stack margin与统一verify/反汇编/实板TF证据。

2026-08-01旧G映像的API级GOOD/STALE、分页、selected-only和日志控制已取得证据，但当前实体TF验收明确`[阻断]`：取卡后对`/dev/disk4s1`的`diskutil verifyVolume`只读调用`fsck_msdos`返回`206`，根目录有end marker后的目录项，`LOG/20260801_175230000_signal-v3.meta`声明`10471 B`而其链最多`8192 B`，同会话CSV声明`1153932 B`而其链最多`32768 B`；candidate/active generation与`active.previous`也有越界/交叉链。卷已卸载且read-only mount失败。故现有CSV/.meta即便可枚举或可计算hash也不构成clean footer、selected-only或持久化通过；不得继续写卡、部署网页、烧录未验证P0候选或声称FAT32事务成功。必须先由用户决定只读镜像保全，或确认丢弃当前故障会话重建FAT32；重建后重新部署静态资产、生成candidate/active，并重新验收G实体日志和新候选最小回归。

用户随后明确授权格式化并重载。执行范围严格限于已经`diskutil list`确认的外置物理`/dev/disk4`：重新建立`MBR + FAT32 CANBUS`，部署`/www/index.html`、151 B `/dbc/active.dbc`及相同`/dbc/candidate.dbc`，创建空`/log /config /sys`，不恢复故障会话、transaction文件、large generation、规则或历史日志。网页SHA-256=`9b96b6a022a9a3c1659abf52096543ebefba3e57e1218fdd5d6c62e92ce0e5c2`、两份DBC均为151 B/SHA-256=`271f20f923343c9f923bd6db4da4599e0349983b0a5bd43edeae87d152855417`，主机逐字节比较通过；卸载后`fsck_msdos -n` exit=`0`，只读重挂载重复`cmp`后再次卸载。该恢复不追溯改变旧损坏文件的失败结论，也不是G CSV或P0新候选的板端证明；必须上电后重新形成可验证large candidate/active和v3实体会话。

### ADR-034：生产RuleFile采用V5-only fail-closed（2026-08-01）

- 状态：已接受并进入最终烧录映像。E阶段曾使用显式V4→V5迁移接口完成现场规则转换，该结果保留为历史证据；G最终生产映像不再携带V1-V4 loader、转换器、旧`/api/rule/config`或`/api/rules/migrate-v5`路由。
- 决策：启动只加载带`definitionHash`的V5。没有任何规则文件时使用空规则集；发现legacy文件时返回明确不兼容状态并保持空RuleEngine，V5损坏或读失败同样fail-closed。不得静默接受同名但定义变化的规则，不自动删除、猜测hash或回退到legacy enabled语义。显式V5 `POST /api/rules`仍是从空/legacy状态建立规则的唯一生产入口；PUT/DELETE要求当前V5有效。
- 依据：A0固定Flash最大值为`126976 B`，G加入selected API和CSV/meta后继续常驻V1-V4生产兼容会超过预算；板端legacy loader精确占用并非零成本。历史格式解析、兼容判定与固定向量仍保留在host代码和测试中，避免丢失格式验证能力，但不链接进固件。
- 证据：最终`./scripts/verify.sh`为CTest=`34/34`；Flash=`121708 B`、RAM_D1=`196664 B`，ELF `text/data/bss=121256/444/196284`，低于Flash合同上限5268 B；ELF/HEX SHA-256=`97cd14af626ecb7951f1f012148819f922c688b339b1f9185d66fbd630afc10b`/`963fa24f74a9808bc0687ab2ab4e5cd7ba2e7d3c5d1b912ae5794c08816e5b7d`。最终符号检查确认V1-V4 loader与旧路由不在ELF，ST-Link下载、校验和软件复位成功。
- 边界：这不是删除TF上的legacy文件，也不宣称自动迁移；legacy介质进入当前固件时应明确拒绝。当前已加载V5规则、active generation7和selected runtime的复位恢复通过；H中的更多异常介质/断电恢复仍`[未验证]`。

### ADR-035：manifest即时回滚与FatFs写失败传播（2026-08-01）

- `current→previous→new current`不是FAT32多文件原子事务。candidate、selection及active manifest在new-current rename、读回或active runtime publish失败时，必须同一TF锁内尝试previous→current；回滚失败保留previous供启动恢复，不触碰旧generation、规则、SignalCache或日志。
- FatFs同步合同要求镜像FAT及FAT32 FSINFO的每个`disk_write`失败均转为`FR_DISK_ERR`，不再把该类失败报告为成功；这不承诺FAT32掉电原子性，也不构成旧卡损坏因果结论。
- 本ADR只有源码、构建和反汇编证据。新候选未烧录；TF写失败注入、active reload失败和物理掉电恢复仍`[未验证]`。

### ADR-036：单socket响应以SEND_OK为收口边界（2026-08-01）

- 观察到长selection响应已完整HTTP200但后续socket0停在`ESTABLISHED`，原TX_FSR refill等待未保留有效ack/disconnect pending。决策：最终chunk收到W5500 `SEND_OK`后立即发起既有graceful `DISCON`；不增加socket、并发、连接复用或应用层重试。
- 依据：W5500的`SEND_OK`是本项目发送完成边界；直接按TCP graceful close收口可避免响应已交付后单socket永久占用。失败仍走既有close/recovery路径。
- 证据：修正后实板冷启动candidate恢复、连续`status→page1`、active长请求及复位后的runtime恢复均已完成。网络断链/恶意半包等扩展场景不因本ADR自动通过。

### ADR-037：F外部Classic CAN证据边界（2026-08-01）

- 当前实板合同是Classic CAN、标准11-bit ID、DLC=8；双ID外部输入期间，RX计数、selected runtime更新和`/api/signals`的8项`GOOD`相互一致，硬件快照记录`ID=0x110`、`DLC=8`、首字节`0x01`，DBC matched last ID为`0x100`。
- 决策：该证据只关闭外部Classic CAN正向RX与selected-only decode；TX self-test不计入RX证据。必须先取得“停止`0x100`、继续`0x110`”的人工确认，才能记录STALE/隔离；CAN-FD实板继续标记`[未验证]`。
