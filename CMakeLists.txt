cmake_minimum_required(VERSION 3.20)

project(kevs)

option(RELEASE "Release build" OFF)
option(NO_SAN "Disable sanitizers" OFF)
option(NO_COV "Disable code coverage" OFF)

message(STATUS "CMAKE_C_COMPILER_ID=${CMAKE_C_COMPILER_ID}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (RELEASE)
  set(NO_SAN ON)
  set(NO_COV ON)
  string(APPEND CMAKE_C_FLAGS " -O3")
endif()

string(APPEND CMAKE_C_FLAGS " -std=c99 -g -Wall -Wextra -Werror")
if (NOT NO_SAN)
  string(APPEND CMAKE_C_FLAGS " -fsanitize=address,undefined -fno-sanitize-recover=all")
endif()

if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
  if (NOT NO_COV)
    string(APPEND CMAKE_C_FLAGS " -fprofile-instr-generate -fcoverage-mapping")
  endif()
endif()

add_executable(kevs src/c/cli.c src/c/kevs.c src/c/util.c)
add_executable(unittests src/c/unittests.c src/c/kevs.c src/c/util.c)
add_executable(example src/c/example.c src/c/kevs.c src/c/util.c)

if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
  add_executable(fuzzer src/c/fuzzer.c src/c/kevs.c)
  target_compile_options(fuzzer PUBLIC -fsanitize=fuzzer)
  target_link_options(fuzzer PUBLIC -fsanitize=fuzzer)
endif()
