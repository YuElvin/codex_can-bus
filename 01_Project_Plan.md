# 项目计划

本文件只记录当前计划和验收边界；长历史保留在 `CONVERSATION_SUMMARY.md`，详细架构设计保留在 `ARCHITECTURE_DESIGN.md`。项目最终通过条件和固定后续阶段见 `PROJECT_FINAL_ACCEPTANCE.md`。

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
| 5 | W25Q128 QSPI | [客观已验证] | 默认启动只读 JEDEC ID；`0x00FFF000` 保留诊断区仅显式触发擦除、写入、读回匹配。2026-07-13 已复核历史 `0xffffffff/erase_count=0` 为未触发诊断的初始化哨兵值，正式单次请求 `result=0/erase_count=1` |
| 6 | FreeRTOS 单任务迁移 | [客观已验证] | 已烧录确认 `g_freertos_task_started=1`、loop 递增、各硬件状态仍通过 |
| 7 | FreeRTOS 多任务拆分 | [客观已验证] | 既有任务、CAN RX/TX、DbcTask、TfTask 和 ConfigTask 队列边界均已烧录验证；RuleFile v3 两槽配置经 ConfigTask 原子保存并由 RuleTask reload，未实现的通用消息总线和无界规则管理属于非目标 |
| 8 | W5500 socket/HTTP status | [客观已验证] | `/api/status`、`/api/can/status` 可访问 |
| 9 | TF 静态文件和 DBC 上传 | [客观已验证] | `/www/index.html` 静态文件512字节循环分块、`POST /api/dbc/upload`候选保存与portable parser报告、`POST /api/dbc/active`最小激活、启动/激活后的运行态快照和`GET /api/dbc/runtime`均已烧录验证；multipart、分片和DBC列表/删除属于未来非一期范围 |
| 10 | 实时解码、日志、规则 | [一期边界客观已验证] | active DBC、外部 CANtest RX、`/api/signals`、默认LogTask和F-26实体CSV均已验证；G-1同映像下DBC激活、外部SignalCache、日志增长、两槽v3规则可逆保存和manual安全态联合通过。QSPI单规则双槽、TF v1/v2/v3固定规则文件、RuleTask reload/优先级/超时/安全态和复位恢复均有客观证据；运行中TF热插拔、无界规则和通用配置服务是明确非目标 |
| 11 | TF 驻留 Web 控制台 | [一期核心三项客观已验证] | 用户固定的一期核心为CAN刷新显示、规则设置和继电器操作：板端冷启动页面哈希一致；独立pcap中首页最终ACK至首API SYN=`303.503 ms`，1次首页+14次CAN状态+14次signals全部200、RST=0；浏览器把slot1 threshold `42435→42436→42435`保存并读回；浏览器手动覆盖`relay1=0/relay2=1`后RuleTask `request/applied=1/1`且GPIOE ODR=`0x100`，关闭覆盖后`2/2`且ODR=`0x80`恢复规则。DBC区域保留并复用既有已验证API，但本次未以浏览器重新执行上传/激活，不计入上述三项现场结论 |
| 12 | 稳定性基线 | [一期边界客观已验证] | F-11/12长跑、F-14网线恢复、F-19冷启动、F-25真实bus-off、F-26实体CSV和F-76响应交付均已通过。G-1在同一最终映像完成19个顺序HTTP、DBC激活、规则可逆恢复、Web哈希、外部CAN/SignalCache及日志增长联合回归；G-2得到完整HTTP500并精确恢复，未写规则/TF/QSPI。G-3确认所有功能/现场域PASS，无需重复高成本故障；运行中TF插拔仍为非支持操作 |

## 阶段 C 当前状态

阶段 C 的范围固定为两槽 RuleFile v3 与受限 HTTP CRUD，现已客观验收完成：`GET /api/rules`、`GET /api/rules/0|1`、`POST /api/rules`（只创建 disabled 槽）、`PUT /api/rules/0|1`（完整替换）和 `DELETE /api/rules/0|1`（禁用）均存在。写入只更新候选并经 ConfigTask 深度 2 队列单消费者保存 `/config/rules-v3.tmp -> /config/rules-v3.conf`、备份 `.prev`，成功后才请求 RuleTask engine reload；不写 QSPI，不改变 v1/v2。

验收包含 DELETE 后 rule_count=`1`/generation 变化、POST 后 rule_count=`2`、PUT 非默认值跨复位持久化、恢复阶段 B 默认、非法 PUT HTTP 400 且 generation/current 不变，以及网络只读回归。开发中发现并修复 `rule_file_v3_build_engine()` 3856 B 自动对象覆盖 ConfigTask TCB 的真实 HardFault；最终反汇编栈帧为 120 B。阶段 C 完成后停止，下一会话只按最终验收路线进入阶段 D。

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
| TF/FatFs 并发损坏或文件错误 | LogTask、HTTP、DBC 共用全局 `fs_mutex`；LogTask 单批失败仅丢弃并计数。硬件操作边界固定为 TF 插拔前先下电，运行中不支持热插拔/recovery；不得以临时 gate/状态旁路提交 |
| DBC 上传过大或半包 | 当前仅支持 1024 字节以内、单连接完整请求体；body 未收全时等待下一轮轮询，不作为完整上传系统；候选文件替换采用 `/dbc/upload.write.tmp` -> `/dbc/candidate.dbc`，旧候选备份为 `/dbc/candidate.prev.dbc` |
| DBC 解码验证边界 | active DBC 已接入CAN2解码和外部`SignalCache`；TX self-test与外部RX继续以独立计数/缓存区分。G-1外部marker/sequence=`42434/4660`、DBC RX持续增长且decode errors=0，已补齐外部RX最终映像证据 |
| DBC/缓存 RAM 占用 | 当前使用候选scratch、运行态双槽`DbcDatabase`、外部RX与TX self-test两个固定`SignalCache`和768 B日志缓冲；一期最终RAM_D1=`242792 B/512 KB=46.31%`，扩大parser、缓存或日志前必须复查内存 |
| QSPI 诊断擦写配置数据 | `0x00FFF000` 已保留为诊断区；单规则配置使用 `0x00FFE000` 主槽和 `0x00FFD000` 备用槽，默认启动不擦写，显式 ST-Link 请求由 ConfigTask 串行执行；通用备份不得使用诊断扇区 |

## 阶段 B 当前状态

已实现 TF `/config/rules-v2.conf` 的固定两规则格式、512 字节上限、完整非法输入校验、最大 priority 选胜、手动覆盖、延时和超时 safeState；保留 v1/QSPI 单规则路径与 HTTP 单规则 API。主机测试、固件构建、ELF 反汇编、OpenOCD 烧录、有效 v2 启动加载和 marker=42434 的 RuleTask/GPIO 已验证。

阶段 B 已客观验证：marker=42435 外部 RX 下 rule1 priority=20 胜出并使 PE7 off；停帧 `49114 ms > 1500 ms` 后 safe active=1 且 PE7/PE8 均 off；手动状态为 0。首次 v2 缺失创建的板端现场未观察到，代码路径和诊断语义已静态确认；本阶段完成后停止，不推进阶段 C。
