#ifndef KEVS_UTIL_H
#define KEVS_UTIL_H

#include "kevs.h"

// kevs.c

Str str_from_cstr(const char *s);
char *str_dup(Str self);
int str_index_char(Str self, char c);
Str str_slice_low(Str self, size_t low);
Str str_slice(Str self, size_t low, size_t high);
Str str_trim_left(Str self, Str cutset);
Str str_trim_right(Str self, Str cutset);
Error str_to_int(Str self, uint64_t base, int64_t *out);

int ucs_to_utf8(uint64_t code, char buf[4]);

Error scan(Tokens *tokens, Params params);
Error parse(Table *table, Params params, Tokens tokens);

const char *tokenkind_str(TokenKind v);

// util.c

void table_dump(Table self);
void list_dump(List self);
Error read_file(Str path, char **out, size_t *out_len);

#endif
