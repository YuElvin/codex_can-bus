cmake_minimum_required(VERSION 3.20)

project(can_bus_gateway C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(CAN_BUS_BUILD_TESTS "Build host-side unit tests" ON)
option(CAN_BUS_BUILD_STM32H750_FIRMWARE "Build STM32H750 firmware image" OFF)

add_library(can_bus_core
  src/core/dbc_parser.c
  src/core/rule_engine.c
  src/core/signal_cache.c
  src/core/signal_codec.c
)

target_include_directories(can_bus_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(can_bus_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(can_bus_ports
  src/ports/can_port.c
  src/ports/lan8720_port.c
  src/ports/tf_card_port.c
  src/ports/w5500_port.c
)

target_include_directories(can_bus_ports
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(can_bus_ports PUBLIC can_bus_core)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(can_bus_ports PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CAN_BUS_BUILD_STM32H750_FIRMWARE)
  enable_language(ASM)

  set(CUBE_MX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cube_mx)
  set(STM32_LINKER_SCRIPT ${CUBE_MX_DIR}/STM32H750VBTX_FLASH.ld)

  file(GLOB STM32_HAL_SOURCES CONFIGURE_DEPENDS
    ${CUBE_MX_DIR}/Drivers/STM32H7xx_HAL_Driver/Src/*.c
  )
  list(FILTER STM32_HAL_SOURCES EXCLUDE REGEX ".*/stm32h7xx_hal_eth(_ex)?\\.c$")

  add_executable(can_bus_gateway_stm32h750
    ${CUBE_MX_DIR}/Core/Startup/startup_stm32h750vbtx.s
    ${CUBE_MX_DIR}/Core/Src/fdcan.c
    ${CUBE_MX_DIR}/Core/Src/gpio.c
    ${CUBE_MX_DIR}/Core/Src/main.c
    ${CUBE_MX_DIR}/Core/Src/quadspi.c
    ${CUBE_MX_DIR}/Core/Src/sdmmc.c
    ${CUBE_MX_DIR}/Core/Src/spi.c
    ${CUBE_MX_DIR}/Core/Src/stm32h7xx_hal_msp.c
    ${CUBE_MX_DIR}/Core/Src/stm32h7xx_it.c
    ${CUBE_MX_DIR}/Core/Src/syscalls.c
    ${CUBE_MX_DIR}/Core/Src/sysmem.c
    ${CUBE_MX_DIR}/Core/Src/system_stm32h7xx.c
    ${CUBE_MX_DIR}/Core/Src/usart.c
    ${CUBE_MX_DIR}/FATFS/App/fatfs.c
    ${CUBE_MX_DIR}/FATFS/Target/bsp_driver_sd.c
    ${CUBE_MX_DIR}/FATFS/Target/fatfs_platform.c
    ${CUBE_MX_DIR}/FATFS/Target/sd_diskio.c
    ${CUBE_MX_DIR}/Middlewares/Third_Party/FatFs/src/diskio.c
    ${CUBE_MX_DIR}/Middlewares/Third_Party/FatFs/src/ff.c
    ${CUBE_MX_DIR}/Middlewares/Third_Party/FatFs/src/ff_gen_drv.c
    ${CUBE_MX_DIR}/Middlewares/Third_Party/FatFs/src/option/ccsbcs.c
    ${STM32_HAL_SOURCES}
    src/ports/can_port.c
    src/ports/tf_card_port.c
    src/ports/w5500_port.c
    src/platform/stm32h750/fdcan_port_stm32.c
    src/platform/stm32h750/tf_card_fatfs_stm32.c
    src/platform/stm32h750/w5500_spi_stm32.c
    firmware/bringup/can_bringup.c
    firmware/bringup/tf_card_bringup.c
    firmware/bringup/w25q128_bringup.c
    firmware/bringup/w5500_bringup.c
  )

  set_target_properties(can_bus_gateway_stm32h750 PROPERTIES
    SUFFIX ".elf"
  )

  target_include_directories(can_bus_gateway_stm32h750 PRIVATE
    include
    ${CUBE_MX_DIR}/Core/Inc
    ${CUBE_MX_DIR}/FATFS/App
    ${CUBE_MX_DIR}/FATFS/Target
    ${CUBE_MX_DIR}/Drivers/CMSIS/Device/ST/STM32H7xx/Include
    ${CUBE_MX_DIR}/Drivers/CMSIS/Include
    ${CUBE_MX_DIR}/Drivers/STM32H7xx_HAL_Driver/Inc
    ${CUBE_MX_DIR}/Drivers/STM32H7xx_HAL_Driver/Inc/Legacy
    ${CUBE_MX_DIR}/Middlewares/Third_Party/FatFs/src
  )

  target_compile_definitions(can_bus_gateway_stm32h750 PRIVATE
    USE_PWR_LDO_SUPPLY
    USE_HAL_DRIVER
    STM32H750xx
  )

  target_compile_options(can_bus_gateway_stm32h750 PRIVATE
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
    -Os
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-missing-field-initializers
    $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
  )

  target_link_options(can_bus_gateway_stm32h750 PRIVATE
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
    -T${STM32_LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/can_bus_gateway_stm32h750.map
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    --specs=nosys.specs
    --specs=nano.specs
  )

  add_custom_command(TARGET can_bus_gateway_stm32h750 POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:can_bus_gateway_stm32h750>
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:can_bus_gateway_stm32h750> ${CMAKE_CURRENT_BINARY_DIR}/can_bus_gateway_stm32h750.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:can_bus_gateway_stm32h750> ${CMAKE_CURRENT_BINARY_DIR}/can_bus_gateway_stm32h750.bin
    BYPRODUCTS
      ${CMAKE_CURRENT_BINARY_DIR}/can_bus_gateway_stm32h750.hex
      ${CMAKE_CURRENT_BINARY_DIR}/can_bus_gateway_stm32h750.bin
  )
endif()

if(CAN_BUS_BUILD_TESTS)
  enable_testing()

  add_executable(test_signal_codec tests/test_signal_codec.c)
  target_link_libraries(test_signal_codec PRIVATE can_bus_core)
  add_test(NAME signal_codec COMMAND test_signal_codec)

  add_executable(test_dbc_parser tests/test_dbc_parser.c)
  target_link_libraries(test_dbc_parser PRIVATE can_bus_core)
  add_test(NAME dbc_parser COMMAND test_dbc_parser)

  add_executable(test_rule_engine tests/test_rule_engine.c)
  target_link_libraries(test_rule_engine PRIVATE can_bus_core)
  add_test(NAME rule_engine COMMAND test_rule_engine)

  add_executable(test_signal_cache tests/test_signal_cache.c)
  target_link_libraries(test_signal_cache PRIVATE can_bus_core)
  add_test(NAME signal_cache COMMAND test_signal_cache)

  add_executable(test_can_port tests/test_can_port.c)
  target_link_libraries(test_can_port PRIVATE can_bus_ports)
  add_test(NAME can_port COMMAND test_can_port)

  add_executable(test_tf_card_port tests/test_tf_card_port.c)
  target_link_libraries(test_tf_card_port PRIVATE can_bus_ports)
  add_test(NAME tf_card_port COMMAND test_tf_card_port)

  add_executable(test_lan8720_port tests/test_lan8720_port.c)
  target_link_libraries(test_lan8720_port PRIVATE can_bus_ports)
  add_test(NAME lan8720_port COMMAND test_lan8720_port)

  add_executable(test_w5500_port tests/test_w5500_port.c)
  target_link_libraries(test_w5500_port PRIVATE can_bus_ports)
  add_test(NAME w5500_port COMMAND test_w5500_port)
endif()
