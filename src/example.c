#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kevs.h"
#include "util.h"

int main() {
  Str file = str_from_cstr("examples/example.kevs");

  char *data = NULL;
  Error err = read_file(file, &data);
  if (err != NULL) {
    fprintf(stderr, "error: failed to read file: %s\n", err);
    return 1;
  }

  int rc = 0;

  Context ctx = {};
  Table table = {};
  const bool ok = table_parse(&table, ctx, file, str_from_cstr(data));
  if (!ok) {
    rc = 1;
  }

  // string
  {
    Str val = {};
    Error err = table_string(table, "str", &val);
    if (err != NULL) {
      fprintf(stderr, "error: %s\n", err);
      rc = 1;
    } else {
      char *val_cstr = str_dup(val);
      printf("string: '%s'\n", val_cstr);
      free(val_cstr);
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
        Str v = {};
        Error err = list_string(val, 1, &v);
        if (err != NULL) {
          fprintf(stderr, "error: %s\n", err);
          rc = 1;
        } else {
          char *v_cstr = str_dup(v);
          printf("list1[1]: '%s'\n", v.ptr);
          free(v_cstr);
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
        Str v = {};
        Error err = list_string(val, i, &v);
        if (err != NULL) {
          fprintf(stderr, "error: %s\n", err);
          rc = 1;
        } else {
          char *v_cstr = str_dup(v);
          printf("list2[%zu]: '%s'\n", i, v_cstr);
          free(v_cstr);
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
      Str name = {};
      Error err = table_string(val, "name", &name);
      if (err != NULL) {
        fprintf(stderr, "error: %s\n", err);
        rc = 1;
      } else {
        char *name_cstr = str_dup(name);
        printf("name: '%s'\n", name_cstr);
        free(name_cstr);
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

  table_free(&table);
  free(data);

  return rc;
}
