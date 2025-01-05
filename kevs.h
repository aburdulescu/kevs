#ifndef KEVS_H
#define KEVS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Error: static null terminated string
typedef const char *Error;

// Str: non owning string, like Go/Rust Slice/C++ std::string_view
typedef struct {
  const char *ptr;
  size_t len;
} Str;

// String: owning string, like Rust String/C++ std::string
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
StrToUintResult str_to_uint(Str self);
StrToIntResult str_to_int(Str self);

// String
String string_from_str(Str s);
void string_resize(String *self, size_t len);
void string_free(String *self);

// kevs
bool kevs_parse(Context ctx, Str file, Str content, Table *table);
void kevs_free(Table *self);
void kevs_dump(Table self);
Error kevs_get_str(Table self, Str key, Str *out);
Error kevs_get_string(Table self, Str key, String *out);

#endif
