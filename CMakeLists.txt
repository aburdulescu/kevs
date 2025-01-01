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

enable_testing()

function(kevs_test)
  set(options )
  set(oneValueArgs NAME PASS_REGEX)
  set(multiValueArgs COMMAND)
  cmake_parse_arguments(arg "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT arg_NAME)
    message(FATAL_ERROR "Need NAME")
  endif()
  if(NOT arg_COMMAND)
    message(FATAL_ERROR "Need COMMAND")
  endif()

  add_test(
    NAME ${arg_NAME}
    COMMAND ${arg_COMMAND}
  )

  if(arg_PASS_REGEX)
    set_property(
      TEST ${arg_NAME}
      PROPERTY
      PASS_REGULAR_EXPRESSION ${arg_PASS_REGEX})
  endif()

  if(COV)
    set_property(
      TEST ${arg_NAME}
      PROPERTY
      ENVIRONMENT "LLVM_PROFILE_FILE=${CMAKE_CURRENT_BINARY_DIR}/${arg_NAME}.profraw"
    )
  endif()
endfunction()

add_custom_target(
  cov
  COMMENT "Generate coverage report"
  COMMAND ${CMAKE_SOURCE_DIR}/scripts/cov-merge.py ${CMAKE_BINARY_DIR}/*.profraw
  COMMAND ${CMAKE_SOURCE_DIR}/scripts/cov-show.py ${CMAKE_BINARY_DIR}/kevs_test -s ${CMAKE_SOURCE_DIR}/kevs.c
)

kevs_test(NAME unittests COMMAND kevs_test)

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
