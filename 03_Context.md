# 当前上下文

更新时间：2026-07-14（阶段 F-11 已完成 HWFC 单字段烧录与短时板端验证；阶段 F 的 30 分钟静态耐久仍待验；后续阶段与最终验收见 `PROJECT_FINAL_ACCEPTANCE.md`）

## 当前仓库

- 工作目录：`/Users/elvin/Desktop/project/can_bus`，当前实际解析到 `/Users/elvin/Desktop/project/can_bus_W5500`。
- 当前分支：`codex/W5500`。
- 当前主线：W5500/SPI2 + FDCAN2/MCP2562FD + TF 卡 + W25Q128 + FreeRTOS。

## 已验证状态

| 模块 | 状态 | 证据摘要 |
| --- | --- | --- |
| W5500 | [客观已验证] | `VERSIONR=0x04`，静态 IP `192.168.1.88`，主机 ping 通过 |
| W5500 HTTP/API | [客观已验证] | 已烧录验证 `GET /api/status`、`GET /api/can/status`、`POST /api/dbc/upload` 返回 `HTTP/1.1 200 OK` JSON；未知路径返回 404 JSON |
| 实时信号 API | [客观已验证] | `GET /api/signals` 已烧录验证返回最多两项 SignalCache 快照，含 key/value/raw/unit/updated_ms/quality；持续 CANtest 下返回两个已解码信号 |
| TF CSV 最小落盘 | [客观已验证] | 旧 bring-up 1 秒循环版本已验证：写入次数 `18→42`、文件大小 `4334→7070`、结果持续为 0；该直接写路径已被本轮 LogTask 源码替换 |
| 最小 LogTask | [默认路径客观已验证] | 独立任务每秒采样、768 B 缓冲在 512 B 或 5 秒 flush；正式固件默认路径连续写入已验证。TF 卡操作边界为插拔前下电，运行中热插拔/recovery 不支持也不作为验收项 |
| FDCAN2 外部 CAN | [客观已验证] | Windows CANtest 可收到开发板 `0x321` 周期帧；开发板收到 Windows 发帧；2026-07-08 22:46 分析仪收发打开后复查 `sendResult=0`、`rx_count=508`、`tx_count=728` |
| TF 卡 | [客观已验证] | SDMMC/FatFs smoke test 写读通过 |
| TF 静态文件服务 | [部分客观已验证] | 已启用 FatFs mutex，缺省创建 `/www/index.html`；`GET /` 和 `GET /index.html` 返回 `text/html` 默认页；固件已改为按文件大小循环 512 字节分块读取并多次 socket 发送 |
| DBC 上传最小接口 | [客观已验证] | `POST /api/dbc/upload` 保存 `/dbc/candidate.dbc` 后从 TF 读回候选并调用 portable `dbc_parse_text()` 生成报告；已烧录验证返回 `bytes=164/lines=4/messages=1/signals=2/errors=0/valid=true`，ST-Link 读数 `candidate_load_result=0/candidate_valid=1` |
| DBC 活动文件激活 | [客观已验证] | `POST /api/dbc/active` 无请求体最小命令已烧录验证：读回候选、portable parser 确认为 `errors=0` 后写入 `/dbc/active.dbc`，旧活动文件备份到 `/dbc/active.prev.dbc`；有效候选返回 `activated=true`，无效候选返回 `HTTP 400 candidate_invalid` |
| DBC 运行态快照 | [客观已验证] | active `0x321`/2 信号 DBC 启动加载正常；本轮 `POST /api/dbc/active` 返回 `activated=true/runtimeGeneration=2`，随后读到 `generation/load=2/2`、`valid=1/result=0` |
| 最小 DBC 解码到 SignalCache | [客观已验证] | active DBC 已接入 CAN2 TX self-test 和外部 RX；本轮新增 DBC mutex，reload 与 CAN 解码共享锁，TX self-test decode/matched/updates 增长、decode errors=0；外部 RX 本轮未验证 |
| W25Q128 | [客观已验证] | 默认启动只读 JEDEC ID `EF4018`，未触发擦除；保留诊断区 `0x00FFF000` 的显式 ST-Link 擦写读回匹配 |
| FreeRTOS 单任务 | [客观已验证] | 已烧录验证 `g_freertos_task_started=1`、`g_freertos_loop_count` 递增，W5500/CAN/TF/W25Q128 状态保持通过 |
| FreeRTOS 基础多任务拆分 | [客观已验证] | TfTask 已烧录接管一次性 TF mount/smoke/default-page 初始化并由 bring-up 有限等待；MonitorTask 已接管 1 s 状态打印；CAN2 仍保持每 50 ms FIFO 接收、每 1 s 诊断发送；W5500 状态轮询与 HTTP socket0 轮询已拆为两个 50 ms 任务并由 mutex 串行化 |
| 最小 RuleTask/继电器 | [客观已验证] | 已烧录 50 ms RuleTask：固定 1000 ms 延时、1500 ms 超时安全低、ST-Link 手动 OFF 覆盖优先、固定高滞回均已验证；单规则配置 reload 与 QSPI 显式保存、读回和复位加载均已实测；新增 HTTP GET/POST 单规则配置闭环并跨复位验证 |
| 最小 QSPI 规则配置备份 | [客观已验证] | v1 单槽记录兼容；v2 使用 `0x00FFE000` 主槽和 `0x00FFD000` 备用槽，含 sequence、参数和 checksum。ConfigTask 两次交替保存、读回、复位加载、无效请求拒绝及候选/运行态隔离均已 ST-Link 实测 |
| TF RuleFile v1 | [客观已验证；非法板端输入未注入] | 首次启动 TF `status=0` 时缺失文件 `load_result=1/created=1`；复位后读取 `75` 字节有效文件，`load_result=0/read_len=75`，RuleTask `generation=2/reload=0`，有效文件把此前 HTTP 写入的 QSPI `42435/42433/1100/1600` 覆盖回 `42434/42432/1000/1500`；纯解析主机测试覆盖缺字段、非法阈值和非法时序且候选不变 |
| TF RuleFile v3 / 受限规则 CRUD | [客观已验证] | `/config/rules-v3.conf` 固定两槽、640 B 严格文本格式；顺序 HTTP `GET`、DELETE 禁用、POST 恢复、PUT、非法 PUT、重启持久化和默认恢复均已烧录验证。写入经 ConfigTask TF tmp/prev 原子替换，成功后 RuleTask 原子 reload；v3 构造函数已消除 3.8 KiB 栈对象导致的实测 HardFault。 |

## 当前阻断项

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
- 当前固件保留候选 scratch `DbcDatabase`、运行态双槽 `DbcDatabase`、外部 RX 与 TX self-test 两个固定 `SignalCache`、768 B 日志缓冲和 LogTask 栈；阶段 A 最终构建 RAM_D1 为 `231144 B / 512 KB = 44.09%`。后续扩大缓存或引入并发读者前必须继续复查内存并补齐同步边界。
- LogTask 不再由 bring-up 监控循环直接写 CSV：它每 100 ms 调度、每 1 秒复制最多两项、缓冲达到 512 B 或 5 秒才在 `fs_mutex` 下单批 `f_open/f_lseek/f_write/f_close`。初始化只读一次默认文件：成功或 `FR_NO_FILE` 选 `/log/signal.csv`，其他失败选 `/log/signal-recovery.csv` 并记录 path mode/switch count/active size；随后不再切换。写失败后清空本批、累计失败和丢弃，不做重试、轮换、下载 API、HTTP 配置或通用队列。临时 probe 代码已移除。
- 2026-07-08 22:34 当前复查中 `/api/can/status` 可访问，但现场读数为 `rx=0/errors=487/tec=128/sendResult=1`；2026-07-08 22:46 用户打开 CAN 分析仪收发后复查恢复为 `sendResult=0`、`rx_count=508`、`tx_count=728`、`tec=0`、`bus_off=0`。
- `CONVERSATION_SUMMARY.md` 已经较长，但仍按用户要求保留为完整对话摘要；当前快照以本文件为准。
- macOS 串口曾出现乱码，硬件结论优先用 ST-Link 变量和外部工具确认。

## 下一步建议

阶段 C、D、E 已关闭；阶段 F 进行中。F-12 已完成当前固件的30分钟静态插卡日志耐久，原长跑子项现有通过证据。F-13 已只读固定 F-14 的网络断开/恢复现场协议：无需代码改动，实时链路以 `g_w5500_link_up/PHYCFGR bit0` 的 `1→0→1` 为准，启动配置字段不能替代它；HTTP 须以串行主机请求和 error 计数交叉确认。下一步 F-14 必须暂停等待用户只拔出 W5500 网线后确认，再按固定读数验证断网，之后再等待插回确认验证恢复；不得先行自行断网、重启、烧录、拔插 TF 或改变 CANtest。

F-14 当前暂停：已连续三次等待用户“已拔出”确认而未收到实际物理操作信号，故未执行任何断网或板端读取。此为外部条件阻断，不得把未形成断网状态写为通过或失败；收到确认后恢复固定流程。

F-14 断网子判定后来已由用户“已拔出网线”确认后通过：两次 `link_up=0/PHYCFGR=0xBA(bit0=0)`、W5500/HTTP 任务循环增长，主机 HTTP 超时且无响应。网络恢复子判定现暂停等待用户“已插回”；连续三次等待仍无确认，禁止自行插回、重启、烧录或把断网子判定写为完整恢复通过。

F-14 已完整通过：用户插回同一网线后，`link_up/PHY bit0` 两次为`1/1`，W5500/HTTP任务循环增长；ping=`2/2`、`/api/status`、`/api/can/status`、`/api/signals` 均200，HTTP request=`9→12`、error=`0`。该证据只覆盖物理网线断开恢复；阶段 F 仍待 CAN bus-off/恢复和复位后 DBC/规则/日志恢复。下一派送 F-15 只读审计当前 FDCAN2 bus-off、错误计数、恢复行为和可观测符号，形成固定现场验收协议；不得改代码、烧录、操作 CANtest 或自行制造 bus-off。

F-15 已固定 F-16 协议：当前 FDCAN2 为500 kbit/s、自动重传，运行时无显式 bus-off恢复代码。必须以 CANtest真正离线造成无ACK，且同时观察 HTTP `busOff=1` 与原始 `PSR.BO=1` 才能开始恢复验证；仅停止发送不成立。下一步 F-16 先等待用户使 CANtest停止发送并关闭/离线后确认；不得先行自行改CAN、断开开发板侧线、重启、烧录或改变TF。

F-16 已完成正常CAN基线，但暂停等待用户“已离线”：三次等待没有实际确认，故未制造bus-off。当前基线 `busOff/tec/rec=0`、`CCCR.DAR=0`、`PSR.BO=0`、CAN tx/rx/poll增长；收到确认后才按固定20秒窗口观测。该暂停是外部条件阻断，不能把正常基线写为bus-off恢复通过。

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
