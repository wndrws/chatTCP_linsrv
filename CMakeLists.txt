cmake_minimum_required(VERSION 3.5)
project(testSneider)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES src/skel.h src/etcp.h src/iofuncs.c src/sofuncs.c src/miscfuncs.c src/main.cpp)
add_executable(chatTCP_linsrv ${SOURCE_FILES})
target_link_libraries(chatTCP_linsrv pthread tbb)