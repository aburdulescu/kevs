#ifndef KEVS_STR_H
#define KEVS_STR_H

#include "kevs.h"

int str_index_char(Str self, char c);
Str str_slice_low(Str self, size_t low);
Str str_slice(Str self, size_t low, size_t high);
Str str_trim_left(Str self, Str cutset);
Str str_trim_right(Str self, Str cutset);
Error str_to_int(Str self, int64_t *out);

#endif
