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
          "  -help    Print this message\n"
          "  -abort   Abort when encountering an error\n"
          "  -scan    Run only the scanner\n"
          "  -dump    Print keys and values, or tokens if -scan is active\n"
          "  -no-err  Exit with code 0 even if an error was encountered\n"
          "  -free    Free memory before exit\n"

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

  int args_index = 0;
  while (args_index < nargs) {
    if (strcmp(args[args_index], "-help") == 0) {
      usage();
      return 0;
    } else if (strcmp(args[args_index], "-abort") == 0) {
      abort_on_error = true;
      args_index++;
    } else if (strcmp(args[args_index], "-scan") == 0) {
      only_scan = true;
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

  Tokens tokens = {};
  Table table = {};
  char err_buf[8193] = {};
  const Params params = {
      .file = file,
      .content = {.ptr = data, .len = data_len},
      .alls = alls,
      .err_buf = err_buf,
      .err_buf_len = sizeof(err_buf) - 1,
      .abort_on_error = abort_on_error,
  };

  err = scan(&tokens, params, &alls);
  if (!only_scan) {
    if (err == NULL) {
      err = parse(&table, params, tokens, &alls);
    }
  }

  if (err != NULL) {
    printf("%s\n", err);
    if (!pass_on_error) {
      rc = 1;
    }
  }

  if (dump) {
    const size_t len = 512 << 10;
    void *ptr = malloc(len);
    Arena a = {};
    arena_init(&a, ptr, len);
    if (only_scan) {
      for (size_t i = 0; i < tokens.len; i++) {
        char *v = str_dup(tokens.ptr[i].value, &a);
        printf("%s %s\n", tokenkind_str(tokens.ptr[i].kind), v);
      }
    } else {
      table_dump(table, &a);
    }
    free(ptr);
  }

  if (free_heap) {
    free(data);
  }

  return rc;
}
