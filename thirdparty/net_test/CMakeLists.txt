cmake_minimum_required(VERSION 3.23)

project("co_net_test")

message(STATUS ${PROTOBUF_LIBS})
message(STATUS ${GRPC_LIBS})

include_directories(../../)
include_directories(../)
link_directories(/usr/local/lib64 /usr/local/lib /usr/lib64/mysql)

add_executable(net_test
        main.cpp)

target_link_libraries(net_test
        cgo
        )


