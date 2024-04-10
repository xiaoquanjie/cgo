cmake_minimum_required(VERSION 3.25)
project(opentelemetry-example)

set(CMAKE_CXX_STANDARD 17)

include_directories(.)
include_directories(../../)

find_package(Protobuf REQUIRED)
find_package(CURL REQUIRED)
find_package(gRPC REQUIRED)
find_package(opentelemetry-cpp REQUIRED)
add_executable(opentelemetry-example main.cpp common/helloworld.grpc.pb.cc common/helloworld.pb.cc)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_libraries(opentelemetry-example cgo opentelemetry-cpp::opentelemetry-cpp grpc::grpc)
else ()
    target_link_libraries(opentelemetry-example dl pthread cgo opentelemetry-cpp::opentelemetry-cpp grpc::grpc)
endif ()