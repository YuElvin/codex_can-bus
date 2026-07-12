# 当前上下文

更新时间：2026-07-13（阶段 12 稳定性基线已完成；外部 CAN RX 与 LogTask recovery 分支仍未验证）

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
| 最小 LogTask | [默认路径客观已验证，recovery 待验证] | 独立任务每秒采样、768 B 缓冲在 512 B 或 5 秒 flush；本次启动默认 `/log/signal.csv` 读取成功，连续读数写/flush `3→12`、大小 `15846→20976`、失败/丢弃为 0。恢复路径选择已编译/单测，但现场未触发 |
| FDCAN2 外部 CAN | [客观已验证] | Windows CANtest 可收到开发板 `0x321` 周期帧；开发板收到 Windows 发帧；2026-07-08 22:46 分析仪收发打开后复查 `sendResult=0`、`rx_count=508`、`tx_count=728` |
| TF 卡 | [客观已验证] | SDMMC/FatFs smoke test 写读通过 |
| TF 静态文件服务 | [部分客观已验证] | 已启用 FatFs mutex，缺省创建 `/www/index.html`；`GET /` 和 `GET /index.html` 返回 `text/html` 默认页；固件已改为按文件大小循环 512 字节分块读取并多次 socket 发送 |
| DBC 上传最小接口 | [客观已验证] | `POST /api/dbc/upload` 保存 `/dbc/candidate.dbc` 后从 TF 读回候选并调用 portable `dbc_parse_text()` 生成报告；已烧录验证返回 `bytes=164/lines=4/messages=1/signals=2/errors=0/valid=true`，ST-Link 读数 `candidate_load_result=0/candidate_valid=1` |
| DBC 活动文件激活 | [客观已验证] | `POST /api/dbc/active` 无请求体最小命令已烧录验证：读回候选、portable parser 确认为 `errors=0` 后写入 `/dbc/active.dbc`，旧活动文件备份到 `/dbc/active.prev.dbc`；有效候选返回 `activated=true`，无效候选返回 `HTTP 400 candidate_invalid` |
| DBC 运行态快照 | [客观已验证] | active `0x321`/2 信号 DBC 启动加载正常；本轮 `POST /api/dbc/active` 返回 `activated=true/runtimeGeneration=2`，随后读到 `generation/load=2/2`、`valid=1/result=0` |
| 最小 DBC 解码到 SignalCache | [客观已验证] | active DBC 已接入 CAN2 TX self-test 和外部 RX；本轮新增 DBC mutex，reload 与 CAN 解码共享锁，TX self-test decode/matched/updates 增长、decode errors=0；外部 RX 本轮未验证 |
| W25Q128 | [客观已验证] | 默认启动只读 JEDEC ID `EF4018`，未触发擦除；保留诊断区 `0x00FFF000` 的显式 ST-Link 擦写读回匹配 |
| FreeRTOS 单任务 | [客观已验证] | 已烧录验证 `g_freertos_task_started=1`、`g_freertos_loop_count` 递增，W5500/CAN/TF/W25Q128 状态保持通过 |
| FreeRTOS 基础多任务拆分 | [客观已验证] | MonitorTask 已烧录接管 1 s 状态打印；CAN2 仍保持每 50 ms FIFO 接收、每 1 s 诊断发送；W5500 状态轮询与 HTTP socket0 轮询已拆为两个 50 ms 任务，并由同一 W5500 mutex 串行化，现场计数和 HTTP 回归通过 |
| 最小 RuleTask/继电器 | [客观已验证] | 已烧录 50 ms RuleTask：固定 1000 ms 延时、1500 ms 超时安全低、ST-Link 手动 OFF 覆盖优先、固定高滞回均已验证；单规则配置 reload 与 QSPI 显式保存、读回和复位加载均已实测 |
| 最小 QSPI 规则配置备份 | [客观已验证] | v1 单槽记录兼容；v2 使用 `0x00FFE000` 主槽和 `0x00FFD000` 备用槽，含 sequence、参数和 checksum。ConfigTask 两次交替保存、读回、复位加载、无效请求拒绝及候选/运行态隔离均已 ST-Link 实测 |

## 当前阻断项

- recovery 分支尚未在实机触发：先前读到 `/log/signal.csv` `FR_DISK_ERR=1`，但最终固件启动时大小读取返回 0，因此按策略选择默认路径。禁止人为破坏原文件以强行覆盖该分支；它保留为待异常条件复验项，不阻断已完成的 LogTask 默认路径验收。
- FreeRTOS 完整多任务架构仍未完成：DbcTask 已以 active DBC reload 窄命令独立运行并实机验证，但 TF/FatFs 专用任务仍未拆分，也未引入通用队列；HTTP 仍是 socket0 单连接最小实现。ConfigTask 现串行执行显式 QSPI 诊断和单规则 QSPI 保存，但不构成通用配置服务。
- W25Q128 已将 `0x00FFF000` 固定为显式诊断保留区，`0x00FFE000`/`0x00FFD000` 固定为单规则配置双槽；默认 bring-up 不擦写。v2 已实测交替写入、读回、sequence 选择、最新槽损坏后回退到较旧槽，以及两槽均无效后保留默认配置；后续通用配置仍需另行定义多记录演进与命令来源。
- 当前最小 RuleTask、固定 1000 ms 延时、固定高滞回、仅供 ST-Link 验收的手动优先级、单规则 reload 及 QSPI 保存成功后的自动 reload 已现场验证。ST-Link 写入的 pending 候选参数在 ConfigTask 保存读回成功前不得改变 RuleTask 运行态；完整规则文件、HTTP 控制仍未实现，不得将该诊断入口表述为完整规则管理功能。
- 阶段 12 稳定性基线已完成：当前固件重新烧录 Verify 通过，两次任务计数增长，关键模块状态为 0，ping 与三个只读 API 串行返回 HTTP 200；本轮外部 CAN RX 为 0，不能作为外部 RX 验证。
- CANtest 现场状态已补充：开始发送前曾未观察到开发板数据；开始发送后连续 HTTP 读数为 `tx=53→68`、`rx=240→397`、`errors=0`、`tec=0`、`rec=0`、`busOff=0`、`sendResult=0`、`poll=52→67`。该结果作为当前外部 CAN RX 与周期 TX/ACK 证据，不表述为代码修复；本轮无源码修改。

## 当前风险

- 后续继续拆分 TF/FatFs、QSPI、DBC 和配置任务时，共享资源必须加串行化或 mutex；当前 W5500 状态/HTTP 两任务已共用 `g_w5500_mutex`，FatFs 仍使用既有 `fs_mutex`。
- 当前 HTTP 服务仍是 socket0 单连接最小实现，不支持并发连接、目录映射、HTTP Range、分块传输编码或通用上传；当前只把 `/` 和 `/index.html` 映射到 `/www/index.html`，只支持 `POST /api/dbc/upload` 的 1024 字节以内单请求体 DBC 上传。
- DBC 上传当前生成候选文件 `/dbc/candidate.dbc`，旧候选保留为 `/dbc/candidate.prev.dbc`；当前 active 文件是本轮验证使用的 `0x321`/2 信号 DBC。运行态快照已接入 CAN2 接收服务、`SignalCache`、最多两项的只读实时信号 API、LogTask 和最小内置 RuleTask；RuleTask 额外保留默认关闭的 ST-Link 手动覆盖验收入口，ConfigTask 仅承载已验证的单规则 QSPI 双槽保存。
- 当前最小解码器在成功发送的 `0x321` 周期诊断帧上执行 TX self-test，也在外部 RX while-loop 上执行同一函数；两条路径使用独立 `SignalCache`，HTTP/Log/RuleTask 只消费外部 RX 缓存，且仍必须用独立来源计数区分验证。
- 当前固件保留候选 scratch `DbcDatabase`、运行态双槽 `DbcDatabase`、外部 RX 与 TX self-test 两个固定 `SignalCache`、768 B 日志缓冲和 LogTask 栈；本轮构建 RAM_D1 为 `226984 B / 512 KB = 43.29%`。后续扩大缓存或引入并发读者前必须继续复查内存并补齐同步边界。
- LogTask 不再由 bring-up 监控循环直接写 CSV：它每 100 ms 调度、每 1 秒复制最多两项、缓冲达到 512 B 或 5 秒才在 `fs_mutex` 下单批 `f_open/f_lseek/f_write/f_close`。初始化只读一次默认文件：成功或 `FR_NO_FILE` 选 `/log/signal.csv`，其他失败选 `/log/signal-recovery.csv` 并记录 path mode/switch count/active size；随后不再切换。写失败后清空本批、累计失败和丢弃，不做重试、轮换、下载 API、HTTP 配置或通用队列。临时 probe 代码已移除。
- 2026-07-08 22:34 当前复查中 `/api/can/status` 可访问，但现场读数为 `rx=0/errors=487/tec=128/sendResult=1`；2026-07-08 22:46 用户打开 CAN 分析仪收发后复查恢复为 `sendResult=0`、`rx_count=508`、`tx_count=728`、`tec=0`、`bus_off=0`。
- `CONVERSATION_SUMMARY.md` 已经较长，但仍按用户要求保留为完整对话摘要；当前快照以本文件为准。
- macOS 串口曾出现乱码，硬件结论优先用 ST-Link 变量和外部工具确认。

## 下一步建议

1. 保持默认文件不被人为破坏；仅在未来实际大小读取失败时，复核 `mode=1/switch_count=1`、recovery 大小和成功写入增长。
2. recovery 分支未上板覆盖前，不加入文件轮换、下载、HTTP 配置、重试或队列；未来发生真实默认路径读取失败时再复验该分支。
3. 后续扩展静态文件服务时再处理目录映射、Content-Type 映射和并发连接，不要把当前 socket0 实现当作完整 Web 服务。
4. 后续规则阶段仅在实际需要时再考虑规则文件来源；不把当前 ST-Link 单规则 reload 或已验证的 QSPI 保存自动 reload 入口直接扩展为未验证的 CRUD/API。
5. ConfigTask 已完成单规则 QSPI 双槽保存、读回、复位加载、候选/运行态隔离及成功自动 reload 最小闭环；后续如扩展为通用配置，先定义多记录模型与正式命令来源，再引入队列、文件读写或 HTTP，不能把当前单规则双槽表述为完整配置管理。
