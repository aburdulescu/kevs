cmake_minimum_required(VERSION 3.25)

find_program(CLANG_C_EXE clang REQUIRED)
find_program(CLANG_CXX_EXE clang++ REQUIRED)
set(CMAKE_C_COMPILER ${CLANG_C_EXE})
set(CMAKE_CXX_COMPILER ${CLANG_CXX_EXE})

find_program(CCACHE_EXE ccache)
if(CCACHE_EXE)
  set(CMAKE_C_COMPILER_LAUNCHER ccache)
  set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
endif()

project(kevs)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(COV "Code coverage" OFF)

string(APPEND CMAKE_C_FLAGS " -std=c11 -g -Wall -Wextra -Werror")

if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  string(APPEND CMAKE_C_FLAGS " -fsanitize=address,undefined -fno-sanitize-recover=all")
endif()

if(COV)
  string(APPEND CMAKE_C_FLAGS " -fprofile-instr-generate -fcoverage-mapping")
endif()

add_executable(kevs kevs_cli.c kevs.c)
if (CMAKE_BUILD_TYPE STREQUAL "Release")
  message(STATUS "static bin")
  target_link_libraries(kevs PUBLIC -static)
endif()

add_executable(kevs_test kevs_test.c kevs.c)

add_executable(kevs_fuzz kevs_fuzz.c kevs.c)
target_compile_options(kevs_fuzz
  PUBLIC
  -fsanitize=undefined,address,fuzzer
  -fno-sanitize-recover=all
  -fprofile-instr-generate
  -fcoverage-mapping
)
target_link_options(kevs_fuzz
  PUBLIC
  -fsanitize=undefined,address,fuzzer
)
