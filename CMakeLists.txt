cmake_minimum_required(VERSION 3.23)
project(cgo)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(-Wno-error=deprecated-declarations)
add_compile_options(-Wno-deprecated-declarations)

add_subdirectory(./cgo)
add_subdirectory(./example)
#add_subdirectory(./thirdparty/grpc-test)
#add_subdirectory(./thirdparty/redis-test)
#add_subdirectory(./thirdparty/curl-test)
#add_subdirectory(./thirdparty/myql-test)
#add_subdirectory(./thirdparty/rmdbclient)
#add_subdirectory(./thirdparty/rmdb-test)
