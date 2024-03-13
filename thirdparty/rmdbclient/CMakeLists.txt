project(rmdbclient)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "using windows")
else ()
    include(common.cmake)

    set(PROTOBUF_LIBS "absl::flags" "absl::flags_parse" ${_PROTOBUF_LIBPROTOBUF})
    set(GRPC_LIBS ${_REFLECTION} ${_GRPC_GRPCPP} ${PROTOBUF_LIBS})
endif ()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++17")
file(GLOB_RECURSE ALL_CPP *.cpp *.cc)

link_directories(/usr/local/lib64 /usr/local/lib)
include_directories(/usr/local/include
        /usr/local/include/libbson-1.0
        ./
        ../
        ../../)

add_library(rmdbclient ${ALL_CPP})

target_link_libraries(rmdbclient PRIVATE
        cgo
        bson-1.0
        ${GRPC_LIBS})
