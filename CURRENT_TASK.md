# 当前任务

更新时间：2026-08-01

## 项目目标

交付基于 STM32H750VBTx、W5500、MCP2562FD、TF、W25Q128 和 FreeRTOS 的 CAN/CAN-FD 数据采集网关；各功能必须以构建、反汇编、烧录和现场证据验收，不能以源码存在或单次 HTTP 200 代替。

## 当前阶段

- 2026-08-01阶段17“大 DBC selected-only闭环”A0-F门禁客观通过，G源码、host、最终固件与实板收口正在进行。恢复双ID外部输入后，最终固件总RX=`2826→3098`、selected matched=`1413→1549`、updates=`11304→12392`，准确满足总RX增量272、selected增量136、updates=`136×8`；page0八项为预期raw/value且`GOOD`、page1仍全`MISSING`。v3日志已在这些`GOOD`值下进入ACTIVE，锁定active generation7、selection CRC=`62BA0D66`、count128和`/log/20260801_175230000_signal-v3.csv`。下一动作需要用户停止`0x100`、保持`0x110`，取得超过3000 ms的STALE和值保持证据；随后停止会话、取卡只读核验CSV/meta。此前MISSING/分页/过滤/400、128项/1000 ms=`422 rate_limit`和日志期间写操作409均已实测。最终`./scripts/verify.sh`为CTest=`34/34`，Flash=`121708 B`、RAM_D1=`196664 B`，ELF `text/data/bss=121256/444/196284`，ELF/HEX SHA-256=`97cd14af626ecb7951f1f012148819f922c688b339b1f9185d66fbd630afc10b`/`963fa24f74a9808bc0687ab2ab4e5cd7ba2e7d3c5d1b912ae5794c08816e5b7d`；H和CAN-FD实板仍`[未验证]`。
- 2026-08-01新增P0审计修正尚未烧录：发现生产active manifest旋转在`tmp→current`或短临界publish失败时只依赖下次previous恢复，不满足即时保留旧current。现已增加同一TF锁内的previous→current回滚；新候选`verify.sh`为CTest=`34/34`、Flash=`121836 B`、RAM_D1=`196664 B`、ELF/HEX SHA-256=`b77fc9de42ef3257b3155ea4f12d942d1903395671bd22c4c2173b04322aae09`/`1ca3ac65704aae4116a77c0ed90e22a46de970b36e0b038758dae0815dd800dd`。当前板仍为先前G映像并有ACTIVE日志；必须先取得STALE/介质证据并正常停日志，才允许烧录新候选并重做必要active/外部CAN回归。
- 2026-08-01旧G映像日志收口：用户停止`0x100`且保持`0x110`后，RX=`13257→13357`仍增长，page0八项保留GOOD末值/raw并全部转为`STALE`，完成最终映像的GOOD→STALE与未选流量隔离。v3会话已`STOPPING→STOPPED`，路径仍为`/log/20260801_175230000_signal-v3.csv`。下一外部动作是用户下电取卡插入Mac；主会话将只读核验CSV/.meta、`fsck_msdos -n`，并在同一次取卡窗口部署当前`www/index.html`。完成后才烧录P0回滚候选并重做active/Classic CAN最小回归。
- 2026-08-01 G实体TF证据阻断：取卡后只读`diskutil verifyVolume /dev/disk4s1`报`fsck_msdos`退出`206`，根目录有end marker后的目录项；current v3 `.meta`链空闲且声明`10471 B`而最多仅`8192 B`，current v3 `.csv`链空闲且声明`1153932 B`而最多仅`32768 B`，并见candidate/active generation链与`active.previous`越界/交叉问题。卷已卸载且只读复挂载失败；不得把可枚举的CSV/meta、`cleanClose=false`或主机`shasum`当作有效持久化/clean footer证据。不得继续写卡、部署网页或烧录待验证P0候选。需要用户决定先制作只读镜像保留故障证据，还是确认放弃当前未可信会话后重建FAT32并从仓库静态资产重部署；重建后G实体日志和新P0候选实板回归均须重新取得。
- 2026-08-01 用户已明确允许修复/格式化并重载。经两次严格限定为外置`/dev/disk4`的`MBR + FAT32 CANBUS`重建，按`deploy/tf/README.md`写入`/www/index.html`、`/dbc/active.dbc`和相同`/dbc/candidate.dbc`，并创建`/log /config /sys`空目录。网页SHA-256=`9b96b6a022a9a3c1659abf52096543ebefba3e57e1218fdd5d6c62e92ce0e5c2`、两DBC均151 B/SHA-256=`271f20f923343c9f923bd6db4da4599e0349983b0a5bd43edeae87d152855417`且`cmp`通过；卸载后`fsck_msdos -n` exit=`0`，只读重挂载再次`cmp`后已卸载。此仅恢复干净主机部署基线，故障会话、large-DBC generation、规则和日志均未恢复/伪造；必须插回板端上电后重新建立candidate/active和G实体日志证据。

- 2026-07-27已确认并修复最新网页回归中的确定性状态机缺口：原固件在socket0为`ESTABLISHED`、`RX_RSR=0`且无ACK/disconnect pending时会永久直接返回，原始TCP客户端连接但不发数据可稳定占满唯一socket并拒绝后续HTTP。最小补丁只为该零数据连接增加100 ms有界等待，超时复用既有graceful `DISCON`；正常请求收到任意字节即退出计时，响应ACK和disconnect recovery仍保持500 ms。最终`./scripts/verify.sh`为CTest=`20/20`，ELF text/data/bss=`112828/768/243948`，ELF/HEX SHA-256=`742dbe264a5f6ea7282123fd151ff67aac30cd410ec5a41a0acb331092b2b92f`/`0e6396e22dfb8d85627e626314aa6d9fca61abf40efeab2fd01d8baf41c01025`；反汇编、ST-Link Programming/Verified/Reset和实板复验均通过。10轮空连接全部在`110.4..146.3 ms`收到EOF且每轮后续HTTP成功，timeout count=`10`、recovery count=`0`、socket最终LISTEN。浏览器5次新页面均成功，manual自动刷新4次最终均应用且无`Failed to fetch`；最终9个API全部HTTP200、ping 3/3、任务循环增长。浏览器一次约27秒长尾的同步诊断中socket已LISTEN且idle count未增长，因此不归因于本补丁；单socket短连接排队仍是已知能力边界，不扩大为并发HTTP改造。
- 安全审查P0整改已完成源码、CTest=`19/19`、最终ELF反汇编、ST-Link烧录、IWDG受控任务卡死复位、Crash Dump保持链路及外部CAN高负载验证。当前板上为紧凑队列候选ELF/HEX SHA-256=`a32adce3c188bf859adaf36bd8c7326ed7b76c0aee0403cf4e0f6ba9fbe14fef`/`4bb09f44080ad7bf8db1ddca8b6f358bd9da484b229388feb986cbfc8165f5ad`：FIFO0=`16`，RX队列为`32 x 16 B Can2RxQueueFrame`。完整32项`CanFrame`候选已因FreeRTOS heap不足导致rule任务创建失败而撤回。烧录实际得到`Programming Finished`、`Verified OK`、`Resetting Target`；外部500 kbit/s负载的两个连续非停机20秒窗口中FIFO `full/lost=2/2->2/2->2/2`、RX queue drop=`0->0->0`，RX入队=`117033->140962->167987`、DBC匹配=`614->653->696`，故当前候选的运行态无丢帧通过。前一生产快照IWDG=`PR=6/RLR=1000/SR=0`、refresh=`32->35`、CAN loop=`1086->1182`、unhealthy=`0`。Crash record在`0x38000100`，写入后Clean D-Cache，受控HardFault handler复位前后校验和一致；这不替代真实硬件异常的根因栈证据。
- 早期“FIFO full/lost和RX drop增长”的GDB暂停式读数已确认被调试暂停污染：目标暂停期间外部1 kfps输入会填满FIFO并在恢复时突发drain，后续只采用非停机读取；高负载P0已闭合。TF首次物理断电复核失败：`/log`及目标会话文件不可见，`fsck_msdos -n`返回206。源码已最小修复`CTRL_SYNC`无条件成功和scratch写分支未等卡ready；最新`./scripts/verify.sh` CTest=`20/20`，ELF/HEX=`5f98bfa3e3af180219e0429734ff99d4c13a65645733356b387387eb17df7987`/`6f3e92ab95c91e79133a57710873c0dc9c20b3b8621bcab9f87aa4c4692144f8`，反汇编确认两条有界ready等待。失败卡整盘镜像已校验，介质重建为MBR/FAT32后恢复网页/active/candidate DBC并以卸载后`fsck=0`确认；候选烧录、冷启动TF/DBC/网页基线、外部CAN持续落盘和第二次物理断电后的主机只读验收均通过。目标CSV为399223 B/4159行，4158条数据全部8列、时间单调、末尾换行且无撕裂行；`fsck_msdos -n`退出0，网页和两份DBC与仓库源一致。TF插回重新上电后RTOS/W5500/TF/QSPI、网页、active DBC和外部CAN恢复；新日志文件=`39223→54775 B`、write/flush=`68→95`，failure/drop=0且底层open/write/sync/close均为0。P0故障注入A/B/C阶段判定通过，下一步在阶段提交后按审查报告选择仍未关闭的下一项高风险整改，不扩大本次P0补丁。执行细节见`docs/P0_FAULT_INJECTION.md`。

- 新一轮“FAT 属性本地时间与记录会话文件”已[最终实体 TF 验收通过]。浏览器本地 UTC offset 用于FAT文件属性本地时间；CSV时间列保持UTC；每次开始记录创建以该次本地开始记录时间开头的会话文件名，停止记录结束该会话；未同步时`get_fattime()`返回FatFs有效下限`1980-01-01 00:00:00`，不再返回导致属性显示为1970的零值。已严格只读确认会话文件`/Volumes/NO NAME/log/20260724_012916204_signal-v2.csv`存在、大小`8035 B`；macOS CST 创建/修改时间为`2026-07-24 01:29:16/01:29:36`，均非1970。表头为`utc_time,unix_ms,updated_ms,key,value,raw,unit,quality`，首条 UTC 记录为`2026-07-23T17:29:16.745Z`。`/log`目录自身创建时间显示1970仅为旧目录历史元数据，不否定新文件属性通过。
- 新需求阶段“网页时间同步与可选时间日志”处于[现场主体通过；实体 CSV 内容已只读复核]：无 RTC/NTP 的 RAM 时间基准由`POST /api/time/sync`的`unixMs`设置，重启后失效；`GET/POST /api/log/control`返回或设置`enabled`、`samplePeriodMs`、`timeSynced`、`unixMs`和新路径。记录默认关闭，周期限制`100..10000 ms`；未同步时首次启用携带`unixMs`会自动同步。
- 新日志仅写`/log/signal-v2.csv`，CSV 新表头为`utc_time,unix_ms,updated_ms,key,value,raw,unit,quality`；旧`/log/signal.csv`既有内容和六列表头未被混写或改动。本轮`./scripts/verify.sh`实际通过，host tests=`18/18`；首次 STM32 构建仅出现`http_handle_log_control()`局部`enabled`可能未初始化警告，已用最小`= false`初始化修复后重新构建无该警告。最终 ELF `text/data/bss=109276/764/243712`，关键反汇编确认时间基准初始化、日志启停/新路径与两个 HTTP 路由均在最终映像中；OpenOCD/ST-Link已报告`Programming Finished`、`Verified OK`、`Resetting Target`，电压=`3.280054 V`。
- 现场主体已完成：TF 新网页正确加载，默认记录关闭、继电器详情折叠；未同步的`250 ms`启动自动同步并可回读，随后完成停止`800 ms`、再启用`1200 ms`、手动同步和最终停止。自动刷新期间安全提交的最终`request/applied=6/6`且 CAN 计数增长；继电器红色闭合/绿色断开两轮反向输出均实际操作并最终恢复关闭。
- 历史首读的`"unixMs":lu`非 JSON 缺陷已由最小 64 位格式化修复后重建、反汇编和重烧录关闭，不能再作为当前阻断。ST-Link 两次采样中日志写入`33→40`、文件`18185→22217 B`、失败/丢弃均为`0`，运行态证明`/log/signal-v2.csv`写入链路已成功 flush 并增长；随后已在`/Volumes/NO NAME/log/signal-v2.csv`完成实体只读复核：大小`28553 B`，表头精确为`utc_time,unix_ms,updated_ms,key,value,raw,unit,quality`，首末UTC为`2026-07-23T16:59:32.051Z→2026-07-23T17:03:34.638Z`，记录含`Can2Data.marker=42434`、`Can2Data.sequence=4660`及`quality=ok`。时间列实体内容现已实际读取确认。
- 阶段14“网页运行中手动 TX 与两槽活动 DBC `signalKey` 规则”现场验收完成，继续保持 W5500 单 socket、非并发边界。正确部署 TF 网站根目录的新网页后，自动刷新现场 TX/RX 从`55/364`增长至`576/5540`；浏览器 warn/error 日志为空。
- 自动刷新运行中两次编辑 TX 均未被覆盖：`C2 A5 78 56 02 03 04 05`等待`1800 ms`仍保留，提交后表格已应用且`result=0`；随后`C2 A5 00 01 02 03 04 05`等待`1300 ms`仍保留并提交恢复。最终 TX DBC `sequence=256`。
- 已完成的静态与烧录证据：CMake STM32 链接选项已增加`-Wl,-u,_printf_float`，修复 RuleFile V4 threshold 浮点格式化为`0`的问题。`./scripts/verify.sh` host tests=`17/17`通过，STM32 firmware构建成功，最终ELF `text/data/bss=106748/764/243688`；`nm`/map确认`_printf_float`、`_dtoa_r`、`_vfiprintf_r`存在，`objdump`确认`rule_file_format_decimal`调用`sniprintf`。2026-07-23 OpenOCD刚完成对`build/stm32h750/can_bus_gateway_stm32h750.hex`的`Programming Finished`、`Verified OK`、`Resetting Target`。
- 候选 DBC 已获用户授权激活；最终 runtime 为`loaded=true/generation=1/bytes=151/messages=1/signals=2`。TX/RX 表均解析出 marker=`42434`，TX `sequence=256`、RX `sequence=4660`。
- 两槽目录均含 marker/sequence；slot1 已保存并从 V4 回读为`Can2Data.sequence`、threshold=`4660`、priority=`20`、action=`off`。外部 RX `sequence=4660`时 manual 状态`relay1Output=0`，与高优先级 off 规则一致。
- 一期全量功能已完成；`W-1 后台网页两轮实际回归`已完成。人工已将更新后的网页文件写入 TF 并重新上电；同一候选 HEX 已烧录，首轮与退出后新页面第二轮均完成。
- 本轮前端 FIFO 修复已在自动 CAN 刷新期间实际验证：DBC 上传与激活均成功，不再出现“已有请求进行中”。CLOSE_WAIT 最小修复也已现场复验：最终概览`lastNonclosedClose=0`；这只说明本次两轮回归未再观察到旧`0x11c`，不作长期绝对结论。
- 网页 CAN 发送控制阶段已客观通过：用户已将修复后网页写入 TF 并上电，随后确认 CANtest 已发送。首次自动刷新后状态灯为绿（`status-lamp ok`），TX/RX累计从`120/273`增至`134/417`；网页可逆关闭再恢复发送，最终配置为`0x321`、DLC=`4`、`C2 A5 34 12 00 00 00 00`、`1000 ms`，应用结果为`requestSeq=appliedSeq`且`lastResult=0`。TX self-test与外部 RX 两张 CANoe 式 DBC 表均显示 marker=`42434`、sequence=`4660`、quality=`ok`；CANtest输入确认后外部 RX 持续增长。
- 修复后的严格串行网页自动刷新及两次 reload 重入均未再出现端口80连接拒绝：两次重入样本分别为 TX/RX=`145/527`、`162/694`。四个详情区默认折叠，浏览器控制台 warn/error 为空。此证据证明本轮网页与外部 RX 输入链路；未直接读取 CANtest 显示屏确认其逐帧收到本轮受控 TX，故不将该项写成外部接收器读回证据。

## 成功标准

1. 以最终提交对应的同一 ELF/HEX 为唯一候选，逐项审计最终合同中的硬件、CAN、DBC、TF/日志、规则/继电器、网络/API、Web 和稳定性证据。
2. 历史高成本证据只在最终差异未触及对应模块且本轮烟雾回归通过时复用；缺口必须明确列出并执行最小补验。
3. 所有必验项均有可追溯的构建、反汇编、烧录、现场读数或文件证据；任何未通过项保持项目未完成。
4. 汇总最终固件哈希、提交哈希、验收矩阵和已知非目标，全部通过后才允许标记项目完成。

## Git 基线与工作树

- 本轮开始基线为`3779590 Record web regression delivery`；HTTP零数据连接修复与阶段治理已提交为`ae01c57 Recover idle HTTP connections`并推送到`origin/codex/W5500`。
- P0 TF掉电同步修复、部署资产测试和治理提交为`691d509 Complete P0 TF power-loss hardening`，已推送到`origin/codex/W5500`。该提交对应最终ELF/HEX SHA-256=`5f98bfa3e3af180219e0429734ff99d4c13a65645733356b387387eb17df7987`/`6f3e92ab95c91e79133a57710873c0dc9c20b3b8621bcab9f87aa4c4692144f8`及本轮20/20、反汇编、烧录、物理断电与恢复证据。
- P0治理提交`b288f48 Record P0 TF delivery`也已推送，本地/远端ahead/behind=`0/0`。重新对照原始5页审查报告后，报告明确列出的5个P0均已关闭；剩余确认高风险必要项为局域网写操作授权和生产CAN TX白名单。当前阻断是缺少生产合同：允许的CAN ID/DLC/数据约束/最小周期，以及采用何种授权载体；不得把测试帧`0x321`或仓库内默认token擅自固化为生产策略。
- 上一阶段功能提交为`13613f637554102f3b8f105fb88f657e7c1ae38e`（`13613f6 Add time-synced web logging controls`），已推送至`origin/codex/W5500`；本轮启动只读核对为工作树干净、本地与远端ahead/behind=`0/0`。
- 当前板上对应固件提交`ae01c57`，ELF/HEX哈希为`742dbe264a5f6ea7282123fd151ff67aac30cd410ec5a41a0acb331092b2b92f`/`0e6396e22dfb8d85627e626314aa6d9fca61abf40efeab2fd01d8baf41c01025`。`691d509`为上一P0固件提交，`0ef7d3e`仅为更早的一期基线。
- 本轮只修改HTTP零数据连接回收和相关诊断字段；未修改网页、CMake、CAN、DBC、规则、继电器、TF或日志业务源码。
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

## 本轮收尾

先前旧页误部署、活动 DBC 缺失、首次 TX 输入被自动刷新覆盖及 RuleFile V4 浮点 threshold 回读为`0`均已对应修复并以最终现场复验关闭。本轮仅同步治理 Markdown，未修改源码、网页、CMake 或`PROJECT_FINAL_ACCEPTANCE.md`，未重新编译或执行新的反汇编；构建、`objdump`与烧录证据均为此前已实际完成的同轮候选事实。TX self-test 不作为外部接收器证明；本轮外部 RX 结论仅基于实际 RX 表及输入。

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

## 2026-08-01 大 DBC P0 提交回滚与 TF 错误传播静态闭环

- 新干净TF的主机只读`fsck_msdos -n`已通过，但卡已安全弹出，尚待重新插板上电；旧损坏卡上的generation、规则与v3 CSV/meta不恢复也不作为通过依据。
- candidate和active manifest均已实现`current→previous→new current`失败即时回滚：new current rename、读回或active runtime publish失败时，同一TF锁内尝试恢复previous；回滚I/O失败时保留previous供启动恢复，绝不删除旧generation。
- FatFs镜像FAT及FAT32 FSINFO的`disk_write`失败现返回`FR_DISK_ERR`，不再被同步路径静默忽略。此仅增强失败可见性，不构成旧卡损坏根因或物理掉电通过。
- `./scripts/verify.sh`实测CTest=`34/34`，FLASH=`121948 B`、RAM_D1=`196672 B`、text/data/bss=`121496/444/196292`；ELF/HEX SHA-256=`2d225a84feee60027e294ea6154c1f7e59882680cdb41fb64c39fa7d212dd60a`/`b886bbf945c6d6a74193c258ab1c14a67548fe48968a5b4cf04ac3e0c0a2dc31`。候选尚未烧录，实板结论均`[待确认]`。

## 2026-08-01 大 DBC新卡实板B–E与HTTP收口修正

- 已烧录新候选（ST-Link `Programming Finished`、`Verified OK`、`Resetting Target`，3.262903 V）。实际目标为`192.168.1.88`；`192.168.1.100`属于Mac `en2`，先前对后者的ping不作为板端证据。冷启动TF=0、W5500 link=1、网页SHA-256=`9b96b6a022a9a3c1659abf52096543ebefba3e57e1218fdd5d6c62e92ce0e5c2`。
- 真实100071 B DBC上传获202（generation1、CRC32=`4B88D9CE`），板端构造candidate token=`0000000000000001-000186E7-4B88D9CE`、112消息/896信号；selection将ordinal0–7提交为generation2、8信号/1消息。active提交为generation1，candidate2，selection CRC=`E5CB6C8F`，selected-only runtime slot0精确8信号。
- 长selection响应曾完整到达但socket0残留`ESTABLISHED`。最小修正为`SEND_OK`后直接graceful `DISCON`，无接口或并发改动；重烧录后candidate恢复查询、紧随status/page1及active长请求均连续HTTP200。受控复位的早期runtime空态是恢复窗口；其后candidate和runtime均回读generation1/2、100071 B/1消息/8信号，正常active reload通过。
- 当前最终候选`verify.sh` CTest=`34/34`，FLASH=`121836 B`、RAM_D1=`196672 B`、text/data/bss=`121384/444/196292`，ELF/HEX SHA-256=`9fb5986eca59f5709ac4ad87d079484e022f7148c2bed6cac50177b7fe598704`/`c3d0608e97e3f42c6136afa0d068be0dde81baf33e989f346c12bdd6793ab5c3`。外部Classic RX、GOOD/STALE、实体v3 CSV/meta及TF写失败/掉电路径仍`[待确认]`。

## 2026-08-01 F外部Classic CAN正向证据

- 用户确认外部设备正在发送标准Classic CAN `0x100`与`0x110`。不暂停目标的HTTP样本显示`rx=665→770`、`errors=0`、`busOff=0`、`tec=0`、`rec=0`；8个selected信号全部`GOOD`，raw/value为`12000/1200`、`-250/-25`、`3300/3.3`、`80/0`、`2/2`、`3/3`、`10/10`、`1/1`，与`0x100` payload=`E0 2E 06 FF E4 0C A5 69`一致；page1 ordinal8..15仍未选且未进入API。
- 受控ST-Link/OpenOCD读取后均执行`monitor resume`、`detach`并关闭调试服务。读数为`g_can2_rx_count=3967`、`g_can2_dbc_rx_frame_count=3967`、`g_can2_dbc_matched_frame_count=1927`、`g_can2_dbc_signal_update_count=15416`、`g_can2_dbc_cache_count=8`、`g_can2_dbc_last_message_id=0x100`、`g_can2_dbc_decode_error_count=0`、`g_can2_dbc_stale_mark_count=0`；最近硬件RX快照为`ID=0x110`、`DLC=8`、首字节=`0x01`。这证明外部Classic RX、selected-only decode和无错误计数，不把TX self-test当作RX证据。
- 当前隔离动作尚未完成：需用户停止`0x100`、保留`0x110`后再判定STALE；实体v3 CSV/meta、TF写失败与物理掉电恢复仍`[待确认]`，CAN-FD实板仍`[未验证]`。

## 2026-08-01 F反向隔离与STALE门禁通过

- 用户确认已停止`0x100`并继续发送未选`0x110`。样本A/B间隔约11 s，CAN RX=`8034→8140`，errors/Bus-Off/TEC/REC均为0；`/api/signals?page=0`的8个selected项两次均保留raw/value=`12000/-250/3300/80/2/3/10/1`，`updatedMs=847693`不变，quality全部为`STALE`。
- 该结果关闭selected-only反向隔离和GOOD→STALE门禁：未选消息仍可进入CAN接收计数，但不更新ActiveRuntime/SignalCache/API selected slots。下一步恢复双ID后验证日志会话及TF上的CSV/.meta实体内容。

## 2026-08-01 G选择性日志控制门禁通过，实体介质待核验

- 恢复双ID后，`/api/signals?page=0` 8项回到`GOOD`且raw/value保持实测值；以`samplePeriodMs=1000`启动v3日志，HTTP200后状态进入`ACTIVE`，路径为`/log/20260801_191151000_signal-v3.csv`，锁定active generation=`0000000000000001`、selection CRC=`E5CB6C8F`、selectedCount=`8`。
- 日志ACTIVE期间尝试`POST /api/dbc/active`得到HTTP409 `logging_active`，日志身份未改变；停止请求经历`STOPPING`并回读`STOPPED`，锁已释放。该证据关闭日志准入、会话锁和正常停止控制面，不替代TF上CSV/.meta实体、clean footer或selected-only介质集合核验。
- 下一步需要用户下电取卡到电脑；主会话只读检查同名`.csv/.meta`、行列/quality/identity、文件大小和`fsck_msdos -n`，不得在证据读取前修复或格式化。

## 2026-08-01 G实体CSV/meta与TF只读核验通过

- 用户下电取卡后，唯一外置介质为`/dev/disk4`、FAT32 `/dev/disk4s1`。先卸载再执行两次`diskutil verifyVolume`，底层`fsck_msdos -n /dev/rdisk4s1`均 exit=`0`；只读挂载成功，读完后再次卸载并`diskutil eject`成功，期间未修复、格式化或写卡。
- `/log/20260801_191151000_signal-v3.csv`为32850 B、289行（含header）/288数据行；标准CSV解析确认8个唯一key严格等于meta中ordinal0..7的selected集合，每个key 36行、无额外key、8列结构正确，`GOOD=253`、`STALE=35`。CSV SHA-256=`c947eac1f1af9a485393dbaf7fb97ccd334635e41c1b7e07f7ca8d068fb8da05`。
- 同名`.meta`为1034 B、SHA-256=`5fadf433ddabf198b1614a6949635ef8684df3d3823904c880244a0c5a755dfd`；最终字段`cleanClose=true`、`rowsWritten=288`、`rowsDropped=0`、`lateSamples=0`、`writeFailures=0`、`flushCount=49`，身份为active generation1/candidate generation2/sourceSize100071/sourceCrc32=`4B88D9CE`/selectionCrc32=`E5CB6C8F`/selectedCount8。
- TF上的candidate current/previous与active current manifest CRC均有效；`dbc_index_dump verify`对三个index均返回`ok source_size=100071 source_crc32=4B88D9CE messages=112 signals=896 total=145232`；selection header/bitmap CRC、popcount=8及manifest引用交叉验证通过。G实体CSV/meta、FAT一致性和selected-only介质集合已关闭。
- H剩余：TF写失败注入、当前大DBC映像的物理掉电/active reload失败路径仍`[未验证]`；CAN-FD实板仍`[未验证]`。不得把既有旧路径或正常STOPPED会话替代这些异常证据。
