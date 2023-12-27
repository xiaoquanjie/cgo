cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(-Wno-error=deprecated-declarations)
add_compile_options(-Wno-deprecated-declarations)

include_directories(../grpc)
include_directories(../../)
include_directories(.)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
else()
    message(STATUS "use cogrpc..")
    add_library(cogrpc
        empty.cpp
)
endif()