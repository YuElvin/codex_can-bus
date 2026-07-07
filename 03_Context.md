# 当前上下文

更新时间：2026-07-08 00:00 +08:00

## 当前仓库

- 工作目录：`/Users/elvin/Desktop/project/can_bus`，当前实际解析到 `/Users/elvin/Desktop/project/can_bus_W5500`。
- 当前分支：`codex/W5500`。
- 当前主线：W5500/SPI2 + FDCAN2/MCP2562FD + TF 卡 + W25Q128 + FreeRTOS。

## 已验证状态

| 模块 | 状态 | 证据摘要 |
| --- | --- | --- |
| W5500 | [客观已验证] | `VERSIONR=0x04`，静态 IP `192.168.1.88`，主机 ping 通过 |
| FDCAN2 外部 CAN | [客观已验证] | Windows CANtest 可收到开发板 `0x321` 周期帧；开发板收到 Windows 发帧 |
| TF 卡 | [客观已验证] | SDMMC/FatFs smoke test 写读通过 |
| W25Q128 | [客观已验证] | JEDEC ID `EF4018`，最后 4KB 扇区擦写读回通过 |
| FreeRTOS | [实现中] | 源码已接入，主机测试、固件编译、反汇编通过；尚未烧录运行复核 |

## 当前阻断项

- FreeRTOS 版本尚未完成烧录后的硬件复核。
- W25Q128 当前 bring-up 自检会擦写 `0x00FFF000` 最后 4KB 扇区，正式配置存储前必须改为按需触发或换成保留测试区。

## 当前风险

- 后续拆分 FreeRTOS 多任务时，W5500、TF/FatFs、QSPI 共享资源必须加串行化或 mutex。
- `CONVERSATION_SUMMARY.md` 已经较长，但仍按用户要求保留为完整对话摘要；当前快照以本文件为准。
- macOS 串口曾出现乱码，硬件结论优先用 ST-Link 变量和外部工具确认。

## 下一步建议

1. 烧录 `build/stm32h750_freertos/can_bus_gateway_stm32h750.hex` 或重新构建后的 FreeRTOS 固件。
2. 读取 `g_freertos_task_started`、`g_freertos_loop_count` 和 W5500/CAN/TF/W25Q128 状态变量。
3. FreeRTOS 单任务上板确认后，再拆分 CAN、W5500、TF/日志、HTTP、配置任务。

