# Large DBC H1 目标板可控故障注入合同

本文件冻结阶段 H 剩余两类目标板验证的实验合同：日志 TF 持久化失败，以及 active 持久化/发布失败时旧 active/runtime 保持。它只定义实验构建和验收，不增加生产 HTTP API，不允许运行中拔卡，也不把软件注入写成 SD 电气故障复现。

## 阶段边界

- 目标：用一次性、可定位、可观测的实验标志触发既有失败/回滚分支。
- 假设：实验开始时日志为`STOPPED`，active generation1/candidate2/selection CRC=`E5CB6C8F`/slot0/8 signals 是受保护基线。
- 不确定点：实验映像的最终符号地址与哈希必须构建后由`nm`/`sha256`取得，禁止写死历史地址。
- 成功标准：每个点只触发一次并返回预期错误；旧 runtime identity、selected values、规则、继电器与非目标日志状态不变；重启仍恢复旧 active。
- 验证：host回滚测试、`./scripts/verify.sh`、独立实验构建、尺寸/符号/反汇编、烧录、HTTP/OpenOCD前后对照、重启恢复、最后重烧正式映像。
- 禁止扩大：不新增调试路由、CAN-FD、多socket、并发HTTP、热插拔、目录扫描或通用故障框架；HTTP 400/409/504不能替代注入。

## 构建隔离

新增独立CMake开关`CAN_BUS_LARGE_DBC_H1_FAULT_INJECTION`，默认`OFF`。不得复用`CAN_BUS_P0_FAULT_INJECTION`；实验构建必须显式保持后者为`OFF`。

仅实验构建存在以下普通`.bss`诊断；复位后arm必须回到0：

```c
volatile uint32_t g_large_dbc_h1_fault_once;
volatile uint32_t g_large_dbc_h1_fault_fire_count;
volatile uint32_t g_large_dbc_h1_fault_last_point;
volatile uint32_t g_large_dbc_h1_fault_last_operation;
volatile uint32_t g_large_dbc_h1_fault_last_result;
```

consume helper必须`used`、`externally_visible`、`noinline`、`noclone`。仅当`fault_once == point`时才在返回失败前依次清零arm、增加fire count并记录point/operation/result；未命中不得改变任何诊断。正式默认构建的`nm`必须不存在全部`large_dbc_h1_fault`符号。

P0 watchdog与H1开关同时为ON时，CMake必须配置失败；runtime publish不是FatFs操作，其`last_operation`固定使用具名sentinel `LARGE_DBC_H1_OPERATION_RUNTIME_PUBLISH=UINT32_MAX`。

## Fail-once 点

| 值 | 名称 | 精确语义 | 预期外部结果 |
| --- | --- | --- | --- |
| 1 | `LOG_APPEND_SYNC` | CSV append的`f_write`成功后、`f_sync`前一次性模拟`FR_DISK_ERR`；仍执行close | 日志进入`FAILED`、`enabled=false`、write failure增加；active/runtime不变 |
| 2 | `ACTIVE_CURRENT_WRITE` | 写`active.current.tmp`前一次性模拟持久化失败；旧current尚未轮转 | `POST /api/dbc/active`为HTTP507；旧manifest/runtime不变 |
| 3 | `ACTIVE_CURRENT_RENAME` | 旧current已转previous后，在`current.tmp→current`前模拟rename失败 | HTTP507；同一事务立即`previous→current`并读回旧manifest |
| 4 | `ACTIVE_CURRENT_READBACK` | 新current已rename后模拟最终读回失败 | HTTP507；删除新current并即时恢复previous，旧runtime未发布 |
| 5 | `ACTIVE_RUNTIME_PUBLISH` | active manifest提交后、真正调用runtime publish callback前返回prepared-slot失败 | HTTP422；回滚manifest、丢弃prepared slot，旧active slot/value不变 |

point 3/4/5的回滚I/O不得再次被注入，因为arm在返回失败前已经清零。point 5禁止在真正slot flip之后触发，否则无法证明旧RAM runtime未被改变。

## 现场矩阵

1. 记录实验ELF/HEX哈希、size、H1符号地址与五个调用点反汇编；确认P0 fault开关为OFF。
2. 烧录实验映像，读取`/api/dbc/runtime`、`/api/signals?page=0`、规则、继电器、日志和CAN基线。
3. `LOG_APPEND_SYNC`：先让新v3日志进入ACTIVE并至少成功flush一次，再用OpenOCD只写`fault_once=1`并恢复运行；等待下一flush，核对fire count、TF诊断、FAILED/disabled和runtime不变，再按接口合同恢复STOPPED。
4. active points 2..5：每点独立写arm并恢复运行，再发起一次正常无body active请求；记录HTTP507/422、专用diagnostics与通用transaction result。每点前后runtime generation/candidate/selection/slot/count、selected值、规则/继电器必须一致。
5. 每个active点后执行冷启动恢复检查；generation2失败残留必须被现有cleanup清理，旧current仍可加载。每次OpenOCD halt/read/write后都必须`resume`，会话结束必须`shutdown`。
6. 全部实验完成后重烧默认OFF正式HEX，记录`Programming Finished`、`Verified OK`、`Resetting Target`；复核正式哈希、`nm`无H1符号以及HTTP/runtime/外部Classic CAN最小烟雾。

## 证据边界

- 注入通过只证明上层收到`f_sync`/write/rename/readback/publish失败后的状态机与回滚；不冒充真实介质电气故障。
- 已完成的活动日志物理掉电、只读FAT/CSV检查与同卡冷启动恢复是独立证据，继续保留。
- host mock与源码分支用于支持定位，不能替代本文件要求的目标板fire count、HTTP/API和重启读数。
