cmake_minimum_required(VERSION 3.25)
project(cgo VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

# 编静态库，使用动态库可能会因为库的加载顺序导致hook功能出问题
add_library(cgo
        common/http_parser.cpp
        common/semaphore.cpp
        common/time_pool.cpp

        coroutine/coroutine.cpp
        coroutine/coroutine_adapter.cpp
        coroutine/coroutine_linux2.cpp
        coroutine/coroutine_win.cpp
        coroutine/structure.cpp

        hook/epoll.cpp
        hook/epoll_iocp.cpp
        hook/hook.cpp
        hook/iocp.cpp

        scheduler/cosignal.cpp
        scheduler/scheduler.cpp
        scheduler/syntax.cpp

        sync/channel.cpp
        sync/mutex.cpp
        sync/waitgroup.cpp
)

target_include_directories(cgo PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_definitions(cgo PRIVATE -DUNICODE)
    target_compile_definitions(cgo PRIVATE -D_UNICODE)
else()
    target_compile_options(cgo PRIVATE -fPIC)
    target_compile_options(cgo PRIVATE -Wno-error=deprecated-declarations)
    target_compile_options(cgo PRIVATE -Wno-deprecated-declarations)
    target_compile_options(cgo PRIVATE -Werror=unused-variable)
    target_compile_options(cgo PRIVATE -Werror=sign-compare)
endif ()

install(FILES ./cgo.h DESTINATION include/cgo)
install(FILES ./common/http_parser.h DESTINATION include/cgo/common)
install(FILES ./scheduler/syntax.h DESTINATION include/cgo/scheduler)
install(DIRECTORY ./sync/ DESTINATION include/cgo/sync FILES_MATCHING PATTERN "*.h")
install(FILES ./hook/hook.h DESTINATION include/cgo/hook)
install(DIRECTORY ./thirdparty/ DESTINATION include/cgo/thirdparty)
install(TARGETS cgo
        EXPORT  cgoTargets
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)
install(EXPORT cgoTargets
        NAMESPACE "cgo::"
        FILE cgo-config.cmake DESTINATION lib/cmake
        FILE cgoConfig.cmake  DESTINATION lib/cmake)

