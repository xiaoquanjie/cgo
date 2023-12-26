cmake_minimum_required(VERSION 3.23)
project(cgo)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(-Wno-error=deprecated-declarations)
add_compile_options(-Wno-deprecated-declarations)

add_library("cgo"
        coroutine/structure.cpp
        coroutine/coroutine.cpp
        coroutine/coroutine_linux2.cpp
        coroutine/coroutine_win.cpp
        coroutine/coroutine_adapter.cpp
        scheduler/syntax.cpp
        scheduler/scheduler.cpp
        scheduler/scheduler_linux.cpp
        scheduler/scheduler_win.cpp

        common/time_pool.cpp
        sync/channel.cpp
        hook/hook.cpp
)

add_executable(cgo_exmaple
        example/main.cpp
        example/test.cpp)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MinGw")
else()
    target_link_libraries(cgo_exmaple
            cgo
            pthread
            dl)
endif()