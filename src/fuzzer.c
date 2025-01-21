#include "kevs.h"

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  Table t = {};
  char err_buf[8193] = {};
  const Params params = {
      .file = str_from_cstr("fuzzer"),
      .content = {.ptr = (char *)Data, .len = Size},
      .err_buf = err_buf,
      .err_buf_len = sizeof(err_buf) - 1,
  };
  table_parse(&t, params);
  table_free(&t);
  return 0;
}
