cmake_minimum_required(VERSION 3.23)
project(cgo)

set(CMAKE_CXX_STANDARD 17)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
else()
    add_compile_options(-fPIC)
    add_compile_options(-Wno-error=deprecated-declarations)
    add_compile_options(-Wno-deprecated-declarations)
    add_compile_options(-Werror=unused-variable)
    add_compile_options(-Werror=sign-compare)
endif ()

include_directories(./)

# 编静态库，使用动态库可能会因为库的加载顺序导致hook功能出问题
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
        common/http_parser.cpp
        sync/mutex.cpp
        sync/channel.cpp
        sync/waitgroup.cpp
        hook/hook.cpp
        hook/epoll_iocp.cpp
        hook/epoll.cpp
        hook/iocp.cpp
)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
else()
    # 貌似对其他项目有继承关系，暂时没搞懂
    target_link_libraries(cgo
            pthread
            dl
            tcmalloc
            )
endif()