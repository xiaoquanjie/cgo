cmake_minimum_required(VERSION 3.25)
project(opentelemetry-example)

set(CMAKE_CXX_STANDARD 17)

include_directories(.)
include_directories(../../)

find_package(opentelemetry-cpp REQUIRED)

if (opentelemetry-proto_FOUND)
    message(STATUS "==============================")
    message(STATUS ${opentelemetry-proto_LIBRARIES})
endif ()

add_executable(opentelemetry-example main.cpp common/helloworld.grpc.pb.cc common/helloworld.pb.cc)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_libraries(opentelemetry-example cgo grpc::grpc opentelemetry-cpp::opentelemetry-cpp)
else ()
    target_link_libraries(opentelemetry-example dl pthread cgo grpc::grpc opentelemetry-cpp::opentelemetry-cpp)
endif ()