cmake_minimum_required(VERSION 3.23)

include_directories(../)

add_executable(example
        main.cpp
        test.cpp
        heap_profiler/heap_profiler.cpp)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_libraries(example
            cgo)
else()
    include_directories(/usr/local/include)
    #add_definitions(-DUSE_HEAP_PROFILER)
    link_directories(/usr/local/lib)

    target_link_libraries(example
            cgo
            pthread
            dl
            #profiler
            tcmalloc)
endif ()

add_dependencies(example cgo)