# 大 DBC P0 冻结合同

状态：`A0 已冻结并经 host 验证`

合同版本：`v1`

日期：`2026-07-31`

权威公共常量：`include/large_dbc_contract.h`

实施依据：`/Users/elvin/Downloads/LARGE_DBC_AGENT_IMPLEMENTATION_GUIDE.md`

## 1. 目标、证据和边界

本文件只冻结后续 A1-H 共用的数据、失败不变量和容量合同，不实现 HTTP、TF、FDCAN、runtime、网页或日志业务。

本轮真实输入为：

- `/Users/elvin/Desktop/project/data/BNE_CLASSIC_CAN_TEST_100KB.dbc`；
- `100071 B`，SHA-256 `f3b7b76b16fc87554be7d5f82aa63e1fd98c5b9cf4b3c679f9cc7e7e65a6e0b8`；
- IEEE CRC-32 `0x4b88d9ce`；
- LF、1606 行、末尾有 LF；
- 112 条 `BO_`、896 条 `SG_`；
- 标准 ID `256..367`、全部 DLC=8；
- 最长 message/signal/key/unit 分别为 `26/25/52/4 B`；
- 672 个 key 超过当前 47 B 有效上限；
- 本文件无 multiplex、科学计数法、长元数据行、CRLF 或无尾换行，因此这些边界必须由合成 fixture 覆盖。

当前实板验收只以标准 11-bit Classic CAN、DLC=8 为必验路径。数据模型保留标准/扩展 ID、Classic/FD 和 DLC 0..64；扩展 CAN-FD 64 B 实板路径保持 `[未验证]`，不得以源码支持、上传成功或 TX self-test替代。

## 2. 公共容量合同

| 项目 | 冻结值 |
| --- | ---: |
| 原始 DBC 上限 | 262144 B |
| TF catalog 消息上限 | 2048 |
| TF catalog 信号上限 | 2048 |
| active runtime 消息上限 | 64 |
| active runtime 信号上限 | 128 |
| selection 位图 | 2048 bit / 256 B |
| message name | 最多 31 B，不含 NUL |
| signal name | 最多 31 B，不含 NUL |
| `message.signal` key | 最多 63 B，不含 NUL |
| unit | 最多 31 B，不含 NUL |
| parser 行 scratch | 512 B |
| API page size | 固定最大 8 项 |
| query | 最多 63 B |
| selection 单请求 ordinal 总数 | 最多 32 |
| upload IO chunk | 512 B |
| upload idle / total timeout | 5000 / 120000 ms |

`catalog messages` 与 `active messages` 是不同概念。真实输入的 112 条消息必须全部进入 TF index；只有被选择信号所属消息的去重集合受 active 64 条限制。

完整 DBC 和完整目录不得常驻 RAM。不得通过把旧 `DbcDatabase` 扩到 2048 信号并继续保留 candidate + runtime 双槽三份数据库实现本功能。

## 3. 编码、数值和 CRC

- 所有二进制格式都按字段显式编码，禁止直接写 `packed C struct`。
- 多字节整数固定 little-endian；字段宽度以公共头中的 offset/size 为准。
- 浮点固定为 IEEE-754 binary64 的 8 B little-endian bit pattern。
- parser 拒绝 NaN、Inf 和数值溢出；`-0.0` 在 definition hash 中规范为 `+0.0`。
- 完整性校验固定为 reflected IEEE CRC-32：polynomial `0xEDB88320`、initial/final XOR 均为 `0xFFFFFFFF`。
- CRC32 只做完整性和并发一致性校验，不是安全哈希或授权凭证。

DBC identifier 第一版限定为 ASCII 标识符；message/signal 名必须匹配 `[A-Za-z_][A-Za-z0-9_]*`。规范化key保留identifier原始大小写，并按byte-exact `message.signal`判重；只有搜索对ASCII字节做大小写折叠。unit允许有效UTF-8，长度按编码后字节计；不得按字节截断UTF-8，非ASCII搜索字节精确匹配。JSON/CSV必须正确转义；非法UTF-8、NUL和未转义控制字符明确拒绝。

## 4. Generation 与文件名

generation 为非零 `uint64_t`，正常提交单调递增；`UINT64_MAX` 后禁止回绕并返回容量错误。candidate 与 active 各维护独立的单调 generation 序列，两者数值不要求相等。candidate 初次默认 selection 的 `selection_generation` 必须等于该 candidate generation；后续 selection 更新发布新的 candidate generation 时两者再次相等。

generation 文件名使用固定 16 位大写十六进制：

```text
/dbc/candidate.<GEN16>.dbc
/dbc/candidate.<GEN16>.idx
/dbc/candidate.<GEN16>.sel
/dbc/candidate.current
/dbc/candidate.previous

/dbc/active.<GEN16>.dbc
/dbc/active.<GEN16>.idx
/dbc/active.<GEN16>.sel
/dbc/active.current
/dbc/active.previous
```

C 阶段工作文件名固定为同一 generation 的 `candidate.<GEN16>.spool.tmp`、`candidate.<GEN16>.idx.tmp`、`candidate.<GEN16>.sel.tmp` 与 `candidate.current.tmp`；工作文件均不是提交点。

`*.current` 和 `*.previous` 都是 64 B manifest 文件。`current` 是唯一提交点；generation 文件仅写完并校验不代表对象已提交。

统一术语是“事务式、断电可恢复提交”。禁止把 FAT32 多文件 rename 写成原子事务。

## 5. Manifest v1

Manifest 固定 64 B，magic 为 `DBCM`，object kind 为 candidate 或 active。字段如下：

| Offset | 类型 | 字段 |
| ---: | --- | --- |
| 0 | u32 | magic |
| 4 | u16 | format version |
| 6 | u16 | header size=64 |
| 8 | u8 | object kind |
| 9 | u8 | flags，v1 必须为 0 |
| 10 | 2 B | reserved，必须为 0 |
| 12 | u64 | generation |
| 20 | u32 | source size |
| 24 | u32 | source CRC32 |
| 28 | u32 | index size |
| 32 | u32 | index CRC32 |
| 36 | u32 | selection size |
| 40 | u32 | selection CRC32 |
| 44 | u16 | selected count |
| 46 | u16 | selected message count |
| 48 | u16 | catalog message count |
| 50 | u16 | catalog signal count |
| 52 | 8 B | reserved，必须为 0 |
| 60 | u32 | manifest CRC32，覆盖 `[0,60)` |

读取时任何 magic/version/size/reserved/count/CRC/引用文件不匹配均判 manifest 无效，不做部分加载。

## 6. Index v1

Index 由 80 B header、16 B message records、160 B signal records 连续组成。header 包含 source size/CRC、两类 record size/count/offset/总大小及各自 CRC；header CRC 覆盖 `[0,76)`。

message record 保存：

- normalized ID；
- source line；
- first signal ordinal / signal count；
- IDE/FD requirement flags；
- declared payload length。

signal record 保存：

- global ordinal 与 message ordinal；
- normalized ID、IDE、FD requirement、Intel/Motorola、signed flags；
- start bit、bit length、declared payload length；
- factor、offset、minimum、maximum；
- `definition_hash`；
- source line/byte offset；
- NUL-padded `key[64]` 和 `unit[32]`。

header/record 的精确 offset 以 `include/large_dbc_contract.h` 为准。host dump 必须输出可审计文本；host verify 必须校验文件大小、所有 offset/size/count、header/record CRC、source fingerprint、ordinal 连续性、message 引用和字符串长度/NUL padding。

## 7. Selection v1

Selection 固定 320 B：64 B header + 256 B 正向位图。header 保存：

- magic `DBCS`、version、header/total/bitmap size；
- candidate generation；
- source size/CRC；
- catalog signal count、selected count；
- bitmap CRC；
- selection generation；
- header CRC，覆盖 `[0,60)`。

位图中 `1` 表示选择该 ordinal，`0` 表示未选择。catalog 末尾之后的所有 bit 必须为 0；`selected_count` 必须等于位图 popcount。

默认 selection：

- catalog signals `<=128`：全部选择；
- catalog signals `>128`：全部不选。

选择为 0 时 candidate 仍有效，但 active 激活明确拒绝。选择 1..128 且所涉及消息 1..64 才可构造 runtime。

Selection 更新先复制 256 B 临时位图，验证 token、ordinal、set/clear 无冲突、计数、消息数、日志状态后，再生成新的 candidate generation。失败时旧 selection/candidate manifest 不变。

## 8. Candidate token

固定文本格式：

```text
GGGGGGGGGGGGGGGG-SSSSSSSS-CCCCCCCC
```

- generation：16 位大写十六进制；
- source size：8 位大写十六进制；
- source CRC32：8 位大写十六进制；
- 正文 34 字符，缓冲至少 35 B。

页面和所有 selection 写请求必须携带完整 token。generation、size、CRC 任一不匹配、请求期间 candidate 改变、ordinal 越界或 set/clear 重叠都返回冲突，不产生持久化或运行态副作用。

## 9. Parser 支持和拒绝矩阵

第一版必须支持：

- LF、CRLF、无末尾换行；
- 标准 11-bit ID；
- Vector bit31 编码的扩展 29-bit ID；
- Intel/Motorola；
- signed/unsigned；
- bit length 1..63；
- decimal 和科学计数法；
- declared payload length 0..64；
- 本合同内的长 message/signal/unit/key；
- 对无需理解的长元数据行流式跳到行尾，不因超过 512 B 而截断为合法语句。

第一版必须拒绝：

- multiplex `M` / `mN`；
- `SG_MUL_VAL_`；
- 重复 `(normalized_id, IDE)` message；
- 重复规范化 key；
- NaN、Inf、数值解析溢出；
- bit length 0 或大于 63；
- 信号布局超过 declared payload；
- catalog/message/signal/string/file 上限；
- 影响解码但未明确支持的未知语法。

对 `CM_`、`BA_`、`VAL_` 等不影响第一版解码的元数据可以跳过，但必须完整消费到行尾并计数。A1 fuzz/截断输入必须有界结束且不得越界、死循环或接受不完整 `BO_`/`SG_`。

## 10. CAN ID、Classic/FD 与 DLC

消息身份固定为 `(normalized_id, IDE)`：

- bit31 未置位且原值 `0..0x7ff`：标准 ID；
- bit31 置位：扩展 ID，normalized ID 为低 29 bit；
- 其他编码拒绝。

帧需求：

```text
declared payload length 0..8  => CLASSIC_OR_FD
declared payload length 9..64 => FD_REQUIRED
```

动态长度使用严格合同：收到帧的实际 payload length 必须等于 DBC declared payload length。`FD_REQUIRED` 还必须收到 FDF；`CLASSIC_OR_FD` 可接收相同长度的 Classic 或 FD 帧。IDE、FDF、BRS 和 DLC/实际字节数必须从 HAL 映射到公共 `CanFrame`，但当前硬件优先验收标准 Classic/DLC8；CAN-FD 为 `[未验证]`。

## 11. Definition hash 与规则兼容

`definition_hash v1` 为 FNV-1a-64，输入是固定 42 B canonical stream：

```text
version:u8
normalized_id:u32le
flags:u8  // IDE, FD_REQUIRED, MOTOROLA, SIGNED
declared_payload_length:u8
start_bit:u16le
bit_length:u8
factor:binary64le
offset:binary64le
minimum:binary64le
maximum:binary64le
```

hash 不是安全凭证。激活时每个启用规则必须同时满足：key 存在于新 selection，且 definition hash 完全相同。缺 key 或同名定义变化都返回明确冲突；不得自动删除、禁用、改名或静默接受规则。

当前 RuleFile V4 没有 definition hash。E 阶段必须新增规则持久化版本并提供明确迁移：旧规则只能在当前 active 中计算并绑定 hash 后写入新版本，或安全拒绝并要求用户重新确认；禁止按新 struct 大小直接读取旧文件。

## 12. Active runtime、SignalValueState 与提交顺序

ActiveRuntime 双槽只保存最多 64 个 selected message 和 128 个 selected signal。signal 按 ordinal 稳定排序，激活时预创建一一对应的 `SignalValueState`，每个 signal 保存稳定 `value_state_index`。未选择信号不得进入 runtime、decode、SignalValueState、`/api/signals`、CSV 或新规则候选列表。

SignalValueState 只保存数值状态，例如 value/raw/updated_ms/update_seq/quality。CAN decode 按 `(normalized_id,IDE)` 定位 selected message，再按 value_state_index 更新；不得按 key 线性插入。

API/LogTask 每次只复制一个或一页小 value slot。快照读取 sequence、复制数值、复核 sequence；不得整体复制约 19 KiB 的旧 SignalCache，也不得在长临界区复制字符串目录。

active 顺序固定为：

1. 读取并完整验证 candidate manifest 与三个文件；
2. 校验 selected count/message count；
3. 检查日志未启用；
4. 检查规则 key + definition hash；
5. 构造并验证非活动 runtime；
6. 写 active generation 的 DBC/index/selection tmp，逐个 `f_sync + close`；
7. 读回校验全部 size/header/CRC；
8. rename 为 generation 正式文件；
9. 写、sync、close、读回 `active.current.tmp`；
10. 在 FatFs mutex 内执行 current→previous、current.tmp→current；
11. 在短 RTOS 临界区同时发布 RAM active manifest snapshot、runtime pointer 和 runtime generation。

文件 rename 不在 CPU 临界区执行。若断电发生在第 10 步后、第 11 步前，当前上电周期仍使用旧 RAM runtime，复位恢复使用已提交的新 manifest；不会运行混合文件组。

任何失败必须保持旧 active manifest/runtime、规则、SignalValueState、继电器和日志不变。

## 13. Candidate 提交与启动恢复

candidate 顺序固定为：

1. 只生成 `upload.<GEN16>.tmp`；
2. 校验 Content-Length、实际长度、上限、增量 CRC、timeout；
3. 从 TF 二次顺序读取 upload；
4. 生成并校验 index tmp；
5. 生成并校验默认 selection tmp；
6. 三文件交叉验证 source fingerprint/count/ordinal；
7. 写正式 generation 文件；
8. 最后提交 candidate current manifest。

上传、parser、index、selection 或 manifest 失败时旧 candidate、active 和 runtime 都不变。B 阶段不能提前替换 candidate。

启动恢复对 candidate 和 active 分别执行：校验 current manifest及全部引用文件；失败则校验 previous；两者都失败则不加载。只清理未被有效 current/previous 引用的 `.tmp` 和孤儿 generation；不得删除仍被任一有效 manifest 引用的文件。

## 14. HTTP 与 JSON 容量

候选与实时 API 每页固定最多 8 项。最长key、最坏unit转义、最大数值下，实时item合同上界为263 B；8项加保守512 B envelope会超过2 KiB。因此第一版响应正文scratch冻结为3072 B（包含NUL），最大合法HTTP body为3071 B。serializer必须先完成有界构造并得到精确Content-Length，再以最多512 B的socket片段发送；这是固定Content-Length响应的分段发送，不是HTTP chunked transfer。容量不足或转义失败返回明确500，禁止发送HTTP200 + 截断/空body。

候选 item 只返回 ordinal/key/selected；实时 item 只返回 ordinal/key/value/raw/unit/quality/updatedMs。公共 envelope 返回 generation、selection CRC、total、matched、page/pageSize。不得重复返回 message/signal 长字符串。

A1/D/G 的 host 测试必须覆盖最长合法 key/unit、引号、反斜杠、控制字符、UTF-8、最坏 8 项、3071/3072 边界、512 B分段和Content-Length一致性。

保持 W5500 socket0 严格串行；不引入 multipart、chunked request、断点续传、并发请求、WebSocket 或 SSE。

## 15. 日志吞吐和会话锁定

日志准入使用不丢精度的乘法判断：

```text
selected_count * 1000 <= MAX_LOG_ROWS_PER_SECOND * sample_period_ms
```

v1 冻结 `MAX_LOG_ROWS_PER_SECOND=20`，采样周期仍为 `100..10000 ms`。该值是保守工程上限，新 selected-only 路径的 20 rows/s 实板能力仍为 `[未验证]`，G 阶段必须用物理 TF 记录验证；不能因运行后 drop 而静默降级。超限在启动日志前明确拒绝且不改变原日志状态。

每个 CSV 会话锁定 active generation、source fingerprint、selection CRC、selected count 和 period。日志启用期间拒绝 upload、selection 更新和 active 激活。

每个 CSV 配套同 basename 的 `.meta`，至少记录：

- format version、firmware build ID/hash；
- active generation；
- source size/CRC；
- selection CRC、selected count；
- selected ordinal/key/definition hash；
- sample period；
- start/end time；
- clean close；
- write/drop/late/failure counters。

当前 v4 宽表会话先流式写入一次 header：首单元格为 `datetime`，随后按锁定 selected ordinal 顺序写入已转义的信号 key。每个采样周期只写一条数据行：UTC ISO-8601 时间戳后按相同顺序写入各信号数值；`MISSING` 或 `ERROR` 保留空单元格，绝不移动后续列。

LogTask 使用 256..512 B scratch 和静态批量缓冲分片序列化 header/数据行；不得在任务栈创建 128 项大数组，也不得为生成宽表累积整行或整套 SignalCache。

## 16. RAM、Flash、heap 与 stack 预算

既有只读基线：

- ELF `text/data/bss=112828/768/243948`；
- FLASH 装载 `113596/131072 B`，剩余 `17476 B`；
- RAM_D1 map 使用 `244648/524288 B`；
- FreeRTOS heap 64 KiB，当前 minimum-ever-free 和各任务 high-water `[未验证]`；
- 历史增加约 1920 B queue 后曾导致最后一个任务创建失败。

冻结门槛：

- 最终 `text+data <=126976 B`，至少保留 4096 B FLASH；
- 新 selected runtime 双槽、value slots和全部新增静态 scratch 合计不超过 65536 B；
- 不新增 FreeRTOS task；
- 不增加 FreeRTOS heap 常驻分配；
- minimum-ever-free heap 必须实测至少 4096 B；
- 受影响任务 stack high-water 必须至少保留 256 B；
- 单个自动对象不超过 512 B，大对象使用静态存储或有界逐项处理。

每个固件阶段都必须以最终 map/size 和运行时 heap/stack 证据重新核对。RAM_D1 总余量不能替代 FreeRTOS heap 证据。

## 17. 授权策略

本大 DBC里程碑是受控实验室验收，当前分支沿用“隔离实验 LAN + 现场操作者明确发起写操作”的授权边界，不把 candidate token 当作凭证，也不宣称具备生产鉴权或 TLS。

所有大 DBC 写操作必须在 UI 中由用户显式触发；自动刷新只允许 GET。生产构建在统一生产写授权尚未实现前必须 fail closed，禁止开放 upload/selection/active。生产 token、设备身份或物理配置模式属于独立生产授权合同，不能硬编码测试 token 或把 `0x321` 实验配置变成生产默认。

本策略允许当前隔离实验网完成 B-H 实板验收，但最终交付必须明确标记为 laboratory-only，不能冒充生产安全闭环。

## 18. A1 解锁门槛

A0 只有在以下证据齐全后才可标记通过：

- 公共头与本文字段/上限一致；
- CTest 合同测试通过；
- `git diff --check` 通过；
- 没有修改 HTTP、TF、FDCAN、网页或 runtime 业务；
- 治理文件记录真实 DBC、预算、授权和 `[未验证]` 边界；
- 用户既有未提交记录和未跟踪旧计划未被覆盖。

A1 仅允许 pure core、host tests 与 host dump/verify 工具；禁止接入 HTTP、提交 candidate、加载 runtime、修改 TF/FDCAN/网页或烧录。
