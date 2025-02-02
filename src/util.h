#ifndef KEVS_UTIL_H
#define KEVS_UTIL_H

#include "kevs.h"

// kevs.c
Str str_from_cstr(const char *s);
char *str_dup(Str self, Arena *arena);
int str_index_char(Str self, char c);
Str str_slice_low(Str self, size_t low);
Str str_slice(Str self, size_t low, size_t high);
Str str_trim_left(Str self, Str cutset);
Str str_trim_right(Str self, Str cutset);
Error str_to_int(Str self, uint64_t base, int64_t *out);

int ucs_to_utf8(uint64_t code, char buf[4]);

Error scan(Tokens *tokens, Params params);
Error parse(Table *table, Params params, Tokens tokens);

const char *tokentype_str(TokenType v);

void *arena_alloc(Arena *self, size_t size);
void *arena_extend(Arena *self, void *old_ptr, size_t old_size,
                   size_t new_size);

// util.c
void table_dump(Table self, Arena *arena);
void list_dump(List self, Arena *arena);
Error read_file(Str path, char **out, size_t *out_len);

#endif
