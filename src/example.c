#include <inttypes.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kevs.h"
#include "util.h"

int main() {
  Str file = str_from_cstr("examples/example.kevs");

  char *data = NULL;
  size_t data_len = 0;
  Error err = read_file(file, &data, &data_len);
  if (err != NULL) {
    fprintf(stderr, "error: failed to read file: %s\n", err);
    return 1;
  }

  int rc = 0;

  const size_t tables_buf_len = 100;
  KeyValue tables_buf[tables_buf_len];

  const size_t lists_buf_len = 100;
  Value lists_buf[lists_buf_len];

  const size_t tokens_buf_len = 1000;
  Token tokens_buf[tokens_buf_len];

  char strings_buf[16 << 10];

  Allocators alls = {};
  arena_init(&alls.tables, tables_buf, tables_buf_len * sizeof(tables_buf[0]));
  arena_init(&alls.lists, lists_buf, lists_buf_len * sizeof(lists_buf[0]));
  arena_init(&alls.tokens, tokens_buf, tokens_buf_len * sizeof(tokens_buf[0]));
  arena_init(&alls.strings, strings_buf, sizeof(strings_buf));

  Table table = {};
  char err_buf[8193] = {};
  const Params params = {
      .file = file,
      .content = {.ptr = data, .len = data_len},
      .alls = alls,
      .err_buf = err_buf,
      .err_buf_len = sizeof(err_buf) - 1,
  };

  err = table_parse(&table, params);
  if (err != NULL) {
    fprintf(stderr, "error: failed parse root table: %s\n", err);
    rc = 1;
  }

  // string
  {
    char *val = NULL;
    Error err = table_string(table, "string_escaped", &val);
    if (err != NULL) {
      fprintf(stderr, "error: %s\n", err);
      rc = 1;
    } else {
      printf("string: '%s'\n", val);
    }
  }

  // integer
  {
    int64_t val = 0;
    Error err = table_int(table, "int", &val);
    if (err != NULL) {
      fprintf(stderr, "error: %s\n", err);
      rc = 1;
    } else {
      printf("integer: %" PRId64 "\n", val);
    }
  }

  // boolean
  {
    bool val = false;
    Error err = table_bool(table, "bool", &val);
    if (err != NULL) {
      fprintf(stderr, "error: %s\n", err);
      rc = 1;
    } else {
      printf("boolean: %s\n", (val ? "true" : "false"));
    }
  }

  // list with elements of different types
  {
    List val = {};
    Error err = table_list(table, "list1", &val);
    if (err != NULL) {
      fprintf(stderr, "error: %s\n", err);
      rc = 1;
    } else {
      // 1st is int
      {
        int64_t v = 0;
        Error err = list_int(val, 0, &v);
        if (err != NULL) {
          fprintf(stderr, "error: %s\n", err);
          rc = 1;
        } else {
          printf("list1[0]: %" PRId64 "\n", v);
        }
      }
      // 2nd is string
      {
        char *v = NULL;
        Error err = list_string(val, 1, &v);
        if (err != NULL) {
          fprintf(stderr, "error: %s\n", err);
          rc = 1;
        } else {
          printf("list1[1]: '%s'\n", v);
        }
      }
      // 3rd is boolean
      {
        bool v = false;
        Error err = list_bool(val, 2, &v);
        if (err != NULL) {
          fprintf(stderr, "error: %s\n", err);
          rc = 1;
        } else {
          printf("list1[2]: %s\n", (v ? "true" : "false"));
        }
      }
    }
  }

  // list with elements of same types
  {
    List val = {};
    Error err = table_list(table, "list2", &val);
    if (err != NULL) {
      fprintf(stderr, "error: %s\n", err);
      rc = 1;
    } else {
      for (size_t i = 0; i < val.len; i++) {
        char *v = NULL;
        Error err = list_string(val, i, &v);
        if (err != NULL) {
          fprintf(stderr, "error: %s\n", err);
          rc = 1;
        } else {
          printf("list2[%zu]: '%s'\n", i, v);
        }
      }
    }
  }

  // table
  {
    Table val = {};
    Error err = table_table(table, "table2", &val);
    if (err != NULL) {
      fprintf(stderr, "error: %s\n", err);
      rc = 1;
    } else {
      char *name = NULL;
      Error err = table_string(val, "name", &name);
      if (err != NULL) {
        fprintf(stderr, "error: %s\n", err);
        rc = 1;
      } else {
        printf("name: '%s'\n", name);
      }

      int64_t age = 0;
      err = table_int(val, "age", &age);
      if (err != NULL) {
        fprintf(stderr, "error: %s\n", err);
        rc = 1;
      } else {
        printf("age: %" PRId64 "\n", age);
      }
    }
  }

  free(data);

  return rc;
}
