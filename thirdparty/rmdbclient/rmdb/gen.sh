#!/bin/sh

this_dir=$(dirname ${0})
cd ${this_dir}

protoc --cpp_out=./ --grpc_out=./ --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` meta.proto
protoc --cpp_out=./ --grpc_out=./ --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` rmdb.proto

cd -