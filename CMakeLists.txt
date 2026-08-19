cmake_minimum_required(VERSION 3.20)

project(dragonfly_ground_station C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(dragonfly
    src/protocol_codec.c
    src/transport.c
    src/commands.c
    src/crc.c
)

target_include_directories(dragonfly PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Dragonfly_Protocol
)

add_executable(dragonfly-cli
    src/cli/main.c
)

target_link_libraries(dragonfly-cli PRIVATE dragonfly)