#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

if [ -f "./env.sh" ]; then
  # shellcheck disable=SC1091
  . ./env.sh
fi
export PATH="$PROJECT_ROOT/dev-tools/node_modules/@xpack-dev-tools/arm-none-eabi-gcc/.content/bin:$PROJECT_ROOT/dev-tools/node_modules/@xpack-dev-tools/cmake/.content/bin:$PROJECT_ROOT/dev-tools/node_modules/@xpack-dev-tools/ninja-build/.content/bin:$PROJECT_ROOT/dev-tools/node_modules/@xpack-dev-tools/openocd/.content/bin:$PATH"

cmake -S . -B build/host -G Ninja
cmake --build build/host
ctest --test-dir build/host --output-on-failure

cmake -S . -B build/stm32h750 -G Ninja \
  -DCAN_BUS_BUILD_STM32H750_FIRMWARE=ON \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake
cmake --build build/stm32h750

echo "Verification completed. For firmware logic changes, run targeted objdump review on build/stm32h750/can_bus_gateway_stm32h750.elf."
