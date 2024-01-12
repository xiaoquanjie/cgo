cmake_minimum_required(VERSION 3.23)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "using windows")
else ()
    include(common.cmake)

    set(PROTOBUF_LIBS "absl::flags" "absl::flags_parse" ${_PROTOBUF_LIBPROTOBUF})
    set(GRPC_LIBS ${_REFLECTION} ${_GRPC_GRPCPP} ${PROTOBUF_LIBS})
endif ()

project("rmdb_test")

message(STATUS ${PROTOBUF_LIBS})
message(STATUS ${GRPC_LIBS})

include_directories(../../)
include_directories(../)
link_directories(/usr/local/lib64 /usr/local/lib)

add_executable(rmdb_test
        main.cpp)

target_link_libraries(rmdb_test
        rmdbclient
        ${PROTOBUF_LIBS}
        cgo
        )


