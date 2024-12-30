#include "kevs.h"

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  Context ctx = {};
  Str file = {};
  Str content = {.ptr = (char *)Data, .len = Size};
  Table t = {};
  kevs_parse(ctx, file, content, &t);
  kevs_free(&t);
  return 0;
}
