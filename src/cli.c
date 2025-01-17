#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kevs.h"
#include "util.h"

static void usage() {
  fprintf(stderr, "usage: ./kevs [-abort] [-dump] [-no-err] [-free] file\n");
}

int main(int argc, char **argv) {
  const int nargs = argc - 1;
  char **args = argv + 1;

  if (nargs < 1) {
    usage();
    return 1;
  }

  Context ctx = {};

  bool dump = false;
  bool pass_on_error = false;
  bool free_heap = false;

  int args_index = 0;
  while (args_index < nargs) {
    if (strcmp(args[args_index], "-abort") == 0) {
      ctx.abort_on_error = true;
      args_index++;
    } else if (strcmp(args[args_index], "-dump") == 0) {
      dump = true;
      args_index++;
    } else if (strcmp(args[args_index], "-no-err") == 0) {
      pass_on_error = true;
      args_index++;
    } else if (strcmp(args[args_index], "-free") == 0) {
      free_heap = true;
      args_index++;
    } else if (strlen(args[args_index]) > 0 && args[args_index][0] == '-') {
      fprintf(stderr, "error: unknown option '%s'\n", args[args_index]);
      usage();
      return 1;
    } else {
      break;
    }
  }

  if (args_index >= nargs) {
    fprintf(stderr, "error: need file\n");
    usage();
    return 1;
  }

  Str file = str_from_cstr(args[args_index]);

  char *data = NULL;
  Error err = read_file(file, &data);
  if (err != NULL) {
    fprintf(stderr, "error: failed to read file: %s\n", err);
    return 1;
  }

  int rc = 0;

  Table table = {};
  const bool ok = table_parse(&table, ctx, file, str_from_cstr(data));
  if (!ok) {
    if (!pass_on_error) {
      rc = 1;
    }
  }

  if (dump) {
    table_dump(table);
  }

  if (free_heap) {
    table_free(&table);
    free(data);
  }

  return rc;
}
