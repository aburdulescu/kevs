#ifndef KEVS_UTIL_H
#define KEVS_UTIL_H

#include "kevs.h"

// kevs.c

char *str_dup(KevsStr self);
int str_index_char(KevsStr self, char c);
KevsStr str_slice_low(KevsStr self, size_t low);
KevsStr str_slice(KevsStr self, size_t low, size_t high);
KevsStr str_trim_left(KevsStr self, KevsStr cutset);
KevsStr str_trim_right(KevsStr self, KevsStr cutset);
KevsError str_to_int(KevsStr self, uint64_t base, int64_t *out);

int ucs_to_utf8(uint64_t code, char buf[4]);

KevsError scan(KevsTokens *tokens, KevsParams params);
KevsError parse(KevsTable *table, KevsParams params, KevsTokens tokens);

const char *tokenkind_str(KevsTokenKind v);

// util.c

void table_dump(KevsTable self);
void list_dump(KevsList self);
KevsError read_file(KevsStr path, char **out, size_t *out_len);

#endif
