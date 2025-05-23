#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kevs.h"
#include "util.h"

static void usage() {
  fprintf(stderr,

          "usage: kevs [FLAGS] file\n"
          "\n"
          "Parse the given KEVS file and perform actions based on the given "
          "flags.\n"
          "\n"
          "Flags:\n"
          "  -help       Print this message\n"
          "  -abort      Abort when encountering an error\n"
          "  -scan       Run only the scanner\n"
          "  -dump       Print keys and values, or tokens if -scan is active\n"
          "  -no-err     Exit with code 0 even if an error was encountered\n"
          "  -free       Free memory before exit\n"
          "  -no-file    Don't print file:line for error\n"

  );
}

int main(int argc, char **argv) {
  const int nargs = argc - 1;
  char **args = argv + 1;

  if (nargs < 1) {
    usage();
    return 1;
  }

  bool only_scan = false;
  bool dump = false;
  bool pass_on_error = false;
  bool free_heap = false;
  bool abort_on_error = false;
  bool errors_with_file_and_line = true;

  int args_index = 0;
  while (args_index < nargs) {
    if (strcmp(args[args_index], "--help") == 0 ||
        strcmp(args[args_index], "-help") == 0) {
      usage();
      return 0;
    } else if (strcmp(args[args_index], "--abort") == 0 ||
               strcmp(args[args_index], "-abort") == 0) {
      abort_on_error = true;
      args_index++;
    } else if (strcmp(args[args_index], "--scan") == 0 ||
               strcmp(args[args_index], "-scan") == 0) {
      only_scan = true;
      args_index++;
    } else if (strcmp(args[args_index], "--dump") == 0 ||
               strcmp(args[args_index], "-dump") == 0) {
      dump = true;
      args_index++;
    } else if (strcmp(args[args_index], "--no-err") == 0 ||
               strcmp(args[args_index], "-no-err") == 0) {
      pass_on_error = true;
      args_index++;
    } else if (strcmp(args[args_index], "--free") == 0 ||
               strcmp(args[args_index], "-free") == 0) {
      free_heap = true;
      args_index++;
    } else if (strcmp(args[args_index], "--no-file") == 0 ||
               strcmp(args[args_index], "-no-file") == 0) {
      errors_with_file_and_line = false;
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

  KevsStr file = kevs_str_from_cstr(args[args_index]);

  char *data = NULL;
  size_t data_len = 0;
  KevsError err = read_file(file, &data, &data_len);
  if (err != NULL) {
    fprintf(stderr, "error: failed to read file: %s\n", err);
    return 1;
  }

  int rc = 0;

  KevsTokens tokens = {};
  KevsTable table = {};
  char err_buf[8193] = {};
  const KevsStr content = {.ptr = data, .len = data_len};
  const KevsOpts opts = {
      .file = file,
      .abort_on_error = abort_on_error,
      .errors_with_file_and_line = errors_with_file_and_line,
  };

  err = scan(&tokens, content, err_buf, sizeof(err_buf) - 1, opts);
  if (!only_scan) {
    if (err == NULL) {
      err = parse(&table, content, err_buf, sizeof(err_buf) - 1, opts, tokens);
    }
  }

  if (err != NULL) {
    printf("error: %s\n", err);
    if (!pass_on_error) {
      rc = 1;
    }
  }

  if (dump) {
    if (only_scan) {
      for (size_t i = 0; i < tokens.len; i++) {
        char *v = kevs_str_dup(tokens.ptr[i].value);
        printf("%s %s\n", tokenkind_str(tokens.ptr[i].kind), v);
        free(v);
      }
    } else {
      table_dump(table);
    }
  }

  if (free_heap) {
    kevs_free(&table);
    free(tokens.ptr);
    free(data);
  }

  return rc;
}
