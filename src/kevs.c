#include "kevs.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const size_t kDefaultCapacity = 32;

static Context global_ctx = {};

static inline bool is_digit(char c) { return c >= '0' && c <= '9'; }

static inline char lower(char c) { return (char)(c | ('x' - 'X')); }

static inline bool is_letter(char c) {
  return lower(c) >= 'a' && lower(c) <= 'z';
}

Str str_from_cstr(const char *s) {
  assert(s != NULL);
  Str self = {
      .ptr = s,
      .len = strlen(s),
  };
  return self;
}

char *str_dup(Str self) {
  char *ptr = malloc(self.len + 1);
  ptr[self.len] = 0;
  memcpy(ptr, self.ptr, self.len);
  return ptr;
}

static bool str_starts_with_char(Str self, char c) {
  if (self.len < 1) {
    return false;
  }
  return self.ptr[0] == c;
}

int str_index_char(Str self, char c) {
  const char *ptr = memchr(self.ptr, c, self.len);
  if (ptr == NULL) {
    return -1;
  }
  return (int)(ptr - self.ptr);
}

static size_t str_count_char(Str self, char c) {
  size_t count = 0;
  for (size_t i = 0; i < self.len; i++) {
    if (self.ptr[i] == c) {
      count++;
    }
  }
  return count;
}

static int str_index_any(Str self, Str chars, char *c) {
  for (size_t i = 0; i < self.len; i++) {
    const int j = str_index_char(chars, self.ptr[i]);
    if (j != -1) {
      *c = chars.ptr[j];
      return (int)i;
    }
  }
  return -1;
}

static bool str_equals(Str self, Str other) {
  if (self.len != other.len) {
    return false;
  }
  return memcmp(self.ptr, other.ptr, self.len) == 0;
}

static bool str_equals_char(Str self, char c) {
  if (self.len != 1) {
    return false;
  }
  return self.ptr[0] == c;
}

Str str_slice_low(Str self, size_t low) {
  assert(self.ptr != NULL);
  assert(self.len != 0);
  assert(low <= self.len);
  self.len -= low;
  self.ptr += low;
  return self;
}

Str str_slice(Str self, size_t low, size_t high) {
  assert(self.ptr != NULL);
  assert(self.len != 0);
  assert(low <= self.len);
  assert(high <= self.len);
  self.len = (self.ptr + high) - (self.ptr + low);
  self.ptr += low;
  return self;
}

Str str_trim_left(Str self, Str cutset) {
  size_t i = 0;
  for (; i < self.len; i++) {
    if (str_index_char(cutset, self.ptr[i]) == -1) {
      break;
    }
  }
  self.ptr += i;
  self.len -= i;
  return self;
}

Str str_trim_right(Str self, Str cutset) {
  for (int i = (int)self.len - 1; i >= 0; i--) {
    if (str_index_char(cutset, self.ptr[i]) == -1) {
      break;
    }
    self.len--;
  }
  return self;
}

static Error str_to_uint(Str self, uint64_t *out) {
  if (self.len == 0) {
    return "empty input";
  }

  uint64_t base = 10;
  if (self.ptr[0] == '0') {
    // stop if 0
    if (self.len == 1) {
      *out = 0;
      return NULL;
    }

    if (self.len < 3) {
      return "leading 0 requires at least 2 more chars";
    }

    switch (self.ptr[1]) {
    case 'x': {
      base = 16;
      self = str_slice_low(self, 2);
    } break;
    case 'o': {
      base = 8;
      self = str_slice_low(self, 2);
    } break;
    case 'b': {
      base = 2;
      self = str_slice_low(self, 2);
    } break;
    default:
      return "invalid base char, must be 'x', 'o' or 'b'";
    }
  }

  const uint64_t max = (uint64_t)(-1);

  // cutoff is the smallest number such that cutoff*base > max.
  const uint64_t cutoff = max / base + 1;

  uint64_t n = 0;
  for (size_t i = 0; i < self.len; i++) {
    const char c = self.ptr[i];

    uint64_t d = 0;
    if (is_digit(c)) {
      d = c - '0';
    } else if (is_letter(c)) {
      d = lower(c) - 'a' + 10;
    } else {
      return "invalid char, must be a letter or a digit";
    }

    if (d >= base) {
      return "invalid digit, bigger than base";
    }

    if (n >= cutoff) {
      return "invalid input, mul overflows";
    }
    n *= base;

    const uint64_t n1 = n + d;
    if (n1 < n || n1 > max) {
      return "invalid input, add overflows";
    }
    n = n1;
  }

  *out = n;

  return NULL;
}

Error str_to_int(Str self, int64_t *out) {
  if (self.len == 0) {
    return "empty input";
  }

  bool neg = false;
  if (self.ptr[0] == '+') {
    self = str_slice_low(self, 1);
  } else if (self.ptr[0] == '-') {
    neg = true;
    self = str_slice_low(self, 1);
  }

  uint64_t un = 0;
  Error err = str_to_uint(self, &un);
  if (err != NULL) {
    return err;
  }

  const uint64_t max = (uint64_t)1 << 63;

  if (!neg && un >= max) {
    return "invalid input, overflows max value";
  }
  if (neg && un > max) {
    return "invalid input, underflows min value";
  }

  int64_t n = (int64_t)un;
  if (neg && n >= 0) {
    n = -n;
  }

  *out = n;

  return NULL;
}

typedef enum {
  kTokenUndefined = 0,
  kTokenKey,
  kTokenDelim,
  kTokenValue,
} TokenType;

static const char *tokentype_str(TokenType v) {
  switch (v) {
  case kTokenUndefined:
    return "undefined";
  case kTokenKey:
    return "key";
  case kTokenDelim:
    return "delim";
  case kTokenValue:
    return "value";
  default:
    return "unknown";
  }
}

static const char kKeyValSep = '=';
static const char kKeyValEnd = ';';
static const char kCommentBegin = '#';
static const char kStringBegin = '"';
static const char kRawStringBegin = '`';
static const char kListBegin = '[';
static const char kListEnd = ']';
static const char kTableBegin = '{';
static const char kTableEnd = '}';

static const char *spaces = " \t";

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

static void tokens_reserve(Tokens *self, size_t cap) {
  self->cap = cap;
  self->ptr = realloc(self->ptr, cap * sizeof(Token));
}

static void tokens_free(Tokens *self) {
  free(self->ptr);
  *self = (Tokens){};
}

static void tokens_append(Tokens *self, Token v) {
  if (self->len == self->cap) {
    tokens_reserve(self, (self->cap + 1) * 2);
  }
  memcpy(self->ptr + self->len, &v, sizeof(v));
  self->len += 1;
}

typedef struct {
  Str file;
  Str content;
  Tokens *tokens;
  int line;
  Context ctx;
} Scanner;

static bool scan_key_value(Scanner *self);
static bool scan_value(Scanner *self);

static void scan_errorf(const Scanner *self, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(stdout, "%s:%d: error: scan: ", self->file.ptr, self->line);
  vfprintf(stdout, fmt, args);
  fprintf(stdout, "\n");
  va_end(args);
  if (self->ctx.abort_on_error) {
    abort();
  }
}

static bool scanner_expect(Scanner *self, char c) {
  if (self->content.len == 0) {
    return false;
  }
  return self->content.ptr[0] == c;
}

static void scanner_advance(Scanner *self, size_t n) {
  self->content = str_slice_low(self->content, n);
}

static void scanner_trim_space(Scanner *self) {
  self->content = str_trim_left(self->content, str_from_cstr(spaces));
}

static void scanner_append(Scanner *self, TokenType type, size_t end) {
  Str val = str_slice(self->content, 0, end);
  val = str_trim_right(val, str_from_cstr(spaces));

  const Token t = {
      .type = type,
      .value = val,
      .line = self->line,
  };

  tokens_append(self->tokens, t);

  scanner_advance(self, end);
}

static void scanner_append_delim(Scanner *self) {
  const Token t = {
      .type = kTokenDelim,
      .value = str_slice(self->content, 0, 1),
      .line = self->line,
  };
  tokens_append(self->tokens, t);
  scanner_advance(self, 1);
}

static bool scan_newline(Scanner *self) {
  self->line++;
  scanner_advance(self, 1);
  return true;
}

static bool scan_comment(Scanner *self) {
  const int newline = str_index_char(self->content, '\n');
  if (newline == -1) {
    scan_errorf(self, "comment does not end with newline");
    return false;
  }
  scanner_advance(self, newline);
  return true;
}

static bool scan_key(Scanner *self) {
  char c = 0;
  const int end = str_index_any(self->content, str_from_cstr("=\n"), &c);
  if (end == -1 || c != kKeyValSep) {
    scan_errorf(self, "key-value pair is missing separator");
    return false;
  }
  scanner_append(self, kTokenKey, end);
  if (self->tokens->ptr[self->tokens->len - 1].value.len == 0) {
    scan_errorf(self, "empty key");
    return false;
  }
  return true;
}

static bool scan_delim(Scanner *self, char c) {
  if (!scanner_expect(self, c)) {
    return false;
  }
  scanner_append_delim(self);
  return true;
}

static bool scan_string_value(Scanner *self) {
  const int end = str_index_char(str_slice_low(self->content, 1), kStringBegin);
  if (end == -1) {
    scan_errorf(self, "string value does not end with quote");
    return false;
  }
  // +2 for leading and trailing quotes
  scanner_append(self, kTokenValue, end + 2);
  return true;
}

static bool scan_raw_string(Scanner *self) {
  const int end =
      str_index_char(str_slice_low(self->content, 1), kRawStringBegin);
  if (end == -1) {
    scan_errorf(self, "raw string value does not end with backtick");
    return false;
  }

  // +2 for leading and trailing quotes
  scanner_append(self, kTokenValue, end + 2);

  // count newlines in raw string to keep line count accurate
  self->line +=
      (int)str_count_char(self->tokens->ptr[self->tokens->len - 1].value, '\n');

  return true;
}

static bool scan_int_or_bool_value(Scanner *self) {
  // search for all possible value endings(;]}\n)
  // if semicolon(or none of them) is not found => error
  char c = 0;
  const int end = str_index_any(self->content, str_from_cstr(";]}\n"), &c);
  if (end == -1 || c != kKeyValEnd) {
    scan_errorf(self, "integer or boolean value does not end with semicolon");
    return false;
  }
  scanner_append(self, kTokenValue, end);
  return true;
}

static bool scan_list_value(Scanner *self) {
  scanner_append_delim(self);
  while (true) {
    scanner_trim_space(self);
    if (self->content.len == 0) {
      scan_errorf(self, "end of input without list end");
      return false;
    }
    if (scanner_expect(self, '\n')) {
      if (!scan_newline(self)) {
        return false;
      }
      continue;
    }
    if (scanner_expect(self, kCommentBegin)) {
      if (!scan_comment(self)) {
        return false;
      }
      continue;
    }
    if (scanner_expect(self, kListEnd)) {
      scanner_append_delim(self);
      return true;
    }
    if (!scan_value(self)) {
      return false;
    }
    if (scanner_expect(self, kListEnd)) {
      scanner_append_delim(self);
      return true;
    }
  }
  return true;
}

static bool scan_table_value(Scanner *self) {
  scanner_append_delim(self);
  while (true) {
    scanner_trim_space(self);
    if (self->content.len == 0) {
      scan_errorf(self, "end of input without table end");
      return false;
    }
    if (scanner_expect(self, '\n')) {
      if (!scan_newline(self)) {
        return false;
      }
      continue;
    }
    if (scanner_expect(self, kCommentBegin)) {
      if (!scan_comment(self)) {
        return false;
      }
      continue;
    }
    if (scanner_expect(self, kTableEnd)) {
      scanner_append_delim(self);
      return true;
    }
    if (!scan_key_value(self)) {
      return false;
    }
    if (scanner_expect(self, kTableEnd)) {
      scanner_append_delim(self);
      return true;
    }
  }
  return true;
}

static bool scan_value(Scanner *self) {
  scanner_trim_space(self);
  bool ok = false;
  if (scanner_expect(self, kListBegin)) {
    ok = scan_list_value(self);
  } else if (scanner_expect(self, kTableBegin)) {
    ok = scan_table_value(self);
  } else if (scanner_expect(self, kStringBegin)) {
    ok = scan_string_value(self);
  } else if (scanner_expect(self, kRawStringBegin)) {
    ok = scan_raw_string(self);
  } else {
    ok = scan_int_or_bool_value(self);
  }
  if (!ok) {
    return false;
  }
  if (!scan_delim(self, kKeyValEnd)) {
    scan_errorf(self, "value does not end with semicolon");
    return false;
  }
  return true;
}

static bool scan_key_value(Scanner *self) {
  if (!scan_key(self)) {
    return false;
  }

  // separator check done in scan_key, no need to check again
  scanner_append_delim(self);

  if (!scan_value(self)) {
    return false;
  }

  return true;
}

static bool scan(Context ctx, Str file, Str content, Tokens *tokens) {
  Scanner s = {
      .file = file,
      .content = content,
      .tokens = tokens,
      .line = 1,
      .ctx = ctx,
  };
  while (s.content.len != 0) {
    scanner_trim_space(&s);
    bool ok = false;
    if (scanner_expect(&s, '\n')) {
      ok = scan_newline(&s);
    } else if (scanner_expect(&s, kCommentBegin)) {
      ok = scan_comment(&s);
    } else {
      ok = scan_key_value(&s);
    }
    if (!ok) {
      return false;
    }
  }
  return true;
}

static void list_free(List *self);

static void value_free(Value *self) {
  switch (self->tag) {
  case kValueTagList: {
    list_free(&self->data.list);
  } break;

  case kValueTagTable: {
    table_free(&self->data.table);
  } break;

  default:
    break;
  }

  *self = (Value){};
}

static void list_reserve(List *self, size_t cap) {
  self->cap = cap;
  self->ptr = realloc(self->ptr, cap * sizeof(Value));
}

static void list_free(List *self) {
  for (size_t i = 0; i < self->len; i++) {
    value_free(&self->ptr[i]);
  }

  free(self->ptr);
  *self = (List){};
}

static void list_append(List *self, Value v) {
  if (self->len == self->cap) {
    list_reserve(self, (self->cap + 1) * 2);
  }
  memcpy(self->ptr + self->len, &v, sizeof(v));
  self->len += 1;
}

static void table_reserve(Table *self, size_t cap) {
  self->cap = cap;
  self->ptr = realloc(self->ptr, cap * sizeof(KeyValue));
}

static void table_append(Table *self, KeyValue v) {
  if (self->len == self->cap) {
    table_reserve(self, (self->cap + 1) * 2);
  }
  memcpy(self->ptr + self->len, &v, sizeof(v));
  self->len += 1;
}

typedef struct {
  Str file;
  Tokens tokens;
  Table *table;
  size_t i;
  Context ctx;
} Parser;

static bool parse_value(Parser *self, Value *out);
static bool parse_key_value(Parser *self, Table parent, KeyValue *out);

static Token parser_get(const Parser *self) {
  return self->tokens.ptr[self->i];
}

static void parser_pop(Parser *self) { self->i++; }

static void parse_errorf(const Parser *self, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(stdout, "%s:%d: error: parse: ", self->file.ptr,
          parser_get(self).line);
  vfprintf(stdout, fmt, args);
  fprintf(stdout, "\n");
  va_end(args);
  if (self->ctx.abort_on_error) {
    abort();
  }
}

static bool parser_expect(const Parser *self, TokenType t) {
  if (self->i >= self->tokens.len) {
    parse_errorf(self, "expected token '%s', have nothing", tokentype_str(t));
    return false;
  }
  return parser_get(self).type == t;
}

static bool parser_expect_delim(const Parser *self, char delim) {
  if (!parser_expect(self, kTokenDelim)) {
    return false;
  }
  return str_equals_char(parser_get(self).value, delim);
}

static bool parse_delim(Parser *self, char c) {
  if (!parser_expect_delim(self, c)) {
    return false;
  }
  parser_pop(self);
  return true;
}

static bool parse_list_value(Parser *self, Value *out) {
  out->tag = kValueTagList;

  // pre-aloc some memory
  list_reserve(&out->data.list, kDefaultCapacity);

  parser_pop(self);

  while (true) {
    if (parse_delim(self, kListEnd)) {
      return true;
    }

    Value v = {};
    if (!parse_value(self, &v)) {
      return false;
    }
    list_append(&out->data.list, v);

    if (parse_delim(self, kListEnd)) {
      return true;
    }
  }

  return true;
}

static bool parse_table_value(Parser *self, Value *out) {
  out->tag = kValueTagTable;

  // pre-alloc some memory
  table_reserve(&out->data.table, kDefaultCapacity);

  parser_pop(self);

  while (true) {
    if (parse_delim(self, kTableEnd)) {
      return true;
    }

    KeyValue kv = {};
    if (!parse_key_value(self, out->data.table, &kv)) {
      return false;
    }
    table_append(&out->data.table, kv);

    if (parse_delim(self, kTableEnd)) {
      return true;
    }
  }

  return true;
}

static bool parse_simple_value(Parser *self, Value *out) {
  if (!parser_expect(self, kTokenValue)) {
    parse_errorf(self, "expected value token");
    return false;
  }

  const Str val = parser_get(self).value;

  bool ok = true;

  if (str_starts_with_char(val, kStringBegin) ||
      str_starts_with_char(val, kRawStringBegin)) {
    out->tag = kValueTagString;
    out->data.string = str_slice(val, 1, val.len - 1);
  } else {
    if (str_equals(val, str_from_cstr("true"))) {
      out->tag = kValueTagBoolean;
      out->data.boolean = true;
    } else if (str_equals(val, str_from_cstr("false"))) {
      out->tag = kValueTagBoolean;
      out->data.boolean = false;
    } else {
      int64_t i = 0;
      Error err = str_to_int(val, &i);
      if (err != NULL) {
        char *s = str_dup(val);
        parse_errorf(self, "value '%s' is not an integer: %s", s, err);
        free(s);
        ok = false;
      } else {
        out->tag = kValueTagInteger;
        out->data.integer = i;
      }
    }
  }

  parser_pop(self);

  return ok;
}

static bool parse_value(Parser *self, Value *out) {
  bool ok = false;
  if (parser_expect_delim(self, kListBegin)) {
    ok = parse_list_value(self, out);
  } else if (parser_expect_delim(self, kTableBegin)) {
    ok = parse_table_value(self, out);
  } else {
    ok = parse_simple_value(self, out);
  }
  if (!ok) {
    value_free(out);
    return false;
  }

  if (!parse_delim(self, kKeyValEnd)) {
    parse_errorf(self, "missing key value end");
    value_free(out);
    return false;
  }

  return true;
}

static bool key_is_valid(Str key) {
  char c = key.ptr[0];
  if (c != '_' && !is_letter(c)) {
    return false;
  }
  for (size_t i = 1; i < key.len; i++) {
    if (!is_digit(key.ptr[i]) && !is_letter(key.ptr[i]) && key.ptr[i] != '_') {
      return false;
    }
  }
  return true;
}

static bool parse_key(Parser *self, Table parent, Str *key) {
  if (!parser_expect(self, kTokenKey)) {
    parse_errorf(self, "expected key token");
    return false;
  }

  const Token tok = parser_get(self);

  if (!key_is_valid(tok.value)) {
    char *s = str_dup(tok.value);
    parse_errorf(self, "key is not a valid identifier: '%s'", s);
    free(s);
    return false;
  }

  // check if key is unique
  Str temp = tok.value;
  bool is_unique = true;
  {
    for (size_t i = 0; i < parent.len; i++) {
      if (str_equals(parent.ptr[i].key, temp)) {
        is_unique = false;
        break;
      }
    }
  }
  if (!is_unique) {
    char *s = str_dup(temp);
    parse_errorf(self, "key '%s' is not unique for current table", s);
    free(s);
    return false;
  }

  *key = temp;

  parser_pop(self);

  return true;
}

static bool parse_key_value(Parser *self, Table parent, KeyValue *out) {
  Str key = {};
  if (!parse_key(self, parent, &key)) {
    return false;
  }

  if (!parse_delim(self, kKeyValSep)) {
    parse_errorf(self, "missing key value separator");
    return false;
  }

  Value val = {};
  if (!parse_value(self, &val)) {
    return false;
  }

  out->key = key;
  out->val = val;

  return true;
}

static bool parse(Context ctx, Str file, Tokens tokens, Table *table) {
  Parser p = {
      .file = file,
      .tokens = tokens,
      .table = table,
      .i = 0,
      .ctx = ctx,
  };

  table_reserve(table, kDefaultCapacity);

  while (p.i < tokens.len) {
    KeyValue kv = {};
    if (!parse_key_value(&p, *table, &kv)) {
      return false;
    }
    table_append(p.table, kv);
  }

  return true;
}

bool table_parse(Table *table, Context ctx, Str file, Str content) {
  global_ctx = ctx;

  bool ok = false;

  Tokens tokens = {};

  // pre-aloc some memory
  tokens_reserve(&tokens, kDefaultCapacity);

  ok = scan(ctx, file, content, &tokens);
  if (ok) {
    ok = parse(ctx, file, tokens, table);
  }

  tokens_free(&tokens);

  return ok;
}

void table_free(Table *self) {
  for (size_t i = 0; i < self->len; i++) {
    value_free(&self->ptr[i].val);
  }

  free(self->ptr);
  *self = (Table){};
}

static bool value_is(Value self, ValueTag tag) { return self.tag == tag; }

static Error table_get(Table self, const char *key, Value *val) {
  Str key_str = str_from_cstr(key);
  for (size_t i = 0; i < self.len; i++) {
    if (str_equals(self.ptr[i].key, key_str)) {
      *val = self.ptr[i].val;
      return NULL;
    }
  }
  return "key not found";
}

Error table_string(Table self, const char *key, Str *out) {
  Value val = {};
  Error err = table_get(self, key, &val);
  if (err != NULL) {
    return err;
  }
  if (!value_is(val, kValueTagString)) {
    return "value is not string";
  }
  *out = val.data.string;
  return NULL;
}

Error table_int(Table self, const char *key, int64_t *out) {
  Value val = {};
  Error err = table_get(self, key, &val);
  if (err != NULL) {
    return err;
  }
  if (!value_is(val, kValueTagInteger)) {
    return "value is not integer";
  }
  *out = val.data.integer;
  return NULL;
}

Error table_bool(Table self, const char *key, bool *out) {
  Value val = {};
  Error err = table_get(self, key, &val);
  if (err != NULL) {
    return err;
  }
  if (!value_is(val, kValueTagBoolean)) {
    return "value is not boolean";
  }
  *out = val.data.boolean;
  return NULL;
}

Error table_list(Table self, const char *key, List *out) {
  Value val = {};
  Error err = table_get(self, key, &val);
  if (err != NULL) {
    return err;
  }
  if (!value_is(val, kValueTagList)) {
    return "value is not list";
  }
  *out = val.data.list;
  return NULL;
}

Error table_table(Table self, const char *key, Table *out) {
  Value val = {};
  Error err = table_get(self, key, &val);
  if (err != NULL) {
    return err;
  }
  if (!value_is(val, kValueTagTable)) {
    return "value is not table";
  }
  *out = val.data.table;
  return NULL;
}

static Error list_get(List self, size_t i, Value *val) {
  if (i >= self.len) {
    return "index out of bounds";
  }
  *val = self.ptr[i];
  return NULL;
}

Error list_string(List self, size_t i, Str *out) {
  Value val = {};
  Error err = list_get(self, i, &val);
  if (err != NULL) {
    return err;
  }
  if (!value_is(val, kValueTagString)) {
    return "value is not string";
  }
  *out = val.data.string;
  return NULL;
}

Error list_int(List self, size_t i, int64_t *out) {
  Value val = {};
  Error err = list_get(self, i, &val);
  if (err != NULL) {
    return err;
  }
  if (!value_is(val, kValueTagInteger)) {
    return "value is not integer";
  }
  *out = val.data.integer;
  return NULL;
}

Error list_bool(List self, size_t i, bool *out) {
  Value val = {};
  Error err = list_get(self, i, &val);
  if (err != NULL) {
    return err;
  }
  if (!value_is(val, kValueTagBoolean)) {
    return "value is not boolean";
  }
  *out = val.data.boolean;
  return NULL;
}

Error list_list(List self, size_t i, List *out) {
  Value val = {};
  Error err = list_get(self, i, &val);
  if (err != NULL) {
    return err;
  }
  if (!value_is(val, kValueTagList)) {
    return "value is not list";
  }
  *out = val.data.list;
  return NULL;
}

Error list_table(List self, size_t i, Table *out) {
  Value val = {};
  Error err = list_get(self, i, &val);
  if (err != NULL) {
    return err;
  }
  if (!value_is(val, kValueTagTable)) {
    return "value is not table";
  }
  *out = val.data.table;
  return NULL;
}
