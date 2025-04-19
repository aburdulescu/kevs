#include "kevs.h"

static char err_buf[8193] = {};

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  KevsTable t = {};

  const KevsParams params = {
      .file = kevs_str_from_cstr("fuzzer"),
      .content = {.ptr = (char *)Data, .len = Size},
      .err_buf = err_buf,
      .err_buf_len = sizeof(err_buf) - 1,
  };

  kevs_table_parse(&t, params);

  kevs_table_free(&t);

  return 0;
}
