# Repository Guidelines

## Governance Workflow

每次任务开始前必须先确认项目真实状态，不依赖聊天记忆直接下结论。

任务开始先读 `CURRENT_TASK.md`；若其内容与实际 Git 状态或现场状态冲突，以实际状态为准并更新该文件。

1. 读取本文件和 `03_Context.md`，确认当前分支、阶段、已验证状态、阻断项和下一步。
2. 按任务关键词查 `05_Lessons.md`，避免重复踩坑。
3. 涉及代码、构建、烧录或验证时读取 `02_Engineering_Rules.md`。
4. 涉及阶段计划、功能状态或架构决策时读取 `01_Project_Plan.md` 和 `04_Features_ADR.md`。
5. 开始实现前写清楚本轮假设、成功标准和验证方式；不确定时先说明不确定点。

## Documentation Requirements

- `CONVERSATION_SUMMARY.md` 是本项目的中文对话交流记录；每次问答、代码修改、调试问题、验证命令和结果都要追加重点摘要。
- 如果发生自动或手动上下文压缩，必须尽可能在压缩前更新 `CONVERSATION_SUMMARY.md`、`03_Context.md`，必要时把长过程放入 `docs/archive/`。
- 阶段或功能状态变化时同步更新 `03_Context.md` 和 `04_Features_ADR.md`。
- 形成可复用经验时同步更新 `05_Lessons.md`。
- 所有状态必须基于实际文件、命令输出、硬件读数或用户确认；未验证内容必须明确写为“未验证”“待确认”或“阻断”。

## Verification Rules

- 固件源码改动后必须编译，并对生成固件做关键路径反汇编检查。
- 每次编译后需要记录固件路径、构建结果、关键尺寸或符号检查、反汇编结论；如果没有编译，必须明确写“本次未编译，因此未执行反汇编检查”。
- 统一验证入口优先使用 `scripts/verify.sh`；如任务需要烧录或硬件验证，还必须补充 OpenOCD/ST-Link、串口、ping、CANtest 或其他现场读数。
- 不编造测试、编译、烧录、反汇编或硬件验证结果。

## Project Structure & Module Organization

This repository contains the architecture, local toolchain, and initial portable core for an STM32H750 CAN/CAN-FD data acquisition gateway. Root-level documentation includes `ARCHITECTURE_DESIGN.md` for system design and `TOOLCHAIN.md` for setup notes. Public headers live in `include/`, portable business logic in `src/core/`, and host-side tests in `tests/`. Tooling lives in `tools/`; `dev-tools/` pins local xPack dependencies.

Keep board/HAL code, OS/driver services, core CAN/DBC/logging logic, configuration, and UI/API code in separate modules. Avoid placing generated binaries, downloaded archives, logs, or local IDE state under version control.

## Build, Test, and Development Commands

Install or restore local development tools:

```sh
npm install --prefix dev-tools
npm run --prefix dev-tools install-binaries
```

Load tool binaries into the current shell:

```sh
. ./env.sh
```

Verify GCC, GDB, CMake, Ninja, and OpenOCD availability:

```sh
npm run --prefix dev-tools doctor
```

Configure, build, and run host tests:

```sh
. ./env.sh
cmake -S . -B build/host -G Ninja
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Run the project verification entrypoint:

```sh
./scripts/verify.sh
```

For firmware logic changes, follow this with targeted disassembly review of the generated ELF and, when requested, flash/runtime hardware checks.

## Coding Style & Naming Conventions

Use concise, modular C/C++ for firmware code. Prefer `snake_case` for C functions, files, and variables, and `PascalCase` only for types where it improves readability, such as `CanFrame` or `SignalCache`. Keep interrupt handlers thin; move work into FreeRTOS tasks and queues. For JavaScript tooling, use ESM (`.mjs`), two-space indentation, `const` by default, and explicit Node built-in imports.

## Testing Guidelines

Host-side tests use CTest with small C test executables. Run `ctest --test-dir build/host --output-on-failure` before committing core logic changes. Add tests for pure logic such as DBC parsing, signal encoding/decoding, rule evaluation, and configuration validation. Name tests after the module under test, for example `test_dbc_parser.c` or `test_rule_engine.c`.

## Commit & Pull Request Guidelines

The current history uses short, imperative commit subjects, for example `Remove local environment paths from toolchain docs`. Keep subjects specific and under roughly 72 characters. Pull requests should include the purpose of the change, commands run, hardware tested when relevant, and links to any issue or design note. Include screenshots only for UI-facing changes.

## Security & Configuration Tips

Do not commit machine-specific absolute paths, credentials, downloaded tool archives, or generated firmware artifacts. Keep local configuration in ignored files and document reproducible setup steps in `TOOLCHAIN.md` or this guide.
