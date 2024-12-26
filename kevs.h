#ifndef KEVS_H
#define KEVS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef const char *Error;

typedef struct {
  const char *ptr;
  size_t len;
} Str;

typedef struct {
  char *ptr;
  size_t cap;
  size_t len;
} String;

typedef struct {
  int64_t value;
  Error err;
} StrToIntResult;

typedef struct {
  uint64_t value;
  Error err;
} StrToUintResult;

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
  bool verbose;
  bool no_parse;
} Context;

// Str
Str str_from_cstring(const char *s);
Str str_from_string(String s);
bool str_starts_with_char(Str self, char c);
int str_index_char(Str self, char c);
int str_index_chars(Str self, Str chars);
bool str_equals(Str self, Str other);
bool str_equals_char(Str self, char c);
Str str_slice_low(Str self, size_t low);
Str str_slice_high(Str self, size_t high);
Str str_slice(Str self, size_t low, size_t high);
Str str_trim_left(Str self, Str cutset);
Str str_trim_right(Str self, Str cutset);
Str str_trim(Str self, Str cutset);
StrToUintResult str_to_uint(Str self);
StrToIntResult str_to_int(Str self);

// String
void string_reserve(String *self, size_t cap);
void string_resize(String *self, size_t len);
void string_free(String *self);
String string_from_str(Str s);

// kevs
bool kevs_parse(Context ctx, Str file, Str content, Table *table);

#endif
