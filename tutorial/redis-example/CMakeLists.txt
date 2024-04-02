cmake_minimum_required(VERSION 3.25)
project(redis-example)

set(CMAKE_CXX_STANDARD 17)

include_directories(.)
include_directories(../../)

find_package(hiredis)
add_executable(redis-example main.cpp)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_libraries(redis-example cgo hiredis::hiredis)
else ()
    target_link_libraries(redis-example dl pthread cgo hiredis::hiredis)
endif ()