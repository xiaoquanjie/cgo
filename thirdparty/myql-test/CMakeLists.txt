cmake_minimum_required(VERSION 3.23)

project("co_redis_test")

include_directories(../../)
include_directories(../)
link_directories(/usr/local/lib64 /usr/local/lib /usr/lib64/mysql)

add_executable(mysql_test
        main.cpp)

target_link_libraries(mysql_test
        mysqlclient
        cgo
        )


