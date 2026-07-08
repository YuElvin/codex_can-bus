# 经验教训

- L-001：硬件状态以最新上电或复位后的实际读数为准，不沿用旧日志结论。
- L-002：`/Users/elvin/Desktop/project/can_bus` 可能是指向 `/Users/elvin/Desktop/project/can_bus_W5500` 的符号链接，执行前先用 `pwd` 或 `git rev-parse --show-toplevel` 归一化路径。
- L-003：W5500 验证要同时看 SPI 寄存器、ST-Link 变量、主机路由/ping/ARP；串口单独不足以定论。
- L-004：W25Q128 自检会擦写最后 4KB 扇区 `0x00FFF000`，正式存储前必须保留测试区或改为按需触发。
- L-005：FreeRTOS 启用后 `SysTick_Handler` 仍必须调用 `HAL_IncTick()`，否则 HAL 超时和 delay 逻辑可能失效。
- L-006：FDCAN2 `PB5/PB6` 才是当前用户实际接线的外部 CAN 通道；FDCAN1 `PD0/PD1` 只作为诊断/保留路径。
- L-007：固件编译后必须做关键路径反汇编检查；如果本轮没有编译，记录中必须明确未执行反汇编。
- L-008：提交前只暂存本轮相关文件，避免把无关 middleware、构建产物或并行改动带入提交。
- L-009：`env.sh` 被脚本 source 时可能因 `$0` 指向外层脚本而算错项目根，还会覆盖同名 `ROOT_DIR`；自动验证脚本应使用独立变量保存项目根并重新设置本地 xPack PATH。
- L-010：W5500 HTTP 功能验证要同时看 `ping`、`curl -i` 响应、ARP MAC 和 ST-Link `g_w5500_http_*` 变量；如果暂停瞬间读数异常，应复位运行后用 HTTP 响应和再次 mdw 交叉确认。
