cmake_minimum_required(VERSION 3.23)

project("co_net_test")

message(STATUS ${PROTOBUF_LIBS})
message(STATUS ${GRPC_LIBS})

include_directories(../../)
include_directories(../)
link_directories(/usr/local/lib64 /usr/local/lib /usr/lib64/mysql)

add_executable(net_test
        main.cpp)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    target_link_libraries(net_test
            cgo)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MinGw")
else()
    target_link_libraries(net_test
            cgo
            tcmalloc
            )
endif()

add_dependencies(example cgo)
