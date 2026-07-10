# 当前上下文

更新时间：2026-07-10 18:35 +08:00

## 当前仓库

- 工作目录：`/Users/elvin/Desktop/project/can_bus`，当前实际解析到 `/Users/elvin/Desktop/project/can_bus_W5500`。
- 当前分支：`codex/W5500`。
- 当前主线：W5500/SPI2 + FDCAN2/MCP2562FD + TF 卡 + W25Q128 + FreeRTOS。

## 已验证状态

| 模块 | 状态 | 证据摘要 |
| --- | --- | --- |
| W5500 | [客观已验证] | `VERSIONR=0x04`，静态 IP `192.168.1.88`，主机 ping 通过 |
| W5500 HTTP/API | [客观已验证] | 已烧录验证 `GET /api/status`、`GET /api/can/status`、`POST /api/dbc/upload` 返回 `HTTP/1.1 200 OK` JSON；未知路径返回 404 JSON |
| FDCAN2 外部 CAN | [客观已验证] | Windows CANtest 可收到开发板 `0x321` 周期帧；开发板收到 Windows 发帧；2026-07-08 22:46 分析仪收发打开后复查 `sendResult=0`、`rx_count=508`、`tx_count=728` |
| TF 卡 | [客观已验证] | SDMMC/FatFs smoke test 写读通过 |
| TF 静态文件服务 | [部分客观已验证] | 已启用 FatFs mutex，缺省创建 `/www/index.html`；`GET /` 和 `GET /index.html` 返回 `text/html` 默认页；固件已改为按文件大小循环 512 字节分块读取并多次 socket 发送 |
| DBC 上传最小接口 | [客观已验证] | `POST /api/dbc/upload` 保存 `/dbc/candidate.dbc` 后从 TF 读回候选并调用 portable `dbc_parse_text()` 生成报告；已烧录验证返回 `bytes=164/lines=4/messages=1/signals=2/errors=0/valid=true`，ST-Link 读数 `candidate_load_result=0/candidate_valid=1` |
| DBC 活动文件激活 | [客观已验证] | `POST /api/dbc/active` 无请求体最小命令已烧录验证：读回候选、portable parser 确认为 `errors=0` 后写入 `/dbc/active.dbc`，旧活动文件备份到 `/dbc/active.prev.dbc`；有效候选返回 `activated=true`，无效候选返回 `HTTP 400 candidate_invalid` |
| DBC 运行态快照 | [客观已验证] | 固件启动后从 `/dbc/active.dbc` 读回并解析到运行态 `DbcDatabase` 双槽快照；2026-07-10 重烧录后 `GET /api/dbc/runtime` 返回 `loaded=true/generation=1/bytes=165/messages=1/signals=2/errors=0`，有效激活后 generation 递增，无效候选激活不替换 runtime |
| 最小 DBC 解码到 SignalCache | [部分客观已验证] | 已将运行态 active DBC 接入 CAN2 周期轮询的 TX self-test 和外部 RX 共用解码路径；2026-07-10 最终重烧录后以 active `0x321`/2 信号 DBC 读取到 `tx_self_test=41`、`SignalCache=2`、`signal_updates=82`、`decode_errors=0`。外部 CANtest → FDCAN2_RX 的解码计数本轮为 0，尚未完成现场 RX 解码验证 |
| W25Q128 | [客观已验证] | JEDEC ID `EF4018`，最后 4KB 扇区擦写读回通过 |
| FreeRTOS 单任务 | [客观已验证] | 已烧录验证 `g_freertos_task_started=1`、`g_freertos_loop_count` 递增，W5500/CAN/TF/W25Q128 状态保持通过 |
| FreeRTOS 基础多任务拆分 | [客观已验证] | CAN2 周期任务、W5500 轮询任务和状态打印任务已编译/反汇编/烧录复核；任务启动标志为 1，loop 均递增 |

## 当前阻断项

- FreeRTOS 完整多任务架构仍未完成：TF/FatFs、QSPI、HTTP、配置保存、日志和 DBC 任务尚未拆分，也未引入队列/mutex。
- W25Q128 当前 bring-up 自检会擦写 `0x00FFF000` 最后 4KB 扇区，正式配置存储前必须改为按需触发或换成保留测试区。

## 当前风险

- 后续继续拆分 TF/FatFs、QSPI、HTTP 和配置任务时，共享资源必须加串行化或 mutex。
- 当前 HTTP 服务仍是 socket0 单连接最小实现，不支持并发连接、目录映射、HTTP Range、分块传输编码或通用上传；当前只把 `/` 和 `/index.html` 映射到 `/www/index.html`，只支持 `POST /api/dbc/upload` 的 1024 字节以内单请求体 DBC 上传。
- DBC 上传当前生成候选文件 `/dbc/candidate.dbc`，旧候选保留为 `/dbc/candidate.prev.dbc`；当前 active 文件是本轮验证使用的 `0x321`/2 信号 DBC。运行态快照已接入 CAN2 轮询的最小解码和 `SignalCache`，但没有实时信号 API、日志、规则或 ConfigTask。
- 当前最小解码器在成功发送的 `0x321` 周期诊断帧上执行 TX self-test，也在外部 RX while-loop 上执行同一函数；本轮仅验证前者。不能把 `g_can2_dbc_tx_self_test_frame_count` 当作外部 RX 证据；必须等待 CANtest 发匹配帧并确认 `g_can2_dbc_rx_frame_count` 递增。
- 当前固件保留候选 scratch `DbcDatabase`、运行态双槽 `DbcDatabase` 和单个 `SignalCache`，最新固件 RAM_D1 占用为 `202512 B / 512 KB = 38.63%`；后续引入日志缓存、并发读者或扩大 parser 上限前必须继续复查内存并补齐同步边界。
- 2026-07-08 22:34 当前复查中 `/api/can/status` 可访问，但现场读数为 `rx=0/errors=487/tec=128/sendResult=1`；2026-07-08 22:46 用户打开 CAN 分析仪收发后复查恢复为 `sendResult=0`、`rx_count=508`、`tx_count=728`、`tec=0`、`bus_off=0`。
- `CONVERSATION_SUMMARY.md` 已经较长，但仍按用户要求保留为完整对话摘要；当前快照以本文件为准。
- macOS 串口曾出现乱码，硬件结论优先用 ST-Link 变量和外部工具确认。

## 下一步建议

1. 请在 Windows CANtest 向 `FDCAN2_RX/PB5` 发送当前 active DBC 匹配的 classic CAN `0x321`（8 字节，例如 `C2 A5 34 12 02 03 04 05`），再读取 `g_can2_dbc_rx_frame_count`、`g_can2_dbc_signal_update_count`、`g_can2_dbc_cache_count`；只有 RX 计数递增才可称为外部 RX 解码已验证。
2. 引入 QSPI 配置保存或日志任务前，继续补齐 mutex/队列边界。
3. 后续扩展静态文件服务时再处理目录映射、Content-Type 映射和并发连接，不要把当前 socket0 实现当作完整 Web 服务。
