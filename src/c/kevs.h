#ifndef KEVS_H
#define KEVS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Error: static null terminated string
typedef const char *KevsError;

// Str: non owning string without null terminator
typedef struct {
  const char *ptr;
  size_t len;
} KevsStr;

typedef enum {
  KevsTokenKindUndefined = 0,
  KevsTokenKindKey,
  KevsTokenKindDelim,
  KevsTokenKindValue,
} KevsTokenKind;

typedef struct {
  KevsStr value;
  KevsTokenKind kind;
  int line;
} KevsToken;

typedef struct {
  KevsToken *ptr;
  size_t cap;
  size_t len;
} KevsTokens;

typedef enum {
  KevsValueKindUndefined = 0,
  KevsValueKindString,
  KevsValueKindInteger,
  KevsValueKindBoolean,
  KevsValueKindList,
  KevsValueKindTable,
} KevsValueKind;

struct KevsValue;

typedef struct {
  struct KevsValue *ptr;
  size_t cap;
  size_t len;
} KevsList;

struct KevsKeyValue;

typedef struct {
  struct KevsKeyValue *ptr;
  size_t cap;
  size_t len;
} KevsTable;

typedef struct KevsValue {
  union {
    int64_t integer;
    bool boolean;
    char *string;
    KevsList list;
    KevsTable table;
  } data;
  KevsValueKind kind;
} KevsValue;

typedef struct KevsKeyValue {
  KevsStr key;
  KevsValue val;
} KevsKeyValue;

typedef struct {
  KevsStr file;
  KevsStr content;
  char *err_buf;
  size_t err_buf_len;
  bool abort_on_error;
} KevsParams;

KevsStr str_from_cstr(const char *s);

KevsError kevs_parse(KevsTable *table, KevsParams params);
void kevs_free(KevsTable *self);

KevsError kevs_table_string(KevsTable self, const char *key, char **out);
KevsError kevs_table_int(KevsTable self, const char *key, int64_t *out);
KevsError kevs_table_bool(KevsTable self, const char *key, bool *out);
KevsError kevs_table_list(KevsTable self, const char *key, KevsList *out);
KevsError kevs_table_table(KevsTable self, const char *key, KevsTable *out);

KevsError kevs_list_string(KevsList self, size_t i, char **out);
KevsError kevs_list_int(KevsList self, size_t i, int64_t *out);
KevsError kevs_list_bool(KevsList self, size_t i, bool *out);
KevsError kevs_list_list(KevsList self, size_t i, KevsList *out);
KevsError kevs_list_table(KevsList self, size_t i, KevsTable *out);

#endif
