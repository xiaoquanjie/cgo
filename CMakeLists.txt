cmake_minimum_required(VERSION 3.25)
project(cgo_project)

add_subdirectory(cgo)
add_subdirectory(tutorial/curl-example)
add_subdirectory(tutorial/redis-example)
add_subdirectory(tutorial/mysql-example)
add_subdirectory(tutorial/net-examples/tcp-server-example)
add_subdirectory(tutorial/net-examples/tcp-client-example)