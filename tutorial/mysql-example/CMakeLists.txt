cmake_minimum_required(VERSION 3.25)
project(mysql-example)

set(CMAKE_CXX_STANDARD 17)

include_directories(.)
include_directories(../../)

find_package(libmysqlclient)
add_executable(mysql-example main.cpp)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_libraries(mysql-example cgo libmysqlclient::libmysqlclient)
else ()
    target_link_libraries(mysql-example dl pthread cgo libmysqlclient::libmysqlclient)
endif ()