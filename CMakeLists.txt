cmake_minimum_required(VERSION 3.25)

project(kevs)

message(STATUS "CMAKE_C_COMPILER_ID=${CMAKE_C_COMPILER_ID}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

string(APPEND CMAKE_C_FLAGS " -std=c99 -g -Wall -Wextra -Werror")
string(APPEND CMAKE_C_FLAGS " -fsanitize=address,undefined -fno-sanitize-recover=all")

if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
  string(APPEND CMAKE_C_FLAGS " -fprofile-instr-generate -fcoverage-mapping")
endif()

add_executable(kevs src/cli.c src/kevs.c src/util.c)
add_executable(unittests src/unittests.c src/kevs.c)
add_executable(example src/example.c src/kevs.c src/util.c)

if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
  add_executable(fuzzer src/fuzzer.c src/kevs.c)
  target_compile_options(fuzzer PUBLIC -fsanitize=fuzzer)
  target_link_options(fuzzer PUBLIC -fsanitize=fuzzer)
endif()
