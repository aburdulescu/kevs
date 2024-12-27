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
  COMMAND ${CMAKE_SOURCE_DIR}/cov-merge.py ${CMAKE_BINARY_DIR}/*.profraw
  COMMAND ${CMAKE_SOURCE_DIR}/cov-show.py ${CMAKE_BINARY_DIR}/kevs ${CMAKE_BINARY_DIR}/kevs_test -s ${CMAKE_SOURCE_DIR}/kevs.c
)

kevs_test(NAME unittests COMMAND kevs_test)
kevs_test(NAME comments COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/testdata/valid/comments.kevs)
kevs_test(NAME simple_values COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/testdata/valid/simple_values.kevs)
kevs_test(NAME multiline_string COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/testdata/valid/multiline_string.kevs)
kevs_test(NAME list_one_line COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/testdata/valid/list_one_line.kevs)
kevs_test(NAME table_one_line COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/testdata/valid/table_one_line.kevs)
kevs_test(NAME one_line_kv COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/testdata/valid/one_line_kv.kevs)
kevs_test(NAME list_with_comments COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/testdata/valid/list_with_comments.kevs)
kevs_test(NAME table_with_comments COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/testdata/valid/table_with_comments.kevs)
kevs_test(NAME table_list_table COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/testdata/valid/table_list_table.kevs)
kevs_test(NAME nested COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/testdata/valid/nested.kevs)
kevs_test(NAME example COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/example.kevs)

kevs_test(
  NAME comment_without_newline
  COMMAND kevs ${CMAKE_SOURCE_DIR}/testdata/not_valid/comment_without_newline.kevs
  PASS_REGEX "invalid syntax: comment does not end with newline"
)

kevs_test(
  NAME missing_kv_separator
  COMMAND kevs ${CMAKE_SOURCE_DIR}/testdata/not_valid/missing_kv_separator.kevs
  PASS_REGEX "invalid syntax: key-value pair is missing separator '='"
)

kevs_test(
  NAME missing_string_end
  COMMAND kevs ${CMAKE_SOURCE_DIR}/testdata/not_valid/missing_string_end.kevs
  PASS_REGEX "invalid syntax: string value does not end with '\"'"
)

kevs_test(
  NAME missing_multiline_string_end
  COMMAND kevs ${CMAKE_SOURCE_DIR}/testdata/not_valid/missing_multiline_string_end.kevs
  PASS_REGEX "invalid syntax: multiline string value does not end with '`'"
)

kevs_test(
  NAME missing_semicolon_after_int
  COMMAND kevs ${CMAKE_SOURCE_DIR}/testdata/not_valid/missing_semicolon_after_int.kevs
  PASS_REGEX "invalid syntax: integer or boolean value does not end with semicolon"
)

kevs_test(
  NAME missing_semicolon_after_bool
  COMMAND kevs ${CMAKE_SOURCE_DIR}/testdata/not_valid/missing_semicolon_after_bool.kevs
  PASS_REGEX "invalid syntax: integer or boolean value does not end with semicolon"
)
