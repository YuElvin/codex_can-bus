# 项目最终验收与阶段路线

本文是 `can_bus_W5500` 的最终验收合同和后续阶段任务来源。项目不得因为单元测试、源码存在或一次 HTTP 200 而宣布完成；每一项必须同时满足本文规定的证据。`03_Context.md` 只保存当前快照，`CONVERSATION_SUMMARY.md` 保存每轮原始过程。

## 1. 最终交付范围

目标设备是 `STM32H750VBTx + W5500 + MCP2562FD + TF + W25Q128 + FreeRTOS` 网关，使用 FDCAN2 `PB5/PB6` classic CAN 500 kbit/s 和 W5500 静态 IP `192.168.1.88`。最终系统应提供：

- 可稳定运行的 FreeRTOS 任务与明确的 CAN、DBC、FatFs、W5500、配置并发边界；
- 外部 CAN 接收、周期发送、DBC 激活与实时信号查询；
- TF 静态页、受限 DBC 上传/激活和日志写入；TF 卡仅支持下电后插拔，不承诺运行中热插拔恢复；
- W25Q128 双槽规则配置持久化；
- 可通过 HTTP 管理的规则配置，以及定义明确的规则文件/多规则能力；
- 现场稳定性、异常和重启恢复证据。

非目标保持不变：不恢复 LAN8720/lwIP 路线，不引入大型前端框架，不把 `0x00FFF000` 诊断区作为正式配置区，不把单 socket HTTP 宣称为并发 Web 服务。

## 2. 最终验收总表

| 验收域 | 最终通过条件 | 必须的现场/产物证据 | 当前状态 |
| --- | --- | --- | --- |
| 硬件与启动 | ST-Link 可烧录/读数；W5500、TF、FDCAN2、W25Q128 启动状态正常 | OpenOCD `Verified OK`；精确变量；ping 与状态 API | 已有基线，后续每次回归 |
| CAN 与 DBC | 外部 CANtest RX、周期 TX、DBC 上传/激活、解码到 SignalCache 持续正确 | CANtest 与板端 RX/TX/error 计数；`/api/signals`；`/api/dbc/runtime` | 主要完成，需稳定性回归 |
| 网络/API | socket0 顺序 GET/POST 语义、错误码、请求体完整性正确 | 顺序 curl；HTTP 200/400/500 行为；W5500 诊断 | 受限最小实现已完成；F-20 已验证非法规则400后的 `+0/+50/+100/+250/+500 ms` 独立短连接均为200 |
| TF 与日志 | TF 卡在插入状态下上电后默认 CSV 路径连续落盘；运行中插拔明确不支持，必须先下电 | TF 文件大小/内容、LogTask write/flush/failure、插卡冷启动状态 | F-26 已下电取卡只读验证 CSV 内容；运行中 recovery 不作为交付条件 |
| 配置持久化 | 双槽保存、读回、断电/复位加载、坏槽回退；HTTP 不绕过 ConfigTask | QSPI save/load/sequence 读数；复位后 API；配置队列读数 | 单规则完成 |
| 规则管理 | 规则模型、文件格式、HTTP CRUD、多规则执行/优先级/安全态均定义并现场验证 | 文件读写、HTTP 请求、RuleTask generation、继电器 GPIO、异常输入 | v2/v3 两规则和 TF 规则文件已验证；通用无限规则管理非目标 |
| 故障与稳定性 | 长跑、断网、CAN bus-off/恢复、插卡冷启动后配置/DBC/日志恢复 | 连续计数、错误计数、恢复证据和明确的未通过项 | 进行中；F-12长跑、F-14物理断网恢复、F-17 HTTP优雅关闭修复、F-19冷启动DBC/v3规则/日志恢复、F-25 CAN bus-off 恢复、F-26 CSV 内容均已验证；仅最终全量复验未完成，RX overrun 仅保留为历史根因未定事项 |
| 发布完整性 | 工作树干净、分支已推送；所有状态文档与最终固件一致 | `git status`、远端哈希、`verify.sh`、ELF 反汇编、最终烧录记录 | 每阶段执行；最终待审计 |

## 3. 统一验收门槛

任何代码阶段均必须按以下顺序完成，缺一项只能标为“未验证”或“阻断”：

1. `git diff --check` 与 `./scripts/verify.sh` 通过，记录 host CTest、FLASH、RAM_D1。
2. 使用最终 ELF 做定向 `nm/objdump` 检查，证明新增关键路径没有被优化或绕过。
3. OpenOCD/ST-Link 烧录目标 HEX，输出必须包含 `Programming Finished` 与 `Verified OK`。
4. 使用精确 ELF 符号地址读取关键状态；每次 halt 后显式 `monitor resume`，不得把暂停后的 HTTP 超时归为固件失败。
5. 使用真实外设/网络做顺序验证：socket0 HTTP 不并发请求；CAN 必须区分 TX self-test 与外部 RX 证据。
6. 更新 `01_Project_Plan.md`、`03_Context.md`、`04_Features_ADR.md`、`05_Lessons.md`、`ARCHITECTURE_DESIGN.md`、`CONVERSATION_SUMMARY.md` 中受影响内容；提交并推送。

文档或治理阶段不编译时，必须在 `CONVERSATION_SUMMARY.md` 明记“本次未编译，因此未执行反汇编检查”。

## 4. 固定后续阶段（主会话派发）

后续派送会话不得自行选择目标。主会话按顺序派送下列阶段；每阶段只领取一个目标及其验收条件。

| 顺序 | 唯一目标 | 完成条件 | 明确不做 |
| --- | --- | --- | --- |
| A | 定义并实现 TF 规则文件最小格式与启动加载 | 有效文件加载为单条/有限规则；无效文件保持已知安全配置；烧录后 HTTP/RuleTask/GPIO 可证实 | 多规则 CRUD、任意 DSL 扩展 |
| B | 多规则运行模型与优先级 | 至少两个持久化规则可独立命中；优先级、手动覆盖、超时安全态均有外部 CAN/GPIO 证据 | 前端批量编辑、无界规则数量 |
| C | 规则 HTTP 管理最小 CRUD | GET 列表/详情、POST/PUT 保存、删除或禁用的语义与文件/运行态一致；非法请求 400 | 并发 API、鉴权、复杂分页 |
| D | TF 下电插拔操作边界 | 明确运行中热插拔不支持；插入状态上电后默认日志连续落盘，临时 recovery 试验代码全部删除 | 运行中 recovery 写入、文件轮换、下载、重试策略 |
| E | QSPI diagnostic 失败复核（已完成） | 当前正式 ELF 精确地址单次请求后 `result=0/erase_count=1`；历史 `0xffffffff` 确认为未触发诊断的初始化哨兵值，规则双槽状态不变 | 把诊断区改成正式配置区 |
| F | 稳定性与异常验收 | 定时长跑、网络断开/恢复、CAN bus-off/恢复、复位后 DBC/规则/日志检查；每个异常有明确结论 | 无证据的“长期稳定”声明 |
| G | 最终全量审计 | 按本文件第2节逐项复验，汇总最终固件哈希、提交哈希、未完成项；所有项通过才可标记项目完成 | 以历史聊天或源码存在代替复验 |

阶段 A 开始前，必须先在 `04_Features_ADR.md` 固化规则文件的字段、版本、容量上限、非法输入行为和与 W25Q128 单规则备份的关系；不能由派送会话自行假设格式。

### F-26：默认 CSV 物理内容复查（待执行）

现有 HTTP 只提供 `/`、`/index.html` 和 API 路由，不提供 `/log/*` 或 CSV 下载；不得为本项临时增加下载 API。现场必须保持外部 CANtest 标准 `500 kbit/s` 输入，先证明 `/api/signals` 的外部缓存为 marker=`42434`、sequence=`4660` 且 CAN 无错误；再间隔至少 6 秒读取两次现有 LogTask/TF 状态，要求 `g_log_path_mode=0`、`g_log_write_count`、`g_log_flush_count`、`g_tf_csv_file_size` 均增长，`g_log_failure_count=0`、`g_tf_csv_write_result=0` 且读失败不增长。

随后必须由用户将开发板完全下电，才可取出 TF 卡。主机只读挂载后读取 `/log/signal.csv`，不格式化、不改写：文件只能有一行表头 `updated_ms,key,value,raw,unit,quality`；至少一组同一采样周期的两行必须包含 `Can2Data.marker,42434.000000,42434` 与 sequence 的 `4660.000000,4660`，每行六字段且 `quality=ok`；主机取得的字节数不得小于下电前最近一次 `g_tf_csv_file_size`，若因人工下电间隔继续增长，新增部分必须以完整成对记录结束。人工断电与板端读数不能原子同步，故不以绝对相等作为有效判定。若现场为 `g_log_path_mode=1`，改查 `/log/signal-recovery.csv`，不得把默认文件冒充当前日志。本协议只证明下电关闭前的落盘内容，不证明热插拔、在线下载或并发文件服务。

## 5. 派送协议

从本文创建后，主会话每次派送都必须给出：

- 固定阶段标识和唯一开发/验证目标；
- 不可改变的范围、非目标和已有基线；
- 具体成功条件、所需 GDB/HTTP/CAN/TF 证据；
- 失败时可接受的结论（例如“路径已验证但底层操作失败”）；
- 完成后必须更新的治理文件、提交和推送条件。

派送模型固定使用 `gpt-5.6-luna`、`high` 推理强度，除非用户再次明确改变。
