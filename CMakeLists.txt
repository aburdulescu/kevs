cmake_minimum_required(VERSION 3.20)

project(kevs)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

string(APPEND CMAKE_C_FLAGS  " -std=c11 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all")

add_executable(kevs kevs_cli.c kevs.c)
add_executable(kevs_test kevs_test.c kevs.c)

enable_testing()

add_test(NAME unittests COMMAND kevs_test)

add_test(NAME comments COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/testdata/comments.kevs)
add_test(NAME simple_values COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/testdata/simple_values.kevs)
add_test(NAME multiline_string COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/testdata/multiline_string.kevs)
add_test(NAME list_one_line COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/testdata/list_one_line.kevs)
add_test(NAME table_one_line COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/testdata/table_one_line.kevs)
add_test(NAME one_line_kv COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/testdata/one_line_kv.kevs)
add_test(NAME list_with_comments COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/testdata/list_with_comments.kevs)
add_test(NAME table_with_comments COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/testdata/table_with_comments.kevs)
add_test(NAME table_list_table COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/testdata/table_list_table.kevs)
add_test(NAME nested COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/testdata/nested.kevs)
add_test(NAME example COMMAND kevs -abort ${CMAKE_SOURCE_DIR}/example.kevs)
