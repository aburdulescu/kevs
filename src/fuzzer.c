#include <stdlib.h>

#include "kevs.h"

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
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

  char err_buf[8193] = {};

  Table t = {};

  const Params params = {
      .file = str_from_cstr("fuzzer"),
      .content = {.ptr = (char *)Data, .len = Size},
      .alls = alls,
      .err_buf = err_buf,
      .err_buf_len = sizeof(err_buf) - 1,
  };

  table_parse(&t, params);

  return 0;
}
