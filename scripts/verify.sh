#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if [ -f "./env.sh" ]; then
  # shellcheck disable=SC1091
  . ./env.sh
fi

cmake -S . -B build/host -G Ninja
cmake --build build/host
ctest --test-dir build/host --output-on-failure

cmake -S . -B build/stm32h750 -G Ninja \
  -DCAN_BUS_BUILD_STM32H750_FIRMWARE=ON \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake
cmake --build build/stm32h750

echo "Verification completed. For firmware logic changes, run targeted objdump review on build/stm32h750/can_bus_gateway_stm32h750.elf."
