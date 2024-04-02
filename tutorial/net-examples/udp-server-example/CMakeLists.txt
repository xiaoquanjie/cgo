cmake_minimum_required(VERSION 3.25)
project(udp-server-example)

set(CMAKE_CXX_STANDARD 17)

include_directories(.)
include_directories(../../../)

add_executable(udp-server-example main.cpp)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_libraries(udp-server-example cgo)
else ()
    target_link_libraries(udp-server-example dl pthread cgo)
endif ()