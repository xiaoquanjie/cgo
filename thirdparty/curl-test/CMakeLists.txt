cmake_minimum_required(VERSION 3.23)

project("co_curl_test")

include_directories(../../)
include_directories(../)
link_directories(/usr/local/lib64 /usr/local/lib)

add_executable(curl_test
        main.cpp)

target_link_libraries(curl_test
        curl
        cgo
        )


