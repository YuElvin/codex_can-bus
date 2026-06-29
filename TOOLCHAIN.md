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
- Xcode Command Line Tools: 已安装
- STM32CubeIDE: 已安装
- STM32CubeMX: 已安装
- STM32CubeProgrammer CLI: 已安装，但默认不在 `PATH` 中
- 命令行下载/调试替代方案: 使用项目内 OpenOCD + ST-Link

## 仍需人工处理的安装

如果后续需要在新机器上图形化生成 `.ioc` 和外设初始化代码，请安装 STM32CubeMX 或 STM32CubeIDE。安装到系统应用目录通常需要管理员权限；如果没有管理员权限，优先选择用户目录安装。

- STM32CubeIDE: 推荐安装 Apple Silicon 原生版本。
- STM32CubeMX: 推荐安装 Apple Silicon 原生版本。
- STM32CubeProgrammer: 可选；本项目也可以使用 OpenOCD + ST-Link 完成下载和调试。

当前项目内工具链已经足够支撑后续 CMake/Ninja 固件工程、ARM 编译、GDB 调试和 OpenOCD 下载。
