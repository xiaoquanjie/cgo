# 使用的protoc和grpc_cpp_plugin来自conan的grpc1.54.3所带的protobuf编译
../../protoc.exe --cpp_out=./ config.proto
../../protoc.exe --cpp_out=./ meta.proto
../../protoc.exe --cpp_out=./ --grpc_out=./ --plugin=protoc-gen-grpc=../../grpc_cpp_plugin.exe rmdb.proto
echo ok
sleep 20

