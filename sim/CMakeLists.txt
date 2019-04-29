cmake_minimum_required(VERSION 3.7)
project(stmsim)

include_directories(src)
set(CMAKE_CXX_STANDARD 17)
file(GLOB_RECURSE SIM_SRCS src/*.cpp)

add_executable(sim ${SIM_SRCS})
