cmake_minimum_required(VERSION 3.25)
project(rmdb-example)

set(CMAKE_CXX_STANDARD 17)

include_directories(.)
include_directories(../../)
include_directories(../../tool)

add_executable(rmdb-example main.cpp)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_libraries(rmdb-example cgo rmdbclient)
else ()
    target_link_libraries(rmdb-example dl pthread cgo rmdbclient)
endif ()

add_dependencies(rmdb-example rmdbclient)