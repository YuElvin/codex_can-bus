# 本地 STM32 工具链配置

## 已安装到项目内的工具

工具安装在 `dev-tools/`，不依赖管理员权限：

| 工具 | 版本 | 用途 |
|---|---|---|
| xPack GNU Arm Embedded GCC | 15.2.1 | STM32H750 裸机/FreeRTOS 固件交叉编译 |
| GNU GDB for Arm | 16.3.90 | SWD 调试 |
| CMake | 3.31.9 | 推荐构建系统 |
| Ninja | 1.13.1 | CMake 后端构建器 |
| OpenOCD | 0.12.0 | ST-Link 下载和调试，包含 `stlink.cfg`、`stm32h7x.cfg` |

## 使用方法

每个新终端先执行：

```sh
. ./env.sh
```

然后可以直接使用：

```sh
arm-none-eabi-gcc --version
cmake --version
ninja --version
openocd --version
```

健康检查：

```sh
npm run --prefix dev-tools doctor
```

重新安装或恢复二进制：

```sh
npm install --prefix dev-tools
node tools/install-xpack-binaries.mjs
```

## 当前环境检查结论

- macOS: 26.5.1, arm64
- Xcode Command Line Tools: 已安装在 `/Library/Developer/CommandLineTools`
- Homebrew: 未安装，且当前用户无管理员权限，不能安装到 `/opt/homebrew`
- 已下载 ST GUI 工具安装包：
  - `/Users/elvin/Downloads/stm32cubeide_2.2.0_29186_20260626_0934-Mac-aarch64.dmg`
  - `/Users/elvin/Downloads/SetupSTM32CubeMX-6.18.0-Mac-aarch64.tar`
  - `/Users/elvin/Downloads/SetupSTM32CubeProgrammer-2.23.0.app`
- STM32CubeProgrammer 当前是 macOS Intel/x86_64 安装器；本机已安装 Rosetta，可运行该版本。
- 命令行下载/调试替代方案: 使用项目内 OpenOCD + ST-Link

## 仍需人工处理的安装

如果后续需要图形化 CubeMX/CubeIDE 生成 `.ioc` 和外设初始化代码，需要运行已下载的安装器。安装到 `/Applications` 通常需要管理员权限；如果没有管理员权限，优先选择用户目录安装。

- STM32CubeIDE: Apple Silicon 原生包已下载。
- STM32CubeMX: Apple Silicon 原生包已下载。
- STM32CubeProgrammer: Intel 包可通过 Rosetta 使用；烧录和 CLI 调试性能影响可以忽略。

当前项目内工具链已经足够支撑后续 CMake/Ninja 固件工程、ARM 编译、GDB 调试和 OpenOCD 下载。
