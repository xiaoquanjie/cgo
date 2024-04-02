cmake_minimum_required(VERSION 3.25)
project(curl-example)

set(CMAKE_CXX_STANDARD 17)

include_directories(.)
include_directories(../../)

find_package(CURL)
add_executable(curl-example main.cpp)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_libraries(curl-example cgo CURL::libcurl)
else ()
    target_link_libraries(curl-example dl pthread cgo CURL::libcurl)
endif ()