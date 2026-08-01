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
| 13 | 网页 CAN 发送控制 | [客观通过] | 合同仅 classic CAN：`GET/POST /api/can/tx`、`GET /api/can/tx/signals`，标准 ID/DLC/最多8字节HEX/`100..10000 ms`；TX/RX缓存分离。CTest=`16/16`、ELF/HEX前缀=`f589...`/`37b9...`、`text/data/bss=93976/384/242448`、50ms poll/队列/解析反汇编与OpenOCD Programming/Verified/Reset均已记录。用户已写TF上电；浏览器绿灯、TX/RX累计、默认折叠、TX/RX CANoe式DBC表、可逆停发/恢复、自动刷新与两次reload重入均通过，最终`0x321`/DLC4/`C2 A5 34 12 00 00 00 00`/1000ms已应用result=0。用户确认CANtest发送且外部RX表增长；未直接读取CANtest接收显示，不宣称外部接收器逐帧确认新TX帧 |
| 14 | 网页手动 TX 与两槽 DBC `signalKey` 规则 | [最终现场验收完成] | 正确部署根目录新网页后，自动刷新 TX/RX=`55/364→576/5540`，两次编辑 TX 分别在`1800/1300 ms`后仍保留并提交，最终 TX DBC `sequence=256`。候选 DBC获用户授权激活，runtime=`loaded=true/generation=1/bytes=151/messages=1/signals=2`；TX/RX解析 marker=`42434`、sequence=`256/4660`。slot1 V4回读`Can2Data.sequence/4660/priority20/action off`，外部 RX sequence=`4660`时 manual `relay1Output=0`与高优先级 off 一致；warn/error为空。此前构建、反汇编和烧录证据已实际完成。 |
| 15 | 安全审查 P0 可靠性整改 | [已验证] | IWDG任务卡死复位、Crash Dump保持、DBC边界、SignalCache stale、FDCAN外部高负载和TF持续写入物理断电均完成源码与现场验收。TF首次断电失败后最小修复`CTRL_SYNC`及scratch写路径的卡ready等待；最终CTest=`20/20`、固件构建/反汇编/烧录通过。干净FAT32介质复测中断电后CSV为399223 B/4159行、无撕裂尾行，`fsck_msdos -n`退出0；重新上电后TF/DBC/网络/CAN恢复，新日志文件与write/flush继续增长且failure/drop=0。结论不扩大为FAT32任意掉电时刻的原子保证。 |
| 16 | HTTP零数据连接回收 | [客观已验证] | 原始TCP连接不发数据可稳定复现唯一socket永久`ESTABLISHED`；只增加100 ms空连接计时并复用既有graceful `DISCON`，不改ACK/recovery/API或业务语义。CTest=`20/20`、最终ELF text/data/bss=`112828/768/243948`、定向反汇编、ST-Link烧录通过；10轮空连接在`110.4..146.3 ms`被回收且后续HTTP均成功，浏览器5次新页面、4次manual提交、最终9 API和ping均通过。 |
| 17 | 大 DBC selected-only 闭环（A0-H） | [A0-F通过，G实板收口中] | candidate9/active7为128项16消息；最终G映像在`0x100+0x110`下RX增量272、matched增量136、updates增量`136×8`且page0八项GOOD/page1 MISSING，selected-only外部隔离通过。6400 ms v3会话已在GOOD输入ACTIVE并锁定generation7/CRC/count；STALE与TF内CSV/meta仍`[待确认]`。生产固件V5-only fail-closed，host保留V1-V4解析测试；H与CAN-FD实板仍`[未验证]`。 |

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

## 2026-07-23 阶段13客观通过

- 用户已把恢复第4请求后250ms收尾等待的网页写入TF并上电，随后确认CANtest已发送。自动刷新TX/RX=`120/273→134/417`；状态灯为`status-lamp ok`，四个details默认折叠，控制台warn/error为空。
- 可逆关闭再恢复后的最终发送配置为`0x321`/DLC4/`C2 A5 34 12 00 00 00 00`/1000ms，应用状态为`requestSeq=appliedSeq`、result=0；TX self-test和外部RX CANoe式DBC表均为marker=`42434`、sequence=`4660`、quality=`ok`，外部RX在用户CANtest发送确认后增长。
- 两次reload重入样本为TX/RX=`145/527`、`162/694`，均未出现此前端口80连接拒绝。因此阶段13客观通过。没有直接读取CANtest接收显示确认板端新TX帧，故该项仅作为证据边界保留，不反写成外部接收器逐帧确认。

## 2026-07-23 阶段14启动：未验收

- 已核实根因：网页的全局 FIFO 对手动 TX 与后台刷新一视同仁，会使手动 TX 排在多个既有请求之后；RuleFile v3 仍用 `marker` 表达规则，未提供每槽绑定活动 DBC `signalKey` 的模型，且无活动 DBC 时尚无规则写入前置拒绝。
- 固定边界：继续单 socket、严格非并发；手动 TX 只等当前一个请求。规则恰好两槽，分别选择活动 DBC `signalKey`；RuleTask 仅导出已配置两项；无活动 DBC 禁止规则写；旧 v3 仅作 `marker` 兼容回退。
- 验收须重新取得本阶段的构建、定向反汇编及单 socket 网页/活动 DBC/无活动 DBC/旧 v3 四类证据。本次只记录计划，未修改源码、未编译、未反汇编、未烧录或现场测试；既有阶段13与v3验收均不能计入阶段14通过。

## 2026-07-23 阶段14进展：静态与烧录完成，现场待执行

- 网页调度已发现并修复“手动操作取消自动轮后不恢复”。`./scripts/verify.sh` host tests=`17/17`通过，STM32 firmware构建成功，ELF `text/data/bss=97180/388/243680`。
- 定向反汇编确认`rule_task`以capacity=`2`调用`can2_signal_cache_export_rule_snapshots_for_engine`，后者在临界区调用`signal_cache_export_rule_snapshots_for_engine`；RuleFile V4 builder与DBC catalog分页路径已检查。
- 2026-07-23 OpenOCD对`build/stm32h750/can_bus_gateway_stm32h750.hex`输出`Programming Finished`、`Verified OK`、`Resetting Target`。这只证明候选已构建、检查和烧录，不证明网页运行、外部RX或继电器。
- 必须等待用户替换TF卡`/www/index.html`、上电并启动CANtest，再取得网页运行中手动TX/自动轮恢复、两槽活动DBC `signalKey`、外部RX和继电器的现场证据。

## 2026-07-24 阶段14最终现场验收

- 正确根目录部署新网页后，自动刷新 TX/RX 从`55/364`增长至`576/5540`；浏览器 warn/error 为空。先前误部署旧页已由本次现场复验关闭。
- 两次刷新运行中编辑 TX 均保留：`C2 A5 78 56 02 03 04 05`经过`1800 ms`后提交成功，`C2 A5 00 01 02 03 04 05`经过`1300 ms`后仍保留并提交恢复；最终 TX DBC `sequence=256`。这关闭此前首次输入被覆盖的问题。
- 候选 DBC经用户授权激活，runtime最终为`loaded=true/generation=1/bytes=151/messages=1/signals=2`。TX/RX表解析 marker=`42434`，sequence分别为`256/4660`；两槽目录均含 marker/sequence，slot1从V4回读为`Can2Data.sequence`、threshold=`4660`、priority=`20`、action=`off`。外部 RX sequence=`4660`时 manual `relay1Output=0`，符合高优先级 off 规则；此前 RuleFile V4 浮点 threshold=`0`问题已修复并复验关闭。
- 本条只记录最终现场验收；构建、`objdump`和烧录均为此前实际完成的证据。本轮不把 TX self-test 作为外部接收器证明。
## 2026-08-01 阶段17 D门禁关闭，进入E

- D已在重建TF和真实100KB DBC上完成实板闭环：板端页面哈希一致；candidate generation1建立；ordinal0..7选择事务产生generation2；每页8项、page1、112项关键词检索、8项仅已选筛选和浏览器console均取得现场证据。
- E范围冻结为：从candidate/index/selection构造最多128信号、64消息的非活动selected-only runtime；按P0 `definition_hash`检查规则；写入并读回active generation；最后只在短临界区提交active manifest/runtime generation。任一失败必须保持旧active、runtime、规则、SignalCache和日志不变。
- E阶段禁止顺手实现F/G/H，不扩展HTTP并发、socket、消息总线或全量目录常驻RAM；Classic CAN标准ID+DLC8是本轮首要运行路径，CAN-FD实板仍标记`[未验证]`。

## 2026-08-01 P0 active manifest即时回滚修正

- 只读审计发现现有生产路径虽然能在冷启动时从`previous`恢复，但在manifest旋转后的rename或runtime publish失败时没有立即把旧`previous`恢复为`current`，与P0“任何失败保持旧active”的即时语义不完全一致。
- 已最小增加同一TF mutex内的rollback：若旧current已轮转，失败时先删除本次current（若有）再将previous重命名回current并完整读验；rollback自身I/O失败时保留previous给启动恢复，绝不删除旧generation对象。host+STM32候选构建通过，但尚未烧录，不能把旧G映像的实板证据贴到新hash。

- 旧G映像的GOOD→STALE门禁已关闭：双ID正向隔离后，停`0x100`保留`0x110`使RX继续增长而page0八项保持末值/raw并全部STALE。6400 ms v3会话已正常STOPPED；但下电取卡后的只读FAT检查返回`fsck_msdos` exit=`206`，current CSV/meta链空闲/长度越界，卷不能只读复挂载。因此实体CSV/meta、clean footer和TF持久化改为`[阻断]`，不能从可见文件名或hash推断通过。先由用户选择只读镜像保全或放弃故障会话重建FAT32；后者完成后必须重新取得G实体日志和未烧录P0回滚候选的最小实板回归。
- 用户已明确选择允许修复/格式化，故障会话未做镜像而被丢弃。主机将明确识别的外置`/dev/disk4`重建为`MBR + FAT32 CANBUS`，部署清单仅含网页、151 B基线active/candidate DBC和空`log/config/sys`目录；网页和DBC均`cmp`/SHA-256一致，卸载后只读`fsck_msdos -n` exit=`0`，read-only重挂载复核后再次卸载。此为可插板的干净部署基线，不复用损坏卡的large generation、规则或日志。下一门禁是板端冷启动，再从真实100071 B输入重建candidate/selection/active、重做v3实体日志与P0新候选实板回归。

## 2026-08-01 大 DBC P0 修正后的恢复计划

- P0实现：candidate、selection与active manifest轮转失败均即时尝试previous回滚；active runtime publish仍位于manifest后短临界区，失败不发布新runtime。FatFs镜像FAT/FSINFO写错误显式传播为`FR_DISK_ERR`。
- 静态验证：`./scripts/verify.sh` CTest=`34/34`，FLASH=`121948 B`、RAM_D1=`196672 B`，ELF/HEX=`2d225a84feee60027e294ea6154c1f7e59882680cdb41fb64c39fa7d212dd60a`/`b886bbf945c6d6a74193c258ab1c14a67548fe48968a5b4cf04ac3e0c0a2dc31`；反汇编确认rollback与FatFs失败分支。候选未烧录。
- 下一门禁：干净卡上板后重新上传真实100071 B DBC、选择/激活，重做外部Classic RX、selected API、v3 CSV/meta和故障恢复。旧损坏会话不计入通过；CAN-FD实板仍`[未验证]`。

## 2026-08-01 新介质B–E实板状态

- B/C/D：真实`BNE_CLASSIC_CAN_TEST_100KB.dbc`（100071 B、CRC32=`4B88D9CE`）在板端生成candidate，112消息/896信号；分页检索和selection将ordinal0–7持久化为candidate generation2、8信号/1消息。
- E：active提交为generation1、candidate2、CRC=`E5CB6C8F`、slot0 selected-only runtime；一次软件复位后最终恢复同一runtime。长响应socket收口缺陷已以`SEND_OK→graceful DISCON`最小修正并实板连续请求复验。
- F/G/H剩余：外部标准Classic CAN `0x100`正向GOOD、混合`0x100/0x110`反向隔离、停`0x100`后的STALE、含真实数据的v3 CSV/.meta和干净TF只读核验，以及TF写失败/掉电异常路径。CAN-FD实板仍`[未验证]`。
