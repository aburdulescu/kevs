#ifndef KEVS_H
#define KEVS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Error: static null terminated string
typedef const char *Error;

// Str: non owning string without null terminator
typedef struct {
  const char *ptr;
  size_t len;
} Str;

// String: owning string with null terminator
typedef struct {
  char *ptr;
  size_t cap;
  size_t len;
} String;

typedef struct KeyValue KeyValue;
typedef struct Value Value;

typedef enum {
  kValueTagUndefined = 0,
  kValueTagString,
  kValueTagInteger,
  kValueTagBoolean,
  kValueTagList,
  kValueTagTable,
} ValueTag;

typedef struct {
  Value *ptr;
  size_t cap;
  size_t len;
} List;

typedef struct {
  KeyValue *ptr;
  size_t cap;
  size_t len;
} Table;

typedef struct Value {
  ValueTag tag;
  union {
    int64_t integer;
    bool boolean;
    Str string;
    List list;
    Table table;
  } data;
} Value;

typedef struct KeyValue {
  Str key;
  Value val;
} KeyValue;

typedef struct {
  bool abort_on_error;
  bool enable_logs;
} Context;

// Str
Str str_from_cstring(const char *s);
Str str_from_string(String s);
int str_index_char(Str self, char c);
Str str_slice_low(Str self, size_t low);
Str str_slice_high(Str self, size_t high);
Str str_slice(Str self, size_t low, size_t high);
Str str_trim_left(Str self, Str cutset);
Str str_trim_right(Str self, Str cutset);
Str str_trim(Str self, Str cutset);
Error str_to_uint(Str self, uint64_t *out);
Error str_to_int(Str self, int64_t *out);

// String
String string_from_str(Str s);
void string_resize(String *self, size_t len);
void string_free(String *self);

// kevs
bool table_parse(Table *table, Context ctx, Str file, Str content);
void table_free(Table *self);
void table_dump(Table self);
Error table_get_str(Table self, const char *key, Str *out);
Error table_get_string(Table self, const char *key, String *out);
Error table_get_int(Table self, const char *key, int64_t *out);
Error table_get_bool(Table self, const char *key, bool *out);
Error table_get_list(Table self, const char *key, List *out);
Error table_get_table(Table self, const char *key, Table *out);
Error list_get_str(List self, size_t i, Str *out);
Error list_get_string(List self, size_t i, String *out);
Error list_get_int(List self, size_t i, int64_t *out);
Error list_get_bool(List self, size_t i, bool *out);
Error list_get_list(List self, size_t i, List *out);
Error list_get_table(List self, size_t i, Table *out);

// TODO: use const char* for keys?

#endif
