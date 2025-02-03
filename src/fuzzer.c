#include <stdlib.h>

#include "kevs.h"

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  Table t = {};
  char err_buf[8193] = {};
  const size_t mem_buf_len = 16 << 20;
  void *mem_buf = malloc(mem_buf_len);
  const Params params = {
      .file = str_from_cstr("fuzzer"),
      .content = {.ptr = (char *)Data, .len = Size},
      .mem_buf = mem_buf,
      .mem_buf_len = mem_buf_len,
      .err_buf = err_buf,
      .err_buf_len = sizeof(err_buf) - 1,
  };
  table_parse(&t, params);
  free(mem_buf);
  return 0;
}
