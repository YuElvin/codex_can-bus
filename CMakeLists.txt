cmake_minimum_required(VERSION 3.20)

project(can_bus_gateway C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(CAN_BUS_BUILD_TESTS "Build host-side unit tests" ON)
option(CAN_BUS_BUILD_STM32H750_FIRMWARE "Build STM32H750 firmware image" OFF)
option(CAN_BUS_P0_FAULT_INJECTION "Build the lab-only P0 watchdog fault injection hook" OFF)
set(LARGE_DBC_REAL_FIXTURE "" CACHE FILEPATH "Optional real large DBC fixture for host acceptance")
if(LARGE_DBC_REAL_FIXTURE AND NOT EXISTS "${LARGE_DBC_REAL_FIXTURE}")
  message(FATAL_ERROR "LARGE_DBC_REAL_FIXTURE does not exist: ${LARGE_DBC_REAL_FIXTURE}")
endif()

add_library(can_bus_core
  src/core/dbc_active_commit.c
  src/core/can_tx_control.c
  src/core/dbc_candidate_catalog.c
  src/core/dbc_candidate_commit.c
  src/core/dbc_candidate_format.c
  src/core/dbc_candidate_http.c
  src/core/dbc_decoder.c
  src/core/dbc_catalog_index.c
  src/core/dbc_parser.c
  src/core/dbc_selected_runtime.c
  src/core/dbc_stream_index.c
  src/core/dbc_stream_parser.c
  src/core/dbc_upload.c
  src/core/dbc_signal_catalog.c
  src/core/manual_relay.c
  src/core/rule_engine.c
  src/core/rule_config.c
  src/core/rule_file.c
  src/core/selected_signal_log.c
  src/core/signal_api.c
  src/core/signal_cache.c
  src/core/signal_csv.c
  src/core/signal_log_control.c
  src/core/signal_log_buffer.c
  src/core/signal_log_time.c
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
    ${CUBE_MX_DIR}/Middlewares/Third_Party/FreeRTOS/Source/list.c
    ${CUBE_MX_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1/port.c
    ${CUBE_MX_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c
    ${CUBE_MX_DIR}/Middlewares/Third_Party/FreeRTOS/Source/queue.c
    ${CUBE_MX_DIR}/Middlewares/Third_Party/FreeRTOS/Source/tasks.c
    ${CUBE_MX_DIR}/Middlewares/Third_Party/FatFs/src/diskio.c
    ${CUBE_MX_DIR}/Middlewares/Third_Party/FatFs/src/ff.c
    ${CUBE_MX_DIR}/Middlewares/Third_Party/FatFs/src/ff_gen_drv.c
    ${CUBE_MX_DIR}/Middlewares/Third_Party/FatFs/src/option/ccsbcs.c
    ${STM32_HAL_SOURCES}
    src/core/dbc_active_commit.c
    src/core/dbc_candidate_catalog.c
    src/core/dbc_candidate_commit.c
    src/core/dbc_candidate_format.c
    src/core/dbc_candidate_http.c
    src/core/dbc_decoder.c
    src/core/dbc_catalog_index.c
    src/core/can_tx_control.c
    src/core/dbc_parser.c
    src/core/dbc_selected_runtime.c
    src/core/dbc_stream_index.c
    src/core/dbc_stream_parser.c
    src/core/dbc_upload.c
    src/core/dbc_signal_catalog.c
    src/core/manual_relay.c
    src/core/rule_engine.c
    src/core/rule_config.c
    src/core/rule_file.c
    src/core/selected_signal_log.c
    src/core/signal_api.c
    src/core/signal_cache.c
    src/core/signal_csv.c
    src/core/signal_log_control.c
    src/core/signal_log_buffer.c
    src/core/signal_log_time.c
    src/core/signal_codec.c
    src/ports/can_port.c
    src/ports/tf_card_port.c
    src/ports/w5500_port.c
    src/platform/stm32h750/fdcan_port_stm32.c
    src/platform/stm32h750/large_dbc_candidate_stm32.c
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
    ${CUBE_MX_DIR}/Middlewares/Third_Party/FreeRTOS/Source/include
    ${CUBE_MX_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1
    ${CUBE_MX_DIR}/Middlewares/Third_Party/FatFs/src
  )

  target_compile_definitions(can_bus_gateway_stm32h750 PRIVATE
    USE_PWR_LDO_SUPPLY
    USE_HAL_DRIVER
    STM32H750xx
  )

  if(CAN_BUS_P0_FAULT_INJECTION)
    target_compile_definitions(can_bus_gateway_stm32h750 PRIVATE CAN_BUS_P0_FAULT_INJECTION=1)
  endif()

  target_compile_options(can_bus_gateway_stm32h750 PRIVATE
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
    -Os
    -flto
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
    -flto
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

  add_executable(test_dbc_active_commit tests/test_dbc_active_commit.c)
  target_link_libraries(test_dbc_active_commit PRIVATE can_bus_core)
  add_test(NAME dbc_active_commit COMMAND test_dbc_active_commit)

  add_executable(test_signal_codec tests/test_signal_codec.c)
  target_link_libraries(test_signal_codec PRIVATE can_bus_core)
  add_test(NAME signal_codec COMMAND test_signal_codec)

  add_executable(test_can_tx_control tests/test_can_tx_control.c)
  target_link_libraries(test_can_tx_control PRIVATE can_bus_core)
  add_test(NAME can_tx_control COMMAND test_can_tx_control)

  add_executable(test_dbc_parser tests/test_dbc_parser.c)
  target_link_libraries(test_dbc_parser PRIVATE can_bus_core)
  add_test(NAME dbc_parser COMMAND test_dbc_parser)

  add_executable(test_dbc_stream_parser tests/test_dbc_stream_parser.c)
  target_link_libraries(test_dbc_stream_parser PRIVATE can_bus_core)
  add_test(NAME dbc_stream_parser COMMAND test_dbc_stream_parser)
  if(LARGE_DBC_REAL_FIXTURE)
    add_test(NAME dbc_stream_parser_real
      COMMAND test_dbc_stream_parser "${LARGE_DBC_REAL_FIXTURE}")
  endif()

  add_executable(test_dbc_catalog_index_v1 tests/test_dbc_catalog_index_v1.c)
  target_link_libraries(test_dbc_catalog_index_v1 PRIVATE can_bus_core)
  add_test(NAME dbc_catalog_index_v1 COMMAND test_dbc_catalog_index_v1)
  if(LARGE_DBC_REAL_FIXTURE)
    add_test(NAME dbc_catalog_index_v1_real
      COMMAND test_dbc_catalog_index_v1 "${LARGE_DBC_REAL_FIXTURE}")
  endif()

  add_executable(dbc_index_dump tools/dbc_index_dump.c)
  target_link_libraries(dbc_index_dump PRIVATE can_bus_core)

  add_executable(dbc_index_build tools/dbc_index_build.c)
  target_link_libraries(dbc_index_build PRIVATE can_bus_core)

  add_executable(test_dbc_stream_index tests/test_dbc_stream_index.c)
  target_link_libraries(test_dbc_stream_index PRIVATE can_bus_core)
  if(LARGE_DBC_REAL_FIXTURE)
    add_test(NAME dbc_stream_index_real
      COMMAND test_dbc_stream_index
        "${LARGE_DBC_REAL_FIXTURE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/deploy/tf/dbc/active.dbc")
  endif()

  add_executable(test_dbc_upload tests/test_dbc_upload.c)
  target_link_libraries(test_dbc_upload PRIVATE can_bus_core)
  add_test(NAME dbc_upload COMMAND test_dbc_upload)

  add_executable(test_dbc_candidate_commit tests/test_dbc_candidate_commit.c)
  target_link_libraries(test_dbc_candidate_commit PRIVATE can_bus_core)
  add_test(NAME dbc_candidate_commit COMMAND test_dbc_candidate_commit)

  add_executable(test_dbc_candidate_format tests/test_dbc_candidate_format.c)
  target_link_libraries(test_dbc_candidate_format PRIVATE can_bus_core)
  add_test(NAME dbc_candidate_format COMMAND test_dbc_candidate_format)

  add_executable(test_dbc_candidate_catalog tests/test_dbc_candidate_catalog.c)
  target_link_libraries(test_dbc_candidate_catalog PRIVATE can_bus_core)
  add_test(NAME dbc_candidate_catalog COMMAND test_dbc_candidate_catalog)

  add_executable(test_dbc_candidate_http tests/test_dbc_candidate_http.c)
  target_link_libraries(test_dbc_candidate_http PRIVATE can_bus_core)
  add_test(NAME dbc_candidate_http COMMAND test_dbc_candidate_http)

  add_executable(test_dbc_selected_runtime tests/test_dbc_selected_runtime.c)
  target_link_libraries(test_dbc_selected_runtime PRIVATE can_bus_core)
  add_test(NAME dbc_selected_runtime COMMAND test_dbc_selected_runtime)

  add_executable(test_large_dbc_contract tests/test_large_dbc_contract.c)
  target_include_directories(test_large_dbc_contract PRIVATE include)
  add_test(NAME large_dbc_contract COMMAND test_large_dbc_contract)

  add_executable(test_tf_deploy_assets tests/test_tf_deploy_assets.c)
  target_link_libraries(test_tf_deploy_assets PRIVATE can_bus_core)
  add_test(NAME tf_deploy_assets COMMAND test_tf_deploy_assets)
  set_tests_properties(tf_deploy_assets PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  add_executable(test_dbc_decoder tests/test_dbc_decoder.c)
  target_link_libraries(test_dbc_decoder PRIVATE can_bus_core)
  add_test(NAME dbc_decoder COMMAND test_dbc_decoder)

  add_executable(test_dbc_signal_catalog tests/test_dbc_signal_catalog.c)
  target_link_libraries(test_dbc_signal_catalog PRIVATE can_bus_core)
  add_test(NAME dbc_signal_catalog COMMAND test_dbc_signal_catalog)

  add_executable(test_rule_engine tests/test_rule_engine.c)
  target_link_libraries(test_rule_engine PRIVATE can_bus_core)
  add_test(NAME rule_engine COMMAND test_rule_engine)

  add_executable(test_manual_relay tests/test_manual_relay.c)
  target_link_libraries(test_manual_relay PRIVATE can_bus_core)
  add_test(NAME manual_relay COMMAND test_manual_relay)

  add_executable(test_rule_config tests/test_rule_config.c)
  target_link_libraries(test_rule_config PRIVATE can_bus_core)
  add_test(NAME rule_config COMMAND test_rule_config)

  add_executable(test_rule_file tests/test_rule_file.c)
  target_link_libraries(test_rule_file PRIVATE can_bus_core)
  add_test(NAME rule_file COMMAND test_rule_file)

  add_executable(test_signal_cache tests/test_signal_cache.c)
  target_link_libraries(test_signal_cache PRIVATE can_bus_core)
  add_test(NAME signal_cache COMMAND test_signal_cache)

  add_executable(test_signal_api tests/test_signal_api.c)
  target_link_libraries(test_signal_api PRIVATE can_bus_core)
  add_test(NAME signal_api COMMAND test_signal_api)

  add_executable(test_signal_csv tests/test_signal_csv.c)
  target_link_libraries(test_signal_csv PRIVATE can_bus_core)
  add_test(NAME signal_csv COMMAND test_signal_csv)

  add_executable(test_selected_signal_log tests/test_selected_signal_log.c)
  target_link_libraries(test_selected_signal_log PRIVATE can_bus_core)
  add_test(NAME selected_signal_log COMMAND test_selected_signal_log)

  add_executable(test_signal_log_control tests/test_signal_log_control.c)
  target_link_libraries(test_signal_log_control PRIVATE can_bus_core)
  add_test(NAME signal_log_control COMMAND test_signal_log_control)

  add_executable(test_signal_log_buffer tests/test_signal_log_buffer.c)
  target_link_libraries(test_signal_log_buffer PRIVATE can_bus_core)
  add_test(NAME signal_log_buffer COMMAND test_signal_log_buffer)

  add_executable(test_signal_log_time tests/test_signal_log_time.c)
  target_link_libraries(test_signal_log_time PRIVATE can_bus_core)
  add_test(NAME signal_log_time COMMAND test_signal_log_time)

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
