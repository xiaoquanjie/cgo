cmake_minimum_required(VERSION 3.23)
project(cgo)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(-Wno-error=deprecated-declarations)
add_compile_options(-Wno-deprecated-declarations)

add_executable(cgo
        main.cpp

        coroutine/structure.cpp
        coroutine/coroutine.cpp
        coroutine/coroutine_linux.cpp
        coroutine/coroutine_linux2.cpp
        coroutine/coroutine_win.cpp
        coroutine/syntax.cpp
        coroutine/scheduler.cpp
        coroutine/scheduler_linux.cpp
        coroutine/scheduler_win.cpp

        common/time_pool.cpp
        )

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MinGw")
else()
    target_link_libraries(cgo
            pthread)
endif()