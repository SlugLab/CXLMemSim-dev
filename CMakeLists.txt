cmake_minimum_required(VERSION 3.11.0)
project(CXL-MEM-Simulator VERSION 0.1.0)

list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

find_package(cxxopts REQUIRED)
find_package(fmt REQUIRED)
file(GLOB_RECURSE SOURCE_FILES src/*.cpp)

execute_process(COMMAND uname -r OUTPUT_VARIABLE arch OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CMAKE_CXX_FLAGS "-Wall -g -pthread -lrt -rdynamic")
set(CMAKE_CXX_STANDARD 23)

add_executable(CXL-MEM-Simulator ${SOURCE_FILES})

include_directories(CXL-MEM-Simulator include ${cxxopts_INCLUDE_DIR} ${fmt_INCLUDE_DIR})
target_link_libraries(CXL-MEM-Simulator fmt::fmt cxxopts::cxxopts)

add_subdirectory(microbench)