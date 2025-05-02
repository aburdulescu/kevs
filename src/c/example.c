#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "kevs.h"
#include "util.h"

int main() {
  KevsStr file = kevs_str_from_cstr("examples/example.kevs");

  char *data = NULL;
  size_t data_len = 0;
  KevsError err = read_file(file, &data, &data_len);
  if (err != NULL) {
    fprintf(stderr, "error: failed to read file: %s\n", err);
    return 1;
  }

  int rc = 0;

  KevsTable root = {};
  char err_buf[8193] = {};
  const KevsParams params = {
      .file = file,
      .content = {.ptr = data, .len = data_len},
      .err_buf = err_buf,
      .err_buf_len = sizeof(err_buf) - 1,
  };

  err = kevs_parse(&root, params);
  if (err != NULL) {
    fprintf(stderr, "error: failed parse root table: %s\n", err);
    rc = 1;
  }

  // list root table keys and they types
  {
    for (size_t i = 0; i < root.len; i++) {
      char *k = kevs_str_dup(root.ptr[i].key);
      printf("key '%s', type '%s'\n", k,
             kevs_valuekind_str(root.ptr[i].val.kind));
      free(k);
    }
  }

  // string
  {
    char *val = NULL;
    KevsError err = kevs_table_string(root, "string_escaped", &val);
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
    KevsError err = kevs_table_int(root, "int", &val);
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
    KevsError err = kevs_table_bool(root, "bool", &val);
    if (err != NULL) {
      fprintf(stderr, "error: %s\n", err);
      rc = 1;
    } else {
      printf("boolean: %s\n", (val ? "true" : "false"));
    }
  }

  // list with elements of different types
  {
    KevsList val = {};
    KevsError err = kevs_table_list(root, "list1", &val);
    if (err != NULL) {
      fprintf(stderr, "error: %s\n", err);
      rc = 1;
    } else {
      // 1st is int
      {
        int64_t v = 0;
        KevsError err = kevs_list_int(val, 0, &v);
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
        KevsError err = kevs_list_string(val, 1, &v);
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
        KevsError err = kevs_list_bool(val, 2, &v);
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
    KevsList val = {};
    KevsError err = kevs_table_list(root, "list2", &val);
    if (err != NULL) {
      fprintf(stderr, "error: %s\n", err);
      rc = 1;
    } else {
      for (size_t i = 0; i < val.len; i++) {
        char *v = NULL;
        KevsError err = kevs_list_string(val, i, &v);
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
    KevsTable val = {};
    KevsError err = kevs_table_table(root, "table2", &val);
    if (err != NULL) {
      fprintf(stderr, "error: %s\n", err);
      rc = 1;
    } else {
      char *name = NULL;
      KevsError err = kevs_table_string(val, "name", &name);
      if (err != NULL) {
        fprintf(stderr, "error: %s\n", err);
        rc = 1;
      } else {
        printf("name: '%s'\n", name);
      }

      int64_t age = 0;
      err = kevs_table_int(val, "age", &age);
      if (err != NULL) {
        fprintf(stderr, "error: %s\n", err);
        rc = 1;
      } else {
        printf("age: %" PRId64 "\n", age);
      }
    }
  }

  kevs_free(&root);
  free(data);

  return rc;
}
