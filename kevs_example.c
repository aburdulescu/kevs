#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kevs.h"

typedef struct ReadFileResult {
  String val;
  Error err;
} ReadFileResult;

static ReadFileResult read_file(Str path) {
  ReadFileResult result = {};

  int fd = open(path.ptr, O_RDONLY);
  if (fd == -1) {
    result.err = strerror(errno);
    return result;
  }

  struct stat stbuf = {};
  if (fstat(fd, &stbuf) == -1) {
    result.err = strerror(errno);
    close(fd);
    return result;
  }

  String content = {};
  string_resize(&content, stbuf.st_size);

  ssize_t nread = read(fd, content.ptr, content.len);
  if (nread == -1) {
    result.err = strerror(errno);
    string_free(&content);
    close(fd);
    return result;
  }
  if ((size_t)nread != content.len) {
    result.err = "short read";
    string_free(&content);
    close(fd);
    return result;
  }

  result.val = content;

  return result;
}

int main() {
  Str file = str_from_cstring("examples/example.kevs");

  ReadFileResult result = read_file(file);
  if (result.err != NULL) {
    fprintf(stderr, "error: failed to read file: %s\n", result.err);
    return 1;
  }

  int rc = 0;

  Context ctx = {};
  Table table = {};
  const bool ok = table_parse(&table, ctx, file, str_from_string(result.val));
  if (!ok) {
    rc = 1;
  }

  // string
  {
    String val = {};
    Error err = table_get_string(table, str_from_cstring("str"), &val);
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
    Error err = table_get_int(table, str_from_cstring("int"), &val);
    if (err != NULL) {
      fprintf(stderr, "error: %s\n", err);
      rc = 1;
    } else {
      printf("integer: %ld\n", val);
    }
  }

  // boolean
  {
    bool val = false;
    Error err = table_get_bool(table, str_from_cstring("bool"), &val);
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
    Error err = table_get_list(table, str_from_cstring("list1"), &val);
    if (err != NULL) {
      fprintf(stderr, "error: %s\n", err);
      rc = 1;
    } else {
      // 1st is int
      {
        int64_t v = 0;
        Error err = list_get_int(val, 0, &v);
        if (err != NULL) {
          fprintf(stderr, "error: %s\n", err);
          rc = 1;
        } else {
          printf("list1[0]: %ld\n", v);
        }
      }
      // 2nd is string
      {
        String v = {};
        Error err = list_get_string(val, 1, &v);
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
        Error err = list_get_bool(val, 2, &v);
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
    Error err = table_get_list(table, str_from_cstring("list2"), &val);
    if (err != NULL) {
      fprintf(stderr, "error: %s\n", err);
      rc = 1;
    } else {
      for (size_t i = 0; i < val.len; i++) {
        String v = {};
        Error err = list_get_string(val, i, &v);
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
    Error err = table_get_table(table, str_from_cstring("table2"), &val);
    if (err != NULL) {
      fprintf(stderr, "error: %s\n", err);
      rc = 1;
    } else {
      String name = {};
      Error err = table_get_string(val, str_from_cstring("name"), &name);
      if (err != NULL) {
        fprintf(stderr, "error: %s\n", err);
        rc = 1;
      } else {
        printf("name: '%s'\n", name.ptr);
      }
      string_free(&name);

      int64_t age = 0;
      err = table_get_int(val, str_from_cstring("age"), &age);
      if (err != NULL) {
        fprintf(stderr, "error: %s\n", err);
        rc = 1;
      } else {
        printf("age: %ld\n", age);
      }
    }
  }

  table_free(&table);
  string_free(&result.val);

  return rc;
}
