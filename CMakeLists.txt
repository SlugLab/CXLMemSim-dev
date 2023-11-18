cmake_minimum_required(VERSION 3.11.0)
project(CXLMemSim VERSION 0.1.0)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options (-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options (-fcolor-diagnostics)
endif ()

add_subdirectory(microbench)
add_subdirectory(workloads)

list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

find_package(cxxopts REQUIRED)
find_package(fmt REQUIRED)
file(GLOB_RECURSE SOURCE_FILES src/*.cpp)

execute_process(COMMAND uname -r OUTPUT_VARIABLE arch OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CMAKE_CXX_FLAGS "-std=c++2b -fPIC -pthread -lrt -mavx512f -mpreferred-stack-boundary=4")

add_executable(CXLMemSim ${SOURCE_FILES})

include_directories(CXLMemSim include ${cxxopts_INCLUDE_DIR} ${fmt_INCLUDE_DIR})
target_link_libraries(CXLMemSim fmt::fmt cxxopts::cxxopts)

add_library(CXLMemSimHook SHARED src/module.cc)


