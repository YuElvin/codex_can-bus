# STM32H750 CAN/CAN-FD 数据采集解析网关软件架构设计方案

## 1. 总体架构

### 1.1 分层

| 层级 | 模块 | 职责 |
|---|---|---|
| BSP/HAL | 时钟、GPIO、FDCAN1、SDMMC1、ETH、QSPI、USART2、Cache/MPU | 外设初始化、DMA 缓冲区布局、中断入口 |
| OS/驱动服务 | FreeRTOS、lwIP、FatFs、内存池、时间服务 | 任务调度、网络栈、文件系统、统一时间戳 |
| 核心业务 | CAN RX/TX、DBC 解析/编码、信号缓存、日志、规则引擎、配置管理 | 数据采集、解析、控制、持久化 |
| 接口层 | HTTP Server、REST API、静态文件服务 | Web 配置、文件上传下载、状态查询 |
| 前端 | `/www/index.html`、`app.js`、`style.css` | 单页中文界面，无框架 |

### 1.2 存储与内存策略

| 资源 | 一期用途 | 策略 |
|---|---|---|
| 内部 Flash 128KB | 启动、HAL 裁剪版、FreeRTOS、lwIP 最小配置、FatFs、HTTP API、核心业务 | 严禁放 Web、DBC、日志；关闭不用外设；减少 printf 浮点；字符串常量集中裁剪 |
| DTCM RAM | CAN RX/TX 队列、规则引擎临时状态、实时任务栈 | CPU 访问快，但 DMA 不可访问 |
| AXI SRAM | DBC 数据库、信号缓存、HTTP 临时缓冲、日志批量缓冲 | 大块数据优先放此处 |
| D2 SRAM | Ethernet DMA 描述符和收发 buffer、SDMMC DMA buffer | MPU 配置为 non-cacheable 或显式 cache clean/invalidate |
| TF 卡 | `/www/`、`/dbc/`、`/log/`、`/config/` | 一期主要资源存储介质 |
| QSPI W25Q128 | 配置备份、最小 Web 资源备份、版本信息；预留 XIP | 一期建议只做读写驱动和备份，不依赖 XIP |

一期边界：单 FDCAN1、REST 轮询、轻量 DBC、CSV 日志、两路继电器。二期通过 `can_if[]`、`transport_if`、`dbc_feature_flags`、`stream_backend` 扩展 FDCAN2、WiFi、WebSocket/SSE、J1939、Multiplex、VAL_、OTA 和登录权限。

## 2. FreeRTOS 任务设计

| 任务 | 优先级 | 栈 | 触发 | 职责 | 队列/共享资源 |
|---|---:|---:|---|---|---|
| `CanRxTask` | 高 | 1024-1536B | FDCAN RX 中断给 semaphore | 从 FDCAN FIFO 取帧，填 `CanFrame`，入 RX 队列，统计错误 | `can_rx_q`，不访问 TF/DBC |
| `DbcDecodeTask` | 高 | 3072-4096B | `can_rx_q` | 查当前 DBC，解码信号，更新缓存，发布日志/规则事件 | DBC 快照、信号缓存、`log_sample_q` |
| `CanTxTask` | 高 | 1536-2048B | `can_tx_q` + 周期 timer | 原始帧/DBC 编码帧发送，处理 TX FIFO 满 | `can_tx_q`、周期发送快照 |
| `RuleTask` | 中高 | 2048B | 20-50ms 周期 | 执行手动/失效保护/自动规则/默认优先级，控制 PE7/PE8 | 信号缓存快照、规则快照、继电器 mutex |
| `LogTask` | 中 | 3072B | 100ms 采样 + buffer 阈值 | 生成 CSV 行，批量写 TF 卡 | FatFs mutex、日志 buffer、信号快照 |
| `HttpTask` | 中低 | 4096-6144B | lwIP 连接 | 静态文件、REST、上传下载 | FatFs mutex、配置 mutex、DBC 管理命令 |
| `ConfigTask` | 低 | 2048B | 命令队列/事件 | 配置校验、保存、备份到 QSPI、应用变更 | `cfg_cmd_q`、FatFs mutex |
| `MonitorTask` | 低 | 1024B | 500ms/1s | LED 心跳、资源统计、总线状态、看门狗喂狗 | 系统状态 |

优先级原则：CAN 收发和解码不被 Web/TF 卡阻塞；所有 FatFs 操作集中由低优先级路径串行化；Web 只读快照，不长时间持锁。

## 3. 数据流设计

CAN 接收：FDCAN 中断只释放 semaphore；`CanRxTask` 搬运硬件 FIFO 到 `can_rx_q`；`DbcDecodeTask` 按 `(id, ide)` 查 `DbcMessage`，生成 `SignalValue`，用双缓冲更新 `SignalCache`；规则、日志、Web 读取缓存快照。

CAN 发送：Web/API 或周期发送生成 `TxRequest`；原始帧直接入 `can_tx_q`；DBC 发送先按当前 DBC 将物理值反向编码到 data，再入队；`CanTxTask` 统一调用 FDCAN HAL 发送并记录结果。

DBC 切换：HTTP 上传流式写入 `/dbc/name.tmp`，校验后改名；ConfigTask 解析到候选 `DbcDatabase` 内存池；解析成功后用指针原子切换 `active_dbc`，旧库延迟释放；失败不影响当前 DBC。

日志：LogTask 按配置周期从信号缓存取快照，写入 RAM 行缓冲；达到 4-16KB 或 1s flush；文件按启动时间命名，TF 不可用时进入 `log_degraded` 状态并丢弃或环形缓存最近 N 行。

关键结构关系：`CanFrame -> DbcMessage -> DbcSignal -> SignalValue -> SignalCache`；`Rule` 引用 `signal_key`；`LogConfig` 决定采样周期；`PeriodicTxItem` 引用 raw frame 或 DBC signal set。

## 4. 共享资源与同步

| 资源 | 机制 | 原因 |
|---|---|---|
| CAN RX/TX | FreeRTOS Queue，固定深度，如 RX 128、TX 64 | 中断/任务解耦，背压可统计 |
| 信号缓存 | 双缓冲 + 版本号 + 短临界区换指针 | Web/规则/日志可无长锁读取一致快照 |
| 当前 DBC | RCU 风格指针切换 + `dbc_mutex` 管理生命周期 | 切换时不中断解码，旧库无人读后释放 |
| FatFs/TF | 全局 `fs_mutex` + 单次操作超时 | FatFs 通常非完全可重入，避免并发损坏 |
| 配置文件 | `config_mutex` + ConfigTask 串行保存 | 防止 Web 多请求交叉写 |
| 周期发送/规则列表 | 写时复制快照 | 执行任务使用稳定数组，Web 编辑不阻塞实时路径 |
| 继电器状态 | `relay_mutex` + 最终状态集中提交 | 保证手动/安全/自动优先级一致 |
| 系统事件 | EventGroup | TF mounted、ETH up、DBC active、CAN bus off、log enabled 等状态广播 |

## 5. DBC 解析器设计

核心结构：`DbcDatabase{messages[], id_index[], signal_pool, string_pool}`；`DbcMessage{id, ide, dlc, name_off, first_signal, signal_count}`；`DbcSignal{name_off, start_bit, bit_len, byte_order, is_signed, factor, offset, min, max, unit_off}`。字符串存入受控 string pool，消息和信号使用固定上限或内存池，例如 512 messages、4096 signals，可由配置裁剪。

逐行解析：按行读取 DBC，不把整文件载入 RAM；只识别 `BO_` 和 `SG_`。`BO_` 提取 message id、name、dlc；`SG_` 提取 signal name、start|len@endian+sign、factor/offset、min/max、unit。其他行跳过并计数。解析后构建按 CAN ID 排序索引或开地址 hash，查找键为 `(ide,id)`。

Intel 提取：DBC start_bit 为 LSB 位置，按 little-endian 位序从 `start_bit` 递增读取 `bit_len` 位组装 raw；写入时反向逐位清零/置位。

Motorola 提取：DBC start_bit 为信号 MSB，按 big-endian DBC 位序遍历：同一字节 bit 从高到低，跨字节跳到下一字节高位。建议实现统一 `dbc_motorola_next_bit(pos)`，测试覆盖 1bit、8bit、12bit、16bit、跨 2/3/8 字节、起点 7/0/3。写入使用同一 bit 序列，按 raw 的 MSB 到 LSB 写入，避免读写定义不一致。

signed 处理：提取 unsigned raw 后按 bit_len 做符号扩展；物理值 `raw * factor + offset`。反向编码先 clamp min/max，再 `(physical-offset)/factor` 四舍五入到整数，signed 检查范围，最后按 byte order 写入。

错误处理：行号、错误码、截断原因写入解析报告；超过上限立即失败；激活新 DBC 必须全部解析成功。Motorola 单元测试用已知 DBC/frame/raw 三元组校验提取和写入互逆。

## 6. REST API 设计

| Method | Path | 功能 | 示例请求/响应摘要 | 模块 |
|---|---|---|---|---|
| GET | `/api/status` | 系统状态 | `{uptime,tf,dbc,relay,heap}` | Monitor |
| GET | `/api/can/status` | CAN 状态 | `{bitrate,busOff,tec,rec,rx,tx}` | CAN |
| GET | `/api/signals?filter=&page=1` | 实时信号 | `{items:[{name,value,unit,ts,timeout}]}` | SignalCache |
| POST | `/api/dbc/upload` | 上传 DBC | multipart，返回 `{ok,report}` | HTTP/DBC |
| GET | `/api/dbc` | DBC 列表 | `{files:[...]}` | FS |
| POST | `/api/dbc/active` | 激活 | `{"file":"x.dbc"}` | DBC/Config |
| DELETE | `/api/dbc/{name}` | 删除 | `{ok}` | FS/DBC |
| POST | `/api/can/send_raw` | 原始发送 | `{id,ide,fd,brs,data}` | CAN TX |
| POST | `/api/can/send_signal` | 按 DBC 发送 | `{message,signals:{rpm:1200}}` | DBC/CAN TX |
| GET/POST/PUT/DELETE | `/api/can/periodic` | 周期发送管理 | `{items:[...]}` | CAN TX/Config |
| POST | `/api/log/control` | 日志开关/周期 | `{enabled,period_ms}` | Log/Config |
| GET | `/api/log/files` | 日志列表 | `{files:[{name,size,time}]}` | FS |
| GET | `/api/log/download/{name}` | 下载 | `text/csv` | HTTP/FS |
| DELETE | `/api/log/{name}` | 删除 | `{ok}` | FS |
| GET/POST/PUT/DELETE | `/api/rules` | 规则 CRUD | `{rules:[...]}` | Rule/Config |
| POST | `/api/relay/manual` | 手动控制 | `{mode:"manual",relay:1,state:true}` | Rule/Relay |
| GET/PUT | `/api/settings` | 系统设置 | `{ip,can,fd,time,defaults}` | Config |
| POST | `/api/reboot` | 重启 | `{delay_ms:500}` | System |

API 统一返回 `{ok:true,data}` 或 `{ok:false,error:{code,message}}`。大列表分页；下载和上传使用流式处理。

## 7. 前端页面组件树

SPA 使用 hash tab：概览、实时数据、DBC 管理、CAN 发送、日志、规则、系统设置。公共组件：顶部状态条、侧边/顶部 Tab、Toast、ConfirmDialog、分页表格、表单校验。

页面/API：概览调用 `/api/status`、`/api/can/status`；实时数据 1s 调 `/api/signals`，搜索本地防抖 300ms；DBC 管理调用 upload/list/active/delete，并显示解析报告；CAN 发送包含 RawForm、SignalForm、PeriodicTable；日志页控制开关、周期、文件列表、下载删除；规则页 RuleEditor 和 RelayManualPanel；设置页读取/保存 `/api/settings` 并触发重启。

资源限制：HTML/CSS/JS 总量建议 <150KB，压缩后放 `/www/`；不用框架和图标库；表格虚拟分页，不一次渲染数千信号；日志列表分页；上传显示字节进度；错误用中文 toast 和字段级提示；避免低于 1s 的实时轮询，二期再换 SSE/WebSocket。

## 8. 文件系统与配置

挂载流程：启动检测 PA8；初始化 SDMMC/FatFs；挂载成功后创建 `/www /dbc /log /config /sys`；读取配置并校验版本和 CRC；失败则加载默认值并尝试从 QSPI 备份恢复。

`config.json`：`version`、`device_name`、`network{ip,mask,gateway}`、`can{nominal_bitrate,data_bitrate,fd,brs,filters[]}`、`dbc{active}`、`log{enabled,period_ms,max_file_mb}`、`relay_defaults{r1,r2}`、`time{epoch,timezone}`、`web{refresh_ms}`。

`rules.json`：`version`、`rules[{id,enabled,signal,op,threshold,on_threshold,off_threshold,relay,action,delay_ms,timeout_ms,safe_state,default_state}]`、`manual{enabled,r1,r2}`。

`can_tx.json`：`version`、`periodic[{id,enabled,period_ms,mode:"raw|dbc",frame{can_id,ide,fd,brs,data},dbc{message,signals}}]`。

CSV 字段：`timestamp_ms,datetime,can_id,ide,dlc,signal_name,physical_value,unit,raw_value,message_name,quality`。写配置采用 `file.tmp -> flush -> rename`；失败时保持旧配置，Web 返回错误，运行态继续使用内存配置。

## 9. 继电器规则引擎

规则结构：`Rule{id, enabled, signal_key, op, threshold, on_th, off_th, relay, action_state, delay_ms, timeout_ms, safe_state, default_state, latched_state, condition_since}`。

执行周期 20-50ms。每轮读取信号缓存快照和信号更新时间。优先级固定：手动模式直接覆盖；关键信号超过 timeout 进入 safe_state；否则执行自动规则；无命中则 default_state。比较运算符统一返回 boolean；滞回规则用 `off->on` 采用 `on_threshold`，`on->off` 采用 `off_threshold`；延时要求条件连续满足 N ms，条件断开清零计时。

多规则冲突：同一继电器按规则列表顺序和显式 `priority` 二选一；建议一期使用列表顺序，后匹配覆盖前匹配，并在 Web 显示冲突提示。上电先 GPIO 低电平关闭，配置加载后才应用默认状态；CAN/DBC 未就绪时只允许手动或默认安全状态。

## 10. 开发阶段拆分

| 阶段 | 目标 | 可验证结果 | 主要风险 |
|---|---|---|---|
| 1 | 时钟/GPIO/USART/LED | 串口日志、心跳 LED | H750 时钟和供电配置 |
| 2 | FDCAN1 收发 | CAN 工具收发标准/扩展/FD 帧 | timing、收发器 RX 电平 |
| 3 | SDMMC+FatFs | 创建目录和读写文件 | TF 兼容、DMA cache |
| 4 | lwIP+LAN8720 | ping `192.168.1.88`、HTTP hello | RMII 时钟、PHY 地址 |
| 5 | REST 框架 | status/can/status 可用 | HTTP 阻塞实时任务 |
| 6 | DBC 上传解析 | DBC 列表、解析报告 | RAM 上限、语法兼容 |
| 7 | 实时解码 | Web 显示物理值 | Motorola 位序 |
| 8 | 日志系统 | 100ms CSV 稳定写入 | TF 写入抖动 |
| 9 | 规则/继电器 | 延时、滞回、超时动作正确 | 误动作 |
| 10 | 前端整合 | 七个页面闭环配置 | 静态资源过大 |
| 11 | 稳定性测试 | 长跑、拔卡、总线关闭、上传大文件 | 资源泄漏、文件损坏 |

推荐开发环境：底层 bring-up 用 STM32CubeIDE 生成和调试初始化更快；正式工程建议迁移到 CMake/Makefile，便于裁剪、CI、单元测试和版本管理。保持 CubeMX `.ioc` 作为外设配置来源，但业务代码放独立目录，避免重新生成覆盖。

## 11. 风险与规避

| 风险 | 规避 |
|---|---|
| 128KB Flash 不足 | 裁剪 HAL/lwIP/FatFs/HTTP；禁用浮点 printf；Web/DBC/日志全放 TF；必要时二期 QSPI XIP |
| 协议栈和业务同时引入资源压力 | 模块化编译开关；静态上限；启动时打印内存水位 |
| Ethernet DMA 与 D-Cache | DMA 区放 D2 SRAM non-cacheable，或每次 clean/invalidate；描述符 32 字节对齐 |
| SDMMC/FatFs 并发 | 全局 fs mutex，批量写，Web 下载限速 |
| 上传 DBC 占 RAM | 流式落盘、逐行解析、固定池，不整文件读入 |
| Motorola 编码错误 | 独立 bit iterator，读写共用测试向量，PC 端单元测试先行 |
| CAN-FD timing 复杂 | 预置常用参数表，Web 只选档位；高级参数隐藏 |
| TF 拔插/损坏 | PA8 检测、写失败降级、配置 tmp+rename、日志可关闭 |
| 高频日志阻塞 | RAM buffer 批量 flush，LogTask 低于 CAN 优先级，丢弃计数可见 |
| Web 阻塞实时任务 | HTTP 只操作快照/队列，长文件操作分块，限制并发连接 |
| 继电器误动作 | 上电默认低；优先级集中决策；手动最高；超时安全；状态变化记录日志 |

