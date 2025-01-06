#include "kevs.h"

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  Context ctx = {};
  Str file = {};
  Str content = {.ptr = (char *)Data, .len = Size};
  Table t = {};
  table_parse(&t, ctx, file, content);
  table_free(&t);
  return 0;
}
