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
  const bool ok = kevs_parse(ctx, file, str_from_string(result.val), &table);
  if (!ok) {
    rc = 1;
  }

  const Str key = str_from_cstring("str");
  String val = {};
  Error err = kevs_get_string(table, key, &val);
  if (err != NULL) {
    fprintf(stderr, "error: %s\n", err);
    rc = 1;
  }

  printf("%s = '%s'\n", key.ptr, val.ptr);

  string_free(&val);
  kevs_free(&table);
  string_free(&result.val);

  return rc;
}
