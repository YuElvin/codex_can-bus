#!/usr/bin/env sh
ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
export PATH="$ROOT_DIR/dev-tools/node_modules/@xpack-dev-tools/arm-none-eabi-gcc/.content/bin:$ROOT_DIR/dev-tools/node_modules/@xpack-dev-tools/cmake/.content/bin:$ROOT_DIR/dev-tools/node_modules/@xpack-dev-tools/ninja-build/.content/bin:$ROOT_DIR/dev-tools/node_modules/@xpack-dev-tools/openocd/.content/bin:$PATH"

