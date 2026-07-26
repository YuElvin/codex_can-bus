# P0 目标板故障注入验收矩阵

本文件只定义`codex/W5500`当前候选的现场验收步骤，不会通过脚本自动烧录、复位、暂停任务、拔卡或断电。执行前必须由操作者明确确认已连接目标板，并在每次调试读取后恢复目标运行。

## 候选映像

本矩阵仅适用于下列本地构建产物；重新构建或修改源码后必须重新计算哈希并重新执行相关项目。

| 文件 | SHA-256 |
| --- | --- |
| `build/stm32h750/can_bus_gateway_stm32h750.elf` | `a32adce3c188bf859adaf36bd8c7326ed7b76c0aee0403cf4e0f6ba9fbe14fef` |
| `build/stm32h750/can_bus_gateway_stm32h750.hex` | `4bb09f44080ad7bf8db1ddca8b6f358bd9da484b229388feb986cbfc8165f5ad` |

先执行：

```sh
./scripts/verify.sh
git diff --check
```

本矩阵不能把 host CTest、ELF 反汇编或 HTTP 成功响应替代为现场故障证据。

## A. FDCAN 接收高负载

前置条件：CANtest/CANoe 使用与固件一致的位率，外部 CAN 输入可持续发送；不得将本机 TX self-test 作为外部 RX 证据。

记录测试前后以下符号：

```text
g_can2_rx_fifo_irq_count   0x2401f744
g_can2_rx_fifo_full_count  0x2401f740
g_can2_rx_fifo_lost_count  0x2401f73c
g_can2_rx_fifo_fill_max    0x2401f738
```

步骤：保持正常业务帧并逐级提高外部帧率，持续足以覆盖多次 LogTask/HTTP 运行周期；记录 RX/DBC 解码计数、软件 RX 队列丢弃和上述硬件 FIFO 计数。每次 GDB/OpenOCD halt 读取后必须 resume。

通过标准：连续稳定负载下业务帧持续解码，`g_can2_rx_fifo_lost_count`不增长；若`full`增长，必须记录负载、最大 fill、是否有 lost 与业务影响，不能仅因 IRQ 已触发就判定通过。失败或 lost 增长时保留读数，不调整任务优先级或 FIFO 深度以掩盖结果。

## B. IWDG 与 Crash Dump

前置条件：已记录当前 CAN/继电器安全状态，调试器连接不会把 IWDG 冻结；只可在可安全复位的台架上操作。

读取符号：

```text
g_watchdog_started          0x240051ac
g_watchdog_refresh_count    0x240051a8
g_watchdog_unhealthy_mask   0x240051a4
g_watchdog_reset_flags      0x240051a0
g_fault_record              0x38000100  (68 bytes)
```

步骤：

1. 正常运行时确认`g_watchdog_started=1`且`g_watchdog_refresh_count`持续增长。
2. 使用经操作者批准的台架方法阻塞一个受监管任务；不要通过网页、CAN 或存储写入制造该故障。
3. 等待硬件复位，重新连接后读取 reset flags、unhealthy mask 和 fault record，再恢复正常业务运行。
4. 另以经过批准的可控 CPU fault 方法触发一次 HardFault、MemManage、BusFault 或 UsageFault；读取`g_fault_record`的 magic、exception、stacked PC/LR、CFSR/HFSR、checksum。

通过标准：任务停止进展后不再刷新 IWDG，并发生自动复位；fault record 的 magic/checksum 有效，exception 与注入类型一致。真实 CPU fault 还必须保留原始 PC/LR/CFSR/HFSR；受控跳转到 handler 只能验证记录和复位路径，不能代替真实异常栈证据。IWDG 超时、调试器冻结、复位标志位义和 RAM_D3 在复位后的保留行为均必须记录原始读数；未读取即不通过。

## C. TF 写入时电源瞬断

前置条件：已启用会话 CSV 记录、外部 CAN 输入持续、TF 卡可安全取出；不能在配置或规则文件写入期间执行本项。

步骤：

1. 启动记录并确认`g_log_write_count`至少连续增长两次，且`g_tf_csv_write_result=0`、`g_tf_write_sync_result=0`、`g_log_failure_count=0`。
2. 在持续采样和 flush 期间由操作者切断目标板电源；不要先停止记录或等待空闲窗口。
3. 断电后取卡，以主机只读方式检查卷可挂载性、目标会话 CSV、唯一表头和尾行完整性。建议记录`fsck_msdos -n <设备分区>`的只读结果，以及最后数行的字段数。
4. 重新上电并确认 TF 初始化、CAN、规则和日志控制不会因旧会话文件阻塞启动。

通过标准：文件系统能只读挂载且检查工具无未修复错误；CSV 保留单一八列表头，已完整写入的行可解析，尾部若有中断行须明确记录而非作为完整样本；重启后系统可继续运行。实际丢失量以断电前后文件和运行计数测得为准，不能把 512 B/5 s 批量策略或`f_sync()`写成保证的掉电上界。

## 结果记录

每项记录映像 SHA-256、日期、供电/CAN 条件、原始计数、结果和恢复动作，并同步到`CONVERSATION_SUMMARY.md`、`03_Context.md`；失败项保持未通过，不以重新烧录或重启后的正常状态覆盖失败证据。

## 2026-07-26 已执行结果

- 默认生产 ELF 不含`g_p0_fault_inject_can_stall`。仅实验室构建使用`-DCAN_BUS_P0_FAULT_INJECTION=ON`，其 CAN 任务反汇编在该标志为1时每50 ms延迟、停止任务进展；该开关默认关闭，未进入最终生产映像。
- 对实验室映像写入该标志后等待12秒，复位后`g_watchdog_reset_flags=0x04460000`，包含`RCC_RSR_IWDG1RSTF=0x04000000`；标志已清零，CAN任务重新运行。随后恢复生产映像。最终生产映像的3秒快照为`g_watchdog_refresh_count=32->35`、`g_can_task_loop_count=1086->1182`、unhealthy mask=`0`，IWDG=`PR=6/RLR=1000/SR=0`。
- 首次 fault record 复测发现软件复位后最后一个缓存字段改变，校验失效。根因是M7 D-Cache未回写RAM_D3；已在记录复制后执行`SCB_CleanDCache_by_Addr()`并保留DSB/ISB，记录区域放在`RAM_D3+0x100`。对最终生产映像受控跳转到`HardFault_Handler`，复位前后17个32位字完全一致：magic=`0x4641554c`、exception=`4`、checksum=`0x1d7da5c8`。这是记录持久化链路的现场通过；该跳转不是硬件产生的真实异常，不能将其堆栈字段或状态寄存器写为真实故障根因。
- A项外部CAN高负载已按下文非停机窗口执行并通过；C项TF写入中物理断电仍未执行，保持未通过。

## 2026-07-26 高负载失败后的候选修复

- 早期外部CAN首测使用暂停式GDB读取，在约1 kfps输入下暂停内核足以填满FIFO；恢复时又会突发drain，使FIFO `full/lost`与软件RX queue `drop`增长。因此该方法不能区分读数期与正常运行期丢帧，原`full=3->5/lost=4->6/drop=15->25`不再作为运行态失败证据。
- 最初的32项完整`CanFrame`队列候选不能启动全部任务，因额外约1920 B FreeRTOS heap使最后rule任务创建失败，已撤回；不得将其烧录或启动状态写为通过。当前候选保留FIFO16，并以32项紧凑classic-CAN队列替换8项完整队列：元素仅含`id/IDE/DLC/8字节数据`，DecodeTask重建现有`CanFrame`后保持原DBC路径。`./scripts/verify.sh`的CTest=`19/19`通过；最终ELF反汇编确认FIFO16、`xQueueGenericCreate(32, 16)`、紧凑队列发送/接收和重建路径。该候选已烧录，OpenOCD输出`Programming Finished`、`Verified OK`、`Resetting Target`。
- 用户确认500 kbit/s外部CANtest持续发送后，用OpenOCD telnet `mdw`读取RAM，日志仅含telnet连接、未出现`halted`，每次读取后显式`shutdown`且无调试服务遗留。C→D→E两个连续20秒窗口：FIFO `full/lost=2/2->2/2->2/2`，RX queue drop=`0->0->0`；RX enqueue=`117033->140962->167987`、dequeue=`117030->140957->167983`；DBC matched=`614->653->696`、signal updates=`1228->1306->1392`、last message ID持续为`0x321`。A项判定通过。
