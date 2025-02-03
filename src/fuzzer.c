#include <stdlib.h>

#include "kevs.h"

const size_t tables_buf_len = 5000 * sizeof(KeyValue);
const size_t lists_buf_len = 5000 * sizeof(Value);
const size_t tokens_buf_len = 5000 * sizeof(Token);
const size_t strings_buf_len = 16 << 10;

static char err_buf[8193] = {};

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  KeyValue *tables_buf = malloc(tables_buf_len);
  Value *lists_buf = malloc(lists_buf_len);
  Token *tokens_buf = malloc(tokens_buf_len);
  char *strings_buf = malloc(strings_buf_len);

  Allocators alls = {};

  arena_init(&alls.tables, tables_buf, tables_buf_len);
  arena_init(&alls.lists, lists_buf, lists_buf_len);
  arena_init(&alls.tokens, tokens_buf, tokens_buf_len);
  arena_init(&alls.strings, strings_buf, strings_buf_len);

  Table t = {};

  const Params params = {
      .file = str_from_cstr("fuzzer"),
      .content = {.ptr = (char *)Data, .len = Size},
      .alls = alls,
      .err_buf = err_buf,
      .err_buf_len = sizeof(err_buf) - 1,
  };

  table_parse(&t, params);

  free(tables_buf);
  free(lists_buf);
  free(tokens_buf);
  free(strings_buf);

  return 0;
}
