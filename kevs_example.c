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

static Error read_file(Str path, String *out) {
  int fd = open(path.ptr, O_RDONLY);
  if (fd == -1) {
    return strerror(errno);
  }

  Error err = NULL;
  String content = {};

  struct stat stbuf = {};
  if (fstat(fd, &stbuf) == -1) {
    err = strerror(errno);
    goto cleanup;
  }

  string_resize(&content, stbuf.st_size);

  ssize_t nread = read(fd, content.ptr, content.len);
  if (nread == -1) {
    err = strerror(errno);
    goto cleanup;
  }
  if ((size_t)nread != content.len) {
    err = "short read";
    goto cleanup;
  }

  *out = content;

cleanup:
  if (err != NULL) {
    string_free(&content);
  }
  close(fd);

  return err;
}

int main() {
  Str file = str_from_cstring("examples/example.kevs");

  String data = {};
  Error err = read_file(file, &data);
  if (err != NULL) {
    fprintf(stderr, "error: failed to read file: %s\n", err);
    return 1;
  }

  int rc = 0;

  Context ctx = {};
  Table table = {};
  const bool ok = table_parse(&table, ctx, file, str_from_string(data));
  if (!ok) {
    rc = 1;
  }

  // string
  {
    String val = {};
    Error err = table_string(table, "str", &val);
    if (err != NULL) {
      fprintf(stderr, "error: %s\n", err);
      rc = 1;
    } else {
      printf("string: '%s'\n", val.ptr);
    }
    string_free(&val);
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
        String v = {};
        Error err = list_string(val, 1, &v);
        if (err != NULL) {
          fprintf(stderr, "error: %s\n", err);
          rc = 1;
        } else {
          printf("list1[1]: '%s'\n", v.ptr);
        }
        string_free(&v);
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
        String v = {};
        Error err = list_string(val, i, &v);
        if (err != NULL) {
          fprintf(stderr, "error: %s\n", err);
          rc = 1;
        } else {
          printf("list2[%zu]: '%s'\n", i, v.ptr);
        }
        string_free(&v);
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
      String name = {};
      Error err = table_string(val, "name", &name);
      if (err != NULL) {
        fprintf(stderr, "error: %s\n", err);
        rc = 1;
      } else {
        printf("name: '%s'\n", name.ptr);
      }
      string_free(&name);

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
  string_free(&data);

  return rc;
}
