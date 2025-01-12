cmake_minimum_required(VERSION 3.25)

find_program(CLANG_C_EXE clang REQUIRED)
find_program(CLANG_CXX_EXE clang++ REQUIRED)
set(CMAKE_C_COMPILER ${CLANG_C_EXE})
set(CMAKE_CXX_COMPILER ${CLANG_CXX_EXE})

project(kevs)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

string(APPEND CMAKE_C_FLAGS " -std=c11 -g -Wall -Wextra -Werror")
string(APPEND CMAKE_C_FLAGS " -fsanitize=address,undefined -fno-sanitize-recover=all")
string(APPEND CMAKE_C_FLAGS " -fprofile-instr-generate -fcoverage-mapping")

add_executable(kevs src/cli.c src/kevs.c)

add_executable(unittests src/unittests.c src/kevs.c)

add_executable(fuzzer src/fuzzer.c src/kevs.c)
target_compile_options(fuzzer PUBLIC -fsanitize=fuzzer)
target_link_options(fuzzer PUBLIC -fsanitize=fuzzer)

add_executable(example src/example.c src/kevs.c)
