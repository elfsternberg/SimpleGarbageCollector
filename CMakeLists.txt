project("Collector")

list(APPEND CMAKE_CXX_FLAGS "${CXXMAKE_C_FLAGS} -std=c++17 -I../src/include/ -g")

add_executable(collector src/collector.cpp)

