# 当前上下文

更新时间：2026-07-24（上一阶段时间同步/可选时间日志已提交；FAT 属性本地时间与记录会话文件最终实体 TF 验收通过）

## 当前仓库

- 工作目录：`/Users/elvin/Desktop/project/can_bus`，当前实际解析到 `/Users/elvin/Desktop/project/can_bus_W5500`。
- 当前分支：`codex/W5500`。
- 当前主线：W5500/SPI2 + FDCAN2/MCP2562FD + TF 卡 + W25Q128 + FreeRTOS。

## 已验证状态

| 模块 | 状态 | 证据摘要 |
| --- | --- | --- |
| W5500 | [客观已验证] | `VERSIONR=0x04`，静态 IP `192.168.1.88`，主机 ping 通过 |
| W5500 HTTP/API | [一期边界客观已验证] | 受限状态/CAN/signals/DBC/rules/manual/静态页API均已验证；F-76完整响应/双向FIN、G-1的200/400/404及G-2的500恢复通过，保持单socket非并发边界 |
| 网页手动 TX / 两槽活动 DBC `signalKey` 规则 | [最终现场验收完成] | 正确部署根目录网页后，自动刷新 TX/RX=`55/364→576/5540`，warn/error为空。两次编辑 TX 在`1800 ms`与`1300 ms`自动刷新后仍保留并提交成功，最终 TX DBC `sequence=256`。候选 DBC经用户授权激活，runtime=`loaded=true/generation=1/bytes=151/messages=1/signals=2`；TX/RX均解析 marker=`42434`，sequence分别为`256/4660`。两槽均含 marker/sequence；slot1 V4回读`Can2Data.sequence/4660/priority20/action off`。外部RX sequence=`4660`时 manual `relay1Output=0`，与高优先级off一致。 |
| W-1 CLOSE_WAIT 关闭修复 | [本轮现场回归通过；长期稳定性未单独证明] | pending DISCON 的`SR=0x1c`已从`http_open_listener()`分离并调用既有 graceful `DISCON`；同一候选已烧录，OpenOCD输出`Programming Finished/Verified OK/Resetting Target`。首轮与退出后新页第二轮完成后，概览`lastNonclosedClose=0`；仅说明本次回归未再观察到旧`0x11c`，不作绝对长期结论。 |
| 网页 CAN 发送控制 | [客观通过] | 经典 CAN窄合同保持为`GET/POST /api/can/tx`、`GET /api/can/tx/signals`、标准 ID、DLC、8字节HEX、周期`100..10000 ms`，TX/RX缓存分离。用户已将更新版`www`写入TF并上电；状态灯`status-lamp ok`、TX/RX累计、默认折叠详情、CANoe式TX/RX DBC表和可逆控制均由浏览器实测。最终配置`0x321`/DLC4/`C2 A5 34 12 00 00 00 00`/1000ms，已应用且result=0；TX self-test与CANtest外部输入RX表均为`42434/4660/ok`。自动刷新`120/273→134/417`，两次reload为`145/527`、`162/694`，无连接拒绝或控制台warn/error。未直接读取CANtest接收显示确认新TX帧，不能把该边界写成外部接收器逐帧证据。 |
| 实时信号 API | [客观已验证] | `GET /api/signals` 已烧录验证返回最多两项 SignalCache 快照，含 key/value/raw/unit/updated_ms/quality；持续 CANtest 下返回两个已解码信号 |
| TF CSV 落盘 | [一期历史边界客观已验证] | 旧`/log/signal.csv`的长跑、冷启动和实体CSV证据保留为历史；新实现不向该旧六列文件混写时间列。运行中热插拔仍不支持 |
| 最小 LogTask | [实体会话文件已只读验证] | 新任务默认关闭，启用且RAM时间已同步后才按`100..10000 ms`采样，缓冲`512 B`或`5 s`flush；新会话文件只写`/log/<本地开始时间>_signal-v2.csv`。最终会话实体文件已只读确认。TF 卡操作边界仍为插拔前下电 |
| 网页时间同步与可选时间日志 | [现场主体通过；实体 CSV 内容已只读复核] | 无 RTC/NTP，`POST /api/time/sync`以`unixMs`建立RAM基准，复位后失效；`GET/POST /api/log/control`查询/设置`enabled`、`samplePeriodMs`、`timeSynced`、`unixMs`。网页、自动同步/回读、启停、手动同步、最终停止及自动刷新下安全提交均已现场完成；ST-Link 写入`33→40`、文件`18185→22217 B`、失败/丢弃`0`证明运行态 flush 落盘。实体只读复核确认`/Volumes/NO NAME/log/signal-v2.csv`为`28553 B`，表头为`utc_time,unix_ms,updated_ms,key,value,raw,unit,quality`，首末UTC为`2026-07-23T16:59:32.051Z→2026-07-23T17:03:34.638Z`，含`Can2Data.marker=42434`、`Can2Data.sequence=4660`和`quality=ok`。 |
| FAT 属性本地时间与记录会话文件 | [最终实体 TF 验收通过] | 已严格只读确认`/Volumes/NO NAME/log/20260724_012916204_signal-v2.csv`存在、`8035 B`；macOS CST 创建/修改时间为`2026-07-24 01:29:16/01:29:36`，均非1970。表头为`utc_time,unix_ms,updated_ms,key,value,raw,unit,quality`，首条UTC记录为`2026-07-23T17:29:16.745Z`。这闭合浏览器本地 UTC offset 修复 FAT 属性及“开始记录按本地开始时间前缀创建会话文件”两项需求；`/log`目录自身1970创建时间仅为旧目录历史元数据，不等于新文件失败。 |
| FDCAN2 外部 CAN | [客观已验证] | Windows CANtest 可收到开发板 `0x321` 周期帧；开发板收到 Windows 发帧；2026-07-08 22:46 分析仪收发打开后复查 `sendResult=0`、`rx_count=508`、`tx_count=728` |
| TF 卡 | [客观已验证] | SDMMC/FatFs smoke test 写读通过 |
| TF 静态文件服务 / Web一期 | [W-1两轮现场回归通过] | 用户已下电写入 TF 的`/www/index.html`并上电。自动 CAN 刷新期间的 DBC 上传/激活实际成功，FIFO队列修复消除了“已有请求进行中”；退出后新页面第二轮继续完成回归。浏览器短等待曾读到旧手动状态，等待队列清空后的最终回读正常，不作为网页错误。 |
| DBC 上传最小接口 | [客观已验证] | `POST /api/dbc/upload` 保存 `/dbc/candidate.dbc` 后从 TF 读回候选并调用 portable `dbc_parse_text()` 生成报告；已烧录验证返回 `bytes=164/lines=4/messages=1/signals=2/errors=0/valid=true`，ST-Link 读数 `candidate_load_result=0/candidate_valid=1` |
| DBC 活动文件激活 | [客观已验证] | `POST /api/dbc/active` 无请求体最小命令已烧录验证：读回候选、portable parser 确认为 `errors=0` 后写入 `/dbc/active.dbc`，旧活动文件备份到 `/dbc/active.prev.dbc`；有效候选返回 `activated=true`，无效候选返回 `HTTP 400 candidate_invalid` |
| DBC 运行态快照 | [客观已验证] | active `0x321`/2 信号 DBC 启动加载正常；本轮 `POST /api/dbc/active` 返回 `activated=true/runtimeGeneration=2`，随后读到 `generation/load=2/2`、`valid=1/result=0` |
| DBC 解码到 SignalCache | [一期边界客观已验证] | active DBC已接入独立TX self-test和外部RX缓存；G-1外部marker/sequence=`42434/4660`、DBC RX持续增长、decode errors=0，reload与CAN解码共享锁 |
| W25Q128 | [客观已验证] | 默认启动只读 JEDEC ID `EF4018`，未触发擦除；保留诊断区 `0x00FFF000` 的显式 ST-Link 擦写读回匹配 |
| FreeRTOS 单任务 | [客观已验证] | 已烧录验证 `g_freertos_task_started=1`、`g_freertos_loop_count` 递增，W5500/CAN/TF/W25Q128 状态保持通过 |
| FreeRTOS 基础多任务拆分 | [客观已验证] | TfTask 已烧录接管一次性 TF mount/smoke/default-page 初始化并由 bring-up 有限等待；MonitorTask 已接管 1 s 状态打印；CAN2 仍保持每 50 ms FIFO 接收、每 1 s 诊断发送；W5500 状态轮询与 HTTP socket0 轮询已拆为两个 50 ms 任务并由 mutex 串行化 |
| 最小 RuleTask/继电器 | [客观已验证] | 已烧录 50 ms RuleTask：固定 1000 ms 延时、1500 ms 超时安全低、ST-Link 手动 OFF 覆盖优先、固定高滞回均已验证；单规则配置 reload 与 QSPI 显式保存、读回和复位加载均已实测；新增 HTTP GET/POST 单规则配置闭环并跨复位验证 |
| 最小 QSPI 规则配置备份 | [客观已验证] | v1 单槽记录兼容；v2 使用 `0x00FFE000` 主槽和 `0x00FFD000` 备用槽，含 sequence、参数和 checksum。ConfigTask 两次交替保存、读回、复位加载、无效请求拒绝及候选/运行态隔离均已 ST-Link 实测 |
| TF RuleFile v1 | [客观已验证；非法板端输入未注入] | 首次启动 TF `status=0` 时缺失文件 `load_result=1/created=1`；复位后读取 `75` 字节有效文件，`load_result=0/read_len=75`，RuleTask `generation=2/reload=0`，有效文件把此前 HTTP 写入的 QSPI `42435/42433/1100/1600` 覆盖回 `42434/42432/1000/1500`；纯解析主机测试覆盖缺字段、非法阈值和非法时序且候选不变 |
| TF RuleFile v3 / 受限规则 CRUD | [客观已验证] | `/config/rules-v3.conf` 固定两槽、640 B 严格文本格式；顺序 HTTP `GET`、DELETE 禁用、POST 恢复、PUT、非法 PUT、重启持久化和默认恢复均已烧录验证。F-19 第二次真实冷启动后再验证 `source="v3"`、两槽默认语义和 ST-Link `load_result=0/rule_count=2/read_len=size=312`。写入经 ConfigTask TF tmp/prev 原子替换，成功后 RuleTask 原子 reload；v3 构造函数已消除 3.8 KiB 栈对象导致的实测 HardFault。 |

## 当前阻断项

- FAT 属性与会话文件的实体只读检查已完成，不再是当前阻断：`/Volumes/NO NAME/log/20260724_012916204_signal-v2.csv`为`8035 B`，macOS CST 创建/修改时间为`2026-07-24 01:29:16/01:29:36`，均非1970；首条UTC记录为`2026-07-23T17:29:16.745Z`。`/log`目录自身1970创建时间为旧目录历史元数据，不否定新文件验收。ST-Link变量采样曾因`unknown state`未成功，但不影响本次基于实体文件属性的结论。
- 新时间阶段的主体现场验收和实体 CSV 内容只读复核均已完成，不再等待网页部署或把历史`"unixMs":lu`列为当前阻断。该历史非 JSON 缺陷已最小修复、重建、反汇编和重烧录；`/Volumes/NO NAME/log/signal-v2.csv`已实际只读确认大小`28553 B`、唯一表头及记录行。运行态计数仍不替代该实体内容读取，旧`/log/signal.csv`历史证据也不能替代新文件内容。
- 现场已确认：TF 新网页加载、默认记录关闭与继电器详情折叠；未同步`250 ms`启动自动同步/回读，停止`800 ms`、再启用`1200 ms`、手动同步和最终停止；继电器红闭合/绿断开两轮反向输出后恢复关闭；自动刷新期间最终`request/applied=6/6`且 CAN 增长。ST-Link 两次采样写入`33→40`、文件`18185→22217 B`、失败/丢弃`0`，证明运行态成功 flush 及增长。
- 新实现仍保持W5500单 socket 串行处理；RAM时间不持久化，复位后必须再次同步。TX self-test仍不等于外部接收器证明，既有外部RX结论不扩大。

### 历史过程与已接受边界（以下不构成当前阻断）

- 用户已明确 TF 卡为“仅支持下电后插拔”：运行中热插拔/recovery 不再是功能或验收目标。历史真实拔插的 `FR_DISK_ERR` 仅保留为硬件边界证据；正式无 gate 固件已烧录，当前插卡启动下默认路径 `write=5/size=9206/failure=0`、ping/API/SignalCache 均正常。PA8 无检测开关且插拔均读高，永久屏蔽；临时检测/重挂载/格式化/gate 代码均不得提交。阶段 D 已按新的硬件操作边界关闭；阶段 E 已确认历史 `0xffffffff/erase_count=0` 是未实际触发时的初始化哨兵值，正式单次诊断已成功，下一固定阶段为 F。
- 阶段 F 的第一个“30 分钟无现场操作静态长跑”子项未通过，不能作为阶段 F 完成证据：同一正式固件已重新烧录，所有任务循环、CAN 队列、W5500 链路和顺序网络 API 曾保持正常，但默认 LogTask 从起始 `write/flush/failure/drop=4/4/0/0` 运行至终态 `15/396/381/1526`，最后 `g_log_last_result=1`、`g_tf_csv_write_result=1`、`g_tf_write_open_result=1`。F-6 已在 TF 保持插入、板级断电至少 10 秒再上电的干净基线捕获首次错误：约 32 秒，read failure `0→1`、call `51→102`，append stage/op=`2/2`，单扇区 `LBA=3826`；调用前 `State=1/Context=0/ErrorCode=0/STA=0/DCOUNT=0`，调用后 `State=1/Context=0/ErrorCode=0x20/HAL_SD_ERROR_RX_OVERRUN/STA=0x29000/DCOUNT=448`。F-7 只读审计确认该调用实际为 CPU polling `HAL_SD_ReadBlocks`（非 DMA/IDMA），故障时 `MASK=0`，不依赖 SDMMC IRQ；FDCAN2 没有 NVIC 接收中断且任务轮询，不能归因于 FDCAN2 ISR。F-8 的关中断实验会冻结 HAL tick，已撤回；F-9 的 `vTaskSuspendAll/xTaskResumeAll` 保留 tick/中断但在外部 `0x321` 输入下越过 F-6 call=102 后仍出现 read failure，终态 call/failure=`120/5`、LogTask failure=`6`、`HAL_TIMEOUT`，同样已撤回并重烧录正式 F-5 路径。任务切换不是已证实的充分原因；下一步只能只读审计 SD 传输/轮询参数与 HAL 机制，不直接改参数。其后网络回归未通过：主机 en2 对 `192.168.1.88` ARP 为 incomplete，ping/HTTP 均超时；但精确 ELF 读数 `bringup=0/VERSIONR=4/PHYCFGR=0xBF/link=1/network_configured=1` 且任务循环递增，不能写成固件或 SD 因果回归。
- 阶段 F 的第一个“30 分钟无现场操作静态长跑”子项未通过，不能作为阶段 F 完成证据：同一正式固件已重新烧录，所有任务循环、CAN 队列、W5500 链路和顺序网络 API 曾保持正常，但默认 LogTask 从起始 `write/flush/failure/drop=4/4/0/0` 运行至终态 `15/396/381/1526`，最后 `g_log_last_result=1`、`g_tf_csv_write_result=1`、`g_tf_write_open_result=1`。F-6 已在 TF 保持插入、板级断电至少 10 秒再上电的干净基线捕获首次错误：约 32 秒，read failure `0→1`、call `51→102`，append stage/op=`2/2`，单扇区 `LBA=3826`；调用前 `State=1/Context=0/ErrorCode=0/STA=0/DCOUNT=0`，调用后 `State=1/Context=0/ErrorCode=0x20/HAL_SD_ERROR_RX_OVERRUN/STA=0x29000/DCOUNT=448`。F-7/F-8/F-9 已排除 DMA/IRQ/任务调度为充分解释。F-10 只读审计确认最终运行配置为 1-bit、上升沿、无 power-save、无硬件流控、`ClockDiv=16`，由 `PLL1Q=100MHz` 推得数据 CK≈3.125MHz；F-6 `CLKCR=0x10/DCTRL=0x90→0x92` 与其精确一致。CubeMX 4-bit/25MHz 已被 BSP 覆盖，不能归因于“总线过快”。下一步固定 F-11 只开启 HardwareFlowControl，预期 `CLKCR=0x20010`，不改频率、宽度、timeout 或其他参数。其后网络回归未通过：主机 en2 对 `192.168.1.88` ARP 为 incomplete，ping/HTTP 均超时；但精确 ELF 读数 `bringup=0/VERSIONR=4/PHYCFGR=0xBF/link=1/network_configured=1` 且任务循环递增，不能写成固件或 SD 因果回归。
- F-11 已只将 HWFC 从 DISABLE 改为 ENABLE 并烧录。当前冷启动外部 CAN 条件下，板端 `CLKCR=0x20010`，read call=`31→420`、LogTask write/flush=`1/1→79/79`、read/LogTask failure 均为 `0`，并越过 F-6 的 call=`102`；ping 2/2、`/api/status`、`/api/can/status`、`/api/signals` 同次正常。该证据支持“HWFC 对当前 polling FIFO 路径有短时缓解”，不证明根因，也不替代 30 分钟稳定性验收。下一固定 F-12 是不改源码、不重烧录的 30 分钟连续静态耐久：保持当前插卡、上电、CANtest 既有帧不变，不做插拔/断电/参数切换；每分钟只读采样 read failure、LogTask write/flush/failure/drop、文件大小和关键快照，结束后复查 ping 与三个只读 API。任何 failure 增长即记录失败并停止把 F-11 写为有效稳定性修复。
- F-12 已在当前 HWFC 固件上通过：`23:18:31→23:48:48` 共30分17秒，无源码/烧录/复位或现场操作。read failure/call 由 `0/595→0/2407`，LogTask drop/failure/write/flush 由 `6/0/114/114→6/0/477/477`，文件 `167828→377524 B`，write close/result 始终 `0/0`。结束 ping=`2/2`，`/api/status`、`/api/can/status`、`/api/signals` 正常，CAN errors/busOff/tec/rec/sendResult=0且外部 marker/sequence=`42434/4660`。F-11/12 支持 HWFC 对当前路径有效，但不证明根因；阶段 F 的静态插卡日志长跑子项现为通过，断网、CAN bus-off/恢复与复位后 DBC/规则/日志恢复仍未验收。
- FreeRTOS 完整多任务架构仍未完成：DbcTask 已以 active DBC reload 窄命令独立运行并实机验证，TfTask 已接管一次性 TF 初始化，外部 CAN RX 已通过深度 8 队列交给独立 CanDecodeTask，CAN TX 已通过深度 1 队列交给现有 CanDecodeTask 发送；ConfigTask 深度 2 命令队列已验证，HTTP 单规则配置已复用该队列并完成保存、reload 和复位加载。HTTP 仍是 socket0 单连接最小实现；正式多记录/多规则配置服务仍未实现。
- W25Q128 已将 `0x00FFF000` 固定为显式诊断保留区，`0x00FFE000`/`0x00FFD000` 固定为单规则配置双槽；默认 bring-up 不擦写。v2 已实测交替写入、读回、sequence 选择、最新槽损坏后回退到较旧槽，以及两槽均无效后保留默认配置；后续通用配置仍需另行定义多记录演进与命令来源。
- 当前最小 RuleTask、固定 1000 ms 延时、固定高滞回、仅供 ST-Link 验收的手动优先级、单规则 reload、QSPI 保存成功后的自动 reload，以及 HTTP GET/POST 单规则配置已现场验证。阶段 C 另已完成固定两槽 RuleFile v3 HTTP CRUD 与 TF 持久化；它不是无界规则管理、第三槽、前端、鉴权或并发配置服务，不得扩大表述。
- 阶段 A RuleFile v1 已完成板端有效/缺失路径：首次缺失创建不阻断启动，复位后有效文件通过既有 RuleTask reload 覆盖非默认 QSPI 参数；RuleTask 读数为 `generation=2/reload=0`，外部 CAN marker=42434 时 PE7=`1`、PE8=`0`、GPIOE ODR=`0x80`。非法文件未通过真实 TF 输入注入，结论限于主机纯解析测试，禁止写成板端非法文件实测。
- 阶段 12 稳定性基线已完成：当前固件重新烧录 Verify 通过，两次任务计数增长，关键模块状态为 0，ping 与三个只读 API 串行返回 HTTP 200；本轮外部 CAN RX 为 0，不能作为外部 RX 验证。
- CANtest 现场状态已补充：开始发送前曾未观察到开发板数据；开始发送后连续 HTTP 读数为 `tx=53→68`、`rx=240→397`、`errors=0`、`tec=0`、`rec=0`、`busOff=0`、`sendResult=0`、`poll=52→67`。该结果作为当前外部 CAN RX 与周期 TX/ACK 证据，不表述为代码修复；本轮无源码修改。
- CANtest 后续确认：用户重启 CAN 接收软件后已实际看到开发板数据，说明此前未显示是接收软件的显示/会话状态，不是板端 TX 故障；此前 `tx=9→17`、`sendResult=0`、`errors=0` 与重启后的可见结果一致支持该判断。
- DbcTask 队列边界已烧录验证：新增深度 1 的 reload 命令队列；ST-Link 读数 `queue_ready=1/enqueue=1/drop=0`、`started=1/request=1/complete=1/last_result=0`，任务循环持续增长；`POST /api/dbc/active` 返回 200 且 `runtimeGeneration=2`，保留原 API 和 DBC 语义。
- CAN TX 队列边界已烧录验证：新增深度 1 的 `CanFrame` 队列；反汇编确认 CAN2 周期任务调用 `xQueueSend`，CanDecodeTask 路径调用 `xQueueReceive` 后再执行 `can_port_send`，实际发送成功才执行 TX self-test 解码。GDB 读数 `tx_queue_ready=1`，入队/出队由 `0x1a/0x1a` 增长到 `0x2c/0x2c`，丢弃为 0；同次 RX 队列为 `0x104/0x104`、丢弃为 0，CAN2 `tx=0x2d/rx=0x1b3/errors=0/sendResult=0`。网络顺序回归 `ping=2/2`，`/api/status`、`/api/can/status`、`/api/signals` 均 HTTP 200。
- 通用 ConfigTask 命令队列边界已烧录验证：新增深度 2、元素 20 字节的 `ConfigCommand` 队列；旧 `g_w25q128_diagnostic_request` 和 `g_rule_task_config_save_request` 只作为兼容入口，均先入队再由 ConfigTask 消费。历史一次 `0xffffffff/erase_count=0` 未能证明底层诊断执行；2026-07-13 用当前正式 ELF 精确地址重试后，命令 `enqueue/dequeue=1/1`、`last_command=1`、`diagnostic_count=1`、`diagnostic_result=0`、`erase_count=1`、`test_addr=0x00FFF000`、JEDEC=`0x00EF4018`，规则双槽 `config_addr=0x00FFE000/sequence=7` 不变，已闭环。

## 当前风险

- 后续继续拆分 TF/FatFs、QSPI、DBC 和配置任务时，共享资源必须加串行化或 mutex；当前 TfTask、W5500/HTTP、DBC、LogTask 共用既有 `fs_mutex`，W5500 状态/HTTP 两任务另共用 `g_w5500_mutex`。
- 当前 HTTP 服务仍是 socket0 单连接最小实现，不支持并发连接、目录映射、HTTP Range、分块传输编码或通用上传；当前只把 `/` 和 `/index.html` 映射到 `/www/index.html`，只支持 `POST /api/dbc/upload` 的 1024 字节以内单请求体 DBC 上传。
- DBC 上传当前生成候选文件 `/dbc/candidate.dbc`，旧候选保留为 `/dbc/candidate.prev.dbc`；当前 active 文件是本轮验证使用的 `0x321`/2 信号 DBC。运行态快照已接入 CAN2 接收服务、`SignalCache`、最多两项的只读实时信号 API、LogTask 和最小内置 RuleTask；RuleTask 额外保留默认关闭的 ST-Link 手动覆盖验收入口，ConfigTask 仅承载已验证的单规则 QSPI 双槽保存。
- 当前最小解码器在成功发送的 `0x321` 周期诊断帧上执行 TX self-test，也在外部 RX while-loop 上执行同一函数；两条路径使用独立 `SignalCache`，HTTP/Log/RuleTask 只消费外部 RX 缓存，且仍必须用独立来源计数区分验证。
- 当前固件保留候选scratch `DbcDatabase`、运行态双槽`DbcDatabase`、外部RX与TX self-test两个固定`SignalCache`、768 B日志缓冲和LogTask栈；一期最终RAM_D1=`242792 B/512 KB=46.31%`。后续扩大缓存或引入并发读者前必须继续复查内存并补齐同步边界。
- LogTask 由独立任务每100 ms调度；当前新实现只在`enabled=true`且RAM时间已同步时采样，按`samplePeriodMs=100..10000`采样，缓冲达到512 B或5 s后在`fs_mutex`下追加`/log/signal-v2.csv`。停止或未同步时清空内存缓冲，不向旧`/log/signal.csv`混写；不做时间或记录开关持久化、重试、轮换、下载API或并发HTTP。
- 旧CSV表头`updated_ms,key,value,raw,unit,quality`与其历史实体文件证据保持原样；新CSV使用`utc_time,unix_ms,updated_ms,key,value,raw,unit,quality`。本轮已完成`verify.sh=18/18`、最终ELF关键反汇编和OpenOCD烧录；首次构建的局部`enabled`告警已最小初始化修复并在随后构建中消失。网页、HTTP、TF记录启停及实体CSV已现场/只读验证；本次实体文件为`/Volumes/NO NAME/log/signal-v2.csv`、`28553 B`，首末记录均为`Can2Data.marker=42434`与`Can2Data.sequence=4660`且`quality=ok`。
- 2026-07-08 22:34 当前复查中 `/api/can/status` 可访问，但现场读数为 `rx=0/errors=487/tec=128/sendResult=1`；2026-07-08 22:46 用户打开 CAN 分析仪收发后复查恢复为 `sendResult=0`、`rx_count=508`、`tx_count=728`、`tec=0`、`bus_off=0`。
- `CONVERSATION_SUMMARY.md` 已经较长，但仍按用户要求保留为完整对话摘要；当前快照以本文件为准。
- macOS 串口曾出现乱码，硬件结论优先用 ST-Link 变量和外部工具确认。

## 下一步建议

- FAT 属性与会话文件需求已由实体只读检查闭合；后续若有新需求，仍不扩大到RTC、NTP、旧CSV改写或并发HTTP。ST-Link变量采样可在连接状态恢复后另行补取，但不是本次实体属性判定的必要条件。
- 新需求仅推进最小时间合同：先实现网页设置 RAM 时间基准及其只读状态，再实现默认关闭、候选频率`100..10000 ms`的独立时间日志；不得改写`/log/signal.csv`、增加日志下载、引入 NTP/RTC 或并发 HTTP。每一步均须重新构建、关键反汇编和现场证据后才可转为已验证。
- 仅在用户完成更新版`www`写入 TF 并上电后，按单 socket 串行约束从浏览器验收状态灯、TX/RX 区、折叠和 TX/RX DBC 表；再由 CANtest 明确确认新控制帧的外部接收。TX self-test `42434/4660`、RX 独立计数和 CAN errors=`0`均不能替代该外部接收证据。

一期无必做下一步。后续新需求必须重新定义阶段与验收，不从下列历史过程继续。

### 历史阶段推进记录（均已被后续验收更新）

阶段 C、D、E 已关闭；阶段 F 进行中。F-12 已完成当前固件的30分钟静态插卡日志耐久，原长跑子项现有通过证据。F-13 已只读固定 F-14 的网络断开/恢复现场协议：无需代码改动，实时链路以 `g_w5500_link_up/PHYCFGR bit0` 的 `1→0→1` 为准，启动配置字段不能替代它；HTTP 须以串行主机请求和 error 计数交叉确认。下一步 F-14 必须暂停等待用户只拔出 W5500 网线后确认，再按固定读数验证断网，之后再等待插回确认验证恢复；不得先行自行断网、重启、烧录、拔插 TF 或改变 CANtest。

F-14 当前暂停：已连续三次等待用户“已拔出”确认而未收到实际物理操作信号，故未执行任何断网或板端读取。此为外部条件阻断，不得把未形成断网状态写为通过或失败；收到确认后恢复固定流程。

F-14 断网子判定后来已由用户“已拔出网线”确认后通过：两次 `link_up=0/PHYCFGR=0xBA(bit0=0)`、W5500/HTTP 任务循环增长，主机 HTTP 超时且无响应。网络恢复子判定现暂停等待用户“已插回”；连续三次等待仍无确认，禁止自行插回、重启、烧录或把断网子判定写为完整恢复通过。

F-14 已完整通过：用户插回同一网线后，`link_up/PHY bit0` 两次为`1/1`，W5500/HTTP任务循环增长；ping=`2/2`、`/api/status`、`/api/can/status`、`/api/signals` 均200，HTTP request=`9→12`、error=`0`。该证据只覆盖物理网线断开恢复；阶段 F 仍待 CAN bus-off/恢复和复位后 DBC/规则/日志恢复。下一派送 F-15 只读审计当前 FDCAN2 bus-off、错误计数、恢复行为和可观测符号，形成固定现场验收协议；不得改代码、烧录、操作 CANtest 或自行制造 bus-off。

F-15 已固定 F-16 协议：当前 FDCAN2 为500 kbit/s、自动重传，运行时无显式 bus-off恢复代码。必须以 CANtest真正离线造成无ACK，且同时观察 HTTP `busOff=1` 与原始 `PSR.BO=1` 才能开始恢复验证；仅停止发送不成立。下一步 F-16 先等待用户使 CANtest停止发送并关闭/离线后确认；不得先行自行改CAN、断开开发板侧线、重启、烧录或改变TF。

F-16 已完成正常CAN基线，但暂停等待用户“已离线”：三次等待没有实际确认，故未制造bus-off。当前基线 `busOff/tec/rec=0`、`CCCR.DAR=0`、`PSR.BO=0`、CAN tx/rx/poll增长；收到确认后才按固定20秒窗口观测。该暂停是外部条件阻断，不能把正常基线写为bus-off恢复通过。

F-16 已在用户确认CANtest离线后得到受控失败边界：20秒无ACK使 TEC=`128`、错误增长且发送入FIFO失败，但 `busOff=0/PSR.BO=0`，故未形成 bus-off，不能验证恢复；同时HTTP连接重置，原因未定。现在暂停等待用户恢复 CANtest 500k、ACK与外部 `0x321` 后确认，才可读取错误被动后的收发恢复；连续三次等待未确认，禁止自行复位、烧录或模拟ACK。

F-16 用户恢复后 CAN 已从错误被动回到 `TEC=0/PSR.BO=0`，TX/RX/任务增长；但主机HTTP持续 `Recv failure: Connection reset by peer`，而板端 W5500 link/PHY/socket监听/任务与HTTP status/error仍表面正常，故完整CAN异常回归失败且不能归因。下一派送 F-17 只读审计 socket0 接收→发送→关闭链路和诊断缺口，固定最小修复假设；不得改代码、烧录、重启、访问板端网络或让用户操作。

F-17 已以最小 socket0 状态机修复并烧录通过：正常响应只发 `DISCON` 并等待后续 `CLOSED/INIT` 再监听，不再同轮强制 `CLOSE`。当前 CAN恢复后在每个短连接间留250ms，三轮 status/can/signals 共9次均HTTP200且无RST；最终 listener/任务/HTTP error/CAN错误均正常。当前HTTP为50ms单 socket轮询，零等待连续连接仍可能在重新监听窗口被拒绝，非并发服务。阶段 F 的 bus-off 本身仍未形成，且复位后DBC/规则/日志恢复仍待验；下一派送 F-18 只读审计冷/软件复位后各持久状态的现有证据与最小现场协议。

F-19 已通过第二次真实冷启动联合验收：用户保持 TF、网线和 CANtest 不变，开发板下电至少10秒再上电。启动后 `/api/status`、`/api/dbc/runtime`、`/api/rules`、`/api/can/status`、`/api/signals` 均为 HTTP 200；DBC=`151 B/3 lines/1 message/2 signals/errors=0`，v3 两槽规则保持默认语义，CAN `rx=1022/errors/busOff/tec/rec/sendResult=0`，外部 marker/sequence=`42434/4660`。ST-Link 为 DBC valid/result=`1/0`、v3 load_result=`0`、rule_count/read_len/size=`2/312/312`、RuleTask started=`1`；LogTask 默认 path、write/flush=`15/15→19/19`、文件 `0x9f648→0x9ff30`、failure/read_failure=`0/0`。F-20 随后通过错误响应现场时序：非法 `enabled=true` 正确返回400且规则不变，curl完成后新的 `/api/rules` 连接在 `+0/+50/+100/+250/+500 ms` 均200；最终 socket=`0x14`、pending/error=`0/0`、last code=`200`。这一证据只覆盖该独立短连接流程，仍不宣称并发服务。阶段 F 仍未形成真实 CAN bus-off。

F-21 只读审计完成：FDCAN2 为自动重传，软件约每秒提交一帧，而硬件 TX FIFO 只有4个元素；F-16 的 `TEC=128/PSR.EP=1/PSR.BO=0/LEC=ACK error/TXBRP=0xF` 客观证明已进入错误被动且四个请求未完成。无ACK继续等待或加快发送不会形成可验收的真实bus-off，不能用软件、loopback或调试器伪造。下一现场步骤仅允许让 CANtest normal active 以错误比特率产生物理时序错误，并且必须以 `PSR.BO=1` 与 HTTP `busOff=1` 同时判定；若仍只有ACK error/TEC=128，停止实验并记录 CANtest 不具备足够错误注入能力，不能改代码掩盖。

F-21 现场已形成并验证真实bus-off：错误比特率下 `PSR=0x7e7(BO=1)`、`ECR=0xfff8`、`IR=0x2b800801(BO bit25=1)`、HTTP `busOff=1`；用户恢复 CANtest normal active/500k/原帧后，观察窗口超过60秒（CAN poll `972→1107`）仍为 `CCCR=0x1001/ECR=0xfff8/PSR=0x7e7/TXBRP=0xF`，CANtest发送失败且无新信号。这是当前固件无恢复路径的客观失败，不能写为bus-off恢复通过。已派送 F-24 只读审计，唯一目标是定义可回退的最小 FDCAN2 STOP/START 恢复状态机；审计完成前不盲改源码。

F-65 已完成 W5500 去扰动稳定性回归：F59 动态 `RX_RSR/TX_FSR` 稳定读取、F61 时序观测保留；F63 的Socket0快照诊断已删除，反汇编确认 `w5500_http_status_poll` 栈帧恢复268B。最新正式候选经 OpenOCD `Verified OK` 后，以257/271/283/307ms间隔完成五个独立20次 `/api/status` 压力轮，均为20/20；压力后 `/api/status`、`/api/can/status`、`/api/signals`、`/api/dbc/runtime`、`/api/rules` 均200，ping=2/2。此前F61曾复现的延迟根因仍未单独证明，但该候选已满足当前单连接稳定性验收，不把它表述为并发HTTP能力。

G-1 当前映像的联合耐久尚未通过：首次60秒空闲后的单GET曾在3秒无字节超时。随后F-67用户抓包中，同一“空闲后单GET”未复现超时，却量得GET后约202.573 ms才收到板端纯ACK、约2.204246 s才收到HTTP响应头，最终557 B body与FIN正常且无RST。该成功样本证明链路并非每次丢失请求，同时暴露接近客户端超时阈值的服务延迟；F-69已烧录最小端到端tick观测，覆盖poll间隔、SR/IR/RSR、请求RX读取、RX消费、status body及header SEND/SENDOK，并在连接结束输出独立短UART行。当前映像尚未用该字段完成用户同步抓包，不得将F65短连接压力结果扩大为G-1通过，也不得把任何候选路径写为根因。

F-25 已在 `can_bringup.c` 实现并烧录最小 bus-off 恢复：持续 BO 时按 1 秒限流逐位 Abort `TXBRP`，再 Stop，Stop 成功才 Start；不执行 DeInit/Init，不改变位率、过滤器、任务周期或队列。`verify.sh`/CTest=`14/14`、定向反汇编、OpenOCD `Verified OK` 均已完成。正常500k基线外部 RX=320、signals=42434/4660；错误250k现场形成真实 BO 后 `attempt=44/result=0`、`CCCR.INIT=0/PSR.BO=0/TXBRP=0`；恢复500k后外部 RX=`1305→2292`、TEC=`95→0`、最终 CAN tx/rx=`352/2292`、errors/busOff/sendResult=0，用户确认信号正常收发。F-26 已完成：正常外部 CAN 与两次 LogTask/TF 计数增长后完全下电取卡，主机只读 `/log/signal.csv` 为`971532 B`、16975行、唯一表头、零字段错误、7024组 marker=`42434`/sequence=`4660` 同时间戳 `quality=ok` 记录，尾部完整。其大小高于最近板端 `968052 B` 3480 B，符合人工下电间隔继续日志的完整追加；绝对相等无法与人工断电原子采样，验收采用“不小于最近读数且完整成对尾部”规则。下一固定阶段为最终全量复验。

F-69 已完成最小端到端HTTP时序观测并烧录验收：空闲60秒后的重采样中，GET到HTTP header=`6.677 ms`、到body=`10.329 ms`、curl首字节/总时长=`11.275/15.011 ms`，无RST或重传；完成态串口trace显示轮询、RX读取、请求读取、RECV、body构造和header进入均在tick `181468`，SEND已写入与SENDOK均为`181469`。这证明F-69能在连接完成后给出SEND阶段的时序，且本次未见长延迟；它不推翻F-67约2.20秒样本，G-1仍未通过，下一阶段必须单独复现或排除间歇延迟。

F-71 固定为不改固件的间歇延迟复现/分类：最多10个独立“严格60秒无TCP/80→唯一GET”轮次，CANtest保持500k持续发送；每轮保存pcap、curl和请求后5秒的完成态`[http-trace]`。任一轮GET到HTTP header为`>=2 s`、3秒无header、RST/非200或trace不完整即停止并保留证据；10轮均短只能写为本预算未复现，不能写为G-1或HTTP稳定通过。

F-71 r01-r04均为有效短样本：`L_header`分别为`30.083/19.560/41.439/43.005 ms`，均为12包的单GET/200/正常四次挥手，无RST或重传；curl总时长已确认r01-r03为`38.287/27.302/49.498 ms`，r04首字节为`42.816 ms`；完成态trace分别为`seq=2/3/4/5`。r04相邻状态的CAN2 RX=`23638→23670`增长且errors/bus-off/TEC/REC均为0。四轮均未复现F67，但样本数不足，不能说明稳定、根因消失或G-1通过；下一步仅执行r05，同一异常停止条件保持不变。

F-71 r05已复现F67类故障，阶段停止：严格60秒空闲后的唯一GET在pcap时间`.717197`发出，板端仅于`.919804`发出零长度ACK，3秒内始终无HTTP header/body/RST；客户端于`.716710`发送FIN，curl报告`Operation timed out after 3005 milliseconds with 0 bytes received`。COMtool在请求后继续保持`hreq=5/htseq=5`且无`seq=6`，`hclose=0000011c`；不能把该状态解释为已处理或根因。r05抓包已正常停止并保存，禁止r06-r10；下一步必须先完成只读F71b路径审计，再基于证据制定最小诊断/修复、构建/反汇编/烧录复验。

F-72最小预诊断已烧录并完成F72-r01..r10固定十轮：全部为单GET/HTTP200/完整body/无RST或重传，`L_header=5.555..178.756 ms`均小于0.5秒，完成trace与`hps=0`均齐全。因此只可写为“该预算未复现F71-r05”，F72异常锁存未覆盖，严禁写为稳定、根因消失或G-1通过。F-73只读审计已固定下一最小F74：只添加HTTP任务`poll gap/mutex wait/poll exec`最大时序观测，和“无F69 trace连接到CLOSE_WAIT”的一次性SR/IR/RX_RSR冻结快照；不得新增W5500操作、重试、延时、优先级或状态机。F74须先实现、构建、反汇编、烧录；随后仅以同一60秒空闲单GET协议等待复现分类，未复现不得提交根因结论。

F-74已实现并烧录，但尚未覆盖目标异常分支：HTTP任务现输出`hpmg/hpmw/hpme`启动以来最大值；一个无F69 trace而最终到CLOSE_WAIT的连接会一次性冻结`hnseq/hnmask/hnsr/hnirr/hnir/hnrr/hnr/hngap`，且只复用既有SR/IR/RX_RSR读取结果，不增加W5500写命令、SPI读取、重试、延时、优先级或状态机。由于2048 B UART行缓冲最初会令MonitorTask调用栈过窄，已移为函数内静态BSS；反汇编栈帧为1060 B，MonitorTask仍为4096 B。`verify.sh`/CTest=14/14、关键反汇编及OpenOCD `Verified OK`均已完成；F74-r03为首个有效唯一会话：60秒空闲后唯一GET=`01:53:36.427350`，HTTP200 header=`.473814`，`L_header=46.464 ms`，750 B body和四次挥手完整，无RST/重传；curl=`200/total=56.846 ms`。同轮COMtool为`hreq=4/htseq=4/htact=0/herr=0`、`hps=0`、`hnseq=0`、`hpmg=57/hpmw=0/hpme=7`，CAN2 RX继续增长、CAN错误为0。它确认F74正常路径无误冻结并完成“有效唯一会话分类”前置，但没有命中历史r05异常，不能宣布根因消失、G-1通过或异常分支已验收；F75可在保持严格顺序HTTP限制下进入源码实现，F74诊断源码暂不单独提交。

一期范围已新增F-75 Web控制台：页面可见时每1000 ms严格串行刷新CAN状态再刷新signals，两个响应之间均留至少250 ms重监听窗口；规则设置使用既有两槽`/api/rules` CRUD；继电器操作新增唯一`GET/POST /api/relay/manual`，完整`enabled/relay1/relay2`覆盖快照在短临界区写入、RuleTask应用后以request/applied序号确认并最终写GPIO，关闭覆盖恢复规则。页面为TF驻留原生HTML/CSS/JS，不使用并发、第三方资源或高频轮询；F74-r03已完成有效唯一会话分类，现可进入F-75源码实现；TF实际页面部署仍必须由用户下电后取卡、覆盖文件、插回上电，再由浏览器/pcap/API/GPIO联合验收。

F-75源码已完成、未烧录：新增可追溯`www/index.html`单文件资产（内嵌CSS/JS、无第三方资源），以及`GET/POST /api/relay/manual`与最小`ManualRelayState`主机测试；RuleTask仍经`rule_apply_relays()`唯一写GPIO，HTTP只短临界区提交/读取完整二值快照并至多等待100 ms应用序号。主会话已独立完成`git diff --check`、`./scripts/verify.sh`和再次`ctest`，15/15通过；当前ELF的`text/data/bss=91520/372/242104`。反汇编确认POST仅调用`rule_task_manual_override_submit`和snapshot、RuleTask在`rule_engine_evaluate`后调用`rule_apply_relays`才更新applied序号，PE7/PE8写入仍只在该函数。下一步必须烧录该ELF并取得API/RuleTask现场证据；用户下电部署TF页面前不得宣称Web已交付。

F74-r02的curl虽为200（首字节15.734 ms），但抓包终端实际报`sudo: a password is required`、pcap不存在，故该轮无效且不作网络稳定性结论；下一轮必须先在终端显示`listening on en2`后才允许唯一GET。

F-75一期Web/手动继电器源码已完成、未烧录：可追溯TF部署源为仓库`www/index.html`（10615 B，需用户下电取卡覆盖到TF的`/www/index.html`后再插回上电）；页面只使用内嵌CSS/JS，含概览/DBC、可见时每1000 ms严格串行`/api/can/status→250 ms→/api/signals→250 ms`、两槽规则CRUD和手动继电器。新增`GET/POST /api/relay/manual`；POST严格要求三个完整`0|1` form字段，在`main.c`短临界区提交手动状态/requestSeq，RuleTask完成既有`rule_apply_relays()`后回写appliedSeq，HTTP最多等100 ms且不直接写GPIO。主机CTest现为15/15（新增`manual_relay`验证0|1、应用序号及绕过0）；最终`verify.sh`已完成STM32 ELF链接，FLASH=`91904 B / 128 KB=70.12%`、RAM_D1=`242480 B / 512 KB=46.25%`。本阶段尚未执行反汇编、OpenOCD烧录、TF部署、浏览器/pcap/CANtest/GPIO验收，不能写成已交付或提交。

## 2026-07-23 阶段14外部条件阻断

- 最新候选的代码、构建与烧录事实已经分别记录，但它们不构成网页、外部RX、规则持久回读或继电器的运行时验收。
- 用户需将最新`www/index.html`写入TF、上电并让CANtest发送；该同一外部条件已连续三次未收到明确确认。当前阶段按治理状态标记为等待现场条件（blocked），不据此推断功能失败或通过。
- 收到用户明确确认“已写TF、已上电、CANtest已发送”后，恢复单socket严格顺序的现场复测；此前不改动源码、网页、CMake或验收文档，也不把候选证据扩写为验收结论。

## 阶段 C 实际快照

阶段 F 更新：F-8 关中断实验已失败并撤回；下一派送 F-9 只以 `vTaskSuspendAll/xTaskResumeAll` 保留 SysTick、抑制任务切换，比较同样的断电冷启动首错。不得改 DMA、timeout、块参数、重试、remount、热插拔或恢复策略。

- 最终构建 `./scripts/verify.sh`、CTest=`14/14`、`git diff --check` 均通过；ELF 为 `build/stm32h750/can_bus_gateway_stm32h750.elf`，FLASH=`85496 B / 128 KB = 65.23%`，RAM_D1=`239664 B / 512 KB = 45.71%`。反汇编确认 v3 parser 的严格校验，ConfigTask type=3 的 TF 原子替换和 RuleTask 3856 B engine 临界复制；修复后 `rule_file_v3_build_engine()` 栈帧仅 120 B，POST 表单 `slot=2` 的 404 分支也已反汇编及实板验证。
- OpenOCD/ST-Link V2 修复版烧录输出 `Programming Finished`、`Verified OK`、`Resetting Target`，电压 `3.250368 V`。首次版本 DELETE 成功后发现 `rule_file_v3_build_engine()` 的 3856 B 自动对象覆盖 ConfigTask 相邻 TCB，CFSR=`0x8200`、BFAR=`0xc4108948`、PC 位于 `xTaskIncrementTick`；该真实失败已修复并重新烧录，不作为验收成功证据。
- 修复版真实 HTTP：初始 v2 两槽 GET 200；DELETE slot1 后 GET source=v3/slot1 disabled，GDB `generation=4/rule_count=1`；POST slot1 恢复为 201，GDB `generation=5/rule_count=2`；PUT slot0 为 `42436/1100/1600` 后复位仍为相同 v3 数据；PUT 恢复默认 `42434/1000/1500`；非法 `delayMs=1601/timeoutMs=1600` 返回 400，generation 保持 `3`、current 未变。最终 GET 两槽均 enabled 且参数为阶段 B 默认；ping 2/2，`/api/status`、`/api/can/status`、`/api/signals` 均 200，CAN 状态 errors/busOff/TEC/REC/sendResult 均为 0。本轮无 CANtest 输入，signals 空且不作为 CAN RX 验收。

## 阶段 B 实际快照

- `./scripts/verify.sh` 通过，CTest `14/14`；FLASH=`80192 B / 128 KB = 61.18%`，RAM_D1=`235048 B / 512 KB = 44.83%`。ELF 已确认 `rule_file_parse_v2`、完整 `RuleEngine` 候选复制/reload、priority winner、manual/timeout 分支和 `FA_CREATE_NEW` v2 创建路径。
- 独立 OpenOCD 烧录输出 `Programming Finished`、`Verified OK`、`Resetting Target`，目标电压 `3.251976 V`。验证结束后发现 PID `41322` 仍监听 3333/6666，已通过 4444 的 `shutdown` 命令释放；随后 `pgrep`/`lsof` 均无 OpenOCD 或 3333/6666 监听。此前“已释放”的早期记录更正为不准确。
- 有效 v2 板端读数：`g_rule_file_v2_load_result=0`、`created=0`、`size/read_len=280/280`、`rule_count=2`；RuleTask `generation=2/reload=0/rule_count=2/winner_rule0=0`；真实 marker=42434 下 PE7=`1`、PE8=`0`、GPIOE ODR=`0x80`、safe=`0`。
- 手动覆盖现场：GDB 精确地址写入既有手动变量后读到 `manual_enabled=1/manual_active=1`、winner 两路=`0xff`、PE7=`1`、GPIOE=`0x80`；随后已清除手动并 resume。每次 halt 后均执行 `monitor resume`。
- 板上本次启动已有 v2 文件，未观察到 `v2_created=1/load_result=1` 缺失创建现场；代码路径已修正为仅 `load_result==0` 停止 fallback，创建/无效/超限/读取失败继续 v1/QSPI，诊断变量可区分“创建”与“已加载”。
- 补充现场完成：marker=42435 下外部 RX raw=`42435`、`rule_count=2`、`winner_rule0=1`、PE7/PE8=`0/0`；停帧后 `uwTick-updated_ms=49114 ms > 1500 ms`、`safe_active=1`、PE7/PE8=`0/0`、manual=`0/0`。首次 v2 缺失创建仍为“未观察”，不得写成板端已实测。

## 本轮已完成：单规则 HTTP 配置闭环

- 新增 `GET /api/rule/config` 与 `POST /api/rule/config`，仅控制已有单规则四个参数；有效 POST 经 ConfigTask 深度 2 队列执行 QSPI 双槽保存，成功后 RuleTask reload，HTTP 等待完成后返回 200。
- 实机顺序证据：重新烧录 Verify 通过；ping 2/2；GET 初始 `42434/42432/1000/1500/generation=1`；POST `42435/42433/1100/1600` 返回 200、generation=2；GDB 读到 QSPI save result/count=`0/1`、ConfigTask enqueue/dequeue/drop=`1/1/0`、RuleTask generation/reload=`2/0`。
- 复位后仍加载 `42435/42433/1100/1600`，`config_load_result=0`、RuleTask generation/load=`1/1`；随后通过 HTTP 已恢复默认 `42434/42432/1000/1500`，`/api/can/status` 仍为 200 且错误、bus-off、TEC、REC、sendResult 均为 0。
- 本功能仍不是规则文件、多规则或完整 CRUD；本轮未人为破坏 TF 文件，LogTask recovery 仍未验证。

## 2026-07-16 F-75 严格修复已烧录，TF页面验收待进行

- `www/index.html`首次可见已自动启动既有严格串行CAN刷新，隐藏时暂停、重新可见时仅恢复此前自动暂停的刷新；用户手动停止或请求失败仍保持停止。`http_form_parse_manual_override()`已收紧为每个`enabled/relay1/relay2`值必须精确单字符`0`或`1`，拒绝`00/01/10`而不改变规则表单的多位数字解析。
- 修复后`git diff --check`与`./scripts/verify.sh`通过，host CTest=15/15；最终ELF反汇编确认RuleTask仍经唯一`rule_apply_relays()`写PE7/PE8，手动解析器先要求值跨度1再限制字符转换结果为0或1。OpenOCD烧录输出`Programming Finished/Verified OK/Resetting Target`（`3.249799 V`）。
- 复位后的严格映像现场顺序API为：初始GET返回`requestSeq/appliedSeq=0/0`；合法`enabled=1&relay1=1&relay2=0`返回200与`1/1`，后续GET保持一致；非法`enabled=01&relay1=1&relay2=0`返回400 `invalid_manual_override`；关闭覆盖POST返回200与`2/2`。COMtool新清空窗口从`rtc=206`开始，至`rtc=231`任务均持续、CAN2 RX=`2257→2532`、CAN错误/bus-off/TEC/REC=0，末态`hreq=5/hpath=10/hcode=200/htseq=5/htact=0/herr=0`。
- 此证据闭合新手动HTTP严格值与RuleTask交接，但`www/index.html`尚未写入实际TF，故浏览器自动CAN刷新、规则UI和页面继电器操作还未验收，不能提交或写成一期Web已交付。下一步必须由用户下电取TF卡，将仓库`www/index.html`覆盖为卡内`/www/index.html`，插回上电后再进行浏览器、pcap、CANtest和GPIO联合验证。

- 用户已在下电状态将TF插入读卡器；主会话确认卡为`disk4s1`/`/Volumes/NO NAME`，用SHA-256和`cmp`逐字节核对后已把仓库`www/index.html`（10935 B，`d15e7c...96bdc7`）覆盖到卡内`/www/index.html`，并已成功`diskutil unmount disk4s1`。页面资产部署完成但尚未插回上电，因此TF服务、浏览器、pcap、CAN刷新、规则UI、页面继电器和GPIO联合验收仍未完成，不能提交。

- 用户已插回上电后的实际冷启动验证：TF状态0、`GET /`为200/10935 B且哈希与仓库`www/index.html`完全一致，证实板端静态服务已读到新页面。实际浏览器首次可见自动启动CAN刷新；用户恢复CANtest后页面实际显示RX=240和marker/sequence两项`quality=ok`。这只闭合TF部署、页面加载与自动CAN显示，不包括严格顺序pcap、规则UI读写、页面继电器POST/GPIO读数；自动页面已手动停止等待独立抓包，仍不可提交。

- F-75独立浏览器pcap发现首页连接竞态：静态`GET /`最终ACK后3.797ms的首个无HTTP负载SYN被RST；首个成功`/api/can/status`距该关闭约259ms，随后24组`can/status→signals`均顺序200且规则/手动只读页面操作也实际200。另有两次无HTTP负载RST，故该抓包不能写成无RST通过。为只处理已证实的首启窗口，`www/index.html`首次自动启动现延后300ms，并使停止/隐藏取消该定时器、恢复可见保持自动启动语义；`verify.sh`/CTest=15/15、关键ELF反汇编和同HEX OpenOCD `Verified OK`（3.268051V）已完成。TF卡内仍为修复前页面，下一步必须由用户下电更新HTML、插回上电并重新抓包；在此之前F-75不可提交。

- F-75的300 ms首启修复页面已重新部署到TF：源与卡内`/www/index.html`均为`11143 B`、SHA-256=`2ed23b7fe6d1047b897d62bb8b6aa6376e4c1e6d90c5c7d4ff11918fc99117da`、`cmp=0`，`disk4s1`已安全卸载。当前开发板仍处于断电且未插回TF的外部等待状态；下一步固定为用户插回并上电，先验证板端`GET /`内容哈希和`tf.status=0`，再启动新的pcap并只重载页面一次，要求静态连接结束至首API SYN至少300 ms、CAN/status与signals均200且整个窗口RST=0，之后才进入规则/继电器页面写入与GPIO联合验收。

- F-75过程记录：300 ms首启修复已完成板端与pcap验收；当时尚待规则与手动GPIO联合验证。该待办已由下方“F-75一期Web核心三项客观验收完成”更新并关闭。

## 2026-07-16 F-75 一期Web核心三项客观验收完成

- 本阶段验收边界以用户明确要求的三项为准：CAN数据刷新显示、规则设置、继电器操作。浏览器规则页已将slot1 threshold从`42435`临时改为`42436`，保存并重新读取确认，再恢复为`42435`并回读；其余字段保持原值，最终`GET /api/rules`确认两槽原始配置。
- 浏览器手动继电器第二次实际提交`enabled=1/relay1=0/relay2=1`成功，页面显示RuleTask确认请求1，API为`requestSeq/appliedSeq=1/1`、实际输出`0/1`；按当前ELF精确地址停机读取后，RuleTask快照与GPIOE ODR均为`0x100`（PE8高、PE7低）。随后浏览器关闭覆盖，页面显示请求2，API为`2/2`；最终ODR=`0x80`（PE7高、PE8低），证明恢复自动规则而不是固定输出。每次OpenOCD读取后均显式resume/shutdown，最终3333/4444/6666无监听。
- 第一次浏览器手动提交期间，板端串口已记录HTTP handler `hcode=200`且任务/CAN持续运行，但浏览器显示`Failed to fetch`，随后主机ping/curl暂时失败；一次明确`reset run`后网络、规则持久化和手动安全态恢复，第二次页面操作完整通过。该异常必须与无RST的CAN刷新pcap分开：F-75三项功能闭环通过，但不能据此宣称HTTP长期稳定或F-71间歇问题消失。
- DBC页面区域继续复用已验证的upload/active/runtime API，但本次没有通过浏览器重新执行DBC上传/激活；不把既有API证据冒充本次页面操作。下一阶段固定为F-76，只复现、定位并修复“服务端记录200但浏览器未收到响应且网络需复位恢复”的单socket响应交付异常，禁止扩展Web功能。

## 2026-07-16 F-76 已关闭，当前进入阶段G

- F-76最终源码以`Sn_TX_FSR`非阻塞等待响应ACK，并在ACK wait期间遇到`CLOSE_WAIT`时调用既有graceful `DISCON`。最终ELF/HEX SHA-256=`26b63222463e9c0cd31d55e5dc40b0ad1c86e2d2ca6d10a8f9b3d0f276b34bc5`/`d1ef3838757beb8478aea6413eb3b12c5f9fdf41f5196b96bfd2d7e8ddb7968c`；CTest=`15/15`，反汇编、OpenOCD Verified OK和精确符号读回均已通过。
- 最终pcap为7653 B、SHA-256=`77a1ed949fb374641968b5d38e9a744e75057bfc7c9c7fa647520d01e418ad6e`、63包/5连接。两条manual POST和manual/status/CAN三个GET全部完整HTTP200、Content-Length匹配、请求/响应ACK、双方FIN最终确认；HTTP数据重传=0、RST=0。关闭覆盖后RuleTask序号=`2/2`、实际输出=`1/0`，ACK wait timeout和W5500 recovery均为0。
- 当前无外部阻断，固定阶段为G。下一步先只读建立最终验收矩阵并最大化复用仍有效的高成本证据，只把明确缺口合并到最少人工现场窗口；未经矩阵明确，不要求用户操作CANtest、TF、网线或上下电，不新增功能。

## 2026-07-17 G-1 最终提交同映像正常联合烟雾通过

- 用户确认CANtest 500 kbit/s持续发送后，当前HEAD=`1e31213aa414c3bb11ede79e251ab02a37c58311`通过`verify.sh`/CTest 15/15；ELF/HEX哈希保持F-76已烧录值，HTTP ACK/CLOSE_WAIT、LogTask、RuleFile v3 120 B栈帧和CAN bus-off Abort→Stop→Start关键反汇编均通过。
- 初次OpenOCD预读误含`reset run`造成一次复位；主会话没有混用复位前后计数，而是从复位后Snapshot A重新执行完整窗口。复位后19个串行HTTP连接的状态码与Content-Length全部匹配，DBC 151 B上传/激活、runtime generation `1→2`、signals `42434/4660`、Web 11143 B哈希、规则`42435→42436→42435`恢复、400/404和manual安全态均通过。
- Snapshot A→B：LogTask sample/write/flush=`101/20/20→281/56/56`，文件=`15326272→15346678 B`；CAN TX/RX与DBC RX=`103/1017/1017→284/2811/2811`。日志failure、TF read failure、CAN error/busOff/TEC/REC/sendResult、DBC decode error、RX/TX queue drop全为0。
- HTTP request=`0→19`，socket最终`0x14(LISTEN)`，HTTP error、ACK timeout、W5500 recovery为0，ACK pending=0；规则generation=`2→4`且v3两槽完全恢复。GDB读取后已resume，OpenOCD/GDB和调试端口已释放；最终ping 2/2与status HTTP200确认继续运行。G-1判定PASS，下一固定任务为G-2安全500协议审计，不重复长跑、拔线、bus-off、取卡或冷启动故障。

## 2026-07-17 G-2 安全HTTP 500现场样本通过

- 只读审计遍历现有500分支，排除TF保存/读取、规则save/reload、兼容配置和manual超时等会扩大副作用的候选；唯一采用规则写handler最前置`rules_source_unavailable`分支，它在candidate、body解析、save_request和任何文件操作之前。
- 实际RAM原值为v3/v2 load result=`0/0xffffffff`。仅临时把v3改为1，发送12 B故意非法body`enabled=true`；命中完整HTTP500、Content-Length=98，错误码=`rules_source_unavailable`，body SHA-256=`85dc32d8f7ddc22a80edfe89d9a461fd5c545e6284e9e575df565f1ea6209a22`。若注入未生效，该body只会400，不会保存。
- v3立即恢复为0后，同一请求返回400 `invalid_rule`。规则正文前后SHA-256均为`85fcd21ea3482d8a6888ec06cf495346b3c4a62d72bb545a7c4dba2bdd824e9d`；v3/v2=`0/0xffffffff`、save request/result=`0/0`、generation=4、socket=LISTEN，HTTP error/ACK timeout/W5500 recovery全0。
- 最终CAN tx/rx=`1056/10468`、所有错误0，RTOS/W5500/TF/QSPI正常，ping 2/2、status HTTP200。所有halt都没有reset，已resume/shutdown并释放OCD/GDB。G-2判定PASS；下一固定阶段G-3只做最终发布完整性审计和治理封口。

## 2026-07-17 G-3 一期最终发布通过

- `0ef7d3e1bf5bd5eecffd1f2f0c912fbe3e230304..HEAD`仅有治理Markdown变化；最终ELF/HEX哈希保持`26b632...34bc5`/`d1ef383...968c`。G-3复用G-1的CTest 15/15构建证据，本轮只核对现有产物的size、哈希和四条关键反汇编路径，未重新构建。
- G-3逐域复核F-76、G-1、G-2以及可复用的长跑、网线、冷启动、bus-off、TF实体CSV、QSPI/规则证据，全部一期验收域PASS，不需要重复现场操作或烧录。
- 旧“部分验证/阶段F进行中/外部RX未验证/队列未建立”只属于历史过程或过时当前描述，本次已在计划、验收合同、Feature索引、架构和当前状态区统一修正。明确非目标继续作为边界，不计为缺口。
- 治理封口提交`fe2154c`已推送；推送后工作树干净，本地与远端ahead/behind=`0/0`，发布完整性转为PASS。

## 2026-07-23 网页 CAN 发送控制首轮、修复部署与重入回归

- 用户已部署网页、上电并确认CANtest正在发送。首轮网页显示绿灯、TX/RX累计；页面停发再恢复后显示标准ID `0x321`、DLC=`4`、`C2 A5 34 12 00 00 00 00`、周期`1000 ms`。TX/RX DBC表为 marker/sequence=`42434/4660`，重复自动刷新期间计数与时间戳持续增长。此结果只证明首轮页面功能和外部RX显示，不替代CANtest接收受控TX帧的证据。
- 页面重入后端口80连续5次连接失败，而ping=3/3。GDB当时为socket0=`0x17 (ESTABLISHED)`、HTTP status/error=`0/0`、requestCount=`403`、`RX_RSR=0`、HTTP任务tick仍增长；网页端到端验收因此未通过。
- 随后仅修改`www/index.html`恢复第4个请求后的250ms收尾等待；`node --check`与`git diff --check`通过。用户随后已将该网页写TF并上电，自动刷新样本为TX/RX=`120/273→134/417`，两次reload重入样本为`145/527`、`162/694`，均无连接拒绝；四个details默认折叠、灯为`status-lamp ok`、控制台warn/error为空。原先ESTABLISHED且无RX失败样本未复现，故不再列为当前阻断或既定根因。
- 网页可逆关闭再恢复发送后，最终受控配置为`0x321`/DLC4/`C2 A5 34 12 00 00 00 00`/1000ms，`requestSeq=appliedSeq`且result=0。TX self-test和用户确认CANtest正在发送的外部RX两张DBC表均显示`42434/4660/ok`，外部RX随后增长。该结论不声称已直接从CANtest接收显示屏读到本轮控制帧。

## 2026-07-26 安全审查 P0 整改与板端故障注入状态

- 当前实际工作目录为`/Users/elvin/Desktop/project/can_bus_W5500`，分支`codex/W5500`。本轮已完成源码、host CTest、固件链接/反汇编、ST-Link烧录和有限故障注入；当前板端候选ELF/HEX SHA-256=`a32adce3c188bf859adaf36bd8c7326ed7b76c0aee0403cf4e0f6ba9fbe14fef`/`4bb09f44080ad7bf8db1ddca8b6f358bd9da484b229388feb986cbfc8165f5ad`。
- 已完成静态整改：FDCAN2 FIFO0 新帧/满/丢失 IRQ 通知与诊断计数、任务健康门控 IWDG、`.noinit` fault record 与复位、标准 CAN/DLC/信号布局 DBC 校验、SignalCache stale 返回计数、TF `f_sync()`。
- 已通过`./scripts/verify.sh`：CTest=`19/19`，最终 STM32H750 ELF=`text/data/bss 112596/768/243932`，FLASH=`113376 B`。最终反汇编已核对 CAN 通知无二次 50 ms 延迟、FDCAN2 IRQ 回调、IWDG条件刷新、IWDG正确启动顺序及 fault record复制后的D-Cache Clean/reset路径。
- IWDG实验室卡死注入已实际导致复位：复位快照`0x04460000`包含IWDG1 reset flag，且任务恢复；生产映像正常3秒窗口中refresh=`32->35`、CAN loop=`1086->1182`、unhealthy=`0`，IWDG=`PR=6/RLR=1000/SR=0`。受控跳转HardFault handler后，RAM_D3的17字fault record在复位前后完全一致且checksum有效；该受控跳转不能替代真实硬件异常堆栈证据。
- 暂停式GDB高负载首测曾显示FDCAN FIFO `full=3->5`、`lost=4->6`和RX队列`drop=15->25`；后续确认该读数方法会暂停内核，而外部约1 kfps输入会在暂停期填满FIFO并在恢复时突发drain到软件队列，故不得作为正常运行丢帧结论。已改用OpenOCD telnet `mdw`非停机读取：在同一外部输入下C→D→E两个连续20秒窗口FIFO `full/lost=2/2->2/2->2/2`、RX queue drop=`0->0->0`，RX入队=`117033->140962->167987`、出队=`117030->140957->167983`、DBC匹配=`614->653->696`，last ID始终`0x321`，因此当前候选的FDCAN高负载运行态通过。TF物理掉电恢复仍待现场试验；真实硬件fault的根因栈也未采集。Cache/DMA当前数据路径未发现DMA传输，但fault record已证明D-Cache开启时需要显式Clean。LAN写授权和CAN TX白名单需要明确生产CAN合同与授权来源后才能实现。

- P0 补充核对：最终 ELF 的`BSP_SD_*_DMA`兼容入口实际调用阻塞`HAL_SD_ReadBlocks/WriteBlocks`，W5500也使用阻塞 SPI；当前应用没有 D-Cache enable。因而不存在当前活跃 DMA/cache 未一致性路径，但任何未来 DMA、D-Cache 或 Ethernet 重新纳入构建的变更必须同时补齐 RAM 区域和 cache 维护验证。`f_sync()`已进入日志 append 成功路径，物理掉电恢复仍未验证。

- 剩余 P0 现场验收已固化到`docs/P0_FAULT_INJECTION.md`：FDCAN外部高负载已取得非停机原始读数并通过，当前仅TF物理断电需要继续取得原始读数。IWDG卡死复位及Crash Dump保持链路已执行并记录；真实硬件fault的根因栈属于更高强度补验，不能用受控handler跳转替代。

- 2026-07-26已完成 IWDG 正向板端验证、IWDG受控卡死复位和Crash Dump D-Cache保持修复。历史`95ac.../d6bf...`候选仅保留为前一轮IWDG正向证据；当前验收必须使用本节开头的`37ede.../3336...`最终生产映像哈希，不得混用。

- 高负载首次缓冲候选`FIFO16 + 32 x CanFrame`已被否定：增加约1920 B FreeRTOS heap后，最后rule任务创建失败，目标停在`bringup_default_task`，不能用于验收。已回退完整队列扩容并烧录稳定的FIFO16中间映像（ELF/HEX=`69eac...f59a5`/`8e9ab...02945`）；其CAN/Decode任务重新运行，但实际RX queue drop仍增长，故仅FIFO16不足。
- `FIFO16 + 32 x Can2RxQueueFrame`候选已烧录：紧凑元素仅携带classic CAN所需`id/IDE/DLC/8字节数据`，32项约512 B，小于原8项完整队列约640 B，DecodeTask取出后重建既有`CanFrame`。`./scripts/verify.sh` CTest=`19/19`、关键反汇编和`git diff --check`通过；候选ELF/HEX=`a32adce3c188bf859adaf36bd8c7326ed7b76c0aee0403cf4e0f6ba9fbe14fef`/`4bb09f44080ad7bf8db1ddca8b6f358bd9da484b229388feb986cbfc8165f5ad`。OpenOCD得到`Programming Finished`、`Verified OK`、`Resetting Target`。随后外部高负载在两段连续非停机20秒窗口内FIFO full/lost和RX drop都无增长，且RX/DBC计数持续增长，已闭合该候选的启动与运行态验收。
