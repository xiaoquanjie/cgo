cmake_minimum_required(VERSION 3.25)
project(rmdbclient)

set(CMAKE_CXX_STANDARD 17)

include_directories(.)
include_directories(../../)

find_package(gRPC REQUIRED)
find_package(mongoc-1.0 REQUIRED)

add_library(rmdbclient
        rmdb/config.pb.cc
        rmdb/meta.pb.cc
        rmdb/rmdb.grpc.pb.cc
        rmdb/rmdb.pb.cc
        algorithm.cpp
        encode.cpp
        rmdbclient.cpp
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_libraries(rmdbclient cgo grpc::grpc mongo::bson_static)
else ()
    target_link_libraries(rmdbclient cgo grpc::grpc mongo::bson_static)
endif ()
