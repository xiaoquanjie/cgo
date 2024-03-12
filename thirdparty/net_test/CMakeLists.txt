cmake_minimum_required(VERSION 3.23)

project("co_net_test")

include_directories(../../)
include_directories(../)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
else()
    link_directories(/usr/local/lib)
endif()

add_executable(net_test
        main.cpp)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_libraries(net_test
            cgo)
else()
    target_link_libraries(net_test
            cgo
            tcmalloc
            )
endif()
