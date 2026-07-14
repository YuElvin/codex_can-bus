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
| W25Q128 | QUADSPI | 已验证 | 默认启动只读 JEDEC ID `EF4018`；`0x00FFF000` 诊断区与 `0x00FFE000`/`0x00FFD000` 单规则双槽均已按显式请求验证 |
| USART2 | PD5/PD6，115200 8N1 | 可用 | Windows 侧读取正常；macOS 侧曾出现乱码，必要时以 ST-Link 变量为准 |
| FreeRTOS | SysTick/SVC/PendSV | 基础多任务已上板验证 | 单 `bringup` 任务已验证通过；CAN2 接收服务每 50 ms、诊断发送每 1 s，W5500 轮询和状态打印任务已拆出 |

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
| QSPI W25Q128 | 单规则配置备份、最小 Web/恢复信息、版本信息 | 默认启动只读识别；`0x00FFF000` 固定为诊断区，`0x00FFE000`/`0x00FFD000` 为单规则双槽，通用备份必须定义新的多记录模型 |
| TF RuleFile | `/config/rule.conf` 单规则启动覆盖 | v1 固定文本字段，最大 256 字节；有效文件优先于 QSPI，缺失只创建不覆盖，非法保持当前安全配置 |

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
9. 创建低优先级 `MonitorTask`，由其每秒打印状态；`bringup` 任务随后删除自身

单任务阶段已经上板验证 `g_freertos_task_started/g_freertos_loop_count` 和各硬件状态正常。基础多任务拆分也已上板验证任务启动和 loop 递增。CAN2 服务每 50 ms 清空 RX FIFO 并更新 active DBC 外部快照，同时维持每 1 s 一次 `0x321` 诊断发送：TX self-test 与外部 CANtest RX FIFO 复用同一解码函数，但分别写入固定 self-test 与外部 RX `SignalCache`，HTTP、日志和规则只消费外部 RX 缓存。本轮已创建独立 `LogTask` 并移除 bringup 监控循环的直接 CSV 写入：任务每 100 ms 运行、每 1 秒取最多两项、768 B 缓冲在 512 B 或 5 秒时单批 flush。任务初始化一次性选择默认或 recovery 路径；插卡冷启动下默认路径已验证。TF 卡硬件操作边界为先下电再插拔，运行中热插拔/recovery 不支持。新增一次性 `TfTask` 复用 `tf_card_bringup_run()` 与默认页面确保动作；bring-up 以 1 ms `vTaskDelay` 等待完成，最多 5000 ms，超时进入 `Error_Handler()`，任务继续复用既有 `fs_mutex`，不改变 recovery 语义。50 ms ConfigTask 现把兼容的 ST-Link 诊断/规则保存请求转换为 `ConfigCommand`，经深度 2 队列由单消费者执行；规则候选在入队时快照。默认启动只读 JEDEC 后从 `0x00FFE000` 主槽与 `0x00FFD000` 备用槽选择有效且 sequence 最新的 v2 记录；显式 ST-Link 保存只擦写另一槽并读回比较，v1 主槽记录可兼容迁移。只有成功读回后才请求 RuleTask reload，保存失败不替换旧 engine；`0x00FFF000` 仍只用于诊断。该队列不包含 HTTP、TF 文件、CRUD、多规则或完整配置事务。另有 50 ms 最小 RuleTask：短临界区复制外部缓存快照，复用 portable `rule_engine` 集中驱动 PE7/PE8；Relay1 使用固定高滞回 `marker on=42434/off=42432` 与 1000 ms 连续匹配延时，实测 42434 置位、42433 保持、42432 释放，停帧超过 1500 ms 两路安全回低。默认关闭的 ST-Link 两路手动覆盖和单规则配置槽都只用于最小目标侧验收：reload 成功才原子替换一条规则，失败保留旧 engine；不构成文件配置或完整规则接口。

### 4.2 目标任务拆分

补充的已验证配置边界：ST-Link 只写独立 pending 候选，ConfigTask 收到保存请求后快照候选；只有 QSPI 双槽擦写、写入和读回比较全部成功，才提交 RuleTask 运行态参数并请求 reload。保存失败或非法候选不得替换旧 engine 或旧运行态参数。

| 任务 | 优先级 | 栈建议 | 触发 | 职责 | 共享资源 |
| --- | ---: | ---: | --- | --- | --- |
| `CanRxTask` | 高 | 1024-1536 words | FDCAN2 RX 中断/semaphore | 从 RX FIFO 搬运帧，入 `can_rx_q`，统计错误 | `can_rx_q` |
| `CanTxTask` | 高 | 1024-1536 words | `can_tx_q` + 周期 timer | 发送原始帧/周期帧，处理 TX FIFO 满 | `can_tx_q` |
| `DbcDecodeTask` | 高 | 3072-4096 words | `can_rx_q` | DBC 查表、信号解码、更新 `SignalCache` | DBC 快照、信号缓存 |
| `RuleTask` | 中高 | 2048 words | 20-50ms 周期 | 继电器规则、超时保护、默认状态 | 信号缓存、规则快照、继电器 |
| `LogTask` | 中 | 1024 words | 100ms 调度、1s 采样 + 512B/5s flush | 最多两项 CSV 行缓冲，单批写 TF 卡 | FatFs mutex、768B 日志 buffer |
| `NetTask` | 中 | 3072-4096 words | W5500 socket 事件/轮询 | W5500 socket 服务、连接维护 | W5500 mutex/socket 状态 |
| `HttpTask` | 中低 | 4096-6144 words | HTTP 请求 | REST、静态文件、上传下载 | FatFs mutex、配置 mutex、命令队列 |
| `ConfigTask` | 低 | 1024 words | 50 ms 轮询 | 消费固定深度 `ConfigCommand` 队列，当前承载 W25Q128 诊断和单规则保存；后续才扩展配置校验、文件保存和备份 | `cfg_cmd_q`、未来 FatFs mutex、QSPI mutex |
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

当前源码由独立 `LogTask` 每 1 秒从 `SignalCache` 复制最多两项，CSV 列保持 `updated_ms,key,value,raw,unit,quality`；使用 768 B RAM 行缓冲，达到 512 B 或 5 秒后才在 FatFs mutex 下单批追加。初始化读取 `/log/signal.csv`：成功或 `FR_NO_FILE` 固定该路径，其他读取错误固定 `/log/signal-recovery.csv`；`g_log_path_mode/g_log_path_switch_count/g_log_active_file_size` 用于诊断。失败时记录 `g_log_failure_count/g_log_drop_count` 并清空本批，不实现重试、轮换、下载、HTTP 配置或队列。TF 卡只能在开发板下电状态插拔；真实热插拔恢复不属于产品功能，临时验证代码已移除。

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

阶段 A 的单规则文件不是通用配置服务：`/config/rule.conf` 只允许 `version=1`、`onThreshold`、`offThreshold`、`delayMs`、`timeoutMs` 五个字段各一次，值为无符号十进制 `uint32_t`；文件不超过 256 字节，且满足 `onThreshold > offThreshold`、`delayMs <= timeoutMs`。缺失时在已挂载 TF 上显式确保 `/config` 后用 `FA_CREATE_NEW` 创建当前有效单规则的最小文本；有效文件解析成功后通过既有 RuleTask reload 覆盖 QSPI/编译默认值，其他错误只记录状态并保留当前值。

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
| 7 | FreeRTOS 多任务拆分 | 部分已验证 | 既有任务、CAN RX/TX、DbcTask、TfTask 和 ConfigTask 队列边界已烧录验证；单规则 HTTP 配置已复用 ConfigTask 完成闭环；完整配置服务仍待实现 |
| 8 | W5500 socket/HTTP status | 已验证 | `/api/status`、`/api/can/status` 可用 |
| 9 | TF 静态文件和 DBC 上传 | 部分已验证 | `/www/index.html` 默认静态页可访问；`POST /api/dbc/upload` 可保存 `/dbc/candidate.dbc`，并已在源码中接入候选读回 + portable parser 报告；`POST /api/dbc/active` 最小激活和 `GET /api/dbc/runtime` 运行态快照诊断已烧录验证 |
| 10 | 实时解码和日志 | 部分已验证 | active DBC、外部 RX、`/api/signals` 和旧最小 CSV 追加已验证；独立 LogTask 默认路径批量写已烧录验证，recovery 分支待真实错误触发 |
| 11 | 规则/继电器 | 部分已验证 | 固定高滞回、延时、超时、单规则 QSPI 双槽和 RuleTask reload 已验证；新增 `GET/POST /api/rule/config` 已验证 HTTP→ConfigTask→QSPI→RuleTask→复位加载；RuleFile v1 有效/缺失路径已板端验证，非法板端输入未注入；多规则仍待做 |
| 12 | 稳定性测试 | 待做 | 长跑、拔卡、断网、总线关闭、大文件上传 |

## 13. 风险与规避

| 风险 | 规避 |
| --- | --- |
| 128KB Flash 不足 | 裁剪 HAL/FatFs/HTTP；禁用浮点 printf；Web/DBC/日志放 TF；必要时 W25Q128 放备份资源 |
| FreeRTOS 多任务后旧硬件验证回归 | 先拆 CAN2/W5500 低风险周期任务，上板读 `g_freertos_*` 和各模块状态后再拆 TF/QSPI/HTTP |
| W5500 socket 层阻塞 CAN | 网络服务单任务或 mutex，限制单次处理时间，CAN 任务优先级更高 |
| TF/FatFs 并发损坏或文件错误 | 全局 `fs_mutex`，LogTask 与 HTTP/DBC 共用该锁；TF 卡必须在开发板下电后插拔，运行中热插拔/recovery 不支持。当前插卡冷启动时格式化、重新挂载、bring-up 和默认日志连续写入均成功；正式产品不保留临时格式化、重挂载或状态旁路 |
| W25Q128 诊断擦写正式数据 | 默认启动已不擦写；`0x00FFF000` 固定诊断保留区，ConfigTask/正式备份必须另选地址并串行化 |
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

## 15. 阶段 B RuleFile v2 与运行态边界

TF `/config/rules-v2.conf` 是唯一 v2 规则集文件，固定 512 字节上限、恰好两条规则和 `Can2Data.marker >= threshold` 语义；每条规则只允许 relay、threshold、action、delayMs、timeoutMs、safeState、priority 七字段。解析器先写局部候选，全部校验成功后才构造完整 `RuleEngine`；RuleTask 通过一次临界区复制完成原子 reload，不再以 `engine.rules[0]` 作为运行模型。

同一继电器按最大 priority 选择唯一 winner。没有匹配候选时保持默认态，匹配但延时未到时保持 winner 的 default state，输入超时使用 winner 的 safeState；手动覆盖在规则评价前生效并使 winner 诊断为 `0xff`。运行态对外保留 `g_rule_task_rule_count`、`g_rule_task_winner_rule0/1`、generation/reload、relay/GPIO/safe 变量供精确 GDB 读取。

启动顺序仍先加载 v1/QSPI 单规则，再检查 v2。v2 有效并 reload 完成才覆盖当前规则；缺失时在 `/config` 存在后使用 `FA_CREATE_NEW` 写固定默认文本，但 `v2_created=1/load_result=1` 的当次启动继续 v1/QSPI fallback，下一次复位才加载；无效、超限或读取失败同样不改变既有安全配置。不写入 QSPI 多规则，不改变 HTTP 单规则 API、CAN 发送逻辑、HTTP 并发模型或 LogTask recovery。

本阶段已客观验证：构建 FLASH=`80192 B / 128 KB`、RAM_D1=`235048 B / 512 KB`，主机 CTest `14/14`；有效 v2、marker=42434、marker=42435 的 priority winner、手动覆盖和暂停输入 timeout 均有外部 CAN/GPIO 现场证据。首次 v2 缺失创建板端未观察，但 `FA_CREATE_NEW` 与创建后 v1/QSPI fallback 已由 ELF/诊断路径确认。
## 本轮验证补充：W5500、HTTP 与 CAN TX 队列边界

W5500 状态轮询与 HTTP socket0 轮询由两个独立 50 ms FreeRTOS 任务执行，共用 `g_w5500_mutex` 串行化 SPI/socket 访问。`DbcTask` 以 50 ms 周期从深度 1 reload 命令队列取出一次性 active DBC 请求，`TfTask` 负责一次性 TF mount/smoke/default-page 初始化；CAN2 周期任务将固定 `0x321` 帧投递到深度 1 TX 队列，现有 `CanDecodeTask` 消费后调用 `can_port_send`，不新增任务栈；ConfigTask 另有深度 2 配置命令队列。运行态 DBC 加载解析/槽切换与 CAN2 解码仍共用 `w5500_http_dbc_lock()`；FatFs 操作继续共用 `fs_mutex`。本轮 TX 队列现场 `ready=1`、入队/出队 `0x2c/0x2c`、drop=0，CAN2 `tx=0x2d/rx=0x1b3/errors=0/sendResult=0`；RX 队列 `0x104/0x104/drop=0`。顺序 ping 2/2，`/api/status`、`/api/can/status`、`/api/signals` 均 HTTP 200。

## 本轮验证补充：单规则 HTTP 配置闭环

本轮新增 `GET /api/rule/config` 和 `POST /api/rule/config`，只控制已有单规则四个参数，不改变 QSPI 地址布局、记录格式或任务数量。POST 校验阈值和时间关系，写入 pending 候选后等待 ConfigTask 队列完成 QSPI 双槽保存，再等待 RuleTask reload 完成后返回 200；这不是规则文件、多规则或完整 CRUD。

重新烧录输出 `Programming Finished`、`Verified OK`，目标电压约 `3.251976 V`。顺序 ping 为 2/2；GET 初始返回 `42434/42432/1000/1500/generation=1`；POST `42435/42433/1100/1600` 返回 200、generation=2，GET 读回相同参数。GDB 读数为 QSPI save result/count=`0/1`、ConfigTask enqueue/dequeue/drop=`1/1/0`、RuleTask generation/reload=`2/0`；复位后非默认参数仍在，config load result=0、RuleTask generation/load=`1/1`。随后恢复默认参数，`/api/can/status` 仍为 200 且 errors、bus-off、TEC、REC、sendResult 均为 0。LogTask recovery 未触发。

## 本轮验证补充：ConfigTask 通用命令队列边界

ConfigTask 现使用深度 2、元素大小 20 字节的 `ConfigCommand` 队列。既有 `g_w25q128_diagnostic_request` 和 `g_rule_task_config_save_request` 仅作为兼容入口；ConfigTask 先把请求转换为命令并快照规则候选，再由同一任务单消费者执行 `w25q128_diagnostic_run()` 或 `w25q128_rule_config_save()`。本轮源码、`./scripts/verify.sh`、13/13 host CTest、ELF 反汇编和 OpenOCD `Programming Finished/Verified OK/Resetting Target` 均完成；固件 FLASH=`75552 B / 128 KB = 57.64%`，RAM_D1=`231136 B / 512 KB = 44.09%`。

现场 GDB 使用精确 ELF 地址写入后，配置队列 `ready=1`，两类命令累计 `enqueue=2/dequeue=2/drop=0/command=2`；规则保存结果为 `g_rule_task_config_result=0`、`g_w25q128_config_save_count=1`、`g_w25q128_config_save_result=0`，因此“配置队列/规则保存”完成。第一次 GDB 直接写变量因 ELF 无 debug symbols 只得到 `unknown type`，没有作为证据；第二次使用无类型地址写入后才纳入结论。

必须分开记录底层结果：历史现场的 `g_w25q128_diagnostic_result=0xffffffff`、`g_w25q128_erase_count=0` 当时不能证明诊断执行，且 `0xffffffff` 是初始化哨兵，不是诊断函数的返回码。2026-07-13 使用当前正式 ELF 的精确地址只写一次请求并等待擦除窗口后，实读 `last_command=1`、队列 `enqueue/dequeue=1/1/drop=0`、`diagnostic_count=1`、`diagnostic_result=0`、`erase_count=1`、`test_addr=0x00FFF000`、JEDEC=`0x00EF4018`；规则双槽 `config_addr=0x00FFE000/sequence=7`、save/load count 均不变。由此关闭 QSPI diagnostic 失败复核，不把诊断区用于正式配置。GDB 读取后已恢复运行；随后 ping 为 2/2，顺序 `/api/status`、`/api/can/status`、`/api/signals` 均 HTTP 200，CAN 两次读数由 `tx/rx=48/469` 增长到 `55/537`，errors、bus-off、TEC、REC 和 sendResult 均为 0。该验证未人为破坏 TF 文件，也未触发 LogTask recovery。

## 本轮验证补充：RuleFile v3 受限双槽 CRUD

`/config/rules-v3.conf` 是固定两槽 RuleFile：`version=3`、`ruleCount=2`，两个 slot 均保留 enabled、relay、threshold、action、delayMs、timeoutMs、safeState、priority 八字段；disabled 不装入 RuleEngine。启动按 v3→v2→v1→QSPI 回退，只有有效 v3 才停止回退，v3 缺失不自动创建。HTTP socket0 顺序接口固定为 GET 列表/详情、POST 创建 disabled 槽、PUT 完整替换、DELETE 禁用；写请求仅更新 pending，ConfigTask 单消费者执行 TF `.tmp`/`.prev` 原子替换，成功后才触发 RuleTask 原子 engine 复制与 generation 递增。

最终固件的主机 CTest=`14/14`，FLASH=`85496 B`、RAM_D1=`239664 B`；目标板烧录输出 `Programming Finished`、`Verified OK`、`Resetting Target`，电压 `3.250368 V`。真实验收：DELETE slot1 后 GDB rule_count=`1`，POST 恢复后 rule_count=`2`；PUT slot0 非默认 `42436/1100/1600` 跨复位保持；恢复 `42434/1000/1500` 后，非法 delay 大于 timeout 返回 HTTP 400 且 generation/current 不变；POST form `slot=2` 返回 HTTP 404 且最终两槽未改变。最后 ping 2/2、`/api/status`、`/api/can/status`、`/api/signals` 均 HTTP 200。初版曾在 ConfigTask 触发 HardFault：build-engine 的 3856 B 自动对象覆盖相邻 TCB；改为直接构造输出 engine 后反汇编栈帧为 120 B，并经相同实板链路复验通过。

## 阶段 F 补充：30 分钟静态长跑发现默认日志写失败

本轮没有修改源码。正式 `can_bus_gateway_stm32h750.hex` 重新烧录并输出 `Programming Finished`、`Verified OK`、`Resetting Target` 后，目标在无热插拔、无断网、无 bus-off 注入且无需用户改变 CANtest/TF 的条件下连续运行约 30 分钟。起始约 10 秒时各任务已启动，LogTask `write/flush/failure/drop=4/4/0/0`、默认文件 size=`29330`；终态 Rule/Config/HTTP/W5500/Log/Monitor/CanDecode 任务循环均严格增长，CAN TX/RX 队列无丢弃、errors/busOff/TEC/REC/sendResult 均为 0，W5500 network/link 均为 1，顺序 ping 与 `/api/status`、`/api/can/status`、`/api/signals`、`/api/dbc/runtime` 均 HTTP 200。

但默认 LogTask 未通过稳定性条件：终态文件 size=`35490`、write/flush=`15/396`，failure/drop=`381/1526`，最后 `g_log_last_result=1`、`g_tf_csv_write_result=1`、`g_tf_write_open_result=1`；SD 最近诊断为 `DCOUNT=512`、`STA=0x1000`、`ErrorCode=0x80000000`、HAL status=`3 (HAL_TIMEOUT)`。这不是热插拔 recovery 路径，也不能由网络/CAN 故障解释。阶段 F 的长跑子项因此失败，下一步只允许定位默认插卡路径的失败点和最小区分验证，不能直接加入重试、remount、状态旁路或扩大到其他异常场景。

F-2 重新烧录同一正式 HEX 后，以约 5 秒间隔只读采样，在 35 秒已捕获首次失败：起始 LogTask `write/flush/failure=9/9/0`、size=`40512`，首次失败为 `write/flush/failure/drop=15/16/1/5`、size=`43872`、`g_log_last_result=1`、`g_tf_csv_write_result=1`。后续同一运行态继续失败，`g_tf_write_open_result=1`。首次自动采样的主机解析曾少取一字并误判成功写次数为 failure；该错误未写目标、立即修正且不作为结论。当前 SD 最近状态在不同采样间为 `DCOUNT=448/STA=0x29000/ErrorCode=0x20/HAL=1` 或 `DCOUNT=512/STA=0x1000/ErrorCode=0x80000000/HAL=3`，说明共享“最近”诊断不能可靠关联到 append 的具体阶段。下一最小改动仅允许为 SD 操作来源与 append stage 增加诊断字段，仍不改变写入、重试、恢复或卡操作策略。

F-3 只在 TF 平台层加入 `g_tf_sd_last_operation` 和 `g_tf_append_stage`，没有改变 HAL/FatFs 调用、timeout、锁、挂载、缓存、写入或返回语义。完整构建 CTest=`14/14`、FLASH=`85616 B / 128 KB = 65.32%`、RAM_D1=`239672 B / 512 KB = 45.71%`；ELF 反汇编确认 read/write 标记 `2/3` 分别在原 `HAL_SD_ReadBlocks/HAL_SD_WriteBlocks` 前，append 保留 `f_open→f_lseek→f_write→f_close` 原顺序，失败阶段在 close 后恢复。烧录 Verify 后 50 秒首次失败为 LogTask `write/flush/failure/drop=15/16/1/5`、`last/csv result=1/1`，而诊断为 append stage=`2(open)`、SD operation=`2(read)`、open result=`1`、`DCOUNT=448/STA=0x29000/ErrorCode=0x20/HAL=1`。顺序 ping=`2/2`，`/api/status`、`/api/can/status`、`/api/signals`、`/api/dbc/runtime` 均 HTTP 200，CAN 无错误。结论限于 `f_open` 触发的底层 read 失败；根因仍未定位。

F-4 只读审计将 `f_open` 的路径固定为 `f_open→find_volume→follow_path→dir_find→move_window→disk_read→SD_read→BSP_SD_ReadBlocks_DMA→HAL_SD_ReadBlocks`；HAL 返回 `HAL_ERROR` 后经 `MSD_ERROR/RES_ERROR` 映射为 `FR_DISK_ERR`。`ErrorCode=0x20` 是 `HAL_SD_ERROR_RX_OVERRUN`，而 `STA=0x29000` 包含 `DPSMACT|RXFIFOHF|RXFIFOF`，与该单扇区 512 B 轮询读的 receive FIFO overrun 相容。BSP 名称虽为 DMA，实际调用阻塞轮询 HAL read；尚没有证据把触发原因归为卡、信号、CAN 负载或 IRQ 优先级。下一步只记录读 LBA、块数、调用/失败计数及 HAL 前后状态/寄存器，不修改传输模式或参数。

F-5 只在同一 read 调用前后记录 LBA、块数、call/failure 计数以及 State/Context/ErrorCode 和 STA/DCOUNT/MASK/DCTRL/CLKCR 快照；构建与反汇编确认原 polling read、1000 ms timeout 和返回路径未变。烧录后 MCU reset 的首次 5 秒读取已是持续失败：request=`LBA 3826/block 1`、read call/failure=`54/4`、append stage/op=`2/2`，前后快照同为 `State=1/Context=0/ErrorCode=0x80000000/DCOUNT=512/STA=0x45000`。诊断字段工作，但该状态不能替代板级冷启动的首错；下一步必须在 TF 保持插入时下电再上电后读取。

F-6 已在用户完成板级断电至少 10 秒、再上电的条件下只读采样完成。冷启动基线 `failure=0/call=51`，约 32 秒首个失败为 `failure=1/call=102`，请求为 `LBA=3826/blocks=1`、append stage/op=`2/2`。同一调用前快照为 `State=1/Context=0/ErrorCode=0/STA=0/DCOUNT=0/MASK=0/DCTRL=0x90/CLKCR=16`，调用后为 `State=1/Context=0/ErrorCode=0x20/STA=0x29000/DCOUNT=448/MASK=0/DCTRL=0x92/CLKCR=16`。因此正式证据是 `f_open` 的单扇区 polling read 首次发生 `HAL_SD_ERROR_RX_OVERRUN`；根因未定，下一步仅审计 FIFO、IRQ、FreeRTOS 中断优先级与缓存维护实现。首错后同机网络回归 ping/API 未通，但板端 `bringup=0/VERSIONR=4/PHYCFGR=0xBF/link=1/network_configured=1`、服务任务循环递增；主机 en2 ARP incomplete，故不把这次网络现象归因于 SD 或固件。

F-7 的只读实现审计确认：`BSP_SD_ReadBlocks_DMA` 名称虽保留，实际调用 `HAL_SD_ReadBlocks(..., 1000ms)`；HAL 轮询 `RXFIFOHF` 后由 CPU 读取 32 B FIFO，`RXOVERR` 即清标志并返回 `HAL_ERROR`。故障时 `MASK=0`，不依赖 `SDMMC1_IRQn`；最终运行参数为 1-bit、上升沿、无硬件流控、`ClockDiv=16`，不是 CubeMX 初始 4-bit/`ClockDiv=2`。FDCAN2 未启用 NVIC 通知，接收由任务轮询；项目未见 DCache/MPU 或 SD DMA cache-maintenance 启用。故下一步唯一实验是临时临界区包裹原 read，比较冷启动首错，不改变 timeout、DMA 或扇区语义。

F-8 的 `taskENTER_CRITICAL→HAL_SD_ReadBlocks→taskEXIT_CRITICAL` 已构建、反汇编和烧录，但冷启动后 LogTask 停在 HAL：连续 30 秒诊断不变，暂停 PC=`0x08009c54` 位于 `HAL_SD_ReadBlocks`，任务循环冻结。原因是该临界区屏蔽 tick，破坏 HAL polling timeout；实验代码已撤回，正式 F-5 HEX 重新烧录 Verify。F-9 若实施，只能以 `vTaskSuspendAll/xTaskResumeAll` 防止任务切换而不屏蔽 SysTick，仍保持原 timeout、参数和快照。

F-9 在外部 CANtest 持续 `0x321`/marker=`42434` 后实际进入连续日志 append；前 17 次 write 成功，read call 从 46 增至 120，随后 LogTask failure=`6`、read failure=`5`，最新 `LBA=3826/blocks=1` 读前后为 `ErrorCode=0x80000000/STA=0x45000/DCOUNT=512`，operation=`2`、append stage=`2`。它越过 F-6 的 call=102 仍失败，故仅抑制任务切换不能消除 SD 错误。F-9 已删除并重新构建、反汇编、烧录正式 F-5 路径；下一步只审计 SD 时钟、总线宽度、硬件 flow control 与 HAL polling 配置的证据，不直接修改。

F-10 审计结果：BSP 在 `HAL_SD_Init` 前固定 `ClockEdge=RISING`、`ClockPowerSave=DISABLE`、`BusWide=1-bit`、`HardwareFlowControl=DISABLE`、`ClockDiv=16`；HSI64/PLL1Q=100MHz 和 HAL 公式给出 CK≈3.125MHz。F-6 的 `CLKCR=0x10` 正是该设置，`DCTRL=0x90→0x92` 是 512 B polling read 的预期配置。下一单字段实验是只开启 HardwareFlowControl；若无错误只能证明其对 FIFO 节流有帮助，不能代替稳定性完整验收或定论根因。

F-11 只将 `HardwareFlowControl` 改为 ENABLE；构建、反汇编和烧录后，冷启动板端 `CLKCR=0x20010`。在外部 CANtest `0x321` 持续输入下，read call 从 31 增至 150、LogTask write/flush 从 1/1 增至 25/25、failure 保持 0，且 ping、`/api/status`、`/api/can/status`、`/api/signals` 同次通过。该结果允许保留 HWFC 配置并进入 30 分钟无现场操作的 F-12 耐久；尚未证明长期稳定或根因，F-12 若失败不得以本结果声称修复。
