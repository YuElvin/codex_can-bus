# 当前任务

更新时间：2026-07-16

## 项目目标

交付基于 STM32H750VBTx、W5500、MCP2562FD、TF、W25Q128 和 FreeRTOS 的 CAN/CAN-FD 数据采集网关；各功能必须以构建、反汇编、烧录和现场证据验收，不能以源码存在或单次 HTTP 200 代替。

## 当前阶段

- 固定阶段：`G`。
- F-76 已完成构建、反汇编、烧录和现场封口；当前唯一目标是按 `PROJECT_FINAL_ACCEPTANCE.md` 执行最终全量审计，不再新增功能。

## 成功标准

1. 以最终提交对应的同一 ELF/HEX 为唯一候选，逐项审计最终合同中的硬件、CAN、DBC、TF/日志、规则/继电器、网络/API、Web 和稳定性证据。
2. 历史高成本证据只在最终差异未触及对应模块且本轮烟雾回归通过时复用；缺口必须明确列出并执行最小补验。
3. 所有必验项均有可追溯的构建、反汇编、烧录、现场读数或文件证据；任何未通过项保持项目未完成。
4. 汇总最终固件哈希、提交哈希、验收矩阵和已知非目标，全部通过后才允许标记项目完成。

## Git 基线与工作树

- F-76 已验收提交：`0ef7d3e1bf5bd5eecffd1f2f0c912fbe3e230304`。
- 分支：`codex/W5500`，跟踪 `origin/codex/W5500`。
- 上述提交已推送，提交后工作树干净且与远端 ahead/behind=`0/0`。

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

执行`G-1 最终提交同映像正常联合烟雾`，不做故障注入。开始前只等待用户将CANtest设置为500 kbit/s、normal active、开启接收/ACK，并持续发送标准ID`0x321`、DLC 8、数据`C2 A5 34 12 00 00 00 00`；用户回复“G窗口已就绪”后，其余构建/反汇编/哈希、顺序API、DBC、规则、日志和精确读回均由主会话自动执行。

## 禁止范围

- 不新增 Web 功能，不引入并发 HTTP，不扩大 API。
- 不改变规则、手动继电器、安全态、CAN、DBC、TF 或日志业务语义。
- 未明确需要 CANtest、TF、网线或上下电操作前不得要求用户介入；需要时必须暂停并给出单一步骤和预期现象。

## 子任务派发规范

- 派发子任务必须使用 `fork_turns=none`。
- 每个子任务必须写明固定阶段、唯一明确目标、成功标准、证据要求和禁止范围。
- 子任务不得自行选择、切换或扩大阶段；未获主会话明确派发时不得实施功能修改。
