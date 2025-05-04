#include "kevs.h"
#include "util.h"

static char err_buf[8193] = {};

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  KevsTable t = {};

  const KevsStr content = {.ptr = (char *)Data, .len = Size};
  const KevsOpts opts = {
      .file = kevs_str_from_cstr("fuzzer"),
      .errors_with_file_and_line = true,
  };

  kevs_parse(&t, content, err_buf, sizeof(err_buf) - 1, opts);

  kevs_free(&t);

  return 0;
}
