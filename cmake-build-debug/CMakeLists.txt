cmake_minimum_required(VERSION 3.13)
project(skiplist)

set(CMAKE_CXX_STANDARD 14)

add_executable(skiplist ../main.cpp ../skiplist.h ../skiplistNode.h ../skiplist.cpp ../skiplistNode.cpp)

