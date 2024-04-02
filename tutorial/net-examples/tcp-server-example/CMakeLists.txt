cmake_minimum_required(VERSION 3.25)
project(tcp-server-example)

set(CMAKE_CXX_STANDARD 17)

include_directories(.)
include_directories(../../../)

add_executable(tcp-server-example main.cpp)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_libraries(tcp-server-example cgo)
else ()
    target_link_libraries(tcp-server-example dl pthread cgo)
endif ()