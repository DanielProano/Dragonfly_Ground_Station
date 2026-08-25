cmake_minimum_required(VERSION 3.20)

project(dragonfly_ground_station C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(dragonfly
    src/transport.c
    src/commands.c
    Dragonfly_Protocol/crc.c
    Dragonfly_Protocol/protocol_codec.c
)

target_include_directories(dragonfly PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Dragonfly_Protocol
)

add_executable(dragonfly_cli
    src/cli/main.c
)

target_link_libraries(dragonfly_cli PRIVATE dragonfly)