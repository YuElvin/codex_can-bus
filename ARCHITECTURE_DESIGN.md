# STM32H750 CAN/CAN-FD 数据采集解析网关软件架构设计方案

本文档按当前 `can_bus_W5500` 工程状态同步更新。旧 LAN8720/RMII/lwIP
路线已经停止使用；当前硬件主路径是 `STM32H750VBTx + W5500 + MCP2562FD +
TF 卡 + W25Q128 + FreeRTOS`。

## 1. 当前基线和边界

### 1.1 已验证硬件基线

| 模块 | 当前接口 | 验证状态 | 结论 |
| --- | --- | --- | --- |
| W5500 网络 | SPI2: PB13/PB14/PB15，PB12 CS，PB11 RST，PA7 INT | 已验证 | W5500 `VERSIONR=0x04`，静态 IP `192.168.1.88`，主机 ping 通过 |
| CAN 收发器 | FDCAN2: PB5 RX，PB6 TX，经 MCP2562FD 到 USBCAN-2E-U | 已验证 | Windows CANtest 可接收开发板周期帧，也可发送帧被开发板收到 |
| FDCAN1 | PD0 RX，PD1 TX | 诊断/保留 | 当前用于 internal/external loopback 诊断，不作为已验证外部主通道 |
| TF 卡 | SDMMC1 | 已验证 | 当前固件跳过 PA8 检卡，保守 SDMMC 配置下读写 smoke test 通过；默认 `/www/index.html` 可通过 HTTP 读取 |
| W25Q128 | QUADSPI | 已验证 | JEDEC ID `EF4018`，最后 4KB 扇区擦写读回通过 |
| USART2 | PD5/PD6，115200 8N1 | 可用 | Windows 侧读取正常；macOS 侧曾出现乱码，必要时以 ST-Link 变量为准 |
| FreeRTOS | SysTick/SVC/PendSV | 基础多任务已上板验证 | 单 `bringup` 任务已验证通过；CAN2 周期任务、W5500 轮询任务和状态打印任务已拆出 |

### 1.2 当前不再使用的旧路径

| 旧路径 | 当前状态 | 说明 |
| --- | --- | --- |
| LAN8720 / RMII | 已移除 | MDIO/HAL/bit-bang 均无有效 PHY 响应，硬件风险过高 |
| ETH HAL / LwIP / ethernetif | 不在活动固件路径 | 网络功能改由 W5500 SPI 和后续 WIZnet/socket 层承担 |
| PA1/PA2/PC1/PC4/PC5 RMII 相关脚 | 已释放 | PB11/PB12/PB13/PA7 已复用于 W5500 |

### 1.3 一期目标

一期目标从“LAN8720 + lwIP 网关”调整为：

1. 用 FreeRTOS 承载已验证的 W5500、CAN2、TF、W25Q128 功能。
2. 以 FDCAN2 + MCP2562FD 作为外部 CAN 主通道，支持经典 CAN 500 kbit/s 起步。
3. 用 W5500 提供静态 IP 网络服务，先实现状态查询和文件/配置接口，再扩展完整 Web UI。
4. TF 卡保存 Web、DBC、日志、配置文件；W25Q128 保存关键配置备份和最小恢复信息。
5. 保持可反汇编验证、可 ST-Link 读取诊断变量、可分模块回归验证。

## 2. 分层架构

| 层级 | 模块 | 职责 |
| --- | --- | --- |
| BSP/HAL | 时钟、GPIO、FDCAN1/FDCAN2、SPI2、SDMMC1、QUADSPI、USART2、NVIC | 外设初始化、中断入口、底层寄存器/HAL 适配 |
| OS/驱动服务 | FreeRTOS、W5500 驱动、FatFs、时间服务、状态监控 | 任务调度、网络收发、文件系统串行化、诊断状态 |
| 核心业务 | CAN RX/TX、DBC 解析/编码、信号缓存、日志、规则引擎、配置管理 | 数据采集、解析、控制、持久化 |
| 接口层 | W5500 socket 服务、HTTP/REST、静态文件服务 | Web 配置、文件上传下载、状态查询 |
| 前端 | `/www/index.html`、`app.js`、`style.css` | 单页中文界面，轻量无框架 |

## 3. 存储与内存策略

| 资源 | 一期用途 | 策略 |
| --- | --- | --- |
| 内部 Flash 128KB | 启动、HAL 裁剪版、FreeRTOS、W5500 驱动、FatFs、HTTP API、核心业务 | 严禁放 Web/DBC/日志；裁剪不用外设；减少 printf 浮点 |
| DTCM RAM | 实时任务栈、CAN 队列、规则引擎临时状态 | CPU 访问快，但 DMA 不可访问 |
| AXI SRAM / RAM_D1 | FreeRTOS heap、DBC 数据库、信号缓存、HTTP 临时缓冲、日志缓冲 | 大块数据优先放此处；当前 FreeRTOS heap 先放 RAM_D1 |
| D2 SRAM | 后续 DMA buffer 预留 | 当前 W5500 SPI、保守 SDMMC 路径不依赖 ETH DMA |
| TF 卡 | `/www/`、`/dbc/`、`/log/`、`/config/` | 一期主要资源存储介质 |
| QSPI W25Q128 | 配置备份、最小 Web/恢复信息、版本信息 | 当前只做读写验证；正式使用前移除上电擦写测试扇区 |

当前 DBC 候选读回 + 最小 active 激活 + 运行态双槽快照源码编译基线：FLASH 约 49.27%，RAM_D1 约 34.91%。后续每次引入网络服务、HTTP、DBC、信号缓存或日志，都要复查 Flash/RAM 水位。

## 4. FreeRTOS 任务设计

### 4.1 当前迁移状态

当前已经手动接入 FreeRTOS Kernel V10.6.2。外设初始化仍在调度器启动前完成，
`main()` 创建一个 `bringup` 任务并启动 `vTaskStartScheduler()`。

当前 `bringup` 任务顺序：

1. `w25q128_bringup_run()`
2. `can_bringup_run()`
3. `can_external_bringup_run()`
4. `can2_analyzer_bringup_run()`
5. `w5500_bringup_run()`
6. `tf_card_bringup_run()`
7. `w5500_http_load_active_dbc()` 尝试从 `/dbc/active.dbc` 加载运行态 DBC 快照
8. 创建独立 CAN2 周期任务和 W5500 轮询任务
9. 原 `bringup` 任务继续每秒打印状态

单任务阶段已经上板验证 `g_freertos_task_started/g_freertos_loop_count` 和各硬件状态正常。基础多任务拆分也已上板验证任务启动和 loop 递增。当前 CAN2 周期轮询已取得 active DBC 快照：TX self-test 与外部 CANtest RX FIFO 复用同一解码函数，但分别写入固定 self-test 与外部 RX `SignalCache`，HTTP、日志和规则只消费外部 RX 缓存。本轮已创建独立 `LogTask` 并移除 bringup 监控循环的直接 CSV 写入：任务每 100 ms 运行、每 1 秒取最多两项、768 B 缓冲在 512 B 或 5 秒时单批 flush。任务初始化一次性选择默认或 recovery 路径；当前现场默认路径已验证，recovery 分支待真实错误触发。另有 50 ms 最小 RuleTask：短临界区复制外部缓存快照，复用 portable `rule_engine` 集中驱动 PE7/PE8；Relay1 使用固定高滞回 `marker on=42434/off=42432` 与 1000 ms 连续匹配延时，实测 42434 置位、42433 保持、42432 释放，停帧超过 1500 ms 两路安全回低。默认关闭的 ST-Link 两路手动覆盖和单规则配置槽都只用于最小目标侧验收：reload 成功才原子替换一条规则，失败保留旧 engine；不构成文件配置或完整规则接口。

### 4.2 目标任务拆分

| 任务 | 优先级 | 栈建议 | 触发 | 职责 | 共享资源 |
| --- | ---: | ---: | --- | --- | --- |
| `CanRxTask` | 高 | 1024-1536 words | FDCAN2 RX 中断/semaphore | 从 RX FIFO 搬运帧，入 `can_rx_q`，统计错误 | `can_rx_q` |
| `CanTxTask` | 高 | 1024-1536 words | `can_tx_q` + 周期 timer | 发送原始帧/周期帧，处理 TX FIFO 满 | `can_tx_q` |
| `DbcDecodeTask` | 高 | 3072-4096 words | `can_rx_q` | DBC 查表、信号解码、更新 `SignalCache` | DBC 快照、信号缓存 |
| `RuleTask` | 中高 | 2048 words | 20-50ms 周期 | 继电器规则、超时保护、默认状态 | 信号缓存、规则快照、继电器 |
| `LogTask` | 中 | 1024 words | 100ms 调度、1s 采样 + 512B/5s flush | 最多两项 CSV 行缓冲，单批写 TF 卡 | FatFs mutex、768B 日志 buffer |
| `NetTask` | 中 | 3072-4096 words | W5500 socket 事件/轮询 | W5500 socket 服务、连接维护 | W5500 mutex/socket 状态 |
| `HttpTask` | 中低 | 4096-6144 words | HTTP 请求 | REST、静态文件、上传下载 | FatFs mutex、配置 mutex、命令队列 |
| `ConfigTask` | 低 | 2048 words | 命令队列 | 配置校验、保存、备份到 W25Q128 | `cfg_cmd_q`、FatFs mutex、QSPI mutex |
| `MonitorTask` | 低 | 1024 words | 500ms/1s | LED 心跳、状态统计、看门狗、诊断打印 | 系统状态 |

优先级原则：CAN 收发和解码不被 Web、TF 卡和 W5500 长操作阻塞；FatFs 和 W25Q128 写操作串行化；Web 读取快照，不长时间持锁。

## 5. 数据流设计

### 5.1 CAN 接收

目标架构中，FDCAN2 中断只释放 semaphore 或设置通知；`CanRxTask` 从硬件 FIFO 取帧，转换为统一 `CanFrame`，写入 `can_rx_q`；`DbcDecodeTask` 按 `(ide,id)` 查当前 DBC，生成 `SignalValue`，用双缓冲更新 `SignalCache`；规则、日志、Web 读取缓存快照。当前最小实现尚未建队列：`can2_analyzer_poll()` 从 RX FIFO 得到帧后直接解码，成功发送的 `0x321` 也进入同一函数作 TX self-test；两者由独立来源计数区分。

### 5.2 CAN 发送

Web/API 或周期发送生成 `TxRequest`；原始帧直接入 `can_tx_q`；DBC 发送先按当前 DBC 将物理值反向编码到 data，再入队；`CanTxTask` 统一调用 FDCAN2 HAL 发送并记录结果。

### 5.3 W5500 网络

当前 W5500 bring-up 已完成 SPI 寄存器级验证和 ping。最小 HTTP 状态接口已经完成：

1. 已引入 W5500 socket0 TCP 80 最小轮询服务。
2. 已实现并烧录验证 `GET /api/status`、`GET /api/can/status` 和 `POST /api/dbc/upload`。
3. 已实现 `/www/index.html` 默认页的分块静态读取；DBC 上传当前保存 `/dbc/candidate.dbc`，再从 TF 读回候选文件并用 portable `dbc_parse_text()` 返回解析报告；无请求体 `POST /api/dbc/active` 已烧录验证，可把有效候选写入 `/dbc/active.dbc`，并在启动/激活后从 active 文件读回解析到运行态双槽 DBC 快照。
4. 最后增加配置保存、周期发送、规则接口。

不再使用 lwIP `netif`、`ethernetif_input()` 或 ETH DMA 描述符路径。

### 5.4 DBC 切换

当前 HTTP 上传仍是单请求体最小实现：`POST /api/dbc/upload` 只接受 1024 字节以内 text body，先写 `/dbc/upload.write.tmp`，然后把旧候选 `/dbc/candidate.dbc` 备份为 `/dbc/candidate.prev.dbc`，再 rename 新候选；新候选 rename 失败时尝试把旧候选恢复。上传成功后当前固件从 TF 读回 `/dbc/candidate.dbc`，调用 portable `dbc_parse_text()` 填充静态候选 `DbcDatabase` 并生成报告。无请求体 `POST /api/dbc/active` 已烧录验证：再次读回候选并确认 `errors=0` 后，写 `/dbc/active.write.tmp`，把旧活动 `/dbc/active.dbc` 备份到 `/dbc/active.prev.dbc`，再 rename 新活动；失败时沿用 FatFs helper 的恢复逻辑并保持当前活动文件不被主动覆盖。激活成功后固件再次从 `/dbc/active.dbc` 读回，解析到非活动运行态槽，只有 `errors=0` 才切换 active DBC 指针、active slot 和 generation；加载失败或无效文件不替换既有运行态快照。该快照现已接入 CAN2 的最小解码、SignalCache 和只读 `GET /api/signals`，但仍没有日志、规则或配置任务。

### 5.5 日志

当前源码由独立 `LogTask` 每 1 秒从 `SignalCache` 复制最多两项，CSV 列保持 `updated_ms,key,value,raw,unit,quality`；使用 768 B RAM 行缓冲，达到 512 B 或 5 秒后才在 FatFs mutex 下单批追加。初始化读取 `/log/signal.csv`：成功或 `FR_NO_FILE` 固定该路径，其他读取错误固定 `/log/signal-recovery.csv`；`g_log_path_mode/g_log_path_switch_count/g_log_active_file_size` 用于诊断。失败时记录 `g_log_failure_count/g_log_drop_count` 并清空本批，不实现重试、轮换、下载、HTTP 配置或队列。当前默认路径已持续写入；recovery 分支待实机验证。

## 6. 共享资源与同步

| 资源 | 机制 | 原因 |
| --- | --- | --- |
| CAN RX/TX | FreeRTOS Queue，固定深度，如 RX 128、TX 64 | 中断/任务解耦，背压可统计 |
| W5500 SPI/socket | 单 W5500 任务或 mutex | 防止多个任务同时访问 SPI/socket 寄存器 |
| 信号缓存 | 当前为 CAN2 轮询独占的单个 `SignalCache`；后续改双缓冲 + 版本号 + 短临界区换指针 | 当前只验证写入；Web/规则/日志并发读者接入前保证一致快照 |
| 当前 DBC | RCU 风格指针切换 + `dbc_mutex` 管理生命周期 | 切换时不中断解码 |
| FatFs/TF | 全局 `fs_mutex` + 单次操作超时 | 避免并发损坏文件系统 |
| W25Q128 | `qspi_mutex` + ConfigTask 串行写 | 防止配置备份与其他 QSPI 操作冲突 |
| 配置文件 | `config_mutex` + ConfigTask 串行保存 | 防止多请求交叉写 |
| 周期发送/规则列表 | 写时复制快照 | 执行任务使用稳定数组 |
| 继电器状态 | `relay_mutex` + 最终状态集中提交 | 保证手动/安全/自动优先级一致 |
| 系统事件 | EventGroup | TF mounted、W5500 link、DBC active、CAN bus off、log enabled 等状态广播 |

## 7. DBC 解析器设计

核心结构：`DbcDatabase{messages[], id_index[], signal_pool, string_pool}`；
`DbcMessage{id, ide, dlc, name_off, first_signal, signal_count}`；
`DbcSignal{name_off, start_bit, bit_len, byte_order, is_signed, factor, offset, min, max, unit_off}`。

解析策略：

- 逐行读取 DBC，不把整文件载入 RAM。
- 一期只识别 `BO_` 和 `SG_`，其他行跳过并计数。
- 解析后构建按 CAN ID 排序索引或开地址 hash，查找键为 `(ide,id)`。
- 超过上限立即失败，不激活半解析 DBC。
- Motorola 位序必须用独立 bit iterator，读写共用同一序列，单元测试覆盖跨字节场景。
- signed 信号提取后按 bit_len 做符号扩展；反向编码先 clamp，再 `(physical-offset)/factor` 四舍五入。

## 8. REST API 设计

| Method | Path | 功能 | 摘要 |
| --- | --- | --- | --- |
| GET | `/api/status` | 系统状态 | `{uptime,rtos,w5500,tf,qspi,heap}` |
| GET | `/api/can/status` | CAN 状态 | `{bitrate,busOff,tec,rec,rx,tx}` |
| GET | `/api/signals` | 实时信号 | 当前最小实现：短临界区复制最多 2 个缓存项，返回 `{items:[{key,value,raw,unit,updated_ms,quality}],count}`；尚无 filter/page |
| POST | `/api/dbc/upload` | 上传 DBC | 当前最小实现为 1024 字节以内 text body，保存 `/dbc/candidate.dbc`，旧候选备份 `/dbc/candidate.prev.dbc`，随后读回候选并用 portable parser 返回 `{ok,data:{candidate,candidateBackup,active,activeBackup,maxBytes,bytes,lines,messages,signals,skipped,errors,valid}}`；后续再扩展 multipart 或分片 |
| GET | `/api/dbc` | DBC 列表 | `{files:[...]}` |
| POST | `/api/dbc/active` | 激活 DBC | 当前最小实现为无请求体命令，激活 `/dbc/candidate.dbc` 到 `/dbc/active.dbc`，随后读回 active 文件并切换运行态 DBC 快照，返回 `{ok,data:{candidate,active,activeBackup,bytes,lines,messages,signals,skipped,errors,valid,activated,runtimeGeneration}}`；后续再扩展指定文件和运行态信号缓存切换 |
| GET | `/api/dbc/runtime` | 运行态 DBC 诊断 | 当前最小实现返回 `{ok,data:{active,loaded,generation,activeSlot,lastResult,bytes,lines,messages,signals,skipped,errors}}`，用于验证 `/dbc/active.dbc` 是否已进入运行态快照 |
| DELETE | `/api/dbc/{name}` | 删除 DBC | `{ok}` |
| POST | `/api/can/send_raw` | 原始发送 | `{id,ide,fd,brs,data}` |
| POST | `/api/can/send_signal` | 按 DBC 发送 | `{message,signals:{rpm:1200}}` |
| GET/POST/PUT/DELETE | `/api/can/periodic` | 周期发送管理 | `{items:[...]}` |
| POST | `/api/log/control` | 日志开关/周期 | `{enabled,period_ms}` |
| GET | `/api/log/files` | 日志列表 | `{files:[{name,size,time}]}` |
| GET | `/api/log/download/{name}` | 下载日志 | `text/csv` |
| DELETE | `/api/log/{name}` | 删除日志 | `{ok}` |
| GET/POST/PUT/DELETE | `/api/rules` | 规则 CRUD | `{rules:[...]}` |
| POST | `/api/relay/manual` | 手动控制 | `{mode:"manual",relay:1,state:true}` |
| GET/PUT | `/api/settings` | 系统设置 | `{ip,can,fd,time,defaults}` |
| POST | `/api/reboot` | 重启 | `{delay_ms:500}` |

API 统一返回 `{ok:true,data}` 或 `{ok:false,error:{code,message}}`。大列表分页；上传和下载必须分块处理，避免阻塞 CAN 任务。

## 9. 前端页面计划

SPA 使用 hash tab：概览、实时数据、DBC 管理、CAN 发送、日志、规则、系统设置。

资源限制：

- HTML/CSS/JS 总量建议 <150KB，压缩后放 `/www/`。
- 不引入大型框架和图标库。
- 表格分页，不一次渲染数千信号。
- 实时数据默认 1s 轮询，二期再换 SSE/WebSocket。
- 错误提示中文化，上传显示字节进度。

## 10. 文件系统与配置

启动流程：

1. 初始化 SDMMC/FatFs；当前固件跳过 PA8 检卡。
2. 挂载成功后创建 `/www /dbc /log /config /sys`。
3. 读取配置并校验版本和 CRC。
4. 失败则加载默认值，并尝试从 W25Q128 备份恢复。

配置文件：

- `config.json`：`version`、`device_name`、`network{ip,mask,gateway}`、`can{nominal_bitrate,data_bitrate,fd,brs,filters[]}`、`dbc{active}`、`log{enabled,period_ms,max_file_mb}`、`relay_defaults{r1,r2}`、`time{epoch,timezone}`、`web{refresh_ms}`。
- `rules.json`：规则列表、手动状态、默认安全状态。
- `can_tx.json`：周期发送列表。

写配置采用 `file.tmp -> flush -> rename`；失败时保持旧配置，Web 返回错误，运行态继续使用内存配置。

## 11. 继电器规则引擎

规则结构：`Rule{id, enabled, signal_key, op, threshold, on_th, off_th, relay, action_state, delay_ms, timeout_ms, safe_state, default_state, latched_state, condition_since}`。

执行周期 20-50ms。优先级固定：

1. 手动模式最高。
2. 关键信号超时进入 safe_state。
3. 自动规则判断。
4. 无命中则 default_state。

滞回规则用 `off->on` 采用 `on_threshold`，`on->off` 采用 `off_threshold`；延时要求条件连续满足 N ms，条件断开清零计时。上电先 GPIO 低电平关闭，配置加载后才应用默认状态。

## 12. 开发阶段拆分

| 阶段 | 目标 | 当前状态 | 可验证结果 |
| --- | --- | --- | --- |
| 1 | 时钟/GPIO/USART/LED | 已完成基础路径 | 串口或 ST-Link 状态变量可读 |
| 2 | TF 卡 SDMMC + FatFs | 已验证 | smoke test 读写通过 |
| 3 | W5500 SPI bring-up | 已验证 | `VERSIONR=0x04`、网络参数回读、ping `192.168.1.88` |
| 4 | CAN2 外部收发 | 已验证 | CANtest 收到 `0x321`，开发板收到 Windows 发帧 |
| 5 | W25Q128 QSPI | 已验证 | JEDEC ID、擦写读回通过 |
| 6 | FreeRTOS 单任务迁移 | 已验证 | `g_freertos_task_started=1`、loop 计数递增，各硬件状态仍为 0 |
| 7 | FreeRTOS 多任务拆分 | 部分已验证 | CAN2 周期任务、W5500 轮询任务、状态打印任务独立运行；完整队列/mutex 待实现 |
| 8 | W5500 socket/HTTP status | 已验证 | `/api/status`、`/api/can/status` 可用 |
| 9 | TF 静态文件和 DBC 上传 | 部分已验证 | `/www/index.html` 默认静态页可访问；`POST /api/dbc/upload` 可保存 `/dbc/candidate.dbc`，并已在源码中接入候选读回 + portable parser 报告；`POST /api/dbc/active` 最小激活和 `GET /api/dbc/runtime` 运行态快照诊断已烧录验证 |
| 10 | 实时解码和日志 | 部分已验证 | active DBC、外部 RX、`/api/signals` 和旧最小 CSV 追加已验证；独立 LogTask 默认路径批量写已烧录验证，recovery 分支待真实错误触发 |
| 11 | 规则/继电器 | 部分已验证 | 最小固定高滞回规则已烧录验证：`on=42434/off=42432`，1000 ms 连续匹配后 PE7 高、42433 保持、42432 释放，PE8 始终低，停帧超过 1500 ms 两路安全回低；ST-Link 单规则 reload 已验证失配、恢复与失败保留旧规则；完整文件配置待做 |
| 12 | 稳定性测试 | 待做 | 长跑、拔卡、断网、总线关闭、大文件上传 |

## 13. 风险与规避

| 风险 | 规避 |
| --- | --- |
| 128KB Flash 不足 | 裁剪 HAL/FatFs/HTTP；禁用浮点 printf；Web/DBC/日志放 TF；必要时 W25Q128 放备份资源 |
| FreeRTOS 多任务后旧硬件验证回归 | 先拆 CAN2/W5500 低风险周期任务，上板读 `g_freertos_*` 和各模块状态后再拆 TF/QSPI/HTTP |
| W5500 socket 层阻塞 CAN | 网络服务单任务或 mutex，限制单次处理时间，CAN 任务优先级更高 |
| TF/FatFs 并发损坏或文件错误 | 全局 `fs_mutex`，LogTask 与 HTTP/DBC 共用该锁；曾读到 CSV `FR_DISK_ERR=1`，最终默认路径运行已恢复成功；不自动修复，保留一次性 recovery 选择和失败/丢弃诊断 |
| W25Q128 上电自检擦写正式数据 | 正式配置备份前移除或改成按需触发最后扇区测试 |
| DBC 上传占 RAM | 流式落盘、逐行解析、固定池，不整文件读入 |
| Motorola 编码错误 | 独立 bit iterator，PC 单元测试先行 |
| CAN-FD timing 复杂 | 一期外部通道先用 FDCAN2 classic CAN 500 kbit/s；FDCAN1 FD 保留诊断 |
| 串口在 macOS 偶发乱码 | 关键结论以 ST-Link 诊断变量、主机 ping、CANtest 实测为准 |
| Web 阻塞实时任务 | HTTP 只操作快照/队列，长文件操作分块，限制并发连接 |
| 继电器误动作 | 上电默认低；优先级集中决策；手动最高；超时安全；状态变化记录日志 |

## 14. 维护规则

- 引脚以 `pin_configuration.md`、`.ioc`、源码和实际验证记录共同确认。
- 修改固件后必须编译并反汇编检查关键逻辑。
- 烧录后优先用 ST-Link 全局变量确认各模块状态，再结合外部工具验证。
- `CONVERSATION_SUMMARY.md` 必须记录每次关键修改、问题点、验证命令和结果。
- 当前分支 `codex/W5500` 是 W5500 方案主线，不再把 LAN8720 问题作为活动软件路线推进。
