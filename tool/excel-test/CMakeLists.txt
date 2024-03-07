cmake_minimum_required(VERSION 3.23)

project("excel-test")

include_directories(./)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "using windows")
else ()
    include(common.cmake)
    set(PROTOBUF_LIBS "absl::flags" "absl::flags_parse" ${_PROTOBUF_LIBPROTOBUF})
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
else()
    link_directories(/usr/local/lib)
endif()

add_executable(excel-test
        main.cpp
        cpp/AITemperamentConfig.pb.cc
        cpp/ErrorCodeKit.pb.cc
        cpp/AllSheetReader.cpp)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_libraries(excel-test
            ${PROTOBUF_LIBS})
else()
    target_link_libraries(excel-test
            ${PROTOBUF_LIBS}
            )
endif()

