cmake_minimum_required(VERSION 3.25)
project(grpc-client-example)

set(CMAKE_CXX_STANDARD 17)

include_directories(.)
include_directories(../)
include_directories(../../../)

find_package(gRPC REQUIRED)
add_executable(grpc-client-example main.cpp ../common/helloworld.pb.cc ../common/helloworld.grpc.pb.cc)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_libraries(grpc-client-example grpc::grpc cgo)
else ()
    target_link_libraries(grpc-client-example dl pthread grpc::grpc cgo)
endif ()

add_dependencies(grpc-client-example cgo)