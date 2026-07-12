# 项目计划

本文件只记录当前计划和验收边界；长历史保留在 `CONVERSATION_SUMMARY.md`，详细架构设计保留在 `ARCHITECTURE_DESIGN.md`。

## 当前目标

把 `STM32H750VBTx + W5500 + MCP2562FD + TF 卡 + W25Q128 + FreeRTOS` 作为当前主线，停止继续推进旧 `LAN8720/RMII/lwIP` 路线。

一期目标：

1. 用 FreeRTOS 承载已经验证过的 W5500、FDCAN2、TF 卡、W25Q128 功能。
2. 以 FDCAN2 `PB5/PB6` 加 MCP2562FD 作为外部 CAN 主通道，先稳定支持 classic CAN 500 kbit/s。
3. 用 W5500/SPI2 提供静态 IP 网络服务，先做状态查询，再扩展文件、DBC、日志和 Web UI。
4. 用 TF 卡保存 Web、DBC、日志、配置文件；用 W25Q128 保存关键配置备份和最小恢复信息。
5. 所有固件阶段保持可编译、可反汇编核查、可 ST-Link 读取诊断变量。

## 阶段计划

| 阶段 | 目标 | 当前状态 | 验收标准 |
| --- | --- | --- | --- |
| 1 | 基础 GPIO/USART/SWD | [客观已验证] | ST-Link 可连接，状态变量或串口可读 |
| 2 | TF 卡 SDMMC + FatFs | [客观已验证] | smoke test 写读通过 |
| 3 | W5500 SPI bring-up | [客观已验证] | `VERSIONR=0x04`、网络参数回读、主机 ping `192.168.1.88` 通过 |
| 4 | FDCAN2 外部 CAN 收发 | [客观已验证] | Windows CANtest 可收开发板帧，开发板可收 Windows 发帧 |
| 5 | W25Q128 QSPI | [客观已验证] | 默认启动只读 JEDEC ID；`0x00FFF000` 保留诊断区仅显式触发擦除、写入、读回匹配 |
| 6 | FreeRTOS 单任务迁移 | [客观已验证] | 已烧录确认 `g_freertos_task_started=1`、loop 递增、各硬件状态仍通过 |
| 7 | FreeRTOS 多任务拆分 | [部分客观已验证] | CAN2、MonitorTask、LogTask、RuleTask、ConfigTask 已独立运行；W5500 状态轮询与 HTTP socket0 轮询已拆为独立 50 ms 任务，并由 W5500 mutex 保护，现场 HTTP/CAN/网络回归通过；完整队列、TF/DBC 任务和通用配置保存仍待做 |
| 8 | W5500 socket/HTTP status | [客观已验证] | `/api/status`、`/api/can/status` 可访问 |
| 9 | TF 静态文件和 DBC 上传 | [部分客观已验证] | `/www/index.html` 默认静态页可访问，静态页读取已改为 512 字节循环分块；`POST /api/dbc/upload` 可保存到 `/dbc/candidate.dbc` 并返回 portable parser 报告；`POST /api/dbc/active` 最小激活已烧录验证；启动/激活后 active DBC 运行态快照和 `GET /api/dbc/runtime` 已烧录验证 |
| 10 | 实时解码、日志、规则 | [部分客观已验证] | active DBC、外部 CANtest RX 和 `/api/signals` 已上板验证；LogTask 默认路径批量写已验证。默认读失败时的 recovery 选择已编译/单测，但本次未触发，仍待现场覆盖；规则待实现 |

## 非目标

- 当前不再把 LAN8720/RMII/lwIP 作为活动软件路线。
- 当前不引入大型前端框架或复杂网络栈抽象。
- 当前不把 W25Q128 最后 4KB 测试扇区直接当作正式配置存储。

## 主要风险

| 风险 | 当前处理 |
| --- | --- |
| 128KB Flash 空间紧张 | Web/DBC/日志放 TF，固件裁剪 HAL 和字符串 |
| FreeRTOS 多任务后硬件功能回归 | CAN2/W5500 低风险周期任务已上板复核；再拆 TF/QSPI/HTTP 前必须先定义共享资源保护 |
| W5500 网络服务阻塞 CAN | 当前 HTTP 仍是 socket0 单连接轮询；后续用单网络任务或 mutex 限制临界区 |
| TF/FatFs 并发损坏或文件错误 | LogTask、HTTP、DBC 共用全局 `fs_mutex`；LogTask 单批失败仅丢弃并计数。默认路径读取失败时仅在启动选择 recovery，后续不切换；当前本次启动默认读取成功，recovery 分支待实机触发 |
| DBC 上传过大或半包 | 当前仅支持 1024 字节以内、单连接完整请求体；body 未收全时等待下一轮轮询，不作为完整上传系统；候选文件替换采用 `/dbc/upload.write.tmp` -> `/dbc/candidate.dbc`，旧候选备份为 `/dbc/candidate.prev.dbc` |
| DBC 解码验证边界 | 当前 active DBC 已接入 CAN2 解码和 `SignalCache`；TX self-test 与外部 CANtest `0x321` RX 均已验证，诊断继续保留来源计数，不能混写两类证据 |
| DBC/缓存 RAM 占用 | 当前使用候选 scratch `DbcDatabase`、运行态双槽 `DbcDatabase`、单个 128 项 `SignalCache` 和 768 B 日志缓冲，RAM_D1 为 38.83%；扩大 parser 上限、缓存或日志前必须复查内存 |
| QSPI 诊断擦写配置数据 | `0x00FFF000` 已保留为诊断区；单规则配置使用 `0x00FFE000` 主槽和 `0x00FFD000` 备用槽，默认启动不擦写，显式 ST-Link 请求由 ConfigTask 串行执行；通用备份不得使用诊断扇区 |
