# 工程规则

## 基本原则

- 先确认事实，再写结论；不把未验证事项写成已完成。
- 只做当前任务需要的最小改动，不顺手重构无关代码。
- 保持现有目录边界：HAL/CubeMX、平台 bring-up、portable core、测试、文档分开维护。
- 发现无关风险时记录到 `03_Context.md`、`04_Features_ADR.md` 或 `05_Lessons.md`，不要扩大本轮修改范围。

## 固件修改规则

- 修改固件源码后必须编译对应 STM32 固件。
- 每次编译后必须检查生成 ELF 的关键路径反汇编，确认启动顺序、任务入口、外设调用或关键常量符合本轮功能。
- 涉及硬件状态时，优先用 ST-Link 全局变量、OpenOCD 输出、串口日志、ping、CANtest 等客观证据闭环。
- 中断处理保持薄；耗时逻辑放任务或轮询服务函数。
- FreeRTOS 变更必须特别复核 `SVC_Handler`、`PendSV_Handler`、`SysTick_Handler`，且保留 `HAL_IncTick()`。

## 验证命令

统一入口：

```sh
./scripts/verify.sh
```

手动等价命令：

```sh
. ./env.sh
cmake -S . -B build/host -G Ninja
cmake --build build/host
ctest --test-dir build/host --output-on-failure
cmake -S . -B build/stm32h750 -G Ninja -DCAN_BUS_BUILD_STM32H750_FIRMWARE=ON -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake
cmake --build build/stm32h750
```

固件改动后还需要用 `arm-none-eabi-objdump` 对生成 ELF 做定向反汇编检查。烧录和硬件验证按任务需要执行，不由 `scripts/verify.sh` 自动执行。

## 提交规则

- 提交前确认 `git status --short --branch` 和差异范围。
- 只暂存本轮相关文件。
- 不提交构建产物、下载包、日志、本地 IDE 状态、私钥或凭据。
- 提交说明要写清楚为什么改、改了什么、如何验证。

## 文档规则

- 每轮结束前更新 `CONVERSATION_SUMMARY.md`。
- 当前快照放 `03_Context.md`，功能/决策状态放 `04_Features_ADR.md`，可复用经验放 `05_Lessons.md`。
- 如果本轮只改文档，没有编译，必须在记录里写明未编译和未反汇编原因。

