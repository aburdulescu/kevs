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
bool scan(Tokens *tokens, Context ctx, Str file, Str content);
bool parse(Table *table, Context ctx, Str file, Tokens tokens);
const char *tokentype_str(TokenType v);

// util.c
void table_dump(Table self);
void list_dump(List self);

void table_dump_json(Table self);
void list_dump_json(List self);

Error read_file(Str path, char **out);

#endif
