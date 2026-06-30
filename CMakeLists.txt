cmake_minimum_required(VERSION 3.20)

project(can_bus_gateway C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(CAN_BUS_BUILD_TESTS "Build host-side unit tests" ON)

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
)

target_include_directories(can_bus_ports
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(can_bus_ports PUBLIC can_bus_core)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(can_bus_ports PRIVATE -Wall -Wextra -Wpedantic)
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
endif()
