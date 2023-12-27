cmake_minimum_required(VERSION 3.23)

include_directories(../)

add_executable(exmaple
        main.cpp
        test.cpp)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MinGw")
else()
    target_link_libraries(exmaple
            cgo
            pthread
            dl)
endif()
