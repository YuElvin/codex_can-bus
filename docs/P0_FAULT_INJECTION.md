# P0 目标板故障注入验收矩阵

本文件只定义`codex/W5500`当前候选的现场验收步骤，不会通过脚本自动烧录、复位、暂停任务、拔卡或断电。执行前必须由操作者明确确认已连接目标板，并在每次调试读取后恢复目标运行。

## 候选映像

本矩阵仅适用于下列本地构建产物；重新构建或修改源码后必须重新计算哈希并重新执行相关项目。

| 文件 | SHA-256 |
| --- | --- |
| `build/stm32h750/can_bus_gateway_stm32h750.elf` | `5f98bfa3e3af180219e0429734ff99d4c13a65645733356b387387eb17df7987` |
| `build/stm32h750/can_bus_gateway_stm32h750.hex` | `6f3e92ab95c91e79133a57710873c0dc9c20b3b8621bcab9f87aa4c4692144f8` |

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
- A项外部CAN高负载和C项TF写入中物理断电复测均已按下文执行并通过。B项IWDG任务卡死复位与Crash Dump保持链路通过；受控跳转到HardFault handler仍不冒充真实硬件异常的原始根因栈。

## 2026-07-26 高负载失败后的候选修复

- 早期外部CAN首测使用暂停式GDB读取，在约1 kfps输入下暂停内核足以填满FIFO；恢复时又会突发drain，使FIFO `full/lost`与软件RX queue `drop`增长。因此该方法不能区分读数期与正常运行期丢帧，原`full=3->5/lost=4->6/drop=15->25`不再作为运行态失败证据。
- 最初的32项完整`CanFrame`队列候选不能启动全部任务，因额外约1920 B FreeRTOS heap使最后rule任务创建失败，已撤回；不得将其烧录或启动状态写为通过。当前候选保留FIFO16，并以32项紧凑classic-CAN队列替换8项完整队列：元素仅含`id/IDE/DLC/8字节数据`，DecodeTask重建现有`CanFrame`后保持原DBC路径。`./scripts/verify.sh`的CTest=`19/19`通过；最终ELF反汇编确认FIFO16、`xQueueGenericCreate(32, 16)`、紧凑队列发送/接收和重建路径。该候选已烧录，OpenOCD输出`Programming Finished`、`Verified OK`、`Resetting Target`。
- 用户确认500 kbit/s外部CANtest持续发送后，用OpenOCD telnet `mdw`读取RAM，日志仅含telnet连接、未出现`halted`，每次读取后显式`shutdown`且无调试服务遗留。C→D→E两个连续20秒窗口：FIFO `full/lost=2/2->2/2->2/2`，RX queue drop=`0->0->0`；RX enqueue=`117033->140962->167987`、dequeue=`117030->140957->167983`；DBC matched=`614->653->696`、signal updates=`1228->1306->1392`、last message ID持续为`0x321`。A项判定通过。
- C项断电前基线已通过：用户修正CANtest为实际“列表发送”后，外部SignalCache返回marker=`42434`、sequence=`4658`、quality=`ok`。非停机A/B读数中`g_log_write_count=2039->2198`、`g_log_flush_count=2039->2199`、文件大小=`1193019->1285557 B`、`g_log_failure_count=0`、历史`g_log_drop_count=56990->56990`；TF open/write/`f_sync`/close结果均为0。用户随后在持续记录和CAN输入期间切断整板/ST-Link电源；HTTP连接超时、ping=`0/2`确认目标离线。
- 断电取卡后确认设备为`/dev/disk4s1` FAT32，并重新以只读方式挂载。`/log`和目标`/log/20260726_194921000_signal-v2.csv`均不可见，根目录存在异常名称；`diskutil verifyVolume /dev/disk4s1`调用`fsck_msdos -n`后报告`.Spotlight-V100`下`Invalid long filename entry`、检查退出码206、`Error -69845`。因此本轮C项失败，不执行CSV尾行或重上电恢复通过判定。卡保持只读，未运行修复或格式化。由于`.Spotlight-V100`异常可能在测试前已存在，当前证据不能把全部损坏唯一归因于本次断电；复测必须从已只读验证无错的干净FAT32介质开始。
- 后续状态复核发现macOS校验流程曾把卷恢复为可写挂载；发现后立即执行`diskutil unmount`和`diskutil mount readOnly`，`Volume Read-Only: Yes`。没有人为写文件，但自动可写挂载进一步使`.Spotlight-V100`异常的形成时点无法唯一归因；复测必须从插卡起就阻止主机可写介入。
- 源码复核发现FatFs `f_sync()`虽会调用`disk_ioctl(..., CTRL_SYNC, ...)`，但整改前`cube_mx/FATFS/Target/sd_diskio.c`的`CTRL_SYNC`分支无条件返回`RES_OK`，没有等待`BSP_SD_GetCardState()`进入`SD_TRANSFER_OK`。因此原板端`f_sync=FR_OK`不能作为介质内部编程已完成的充分证据。该底层合同缺陷是本轮最小整改目标；修复和干净卡复测完成前，C项保持失败。
- 最小整改另覆盖实际可能使用的非32字节对齐scratch写分支：该分支原先只等待DMA回调，没有像对齐分支一样等待卡ready。现在每扇区回调后调用`SD_CheckStatusWithTimeout(SD_TIMEOUT)`；`CTRL_SYNC`也调用同一函数，超时均返回`RES_ERROR`。未改变LogTask、CSV/HTTP合同、512 B/5 s批量策略、长文件名或存储架构。
- 修改后`./scripts/verify.sh`通过，CTest=`19/19`，固件text/data/bss=`112700/768/243932`；ELF/HEX SHA-256=`5f98bfa3e3af180219e0429734ff99d4c13a65645733356b387387eb17df7987`/`6f3e92ab95c91e79133a57710873c0dc9c20b3b8621bcab9f87aa4c4692144f8`。`SD_CheckStatusWithTimeout`反汇编包含`HAL_GetTick`、30000 ms比较和`BSP_SD_GetCardState`轮询；`SD_ioctl`与`SD_write`均实际调用它。该候选尚未烧录，必须在测试前只读校验无错的FAT32介质上重复完整断电流程。
- 用户授权后，对失败卡先创建完整物理盘备份`/Users/elvin/Downloads/tf_card_powerloss_failure_20260726.dmg`。镜像覆盖15.6GB/30560256个512 B扇区，`hdiutil verify`为VALID；只读挂载回读为相同MBR/FAT32结构，SHA-256=`0d0da27415ed931651ab0656aa06d63db6f824e5ead25d6d1e0dbc51d4af5382`。确认镜像有效后，才对再次核对为外置、可移除、非虚拟15.6GB的`/dev/disk4`执行`diskutil eraseDisk FAT32 CANLOG MBRFormat`。
- 格式化完成后立即卸载，以未挂载卷执行`diskutil verifyVolume`；底层`fsck_msdos -n`完成FAT、目录和孤立簇三阶段，退出码0。macOS在格式化后自动挂载期间创建了`.Spotlight-V100/.fseventsd`，但两者已包含在该退出0的测试前基线中；卡随后显式只读检查并安全弹出。下一步须重新拔插电脑，恢复仓库最新`/www/index.html`以及SHA-256=`271f20f923343c9f923bd6db4da4599e0349983b0a5bd43edeae87d152855417`的151 B标准DBC到`/dbc/active.dbc`和`/dbc/candidate.dbc`；`/config`默认文件由固件启动创建，`/log`由记录会话创建，不从损坏镜像回拷。
- 重新插入电脑后，卡仍解析为15.6GB外置`/dev/disk4`、卷`CANLOG`。已复制仓库最新网页到`/www/index.html`，并从`deploy/tf/dbc/active.dbc`复制`/dbc/active.dbc`与`/dbc/candidate.dbc`。网页`28989 B`、SHA-256=`c9c8e057f1d7bd89672b2c84ef6b03c00b6ac13f3677f779373a9f4d4504ca9c`；两份DBC均为`151 B`、SHA-256=`271f20f923343c9f923bd6db4da4599e0349983b0a5bd43edeae87d152855417`，源/目标和两DBC之间`cmp`均通过。删除本次复制产生的5个精确AppleDouble旁车后，卡内项目文件只剩上述三项。
- 复制完成后再次在卸载状态执行`fsck_msdos -n`，退出码0；只读重挂载复核三个大小/哈希不变且不存在`._*`，随后卡已安全弹出。仓库新增`deploy/tf/README.md`记录最小复制/禁止恢复边界，新增`tf_deploy_assets`主机测试直接解析实际部署DBC；最新`./scripts/verify.sh`为CTest=`20/20`，固件尺寸/哈希不变，重新反汇编仍确认`SD_ioctl`与scratch写路径调用有界ready等待。
- 用户将卡插回断电板并恢复供电后，OCD预检查识别`STLINK V2J37S7`、STM32H7 Cortex-M7、目标电压3.266890 V；预检查与烧录会话均显式shutdown。对修复HEX执行烧录，实际输出`Programming Finished`、`Verified OK`、`Resetting Target`，烧录电压3.247626 V。
- 冷启动基线：en2路由正确、ping=`3/3`；状态API为RTOS ready、W5500 status/link/version=`0/1/4`、TF/QSPI=`0/0`。网页内容哈希=`c9c8e057...ca9c`，与仓库一致；active DBC=`loaded=true/generation=1/bytes=151/lines=3/messages=1/signals=2/skipped=0/errors=0`；规则从缺失V4安全回退v2。CAN当前`tx=55/rx=0/errors=0`，SignalCache为空，日志默认`enabled=false/timeSynced=false`。因此尚不具备外部有效信号持续写入前置，不能执行第二次物理断电。
- 用户恢复CANtest列表发送后，两组API快照中CAN RX=`57899→60031`，错误/Bus-Off/TEC/REC均为0；SignalCache稳定为marker=`42434`、sequence=`4658`、quality=`ok`。以100 ms启动`/log/20260726_222238000_signal-v2.csv`，非停机A/B快照中活动文件=`122743→138295 B`，write/flush=`213/213→240/240`，failure/drop均为0，TF open/write/sync/close结果均为0。关闭OpenOCD后CAN RX继续增至145592，日志仍启用，随后用户在CAN和日志持续活动时直接断电。
- 断电后用户把卡插入电脑；介质仍为15.6 GB外置物理`/dev/disk4`、FAT32 `CANLOG`，立即卸载自动挂载并显式只读重挂载。目标CSV存在且为399223 B、4159行、SHA-256=`6d520cd29fede3553566062ca182c1f984276c6b313e4fab20e64794960f1a04`；其中4158条数据全部8列、时间单调、marker/sequence成对、quality=`ok`，最后字节为换行，没有撕裂尾行。
- 卸载卷后`diskutil verifyVolume /dev/disk4s1`实际调用`fsck_msdos -n`，完成FAT、目录和孤立簇检查，退出码0。只读重挂载后网页和active/candidate DBC分别与仓库源`cmp`一致；TF随后安全卸载并弹出。
- TF插回板端重新上电后的恢复验收通过：ping=`3/3`，ST-Link电压3.267470 V；RTOS、W5500、TF、QSPI状态正常，W5500 version复测为4，网页哈希一致，active DBC为151 B/1 message/2 signals/0 errors。CAN RX=`76447`且两项信号quality=`ok`，错误/Bus-Off/TEC/REC均为0。
- 复位后日志按设计恢复为关闭且时间未同步；重新以100 ms启动`/log/20260726_223433000_signal-v2.csv`，非停机A/B快照中文件=`39223→54775 B`、write/flush=`68/68→95/95`，failure/drop均为0，TF open/write/sync/close结果均为0。验证后正常停止日志，CAN RX已增至146762且总线错误仍为0，OpenOCD/GDB和调试端口均已释放。C项判定通过；该结论证明本轮干净介质与修复候选的实测恢复，不扩大为FAT32任意掉电时刻的原子保证。
