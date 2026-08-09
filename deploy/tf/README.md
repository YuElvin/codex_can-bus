# TF 卡最小部署清单

本目录只保存可重复部署且已验证的静态资产，不保存运行日志、事务临时文件或无法确认的用户配置。

## 需要复制

| 仓库源文件 | TF 目标路径 | 用途 |
| --- | --- | --- |
| `www/index.html` | `/www/index.html` | 完整 Web 控制台；固件自动生成的页面只是极简备用页 |
| `deploy/tf/dbc/active.dbc` | `/dbc/active.dbc` | 启动时加载的活动 Can2Data DBC |
| `deploy/tf/dbc/active.dbc` | `/dbc/candidate.dbc` | 保留与活动文件一致的可再次激活候选 |

`active.dbc`固定为标准 classic CAN `0x321`、DLC 8、两个16位Intel无符号信号：

- `Can2Data.marker`：bit 0..15
- `Can2Data.sequence`：bit 16..31

文件长度为151 B，SHA-256为
`271f20f923343c9f923bd6db4da4599e0349983b0a5bd43edeae87d152855417`。

## 不应回拷

- `/log/*`：P0掉电复测必须从空日志目录开始，目录和会话文件由固件创建。
- `/dbc/*.tmp`、`/config/*.tmp`：事务临时文件不能作为部署源。
- `candidate.prev.dbc`、`active.prev.dbc`、`rules-v4.prev`：由后续替换流程自动生成。
- 失败介质镜像中的文件：镜像只用于证据保全，不整卷或选择性恢复异常文件系统内容。
- macOS的`.Spotlight-V100`、`.fseventsd`、`._*`等元数据。

## 配置文件边界

原卡的`/config/rules-v4.conf`无法从只读故障镜像可靠提取，因此不得猜测重建。空卡首次启动会创建默认v2/v1文件，但不会在同一启动轮重新加载新建v2；本次P0日志复测依赖活动DBC，不依赖预置V4。需要恢复两槽`signalKey`规则时，应在板端启动后通过现有网页/API重新保存V4。

## 复制后验证

1. 对网页执行`cmp`和SHA-256，确认与仓库`www/index.html`逐字节一致。
2. 对`/dbc/active.dbc`和`/dbc/candidate.dbc`执行`cmp`，并确认两者均为151 B和上述SHA-256。
3. 安全卸载后执行只读文件系统检查；只有退出码0才可插回目标板。
