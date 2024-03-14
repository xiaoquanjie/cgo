cmake_minimum_required(VERSION 3.23)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "using windows")
else ()
    include(common.cmake)
    link_directories(/usr/local/lib)
    set(PROTOBUF_LIBS "absl::flags" "absl::flags_parse" ${_PROTOBUF_LIBPROTOBUF})
    set(GRPC_LIBS ${_REFLECTION} ${_GRPC_GRPCPP} ${PROTOBUF_LIBS})
endif ()

project("grpc-test")

message(STATUS ${PROTOBUF_LIBS})
message(STATUS ${GRPC_LIBS})

include_directories(../../)
include_directories(../)

add_executable(grpc-test
        helloworld.grpc.pb.cc
        helloworld.pb.cc
        main.cpp)

target_link_libraries(grpc-test
        ${GRPC_LIBS}
        cgo
        )
