#include <assert.h>
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

int main(int argc, char **argv) {
  const int nargs = argc - 1;
  char **args = argv + 1;

  if (nargs < 1) {
    printf("usage: ./kevs [-abort] [-logs] file\n");
    return 1;
  }

  Context ctx = {};

  int args_index = 0;
  while (args_index < nargs) {
    if (strcmp(args[args_index], "-abort") == 0) {
      ctx.abort_on_error = true;
      args_index++;
    } else if (strcmp(args[args_index], "-logs") == 0) {
      ctx.enable_logs = true;
      args_index++;
    } else {
      break;
    }
  }

  Str file = str_from_cstring(args[args_index]);

  if (ctx.enable_logs) {
    printf("file=%s, abort=%d, logs=%d\n", file.ptr, ctx.abort_on_error,
           ctx.enable_logs);
  }

  ReadFileResult result = read_file(file);
  if (result.err != NULL) {
    fprintf(stderr, "error: failed to read file: %s\n", result.err);
    return 1;
  }

  int rc = 0;

  Table table = {};
  const bool ok = kevs_parse(ctx, file, str_from_string(result.val), &table);
  if (!ok) {
    rc = 1;
  }

  kevs_free(&table);

  string_free(&result.val);

  return rc;
}
