# 使用的protoc和grpc_cpp_plugin来自conan的grpc1.54.3所带的protobuf编译
../../../tool/protoc.exe -I. *.proto --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=../../../tool/grpc_cpp_plugin.exe

# 生成cogrpc的模板代码
../../../tool/cogrpc/cogrpc.exe --proto ./ --out ./template