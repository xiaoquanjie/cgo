cmake_minimum_required(VERSION 3.23)
project(cgo)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(-Wno-error=deprecated-declarations)
add_compile_options(-Wno-deprecated-declarations)

include_directories(./)

add_library(cgo
        coroutine/structure.cpp
        coroutine/coroutine.cpp
        coroutine/coroutine_linux2.cpp
        coroutine/coroutine_win.cpp
        coroutine/coroutine_adapter.cpp
        scheduler/syntax.cpp
        scheduler/scheduler.cpp
        scheduler/cosignal.cpp

        common/time_pool.cpp
        common/semaphore.cpp
        sync/mutex.cpp
        sync/channel.cpp
        hook/hook.cpp
)

target_link_libraries(cgo
        pthread
        dl
)