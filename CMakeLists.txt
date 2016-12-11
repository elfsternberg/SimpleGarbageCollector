project("Collector")

list(APPEND CMAKE_C_FLAGS "-std=c99 ${CMAKE_C_FLAGS} -g -ftest-coverage -fprofile-arcs")

add_executable(collector
    src/collector.c)

