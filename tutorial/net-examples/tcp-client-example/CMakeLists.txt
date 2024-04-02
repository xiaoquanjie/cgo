cmake_minimum_required(VERSION 3.25)
project(tcp-client-example)

set(CMAKE_CXX_STANDARD 17)

include_directories(.)
include_directories(../../../)

add_executable(tcp-client-example main.cpp)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_libraries(tcp-client-example cgo)
else ()
    target_link_libraries(tcp-client-example dl pthread cgo)
endif ()