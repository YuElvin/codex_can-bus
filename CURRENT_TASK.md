# 当前任务

更新时间：2026-07-17

## 项目目标

交付基于 STM32H750VBTx、W5500、MCP2562FD、TF、W25Q128 和 FreeRTOS 的 CAN/CAN-FD 数据采集网关；各功能必须以构建、反汇编、烧录和现场证据验收，不能以源码存在或单次 HTTP 200 代替。

## 当前阶段

- 固定阶段：`一期全量功能完成`。
- G-1最终映像联合烟雾、G-2安全HTTP 500、G-3功能/现场域审计和发布完整性均已通过；一期无剩余必做项。

## 成功标准

1. 以最终提交对应的同一 ELF/HEX 为唯一候选，逐项审计最终合同中的硬件、CAN、DBC、TF/日志、规则/继电器、网络/API、Web 和稳定性证据。
2. 历史高成本证据只在最终差异未触及对应模块且本轮烟雾回归通过时复用；缺口必须明确列出并执行最小补验。
3. 所有必验项均有可追溯的构建、反汇编、烧录、现场读数或文件证据；任何未通过项保持项目未完成。
4. 汇总最终固件哈希、提交哈希、验收矩阵和已知非目标，全部通过后才允许标记项目完成。

## Git 基线与工作树

- 最终固件源码提交：`0ef7d3e1bf5bd5eecffd1f2f0c912fbe3e230304`；之后只有治理Markdown变化。
- 分支：`codex/W5500`，跟踪 `origin/codex/W5500`。
- G-3治理封口提交`fe2154c`已推送；推送后工作树干净，本地与远端ahead/behind=`0/0`。

## F-76 已验收基线

- 最终实现以 `TX_FSR` 非阻塞门控响应 ACK；`CLOSE_WAIT` 只调用既有 graceful `DISCON`，不再硬 `CLOSE` 重开监听；500 ms 超时才进入既有完整 W5500 恢复。
- `git diff --check`、`./scripts/verify.sh`通过，host CTest=`15/15`；FLASH/RAM_D1=`92628/242792 B`，ELF `text/data/bss=92232/384/242408`。
- 最终ELF/HEX SHA-256=`26b63222463e9c0cd31d55e5dc40b0ad1c86e2d2ca6d10a8f9b3d0f276b34bc5`/`d1ef3838757beb8478aea6413eb3b12c5f9fdf41f5196b96bfd2d7e8ddb7968c`。
- 反汇编确认 `SR=0x1c` 分支清 ACK 内部状态后调用 graceful `DISCON`；CLOSED/INIT 重监听、FSR=2048 门槛、500 ms 超时和完整恢复边界均保留。
- OpenOCD/ST-Link 烧录得到 `Programming Finished`、`Verified OK`、`Resetting Target`，电压=`3.249799 V`；烧录后无 OpenOCD/GDB 残留。
- 最终 pcap `/tmp/f76-final-r01.pcap`：7653 B，SHA-256=`77a1ed949fb374641968b5d38e9a744e75057bfc7c9c7fa647520d01e418ad6e`，63包/5连接。
- 两条浏览器 POST 和 manual/status/CAN 三条 GET 均完整 HTTP 200；5/5 请求与响应全部 ACK，Content-Length 全部匹配，HTTP 数据重传=0、RST=0，5/5 双方 FIN 均最终确认。
- 两条 POST 的 request/applied=`1/1`、`2/2`；关闭覆盖后实际输出=`1/0`。ST-Link 读回 ACK wait count=`2`、elapsed=`50 ms`、timeout=`0`、W5500 recovery count=`0`。
- 顺序 ping 2/2；manual/status/CAN 均 HTTP 200；RTOS、W5500、TF、QSPI、CAN 正常。F-76 判定 PASS。

## 唯一下一动作

无一期必做开发任务。后续新需求必须由主会话先定义新阶段、唯一目标和验收标准；不得把明确非目标自动扩入已完成的一期。

## G-3 最终结论

- 最终固件源码提交为`0ef7d3e1bf5bd5eecffd1f2f0c912fbe3e230304`，之后只有治理Markdown变化；ELF/HEX SHA-256=`26b63222463e9c0cd31d55e5dc40b0ad1c86e2d2ca6d10a8f9b3d0f276b34bc5`/`d1ef3838757beb8478aea6413eb3b12c5f9fdf41f5196b96bfd2d7e8ddb7968c`。
- G-3复用G-1同一最终映像的`verify.sh`/CTest=15/15证据；本轮只核对现有ELF/HEX哈希、`text/data/bss=92232/384/242408`以及HTTP、LogTask、RuleFile v3和bus-off关键`nm/objdump`结果，未重新构建。
- F-76烧录/pcap、G-1同映像联合烟雾、G-2安全500以及历史长跑/网线/冷启动/bus-off/TF/QSPI证据共同覆盖最终验收矩阵。G-3判定所有一期域PASS。

## G-2 已验收基线

- G-2只读审计确认当前HEAD没有纯curl且无副作用的500；唯一安全候选是`http_handle_rules_write()`在candidate构造、body解析和`g_rule_file_v3_save_request=1`之前的`rules_source_unavailable`前置分支。
- 实际原值为`g_rule_file_v3_load_result=0`、`g_rule_file_v2_load_result=0xffffffff`。仅将v3 RAM值临时改为1，并发送故意非法的12 B body `enabled=true`；即使注入未生效也只会返回400，绝不会进入保存。
- 实际响应为HTTP500 `Internal Server Error`，Content-Length=98，body SHA-256=`85dc32d8f7ddc22a80edfe89d9a461fd5c545e6284e9e575df565f1ea6209a22`，错误码=`rules_source_unavailable`。随后立即将v3恢复为0，v2保持`0xffffffff`。
- 恢复后同一请求返回400 `invalid_rule`；规则响应前后SHA-256同为`85fcd21ea3482d8a6888ec06cf495346b3c4a62d72bb545a7c4dba2bdd824e9d`。`save_request/save_result=0/0`、generation=4、socket=LISTEN，HTTP error/ACK timeout/W5500 recovery全0。
- 最终CAN tx/rx=`1056/10468`且错误全0，RTOS/W5500/TF/QSPI正常，ping 2/2、status HTTP200；所有halt均无reset并已resume/shutdown，OpenOCD/GDB及调试端口已释放。G-2判定PASS。

## G-1 已验收基线

- G-1执行时HEAD=`1e31213aa414c3bb11ede79e251ab02a37c58311`，工作树/远端一致；`verify.sh`与CTest=`15/15`通过，ELF/HEX哈希保持F-76已烧录值不变，HTTP、LogTask、RuleFile v3和CAN bus-off关键路径反汇编均通过。
- OpenOCD预读命令误含`reset run`导致一次复位；因此复位前HTTP结果未与后续计数混用。复位后重新取得Snapshot A，并从统一基线执行完整G-1。
- 复位后19个严格串行HTTP连接全部返回预期状态码且Content-Length等于实际body：正常200链路、非法规则400和未知路径404均通过；页面11143 B且SHA-256=`2ed23b7fe6d1047b897d62bb8b6aa6376e4c1e6d90c5c7d4ff11918fc99117da`。
- DBC精确151 B上传/激活成功，runtime generation=`1→2`；signals持续为marker=`42434`、sequence=`4660`、quality=`ok`。规则slot1完成`42435→42436→42435`并完整回读恢复；manual保持disabled、输出=`1/0`。
- Snapshot A→B：LogTask sample/write/flush=`101/20/20→281/56/56`，文件=`15326272→15346678 B`；CAN TX/RX=`103/1017→284/2811`。日志、TF read、CAN、DBC decode、RX/TX队列错误/丢弃均为0。
- HTTP request count=`0→19`，socket最终LISTEN，HTTP error、ACK timeout、W5500 recovery均为0，ACK pending=0；规则generation=`2→4`且最终v3两槽恢复。每次GDB读取后已resume，OpenOCD/GDB和3333/4444/6666监听均释放；最终ping和status HTTP200。

## 禁止范围

- 不新增 Web 功能，不引入并发 HTTP，不扩大 API。
- 不改变规则、手动继电器、安全态、CAN、DBC、TF 或日志业务语义。
- 未明确需要 CANtest、TF、网线或上下电操作前不得要求用户介入；需要时必须暂停并给出单一步骤和预期现象。

## 子任务派发规范

- 派发子任务必须使用 `fork_turns=none`。
- 每个子任务必须写明固定阶段、唯一明确目标、成功标准、证据要求和禁止范围。
- 子任务不得自行选择、切换或扩大阶段；未获主会话明确派发时不得实施功能修改。
