project("Collector")

list(APPEND CMAKE_CXX_FLAGS "${CXXMAKE_C_FLAGS} -std=c++1y -I../src/include/ -g")

add_executable(collector src/collector.cpp)

