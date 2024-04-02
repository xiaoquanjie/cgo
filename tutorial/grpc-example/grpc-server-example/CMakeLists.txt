cmake_minimum_required(VERSION 3.25)
project(grpc-server-example)

set(CMAKE_CXX_STANDARD 17)

include_directories(.)
include_directories(../)
include_directories(../../../)

find_package(gRPC REQUIRED)
add_executable(grpc-server-example main.cpp ../common/helloworld.grpc.pb.cc ../common/helloworld.pb.cc)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_libraries(grpc-server-example grpc::grpc cgo)
else ()
    target_link_libraries(grpc-server-example dl pthread grpc::grpc cgo)
endif ()