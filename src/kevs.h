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

typedef enum {
  kTokenUndefined = 0,
  kTokenKey,
  kTokenDelim,
  kTokenValue,
} TokenType;

typedef struct {
  Str value;
  TokenType type;
  int line;
} Token;

typedef struct {
  Token *ptr;
  size_t cap;
  size_t len;
} Tokens;

typedef enum {
  kValueTagUndefined = 0,
  kValueTagString,
  kValueTagInteger,
  kValueTagBoolean,
  kValueTagList,
  kValueTagTable,
} ValueTag;

struct Value;

typedef struct {
  struct Value *ptr;
  size_t cap;
  size_t len;
} List;

struct KeyValue;

typedef struct {
  struct KeyValue *ptr;
  size_t cap;
  size_t len;
} Table;

typedef struct Value {
  union {
    int64_t integer;
    bool boolean;
    char *string;
    List list;
    Table table;
  } data;
  ValueTag tag;
} Value;

typedef struct KeyValue {
  Str key;
  Value val;
} KeyValue;

typedef struct {
  void *ptr;
  size_t cap;
  size_t index;
} Arena;

typedef struct {
  Arena tables;
  Arena lists;
  Arena tokens;
  Arena strings;
} Allocators;

typedef struct {
  Allocators alls;
  Str file;
  Str content;
  char *err_buf;
  size_t err_buf_len;
  bool abort_on_error;
} Params;

Str str_from_cstr(const char *s);

void arena_init(Arena *self, void *ptr, size_t len);

Error table_parse(Table *table, Params params);

Error table_string(Table self, const char *key, char **out);
Error table_int(Table self, const char *key, int64_t *out);
Error table_bool(Table self, const char *key, bool *out);
Error table_list(Table self, const char *key, List *out);
Error table_table(Table self, const char *key, Table *out);

Error list_string(List self, size_t i, char **out);
Error list_int(List self, size_t i, int64_t *out);
Error list_bool(List self, size_t i, bool *out);
Error list_list(List self, size_t i, List *out);
Error list_table(List self, size_t i, Table *out);

#endif
