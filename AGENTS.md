# Repository Guidelines

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

## Coding Style & Naming Conventions

Use concise, modular C/C++ for firmware code. Prefer `snake_case` for C functions, files, and variables, and `PascalCase` only for types where it improves readability, such as `CanFrame` or `SignalCache`. Keep interrupt handlers thin; move work into FreeRTOS tasks and queues. For JavaScript tooling, use ESM (`.mjs`), two-space indentation, `const` by default, and explicit Node built-in imports.

## Testing Guidelines

Host-side tests use CTest with small C test executables. Run `ctest --test-dir build/host --output-on-failure` before committing core logic changes. Add tests for pure logic such as DBC parsing, signal encoding/decoding, rule evaluation, and configuration validation. Name tests after the module under test, for example `test_dbc_parser.c` or `test_rule_engine.c`.

## Commit & Pull Request Guidelines

The current history uses short, imperative commit subjects, for example `Remove local environment paths from toolchain docs`. Keep subjects specific and under roughly 72 characters. Pull requests should include the purpose of the change, commands run, hardware tested when relevant, and links to any issue or design note. Include screenshots only for UI-facing changes.

## Security & Configuration Tips

Do not commit machine-specific absolute paths, credentials, downloaded tool archives, or generated firmware artifacts. Keep local configuration in ignored files and document reproducible setup steps in `TOOLCHAIN.md` or this guide.
