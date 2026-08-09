# 经验教训

- L-073：规则表单的 `enabled` 只接受十进制 `0/1`；`enabled=true` 正确返回 `HTTP 400 invalid_rule`，并在两次 `SENDOK` 后沿用 F-17 的 pending `DISCON` 路径，不存在400专属强制 `CLOSE`。现场独立短连接在收到完整400后的 `+0/+50/+100/+250/+500 ms` 均返回200、最终 socket=`LISTEN`/pending/error=`0/0`，只能证明该顺序流程，不可据此宣称支持并发。
- L-074：CAN 无ACK现场不能替代真实 bus-off 注入。F-16 的 `TEC=128/EP=1/BO=0/LEC=ACK error/TXBRP=0xF` 表明节点错误被动且4个硬件发送请求未完成；自动重传和加快应用发送都不会构成 `PSR.BO=1` 证据。bus-off 验收必须使用外部物理 bit/stuff/form/CRC 类错误并同时观测 `PSR.BO=1`、HTTP `busOff=1`，不能用软件、loopback或调试器伪造。
- L-075：真实 bus-off 形成不等于恢复。错误比特率现场已读到 `PSR.BO=1` 与 HTTP `busOff=1`；恢复外部500k后超过60秒仍为 `CCCR.INIT=1`、`PSR.BO=1`、`TXBRP=0xF`，CANtest会因开发板不ACK显示发送失败，SignalCache不再有新输入。不得把CANtest发送失败或旧缓存误写为恢复；必须用明确、可回退的FDCAN停止/启动恢复状态机并重新现场验证。
- L-076：FDCAN bus-off 恢复必须只在真实 `bus_off` 快照下执行，并先释放硬件 `TXBRP` 请求。F-25 的 Abort→Stop→Start 以首次立即、持续 BO 每1000ms一次限流运行，实测错误250k后的 BO 在不复位条件下退出；恢复500k后外部 RX、SignalCache 与 TEC 均恢复。不得用 DeInit/Init、调试器改状态或普通 ACK 错误被动代替该证据。
- L-077：LogTask 写入计数、文件大小和 `/api/signals` 不能证明 CSV 的物理内容。当前产品不含日志下载 API；最终内容验收应在外部 CAN 和两次运行态增长通过后完全下电取卡，在主机只读核对当前路径的单表头、目标信号行、字段合同、完整成对尾部与不小于最近板端值的字节数。人工下电不能与 MCU 计数原子同步，禁止把之后继续正常追加造成的更大文件误判为失败；此证据不能外推为热插拔、在线下载或并发文件服务。

- L-001：硬件状态以最新上电或复位后的实际读数为准，不沿用旧日志结论。
- L-002：`/Users/elvin/Desktop/project/can_bus` 可能是指向 `/Users/elvin/Desktop/project/can_bus_W5500` 的符号链接，执行前先用 `pwd` 或 `git rev-parse --show-toplevel` 归一化路径。
- L-003：W5500 验证要同时看 SPI 寄存器、ST-Link 变量、主机路由/ping/ARP；串口单独不足以定论。
- L-004：W25Q128 的 `0x00FFF000` 最后 4KB 已固定为诊断保留区；启动仅做唤醒、ready 与 JEDEC 检查，擦写读回只能由显式诊断请求触发。验证必须同时读取 `g_w25q128_erase_count=0` 的默认启动证据和显式请求后的计数/读回匹配，正式配置备份不得使用该扇区。
- L-005：FreeRTOS 启用后 `SysTick_Handler` 仍必须调用 `HAL_IncTick()`，否则 HAL 超时和 delay 逻辑可能失效。
- L-006：FDCAN2 `PB5/PB6` 才是当前用户实际接线的外部 CAN 通道；FDCAN1 `PD0/PD1` 只作为诊断/保留路径。
- L-007：固件编译后必须做关键路径反汇编检查；如果本轮没有编译，记录中必须明确未执行反汇编。
- L-008：提交前只暂存本轮相关文件，避免把无关 middleware、构建产物或并行改动带入提交。
- L-009：`env.sh` 被脚本 source 时可能因 `$0` 指向外层脚本而算错项目根，还会覆盖同名 `ROOT_DIR`；自动验证脚本应使用独立变量保存项目根并重新设置本地 xPack PATH。
- L-010：W5500 HTTP 功能验证要同时看 `ping`、`curl -i` 响应、ARP MAC 和 ST-Link `g_w5500_http_*` 变量；如果暂停瞬间读数异常，应复位运行后用 HTTP 响应和再次 mdw 交叉确认。
- L-011：阶段 9 的静态文件服务先用 FatFs mutex 和小文件闭环验证；当前 socket0 HTTP 是单连接最小实现，并行 curl 可能失败，不能把它当作并发 HTTP 服务。
- L-012：静态文件从 TF 分块读取时，验证不要只看 `curl` 返回 200；还要用反汇编确认 `http_handle_request` 走文件大小读取、循环 `f_lseek/f_read` 和多次 `SEND` 路径，并用 ST-Link 读取 `g_w5500_http_static_file_size/g_w5500_http_static_bytes_sent`。
- L-013：HTTP 上传接口要先处理 RX 半包边界；`Content-Length` 对应 body 未完整到达时不能推进 `S0_RX_RD`，否则会消费掉未完成请求并关闭连接。
- L-014：把大 HTTP 请求/响应缓冲放入静态 BSS，避免 W5500 任务在上传路径叠加 FatFs `FIL` 栈帧后逼近 4KB 任务栈。
- L-015：DBC 上传进入可复用配置流程前，先把文件策略写进源码常量和接口响应；当前候选文件为 `/dbc/candidate.dbc`，旧候选备份为 `/dbc/candidate.prev.dbc`，活动文件预留为 `/dbc/active.dbc`。
- L-016：候选 DBC 读回解析要复用 portable `dbc_parse_text()`，不要在 HTTP 上传路径继续扩展手写 `BO_`/`SG_` 扫描；同时静态 `DbcDatabase` 会显著增加 RAM_D1，占用变化必须随构建记录。
- L-017：STM32 固件使用 `nano.specs` 时不要依赖 `sscanf("%lf")` 解析 DBC 浮点字段；板端实测会导致 signal 行解析失败，改用手写轻量十进制解析可保持功能正确并避免 Flash 暴涨。
- L-018：DBC 活动文件切换前必须重新从 TF 读回候选并复用 portable parser 校验；无效候选只返回错误，不触发 `/dbc/active.dbc` 替换。最小阶段先保留候选文件，使用 `/dbc/active.write.tmp` + `/dbc/active.prev.dbc` 保护旧活动文件。
- L-019：运行态 active DBC 加载要解析到非活动槽，只有 parser 返回有效后才切换 active 指针、slot 和 generation；reload 失败或无效文件不能清空既有有效快照。双槽 `DbcDatabase` 会显著增加 RAM_D1，本轮从 25.12% 升到 34.91%，后续接信号缓存前必须继续复查内存。
- L-020：CAN 解码验证必须区分来源：成功发送的周期诊断帧可作为 TX self-test 验证 active DBC→decoder→`SignalCache`，不能替代外部 CANtest→FDCAN2_RX 验证；分别读取 `g_can2_dbc_tx_self_test_frame_count` 和 `g_can2_dbc_rx_frame_count`。

- L-021：新增 FreeRTOS 任务前必须按 `configTOTAL_HEAP_SIZE` 核对任务栈总量；本轮新 CanDecodeTask 使用 1024 words 栈会在真实启动时触发 `Error_Handler()`，改为 512 words 后才完成任务创建和现场运行验证。队列任务必须同时记录 ready、enqueue、dequeue、drop，不能只凭任务 started 宣称解耦成功。
- L-022：CAN RX 队列拆分后仍必须保持 TX self-test 缓存与外部 RX 缓存隔离；现场验收要同时看队列入/出队、`g_can2_dbc_rx_frame_count`、decode errors 和 `/api/signals`，不能用 TX 计数代替外部 RX。
- L-080：外部 CAN 解码验收至少要连续两次读取并确认 `g_can2_dbc_rx_frame_count`、matched 与 signal_updates 同步增长，同时核对 last RX ID/DLC；单次静态计数不能证明持续外部输入。
- L-081：在当前 CAN2 单写者、W5500 单读者阶段，HTTP 读取 SignalCache 时用短 FreeRTOS 临界区复制固定小快照；不要直接序列化正在被 CAN2 任务更新的缓存结构。
- L-023：最小 CSV 可复用同一 `can2_signal_cache_copy()` 快照和 FatFs mutex；空文件只写一次表头，后续在 `f_lseek(f_size())` 后追加。验收必须连续读取 `g_tf_csv_write_count/g_tf_csv_write_result/g_tf_csv_write_len/g_tf_csv_file_size`，同时确认 CAN RX、匹配和信号更新继续增长。
- L-024：LogTask 首次检查 `/log/signal.csv` 时，`FR_NO_FILE` 仅表示新卡/首启，应以文件大小 0 继续并写表头；其他 FatFs 返回码不能伪装为首启。2026-07-10 曾现场读到 `FR_DISK_ERR=1`，但最终恢复固件启动时同一路径读取成功；因此错误可能间歇，不能人为破坏文件复现。
- L-025：隔离 append probe 可用于一次性区分“原文件”与“介质”问题，但最终产品必须移除其代码和全局。最终恢复策略应在 LogTask 初始化只选择一次：默认大小读取成功或 `FR_NO_FILE` 用默认路径，其他错误用固定 recovery 路径；不得循环切换或自动修复。recovery 分支只有在真实读取失败时才能声称已验证。
- L-052：真实 TF 拔卡能使默认 `f_open` 在约数十秒后返回并选 recovery，但本板同一上电周期插回后 FATFS remount 连续 `FR_DISK_ERR`；`HAL_SD_Init` 成功、`hsd1.State=READY` 不能单独证明 FATFS 已可读。SDMMC reset 和仅 remount 窗口跳过第二次 CMD13均未恢复写入，所有临时 gate/旁路必须删除并重新烧录正式固件。2026-07-13 ST-Link 在用户确认 TF 插入和随后拔出时均读取 `GPIOA_IDR=0x0000c180`、PA8=`1`，故不论原理图标注如何，当前实物检测脚没有可用状态变化；基于高=插卡的临时修改必须撤回。后续必须以 recovery 文件写计数和大小增长作为成功证据，不能以 path_mode、mount init 或 PA8 电平代替。
- L-053：本板简易 TF 卡座没有可用检测开关，PA8 在插卡/拔卡均为高，必须永久屏蔽。真实拔插后即使 TF 保持插入并对开发板断电 10 秒再上电，仍可能出现 `HAL_TIMEOUT`、`ErrorCode=0x80000000`、`DCOUNT=512`，表现为 flush 增长而 write 不增。此时不能继续把问题归因于 PA8、重挂载时序或应用逻辑；先用已知正常 FAT32 卡或在主机端检查当前卡，且格式化必须先获得用户明确授权。
- L-054：用户明确授权后，可用一次性不提交固件在 TfTask 单独运行阶段执行 `f_mount(NULL)`、`f_mkfs(SDPath, FM_FAT32, 0, work, 512)`、重新 `f_mount`，成功后再运行既有 TF bring-up；必须逐项读取 FatFs 返回码，删除该临时代码、重新编译反汇编并烧录正式固件。三项均为 0 仅证明格式化和基础文件系统初始化成功，不能替代带外部信号的 LogTask 持续写入或 recovery 验收。
- L-055：板端格式化会清除 `/dbc/active.dbc`，即使 CAN RX 持续增长，`g_can2_dbc_rx_frame_count=0`、`/api/signals` 空、LogTask 也只会丢弃无信号样本。恢复标准 DBC 必须通过既有顺序 `POST /api/dbc/upload` 与 `POST /api/dbc/active`，确认 HTTP 200、SignalCache 有值后，再以连续 write/flush 和文件大小增长证明新格式化介质的默认日志写入。
- L-056：当前 TF 即使经板端 FAT32 格式化、重新挂载、bring-up 和冷启动默认日志写入均成功，仍会在同一上电周期“真实拔出→默认读失败选 recovery→插回→原 append”时返回 `FR_DISK_ERR`。因此格式化不能证明或修复热插拔 recovery；需要 recovery success 时，唯一有效证据仍是 `path_mode=1/switch=1` 后同一轮 `last_result=0`、write 增长、recovery size 从 0 增长。失败后必须删除 gate 并重新烧录正式固件。
- L-057：当硬件能力与产品需求不匹配时，应把真实验证结果固化为操作边界。当前板载简易 TF 卡座定义为仅支持下电后插拔；插卡冷启动默认日志持续写入是验收证据，运行中热插拔/recovery 不再作为功能目标。不得保留临时 gate、remount、CMD13 旁路或 PA8 检测代码。
- L-058：默认日志的短时写入和文件大小增长不能替代分钟级稳定性证据。2026-07-13 在未热插拔、正式固件连续运行约 30 分钟时，任务、CAN 与网络仍正常，但 LogTask 最终出现 `g_log_last_result=1`、`g_tf_csv_write_result=1`、`g_tf_write_open_result=1`，failure/drop 增长；必须先按默认插卡路径定位，不能误归因于热插拔，也不能直接加入重试、remount 或状态旁路。
- L-059：当多个 TF 调用共用 `g_tf_write_open_result` 与 SD 最近状态时，终态只能作为线索，不能把 HAL 错误绝对归属给最后一个 append 阶段。首次失败应先以短周期只读采样捕获；若仍缺操作来源，最小诊断只记录最后 SD 操作和 append 阶段，不应在证据不足时改 timeout、重试、remount 或缓存策略。
- L-060：来源诊断必须在原调用前记录，并让 append 失败保留其阶段。2026-07-13 的正式板端首次失败为 `g_tf_append_stage=2(open)`、`g_tf_sd_last_operation=2(read)`、`g_tf_write_open_result=1`；因此 `f_open` 的目录/FAT 底层读已失败。该结论仍不等于读超时的根因，下一步应审计 read 链路而非修改写入或恢复策略。
- L-061：`FR_DISK_ERR` 必须继续沿 FatFs→diskio→BSP→HAL 逐层还原。当前 `f_open` 的单扇区读最终为 `HAL_SD_ERROR_RX_OVERRUN=0x20`，现场 `DCOUNT=448/STA=0x29000` 与 RX FIFO 溢出相容。`BSP_SD_ReadBlocks_DMA` 名称不等于 DMA 实现：当前实际调用轮询 `HAL_SD_ReadBlocks`；这只是机制事实，不足以直接归因于 IRQ、卡或总线，先记录请求/寄存器快照。
- L-062：MCU reset/reflash 不能自动提供 SDMMC 的干净首错状态。F-5 重新烧录后 5 秒内已有 read failures，前后快照都携带旧 `HAL_TIMEOUT`；此时最后 LBA 仅说明持续失败时的请求，不能当作触发首错。要验证首错快照，需要保持 TF 插卡并执行板级断电上电。
- L-063：冷启动首错必须同时满足 failure 从 0 首次变为 1、read call 递增，以及同一次前后快照由无错误变为错误。2026-07-14 的 F-6 首错为单扇区 `LBA=3826`、append stage/op=`2/2`，前 `ErrorCode/STA/DCOUNT=0/0/0`、后 `0x20/0x29000/448`；它证明 polling read 的 RX FIFO overrun，但不能单独证明卡、信号、CAN、IRQ 或 timeout 根因。
- L-064：不要依据 `BSP_SD_ReadBlocks_DMA` 的名称判断实际传输方式。F-7 证实当前失败路径调用的是 `HAL_SD_ReadBlocks` polling；故障快照 `MASK=0` 也说明 FIFO 处理不依赖 SDMMC IRQ。排除 DMA/IDMA 与 FDCAN2 RX ISR 后，仍需用受控临界区实验验证任务切换假设，不能直接改 SD 参数。
- L-065：不能用 `taskENTER_CRITICAL()` 包裹依赖 `HAL_GetTick()` 超时的 polling HAL 调用。F-8 现场任务循环冻结、PC 位于 `HAL_SD_ReadBlocks`，说明临界区屏蔽 tick 后会让实验自身停滞；此结果只否定该实验方法，不证明任务切换是否为 RX overrun 根因。
- L-066：在不中断 SysTick/外设 IRQ 的前提下，`vTaskSuspendAll/xTaskResumeAll` 不能阻止当前默认日志 SD 故障。F-9 外部 CAN 输入下越过原首错 call=102 后仍出现 `HAL_TIMEOUT` 与 read failures；因此不能把任务切换写成根因，调度挂起代码必须撤回。
- L-067：SDMMC 最终运行配置必须以 BSP 覆盖和 `CLKCR` 现场读数为准，而不是 `.ioc` 初始值。当前为1-bit、约3.125MHz、无HWFC；低速窄总线仍 RX overrun，若验证硬件流控必须只改变该位并用 `CLKCR=0x20010` 与既有首错快照验收。
- L-068：HWFC 单字段验证必须同时验证源码、ELF、板端 `CLKCR` 和受外部 CAN 驱动的日志路径。F-11 仅把 HWFC 置 ENABLE 后，`CLKCR=0x20010`、read call 越过历史首错阈值 `102` 至 `150` 且 LogTask failure=0，说明短时路径有效；它不能替代 30 分钟耐久，也不能据此把 FIFO overrun 根因归为某一单点。
- L-069：30分钟静态耐久必须采用连续的可比较输入，并同时验证存储与网络。F-12 在 HWFC 固件、TF 插卡和外部 `0x321` 输入下运行30分17秒，read failure/log failure 保持0，read/write/flush/文件持续增长，结束 ping/API/CAN/SignalCache 通过；`g_log_drop_count=6` 全程不变且其源码含义包括“无快照”，不能脱离 `g_log_failure_count` 单独解释为存储故障。一个分钟样本若输出筛选漏字段，只能记为不完整，不能虚构完整独立读数。
- L-070：W5500 物理链路恢复不能用启动配置状态代替。F-14 只有同时观察到 `g_w5500_link_up/PHYCFGR bit0` 的 `1→0→1`、W5500/HTTP任务循环持续增长、断网 curl 超时、恢复后的 ping/API成功和 HTTP request 增长/error不增，才可写为网线断开恢复通过；`network_configured=1`、`init_result=0` 与 `last_code=200` 单独均不足以证明恢复。
- L-071：W5500 的 `SENDOK`/内部HTTP200仅证明芯片接受发送，不保证TCP优雅完成。成功响应同轮 `DISCON→CLOSE` 会在实板形成主机RST；必须等待 `Sn_SR` 进入 CLOSED/INIT 后才重新监听。当前单 socket/50ms轮询在关闭重监听间有短窗口，验收和客户端操作应在短连接间留至少一个轮询周期（本轮250ms）；这不是并发HTTP能力。
- L-084：`SENDOK`之后立即`DISCON`仍可能让浏览器收到0字节；用`Sn_TX_FSR`观察未确认响应字节，回到完整2048后才能开始正常断开。客户端先FIN时socket会进入`CLOSE_WAIT`，此时必须发送graceful `DISCON`完成板端FIN，不能硬CLOSE并重开listener。最终验收必须同时检查完整body、响应ACK、双方FIN最终ACK和RST=0，单看handler=200、SENDOK或页面状态都不足以判定交付成功。
- L-085：只读GDB快照启动OpenOCD时不得带`reset run`；它会重置运行计数并切断前后证据连续性。误复位后必须明确废弃复位前计数比较，在同一最终映像复位后重新建立Snapshot A并完整重跑目标窗口。无debug type的ELF读取用`x/wx &symbol`，每次halt后先`monitor resume`再断开并确认OpenOCD/GDB端口释放。
- L-078：RAM-only故障注入必须先读真实原值并按原值恢复，不能把未加载哨兵`0xffffffff`假定成0。安全HTTP错误样本应把故障分支放在任何candidate/save_request之前，并让请求本身是失效保险：本轮`enabled=true`在注入成功时前置返回500，注入失败时只返回400，两条路径都不写TF/QSPI；恢复后还必须比较完整配置哈希和保存请求/结果。
- L-079：最终功能通过后仍要审计治理文件的“当前状态区”，不能让追加式历史记录中的“未完成/下一步”冒充现状。封口时保留历史事实但加上“已被后续验收更新”的边界，并把验收合同、计划、Feature索引、架构当前实现和CURRENT_TASK统一到同一一期范围；明确非目标不能被升级为发布阻断。
- L-072：zsh 的 `path` 是连接到 `PATH` 的保留数组，不能用作脚本循环变量。F-17 首轮因此在主机端得到 curl exit=127，但未访问或写入目标；此类主机脚本错误必须明确排除，不得作为固件回归或成功证据。
- L-083：页面显示`Failed to fetch`时，板端handler记录HTTP 200只证明业务处理完成，不证明响应已被浏览器收到，也不证明socket已恢复监听。必须把浏览器结果、同一连接pcap、后续ping/API、串口trace和W5500寄存器分层记录；若以OpenOCD停机读取，命令应分步执行并显式`resume`/`shutdown`，不能把遗漏恢复造成的网络中断算作固件故障。
- L-026：新建 Codex 会话的短时无 shell 进程、长推理或延后显示工具输出不能证明其异常关闭。排查时应先读取 turn 的 `status/error`；只有明确错误、用户要求或不可恢复冲突才归档。2026-07-11 两个 `interrupted/error=null` 会话均由根会话手动归档，而非系统自动关闭。
- L-027：尚未定义正式配置数据和备份地址时，最小 ConfigTask 只能拥有既有的显式 QSPI 诊断写路径；用默认启动的 `erase_count=0` 与单次请求后的 `erase_count=1/diagnostic_count=1` 分别证明默认安全和任务实际执行，不能把它表述为配置保存。
- L-028：单规则持久化的第一份正式 QSPI 记录固定使用独立扇区 `0x00FFE000`，绝不复用 `0x00FFF000` 诊断区。启动加载只能读；保存必须经 ConfigTask 显式请求，校验 magic/version/checksum/参数关系并读回比较。验收至少要覆盖空扇区、非默认配置跨复位恢复，以及恢复默认配置，不能只凭一次写入成功声称持久化。
- L-029：单规则 QSPI 保存接入运行态时，只有 `w25q128_rule_config_save()` 成功完成读回比较后才能置位 RuleTask reload；保存失败不得请求 reload，避免未持久化候选替换旧有效 engine。验收须在新鲜外部 CAN 输入下同时读取保存计数、RuleTask generation/load、safe 状态与 PE7，再恢复默认记录并复位确认。
- L-030：单槽 QSPI 配置在擦除到读回完成之间无法保留恢复副本。最小双槽做法是固定 `0x00FFE000` 主槽和 `0x00FFD000` 备用槽，v2 记录加入 sequence；每次只擦写非当前有效槽，读回匹配后才把它视为最新。升级时必须兼容既有 v1 主槽，首次 v2 写入备用槽；验收必须覆盖两次交替保存、每次复位加载、无效保存不擦除、最新槽损坏回退和两槽无效默认保留。若需人为制造损坏，只能用验证固件中的显式受限入口，完成后必须删除并重新烧录正式固件。
- L-031：若既有 `can2_analyzer_poll()` 同时承担周期发送和 FIFO 接收，不能直接把整个函数从 1 s 改为 50 ms，否则会把诊断发送频率放大 20 倍。应抽取只接收、解码和采集状态的函数，由 50 ms CAN 任务调用，同时保持原 1 s 发送节奏；验收需连续对比任务循环、poll、外部 RX 与 DBC 解码计数。
- L-032：把 bring-up 的永久循环移交给新任务前，先核对 `FreeRTOSConfig.h` 的 `INCLUDE_vTaskDelete`；若要用 `vTaskDelete(NULL)` 回收原任务栈，必须启用该可选 API，并在 ELF 中确认创建新任务后确实跳转到 `vTaskDelete`。
- L-033：运行态 DBC 双槽不能只保护 active 指针赋值；加载可能覆盖下一槽，而 CAN 解码仍持有旧指针。加载解析/槽切换和解码查表/写缓存必须共用同一 mutex；本轮未引入独立 DbcTask 或队列。
- L-034：DBC mutex 验收必须分开记录 active reload、TX self-test 和外部 RX；本轮 reload `generation/load=2/2`、TX decode errors=0，但没有持续外部 CAN 输入，外部 RX 必须记为“未验证”。
- L-035：阶段 7 的 DbcTask 先只承载 active DBC reload 一次性命令；HTTP 写入 active 文件后提交请求并有限等待，任务调用既有加锁加载函数。这样可验证任务边界而不引入通用消息总线或改变 API 语义；验收必须同时看 `request/complete/result`、runtime generation 和 HTTP 激活响应。
- L-082：ST-Link 配置诊断入口必须把 pending 候选与 RuleTask 当前运行态参数分开。ConfigTask 先复制候选并调用 QSPI 保存；只有保存及读回比较成功，才一次性提交运行态参数并请求 reload。无效或失败候选可保留在 pending 供诊断，但不得改变当前 active 参数、RuleTask generation 或继电器输出。
- L-036：一次性 TF 初始化拆为 `TfTask` 时，任务必须复用既有 `fs_mutex`，写入明确完成/结果变量；bring-up 只能用带 `vTaskDelay` 的有限等待，超时进入 `Error_Handler()`。GDB 读取后要显式 `monitor resume` 再做 HTTP，不能把 halted 目标的超时算作固件回归。
- L-037：阶段 7 的最小队列通信应先替换已有单次标志，而不是同时引入通用消息总线；深度 1 的 DbcTask reload 队列可用 `ready/enqueue/drop`、`request/complete/result` 和 runtime generation 共同验收。GDB 读数后必须显式 resume 再执行 HTTP。
- L-038：CANtest 未显示开发板帧时，若板端 `tx` 持续增长且 `sendResult=0/errors=0`，应先重启接收软件复核会话/显示状态；本轮用户确认重启后可见，不能把接收软件显示问题归因于板端 TX。
- L-039：在当前 64 KB FreeRTOS heap 和既有任务栈预算下，CAN TX 队列可复用现有 `CanDecodeTask` 消费，不新增任务；用深度 1 `CanFrame` 队列即可验证入队、出队和发送边界。必须把 TX self-test 解码放在实际 `can_port_send` 成功之后，并同时读取 TX/RX 队列 drop 计数。
- L-040：通用配置队列的最小边界可以保留既有 ST-Link 请求标志作为兼容入口，在 ConfigTask 内快照为固定 `ConfigCommand` 后投递到深度 2 队列，再由单消费者执行诊断或规则保存；验收必须同时读取 `ready/enqueue/dequeue/drop` 和实际命令结果。本轮两类命令累计 `enqueue=2/dequeue=2/drop=0`，规则保存成功，但不能把队列消费成功表述为 QSPI diagnostic 成功。
- L-041：ELF 没有 debug symbols 时，GDB 直接 `set var` 可能只得到 `unknown type` 且不改变目标内存；必须使用 ELF 的精确符号地址和 `set *(unsigned int*)address=value`，并在每次 halt 读取后显式 `monitor resume`。本轮第一次 diagnostic 请求因此未生效，第二次精确地址写入才证明队列入队/出队。
- L-042：配置队列与底层存储结果必须分层记录。`g_w25q128_diagnostic_result` 的上电值 `0xffffffff` 是初始化哨兵，不是 `w25q128_diagnostic_run()` 的合法返回码；只有同次读取到请求清零、队列入/出增长、`diagnostic_count` 增长和实际 result 后，才能判断诊断结论。2026-07-13 以当前 ELF 精确地址单次请求实测 `result=0/erase_count=1/test_addr=0x00FFF000`，同时双槽配置地址与 sequence 不变。
- L-043：HTTP 配置写入应复用已有 pending/ConfigTask/RuleTask 边界。板端验证必须同时看 HTTP 200、ConfigTask enqueue/dequeue/drop、QSPI save result/count、RuleTask generation/reload 和复位后的 load result；仅返回 HTTP 200 不能证明持久化完成。当前单连接 socket0 下，GET/POST 请求必须顺序执行。
- L-044：TF RuleFile v1 的有效输入必须先落到独立候选，再通过既有 RuleTask reload 边界生效；解析失败、读取失败或超容量都不能覆盖 QSPI/编译默认安全配置。缺失文件创建必须在 `fs_mutex` 下显式确保 `/config` 存在，并使用 `FA_CREATE_NEW`，不能以 `FA_OPEN_ALWAYS` 覆盖已有文件。
- L-045：RuleFile v2 缺失时，`v2_created=1/load_result=1` 只证明 `FA_CREATE_NEW` 创建成功，不能视为本次启动已加载；本次启动必须继续使用已加载的 v1/QSPI，只有下一次复位读取并完整 reload 成功后才记录 `load_result=0`。
- L-046：固定两规则模型必须拒绝未知、重复、缺失、非十进制、uint32 溢出、非法 relay/state、priority 冲突、超时关系错误和超过 512 字节的输入；所有拒绝路径先保持候选 `RuleEngine` 不变。
- L-047：同一继电器的 priority winner 必须在引擎内部选择并保留 winner index，不能靠 RuleTask 按数组顺序覆盖输出；手动覆盖应在规则评价前直接返回，并清空 winner 诊断。
- L-048：阶段 B 的 CAN 证据必须区分真实外部 marker 值；marker=42434 的 PE7 证据和 TX self-test 不能替代 marker=42435 的 priority 冲突或暂停输入后的 timeout 证据。
- L-049：OpenOCD 烧录前和验证结束后均执行 `pgrep`/`lsof` 检查；不得连接遗留服务，必须使用独立实例，GDB 每次 halt 后执行 `monitor resume`，结束后显式 `shutdown` 并确认 3333/6666 无监听。
- L-050：阶段 B 最终验收必须把三类现场证据分开记录：marker=42435 的 priority winner、停帧超过 timeout 的 safeState、手动覆盖；本轮三项均已由精确 GDB 和外部 CAN/GPIO 读数完成，但首次 v2 缺失创建板端未观察，不能补写为实测。
- L-051：后续任何需要用户操作 CANtest 的验收，主会话必须先暂停派送任务和硬件操作，给出精确发送参数或停止步骤；只有收到用户明确“已发送”“已停止”等确认后，才可继续现场读取。不能依据默认输入状态或沉默继续试探。
- L-086：网页 CAN 发送候选必须先冻结总线合同，再把证据分层。当前用户确认的合同仅为经典 CAN：标准ID、DLC、最多8字节HEX、周期`100..10000 ms`；`GET/POST /api/can/tx`和`GET /api/can/tx/signals`不得借机扩展CAN-FD、扩展ID或通用周期管理。TX self-test=`42434/4660`、RX独立缓存和CAN errors=`0`只能证明各自边界，不能替代CANtest对新控制帧的外部接收证明。
- L-087：网页候选构建/烧录通过也不等于网页交付。即使已有 CTest=`16/16`、ELF/HEX前缀=`f589...`/`37b9...`、`text/data/bss=93976/384/242448`、50 ms poll/队列/解析反汇编及OpenOCD `Programming Finished/Verified OK/Resetting Target`，若更新版`www`尚未下电写入TF、浏览器新UI未验收，就必须保持“候选未验收、不得完成或提交”。
- L-088：单socket网页刷新即使首轮功能通过，也必须重复页面重入和连续端口80访问。此次首轮绿灯、TX/RX、网页停发/恢复、受控帧及DBC表均通过，刷新计数/时间戳增长；但重入后端口80连续5次失败，不能用ping=3/3替代。GDB的socket0=`0x17(ESTABLISHED)`、status/error=`0/0`、requestCount=`403`、RX_RSR=`0`和HTTP tick增长只证明当时状态，不能单独证明根因。第4个串行页面请求后恢复250ms收尾等待是当前最小且首要的时序修复；部署后必须复测，若仍见ESTABLISHED无RX再审计状态机超时边界。
- L-089：网页单socket时序修复必须以TF实际部署后的重复浏览器证据关闭，而不能停在静态`node --check`。本次恢复第4请求后的250ms收尾等待后，自动刷新TX/RX=`120/273→134/417`、两次reload=`145/527`和`162/694`均无连接拒绝，状态灯正常、details默认折叠、控制台warn/error为空。该结果仅覆盖本次窗口；CANtest已确认作为外部输入并推动RX表增长，但未直接读取CANtest作为接收器对新控制帧的显示，仍必须和TX self-test分开记录。
- L-090：当网页仍受 W5500 单 socket 非并发约束时，不能用全局刷新 FIFO 的历史积压来阻塞手动 TX；手动操作最多只等待提交瞬间的一个在途请求。把规则从旧 v3 `marker` 迁移为活动 DBC `signalKey` 选择时，必须固定为两槽、只让 RuleTask 导出已配两项，并在无活动 DBC 时于写入前无副作用拒绝；旧 v3 只作为新配置加载失败后的兼容回退。该经验对应新阶段设计边界，尚无实现或现场验收。
- L-091：自动刷新响应不得无条件重绘用户正在编辑的 TX 表单；现场首次编辑并提交时，旧`/api/can/tx`响应覆盖输入，说明“请求严格串行”不等于“表单可随时变更”。最小修复应以 dirty-state 抑制旧响应覆盖，部署后在自动刷新中实测。
- L-092：嵌入式 nano `printf` 未链接浮点支持时，规则的浮点阈值可能在序列化中变成`0`；必须以实际持久回读验证格式化路径，不能仅凭 UI 已选择`signalKey`或保存请求成功判断规则值正确。
- L-093：网页自动刷新、活动 DBC 和两槽规则的最终验收必须同时记录可观察的刷新增长、编辑值跨刷新保留、授权后的 runtime、V4持久回读和外部 RX 下的继电器状态；其中 TX self-test 只能证明TX缓存，不能替代外部接收器或外部 RX 规则输入证据。
- L-094：FatFs上层`f_sync()==FR_OK`不能替代底层介质完成确认。若`disk_ioctl(CTRL_SYNC)`无条件返回`RES_OK`，同步成功只是假证据；同时必须检查非对齐scratch写分支，因为其DMA回调完成也不等于卡已退出busy。最小修复是两条路径都以有界`BSP_SD_GetCardState()==SD_TRANSFER_OK`确认，超时返回错误；这仍不把非日志型FAT32提升为原子掉电文件系统。
- L-095：TF物理断电验收必须在测试前先对介质做只读无错基线，断电取卡后还应避免macOS自动可写挂载，先卸载再显式只读挂载。若只读`fsck`发现`.Spotlight-V100`等可能早已存在或由主机挂载介入的元数据异常，应明确判定本轮验收失败，但不能把全部损坏唯一归因于当前断电；保留旧卡只读证据，使用已验证干净的FAT32介质复测，避免修复旧卡后混淆因果。
- L-096：空白TF部署必须有受版本控制的最小资产来源和逐文件校验。完整网页不能用固件极简备用页替代；active DBC缺失会让CAN RX存在但SignalCache/日志无有效输入。macOS普通`cp`可能生成`._*` AppleDouble旁车，应精确删除本次新生成项，并在卸载后以`fsck_msdos -n`退出0、只读重挂载哈希一致作为部署完成证据。旧日志、事务tmp和无法确认的V4规则不得从故障镜像回拷或猜测重建。
- L-097：完整网页回归必须把“业务状态已应用”“浏览器收到成功响应”“socket恢复LISTEN”和“后续连接可用”作为四个独立门槛。本轮manual POST已应用但页面`Failed to fetch`，后续两次socket0停在`ESTABLISHED`且80端口拒绝，关闭标签也不能恢复；即使复位后所有接口最终200、浏览器console无warn/error，也不能反向覆盖失败样本。下一次定位必须在单次会话同时保留pcap、HTTP trace、`Sn_SR/IR/TX_FSR/RX_RSR`与请求计数，先证明请求/响应/ACK/FIN时序，再授权最小状态机补丁。
- L-098：任务循环增长只能证明调度仍运行，不能证明唯一HTTP socket会自行回收。对`ESTABLISHED + RX_RSR=0`应先用原始TCP“连接不发送”做确定性复现，再加入仅覆盖该状态的有界watchdog并复用graceful `DISCON`；正常ACK和恢复窗口不得一起缩短。浏览器操作耗时也不能直接归因于该watchdog：必须在同一时间点对照socket状态和timeout count。本轮约27秒浏览器长尾时socket已LISTEN且idle计数不变，因此保持为单socket排队边界，不追加未经复现的半包超时或并发改造。
- L-099：大DBC的parser统计、合成index形状和真实source CRC分别通过，不能拼接成真实端到端证据。A1必须让同一真实文件实际走`stream parser -> builder -> index verify -> dump`，并比较多种chunk生成的index逐字节一致；fixture只能由显式CMake参数传入，路径不存在必须配置失败，不能猜相对目录或静默少跑。index verify还必须独立复核key语法、UTF-8/control、CRC与definition hash，不能只信builder已经验证。
- L-100：STM32 nano `printf`对64位格式符的限制也会破坏generation和后续可变参数，不能只在浮点/时间字段上防范。大DBC B阶段真实上传先后发现响应与tmp路径中的`%llX`输出为字面`lX`并使后续字段错位；固定16位generation必须拆成两个显式`uint32_t`的`%08lX%08lX`，并以host格式测试加真实JSON/path回读共同验收。HTTP 200、CRC正确或TF写完都不能掩盖文件名合同失败。
- L-101：大DBC parser/index首次进入128 KiB STM32 Flash时必须以最终链接结果而非对象大小估算。C阶段未优化链接实际溢出2352 B，不能放宽P0预算；对固件目标启用`-flto`后Flash降至120728 B并保留6248 B合同余量。长TF构造也不能只看DbcTask loop：每次FatFs调用都更新独立progress，IWDG判据接受loop或progress任一增长；逐调用`vTaskDelay(1)`会把约9.5万次I/O放大为分钟级，改为每32次让出1 tick后仍保持IWDG/HTTP可调度且显著缩短构造。
- L-102：单socket固件中，HttpTask等待DbcTask长TF查询时会继续持有W5500 mutex，W5500Task loop不增长并不必然表示W5500死锁。IWDG必须把同一受控操作的candidate progress作为替代健康证据；本轮初版查询在progress持续增长时仍因W5500 bit复位，修正为“W5500 loop或candidate progress任一增长”后同一19.67 s查询完成且`unhealthy=0`。该例外只覆盖明确受控的candidate长操作，不能泛化为忽略W5500Task停转。
- L-103：用户确认“已覆盖网页并插回上电”只能作为人工动作证据，不能替代板端TF挂载、页面哈希和candidate恢复。若板端`FR_NO_FILESYSTEM`而SD扇区读取无失败，应下电取卡并先做主机只读`fsck`；本轮`Invalid BS_jmpBoot ... 555342`/exit201与板端0x0D相互印证。介质含日志和generation文件时，repair/格式化前必须单独确认数据保留策略；重建后不得复用旧卡generation恢复结论。
- L-104：active manifest持久成功后再调用可能失败的void callback会形成“磁盘新、RAM旧但HTTP成功”的静默分裂。正确边界是在manifest前验证prepared generation/slot，持久提交后仍持有TF mutex执行只含RTOS短临界区的checked slot flip；callback返回异常时回滚本次current/formal并保留previous，同时显式discard未发布runtime/value slot。
- L-105：legacy V4有两条enabled规则且没有definition hash时，逐槽迁移会被另一槽阻断；不能猜hash、自动删除或静默接受。应提供一次显式原子迁移动作，原样保留两槽定义、先全部disabled写V5，之后仅在active selected runtime存在时由用户逐条启用并绑定真实hash。V5文件存在但损坏时必须fail-closed，不能回退磁盘旧V4 enabled语义。
- L-107：嵌入式兼容代码必须按最终ELF而不是“很少运行”估算Flash成本。大DBC G在126976 B硬上限下不能同时常驻V1-V4规则loader/转换器与V5生产路径；正确收敛方式是生产固件V5-only并对legacy/损坏输入明确fail-closed，同时在host保留旧格式parser和固定向量测试。历史迁移工具可以用于一次受控转换，但不应为了兼容便利长期占用生产Flash，更不能在预算压力下改成静默回退或自动删除规则。
- L-108：在OpenOCD/GDB停机读数后，`monitor shutdown`不能代替已证实的运行态恢复。若调试会话关闭时CPU仍halted，W5500可能继续回应ICMP而FreeRTOS/HTTP停滞，形成“ping通、port80拒绝”的假固件故障。结束顺序应先用独立控制通道确认`resume`已生效，再关闭OpenOCD，并以一次延后HTTP读取确认任务恢复；GDB命令与OpenOCD telnet命令不可混用（`mdw`不是GDB命令）。
- L-109：`current→previous→new current`的FAT32 manifest轮转即使启动能从previous恢复，也不自动满足“失败即时保持旧active”。每个新current rename、读回和manifest后的短临界publish失败出口都必须在同一TF锁内尝试previous→current回滚；若回滚I/O失败，保留previous供独立启动恢复，不能删除旧generation或声称rename原子。
- L-110：TF上可列目录、可读取部分字节或可计算宿主hash，都不能替代干净FAT链与文件长度的一致性。大DBC v3会话中，`fsck_msdos`只读退出`206`、CSV/meta链标记为空闲且容量低于目录声明、卷无法read-only mount时，必须把实体日志、clean footer、manifest持久化全部降为`[阻断]`；不得继续写卡、烧录依赖该卡的候选或以repair结果掩盖原始证据。先让用户在“只读镜像保全”和“放弃故障会话重建”之间明确决定，重建后的所有实体证据必须从零复验。
- L-111：用户明确选择放弃故障会话时，TF重建必须先以`diskutil list`精确确认外置物理盘，再格式化为MBR+FAT32，并且只部署可复现静态资产（网页、基线DBC及必要空目录）。部署完成应执行源/卡`cmp`与SHA-256、卸载后的只读`fsck_msdos -n`、read-only重挂载再次`cmp`，然后安全卸载；这些只证明主机部署介质可用，不能替代板端mount、DBC generation恢复、CSV clean footer、断电恢复或任何旧损坏文件的通过结论。
- L-106：长active请求的命令会话可能先返回session ID且暂时无stdout；必须继续poll到明确exit code和完整HTTP响应，不能把工具尚未交付输出误判为板端无响应。本轮定点trace最终证明path5/code200、header/body两次send和disconnect完成，IWDG unhealthy=0。
- L-112：FAT32的`current→previous→new current`只能构造可恢复协议，不能假设多文件rename原子。若new current rename、读回或runtime publish在previous已存在后失败，必须同锁立即尝试previous回写current；若该尝试也失败，保留previous并把结论降为启动恢复待验。镜像FAT与FSINFO写的返回值也必须向上传播，否则`f_sync`可能表面成功而关键元数据写失败未被调用方看到。
- L-113：单socket长业务请求的HTTP200只证明响应字节到达，不能证明socket已回到LISTEN。若`SEND_OK`后的TX_FSR等待没有保持一致的ack/disconnect状态，可能留下`ESTABLISHED`并拒绝所有后续请求。应将W5500 `SEND_OK`作为响应收口边界，立即走既有graceful DISCON，并用“长请求后两次独立请求”而非单次200验证。
- L-114：W5500网页故障若总在manual POST后出现，不能直接归因继电器线圈瞬态。保持`relay1=relay2=0`、只切换manual enabled仍能复现0字节响应和网络失联时，应把硬件动作假设排除，继续用同连接pcap与`Sn_SR/IR/TX_FSR/RX_RSR`对齐TCP发送/关闭时序。ACK超时延长或header/body合包只有现场对照通过后才能保留。
- L-114：外部Classic CAN正向RX必须同时保留三层证据：CAN状态RX/错误计数增长、受控GDB的ID/DLC与DBC matched/update/decode-error计数、selected-only API的GOOD raw/value；TX self-test或单次HTTP 200不能替代。反向隔离必须在用户明确停止某一ID后再做，不能从混合流量推断STALE。
- L-115：STALE隔离验收需要在用户明确停止selected消息后，证明未选消息仍让CAN RX增长、selected raw/value和`updatedMs`保持、quality超过阈值进入`STALE`。仅看总RX或单个MISSING不能证明未选消息未污染SignalCache。
- L-116：选择性日志的HTTP `ACTIVE/STOPPED`只证明控制面状态机和会话锁，不能证明TF文件实体可读或clean footer。必须记录日志路径、active generation、selection CRC、selected count、锁定期间写请求409，并在下电取卡后以只读文件/行列/哈希/FAT证据关闭介质门禁。
- L-117：TF日志实体验收必须用标准CSV解析而不是`wc`或肉眼抽样：先以meta的signal ordinal/key集合建立白名单，再检查header/列数、每key行数、quality、rowsWritten/drop/failure、cleanClose和FAT链。两次`fsck_msdos -n`与只读挂载/安全弹出共同证明介质一致；可见文件名或HTTP STOPPED不能替代。
- L-118：物理掉电CSV不应以“最后batch不足selectedCount行”直接判定撕裂。必须区分批次级部分保留与行级撕裂：本轮末batch只写ordinal0..3，但708行全部8列、末尾换行、时间单调、key白名单与FAT链均有效，meta保持`cleanClose=false`。完成结论仍必须补板端重启恢复，不能只停在介质可读。
- L-119：掉电介质可读后必须把同一TF插回断电目标，分别核对网络/TF、active runtime身份、selected外部RX、旧日志锁状态和新日志写入能力，才能关闭冷启动恢复。现场目标地址必须由当前源码或板端配置复核；本轮固件静态IP为`192.168.1.88`，沿用旧地址会制造假网络故障。
- L-120：默认OFF的实验故障注入不能只靠文档约定隔离。应以CMake配置期拒绝互斥实验开关、独立构建目录、正式ELF `nm`无符号/哈希不变、实验ELF符号与调用点反汇编、试验后重烧正式哈希形成可执行闭环；非I/O sentinel也必须具名，避免现场读数无法解释。
## 2026-08-09：回滚验证不得污染失败交易的对象归属

- `read_active_manifest_with_references()`会为被验证manifest重建全局`CandidatePaths`。若回滚时验证的是旧generation，而后续cleanup仍根据新generation的owned/renamed标志使用该可变全局路径，就会删除刚恢复旧manifest所引用的对象，HTTP即时状态可保持正常但冷启动失败。
- 对带“对象归属标志 + 可变路径缓存”的事务，回滚读回后必须恢复失败交易路径，或让cleanup接收显式owned路径；验证至少应包含失败后冷启动，不能只看同一运行期的HTTP/runtime快照。
- L-121：单socket HTTP关闭问题必须同时看pcap和板端业务状态。客户端可能已完整ACK上一响应，却在下一连接中出现“请求已被MCU应用、W5500两次SEND已完成、线上仍无ACK/响应”；因此业务副作用、SEND_OK、HTTP响应和下一次listener可用性是四个不同证据。无收益的超时、合包、轮询和关闭实验应逐项撤销，最终只保留能追溯到已验证基线的最小差异；正常网页/串行通过也不能扩写为零间隔短连接压力通过。
- L-122：单socket断开超时不应默认升级为W5500全芯片复位。`w5500_port_init()`会操作RSTn并重写公共网络寄存器，可能把单连接故障扩大为ping和HTTP同时失联；恢复必须先以`CLOSE->CLOSED->OPEN->INIT->LISTEN->LISTEN`重建目标socket，并用命令序列、阶段SR、后续连接和“未触发RSTn”共同验证，只有重建失败才允许全芯片fallback。

- W5500显示`LISTEN`、PHY link正常仍不能证明公共网络配置完整；GAR/SUBR/SHAR/SIPR必须作为同一运行时不变量读回。MCU软件复位也不等于TF卡物理掉电复位，若静态页和active runtime同时消失，应先区分介质供电状态，不能据此宣称文件已损坏。

- W5500公共配置可在listener建立命令之后损坏；若只在OPEN前检查会留下窗口。对已冻结的单socket建监听器序列，至少在`LISTEN`状态读回成功后再做一次配置不变量检查。

- 大静态HTML响应与随后立即短连接必须作为同一交接场景验证；“静态文件最终完整返回”与“下一API立即可达”是不同证据。若只能在约3 s后恢复，网页功能可记录为带恢复窗口可用，但HTTP稳定性不得关闭。

- L-123：单socket现场验收中的“上一次请求已安全交接”必须可观测。将SR、ACK pending/elapsed/timeouts、DISCON pending和recovery计数加入既有状态快照后，才能把HTTP200与listener状态分开解释；当前轮的50 ms ACK且timeout/recovery为0只证明该轮正常交接，不能外推为零间隔短连接问题已根治。

- L-124：对W5500单socket服务，`Sn_SR=LISTEN`只能证明socket寄存器状态，不能证明GAR/SUBR/SHAR/SIPR仍完整。应在空闲LISTEN轮询复用既有公共配置读回/精确修复逻辑，并以真实长事务至少多轮“长响应→交接status→下一连接”验收；计数为零时可证明该窗口无需修复，但不能冒充已命中故障分支。

- L-125：前端的“未知”状态不能被误作“禁止用户准备操作”。对candidate selection，日志未知应允许浏览、翻页和跨页编辑，只在提交前串行读取日志状态并保留后端ACTIVE门禁；对规则，活动signalKey目录应异步提供建议而非禁用保存，直接输入仍由后端活动DBC/definition-hash校验。分页渲染只能在candidate token改变时清空待提交set/clear，不能在换页时丢弃用户编辑。

- L-126：宽表日志不能把“每个信号一个写计数”沿用为“每行一个采样”。必须先流式写完整 header（`datetime`加锁定 key），再把每个采样写成一条固定列数的数据行，并只在该行换行成功后递增`rowsWritten`。实体核验应以标准CSV解析同时交叉 header key、每行列数、UTC时间、meta selectedCount/rowsWritten/cleanClose；HTTP STOPPED或肉眼首行不足以证明列没有错位。

- L-127：大DBC分页不能先读全量record再丢弃非本页数据。无搜索时应先以index元数据计算匹配/分页，只读取当前页record；generation级缓存必须同时校验manifest、selection、index尺寸/CRC来源，缓存失配就回退完整验证。搜索天然需要全扫描，必须单独量测并如实保留边界。

- L-128：HTTP快速受理与TF事务完成是两种证据。selection/active返回202只能证明请求已校验并排队，必须继续轮询candidate token/runtime并核对generation、CRC、selected数和lastResult；写请求不能因传输错误自动重试。将长TF持久化移出HTTP等待可提高页面响应稳定性，但不能写成底层写入只需几毫秒。

- L-129：STM32/FatFs缓冲扩大不是免费优化。4 KiB多扇区实验即使理论上减少调用次数，也可能破坏当前DMA/cache/驱动合同并造成TF锁死；必须用实板正反例决定，失败方案立即恢复到明确32字节对齐的512 B，并重新完成构建、烧录和冷启动验证。

- L-130：网页“所有功能”验收需要状态恢复和会话边界。每轮应覆盖真实文件选择、上传、selection、激活及所有控制面，写后以独立GET回读；第一轮关闭全部标签后再新建第二轮，最后还要用接口循环和W5500 lifecycle计数审计。页面提示、HTTP202或单次成功都不能替代最终runtime与安全状态。
