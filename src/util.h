#ifndef KEVS_UTIL_H
#define KEVS_UTIL_H

#include "kevs.h"

Str str_from_cstr(const char *s);
char *str_dup(Str self);
int str_index_char(Str self, char c);
Str str_slice_low(Str self, size_t low);
Str str_slice(Str self, size_t low, size_t high);
Str str_trim_left(Str self, Str cutset);
Str str_trim_right(Str self, Str cutset);
Error str_to_int(Str self, uint64_t base, int64_t *out);

void table_dump(Table self);
void list_dump(List self);

Error read_file(Str path, char **out);

#endif
