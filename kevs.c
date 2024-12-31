#include "kevs.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Context global_ctx = {};

static void kevs_logf(const char *level, const char *fn, int ln,
                      const char *fmt, ...) {
  assert(level != NULL);
  assert(fn != NULL);
  assert(fmt != NULL);

  va_list args;
  va_start(args, fmt);

  fprintf(stdout, "%s %s() %d ", level, fn, ln);
  vfprintf(stdout, fmt, args);
  fprintf(stdout, "\n");

  va_end(args);
}

#define INFO(...)                                                              \
  if (global_ctx.enable_logs) {                                                \
    kevs_logf("INFO ", __FUNCTION__, __LINE__, __VA_ARGS__);                   \
  }

#define ERROR(...)                                                             \
  if (global_ctx.enable_logs) {                                                \
    kevs_logf("ERROR", __FUNCTION__, __LINE__, __VA_ARGS__);                   \
  }

static inline bool is_digit(char c) { return c >= '0' && c <= '9'; }

static inline char lower(char c) { return (char)(c | ('x' - 'X')); }

static inline bool is_letter(char c) {
  return lower(c) >= 'a' && lower(c) <= 'z';
}

Str str_from_cstring(const char *s) {
  assert(s != NULL);
  Str self = {
      .ptr = s,
      .len = strlen(s),
  };
  return self;
}

Str str_from_string(String s) {
  Str self = {
      .ptr = s.ptr,
      .len = s.len,
  };
  return self;
}

bool str_starts_with_char(Str self, char c) {
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

int str_index_chars(Str self, Str chars) {
  for (size_t i = 0; i < self.len; i++) {
    if (str_index_char(chars, self.ptr[i]) != -1) {
      return (int)i;
    }
  }
  return -1;
}

int str_index_any(Str self, Str chars, char *c) {
  for (size_t i = 0; i < self.len; i++) {
    const int j = str_index_char(chars, self.ptr[i]);
    if (j != -1) {
      *c = chars.ptr[j];
      return (int)i;
    }
  }
  return -1;
}

bool str_equals(Str self, Str other) {
  if (self.len != other.len) {
    return false;
  }
  return memcmp(self.ptr, other.ptr, self.len) == 0;
}

bool str_equals_char(Str self, char c) {
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

Str str_slice_high(Str self, size_t high) {
  assert(self.ptr != NULL);
  assert(self.len != 0);
  assert(high <= self.len);
  self.len -= high;
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

Str str_trim(Str self, Str cutset) {
  self = str_trim_left(self, cutset);
  self = str_trim_right(self, cutset);
  return self;
}

StrToUintResult str_to_uint(Str self) {
  StrToUintResult result = {};

  if (self.len == 0) {
    result.err = "empty input";
    return result;
  }

  uint64_t base = 10;
  if (self.ptr[0] == '0') {
    // stop if 0
    if (self.len == 1) {
      result.value = 0;
      return result;
    }

    if (self.len < 3) {
      result.err = "leading 0 requires at least 2 more chars";
      return result;
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
      result.err = "invalid base char, must be 'x', 'o' or 'b'";
      return result;
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
      result.err = "invalid char, must be a letter or a digit";
      return result;
    }

    if (d >= base) {
      result.err = "invalid digit, bigger than base";
      return result;
    }

    if (n >= cutoff) {
      result.err = "invalid input, mul overflows";
      return result;
    }
    n *= base;

    const uint64_t n1 = n + d;
    if (n1 < n || n1 > max) {
      result.err = "invalid input, add overflows";
      return result;
    }
    n = n1;
  }

  result.value = n;

  return result;
}

StrToIntResult str_to_int(Str self) {
  StrToIntResult result = {};

  if (self.len == 0) {
    result.err = "empty input";
    return result;
  }

  bool neg = false;
  if (self.ptr[0] == '+') {
    self = str_slice_low(self, 1);
  } else if (self.ptr[0] == '-') {
    neg = true;
    self = str_slice_low(self, 1);
  }

  StrToUintResult uresult = str_to_uint(self);
  if (uresult.err != NULL) {
    result.err = uresult.err;
    return result;
  }

  const uint64_t un = uresult.value;
  const uint64_t max = (uint64_t)1 << 63;

  if (!neg && un >= max) {
    result.err = "invalid input, overflows max value";
    return result;
  }
  if (neg && un > max) {
    result.err = "invalid input, underflows min value";
    return result;
  }

  int64_t n = (int64_t)un;
  if (neg && n >= 0) {
    n = -n;
  }

  result.value = n;

  return result;
}

void string_reserve(String *self, size_t cap) {
  self->ptr = realloc(self->ptr, sizeof(char) * cap + 1);
  self->cap = cap;
  self->ptr[self->len] = 0;
}

void string_resize(String *self, size_t len) {
  string_reserve(self, len);
  self->len = len;
}

void string_free(String *self) {
  free(self->ptr);
  *self = (String){};
}

String string_from_str(Str s) {
  String self = {
      .ptr = malloc(s.len + 1),
      .cap = s.len,
      .len = s.len,
  };
  self.ptr[self.len] = 0;
  memcpy(self.ptr, s.ptr, s.len);
  return self;
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
  int line;
  int column;
} Position;

typedef struct {
  TokenType type;
  Str value;
  Position position;
} Token;

typedef struct {
  Token *ptr;
  size_t cap;
  size_t len;
} Tokens;

static void tokens_free(Tokens *self) {
  free(self->ptr);
  *self = (Tokens){};
}

static void tokens_add(Tokens *self, Token v) {
  if (self->len == self->cap) {
    self->cap = (self->cap + 1) * 2;
    self->ptr = realloc(self->ptr, self->cap * sizeof(v));
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
  self->content = str_trim_left(self->content, str_from_cstring(spaces));
}

static void scanner_add(Scanner *self, TokenType type, size_t end) {
  Str val = str_slice(self->content, 0, end);
  val = str_trim_right(val, str_from_cstring(spaces));

  const Token t = {
      .type = type,
      .value = val,
      .position.line = self->line,
  };

  tokens_add(self->tokens, t);

  scanner_advance(self, end);
}

static void scanner_add_delim(Scanner *self) {
  const Token t = {
      .type = kTokenDelim,
      .value = str_slice(self->content, 0, 1),
      .position.line = self->line,
  };
  tokens_add(self->tokens, t);
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
  const int end = str_index_any(self->content, str_from_cstring("=\n"), &c);
  if (end == -1 || c != kKeyValSep) {
    scan_errorf(self, "key-value pair is missing separator");
    return false;
  }
  scanner_add(self, kTokenKey, end);
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
  scanner_add_delim(self);
  return true;
}

// TODO: handle escapes and unicode
static bool scan_string_value(Scanner *self) {
  const int end = str_index_char(str_slice_low(self->content, 1), kStringBegin);
  if (end == -1) {
    scan_errorf(self, "string value does not end with quote");
    return false;
  }
  // +2 for leading and trailing quotes
  scanner_add(self, kTokenValue, end + 2);
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
  scanner_add(self, kTokenValue, end + 2);
  return true;
}

static bool scan_int_or_bool_value(Scanner *self) {
  // search for semicolon or new newline
  // if semicolon is not found or newline is found => error
  // TODO: maybe check for other delimiters? i.e. ] }
  char c = 0;
  const int end = str_index_any(self->content, str_from_cstring(";\n"), &c);
  if (end == -1 || c != kKeyValEnd) {
    scan_errorf(self, "integer or boolean value does not end with semicolon");
    return false;
  }
  scanner_add(self, kTokenValue, end);
  return true;
}

static bool scan_list_value(Scanner *self) {
  scanner_add_delim(self);
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
      scanner_add_delim(self);
      return true;
    }
    if (!scan_value(self)) {
      return false;
    }
    if (scanner_expect(self, kListEnd)) {
      scanner_add_delim(self);
      return true;
    }
  }
  return true;
}

static bool scan_table_value(Scanner *self) {
  scanner_add_delim(self);
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
      scanner_add_delim(self);
      return true;
    }
    if (!scan_key_value(self)) {
      return false;
    }
    if (scanner_expect(self, kTableEnd)) {
      scanner_add_delim(self);
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
  scanner_add_delim(self);

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

static const char *valuetag_str(ValueTag v) {
  switch (v) {
  case kValueTagUndefined:
    return "undefined";
  case kValueTagString:
    return "string";
  case kValueTagInteger:
    return "integer";
  case kValueTagBoolean:
    return "boolean";
  case kValueTagList:
    return "list";
  case kValueTagTable:
    return "table";
  default:
    return "unknown";
  }
}

static bool table_check(Table self);

static void list_free(List *self);
static void list_dump(List self);
static bool list_check(List self);

static void value_free(Value *self) {
  switch (self->tag) {
  case kValueTagList: {
    list_free(&self->data.list);
  } break;

  case kValueTagTable: {
    kevs_free(&self->data.table);
  } break;

  default:
    break;
  }

  *self = (Value){};
}

static bool value_check(Value self) {
  switch (self.tag) {
  case kValueTagList:
    return list_check(self.data.list);
  case kValueTagTable:
    return table_check(self.data.table);
  default:
    return true;
  }
}

static void list_free(List *self) {
  for (size_t i = 0; i < self->len; i++) {
    value_free(&self->ptr[i]);
  }

  free(self->ptr);
  *self = (List){};
}

static void list_add(List *self, Value v) {
  if (self->len == self->cap) {
    self->cap = (self->cap + 1) * 2;
    self->ptr = realloc(self->ptr, self->cap * sizeof(v));
  }
  memcpy(self->ptr + self->len, &v, sizeof(v));
  self->len += 1;
}

static void list_dump(List self) {
  for (size_t i = 0; i < self.len; i++) {
    const Value v = self.ptr[i];

    switch (v.tag) {
    case kValueTagTable: {
      printf("%s\n", valuetag_str(v.tag));
      kevs_dump(v.data.table);
    } break;

    case kValueTagList: {
      printf("%s\n", valuetag_str(v.tag));
      list_dump(v.data.list);
    } break;

    case kValueTagString: {
      String s = string_from_str(v.data.string);
      printf("%s '%s'\n", valuetag_str(v.tag), s.ptr);
      string_free(&s);
    } break;

    case kValueTagBoolean: {
      printf("%s %s\n", valuetag_str(v.tag),
             (v.data.boolean ? "true" : "false"));
    } break;

    case kValueTagInteger: {
      printf("%s %ld\n", valuetag_str(v.tag), v.data.integer);
    } break;

    default: {
      printf("%s\n", valuetag_str(v.tag));
    } break;
    }
  }
}

// check that the list elements have the same type
static bool list_check(List self) {
  if (self.len == 0) {
    return true;
  }
  // TODO: test with nested lists/tables
  ValueTag t = self.ptr[0].tag;
  for (size_t i = 1; i < self.len; i++) {
    if (self.ptr[i].tag != t) {
      ERROR("first element is %s, element #%zu is %s", valuetag_str(t), i,
            valuetag_str(self.ptr[i].tag));
      return false;
    }
    if (self.ptr[i].tag == kValueTagList || self.ptr[i].tag == kValueTagTable) {
      if (!value_check(self.ptr[i])) {
        return false;
      }
    }
  }
  return true;
}

static bool table_check(Table self) {
  for (size_t i = 0; i < self.len; i++) {
    // TODO: check if key is unique
    if (!value_check(self.ptr[i].val)) {
      return false;
    }
  }
  return true;
}

static void table_add(Table *self, KeyValue v) {
  if (self->len == self->cap) {
    self->cap = (self->cap + 1) * 2;
    self->ptr = realloc(self->ptr, self->cap * sizeof(v));
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
static bool parse_key_value(Parser *self, KeyValue *out);

static Token parser_get(const Parser *self) {
  return self->tokens.ptr[self->i];
}

static void parse_errorf(const Parser *self, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(stdout, "%s:%d: error: parse: ", self->file.ptr,
          parser_get(self).position.line);
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

static Token parser_pop(Parser *self) {
  Token t = parser_get(self);
  self->i++;
  return t;
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

  parser_pop(self);

  while (true) {
    if (parse_delim(self, kListEnd)) {
      return true;
    }

    Value v = {};
    if (!parse_value(self, &v)) {
      return false;
    }
    list_add(&out->data.list, v);

    if (parse_delim(self, kListEnd)) {
      return true;
    }
  }

  return true;
}

static bool parse_table_value(Parser *self, Value *out) {
  out->tag = kValueTagTable;

  parser_pop(self);

  while (true) {
    if (parse_delim(self, kTableEnd)) {
      return true;
    }

    KeyValue kv = {};
    if (!parse_key_value(self, &kv)) {
      return false;
    }
    table_add(&out->data.table, kv);

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
    if (str_equals(val, str_from_cstring("true"))) {
      out->tag = kValueTagBoolean;
      out->data.boolean = true;
    } else if (str_equals(val, str_from_cstring("false"))) {
      out->tag = kValueTagBoolean;
      out->data.boolean = false;
    } else {
      StrToIntResult res = str_to_int(val);
      if (res.err != NULL) {
        String s = string_from_str(val);
        parse_errorf(self, "value is not an integer: %s", res.err);
        string_free(&s);
        ok = false;
      } else {
        out->tag = kValueTagInteger;
        out->data.integer = res.value;
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
    value_free(out);
    ERROR("missing key value end");
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

static bool parse_key(Parser *self, Str *key) {
  if (!parser_expect(self, kTokenKey)) {
    parse_errorf(self, "expected key token");
    return false;
  }
  if (!key_is_valid(parser_get(self).value)) {
    String s = string_from_str(parser_get(self).value);
    parse_errorf(self, "key is not a valid identifier: '%s'", s.ptr);
    string_free(&s);
    return false;
  }
  *key = parser_pop(self).value;
  return true;
}

static bool parse_key_value(Parser *self, KeyValue *out) {
  Str key = {};
  if (!parse_key(self, &key)) {
    return false;
  }

  if (!parse_delim(self, kKeyValSep)) {
    ERROR("missing key value separator");
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

bool parse(Context ctx, Str file, Tokens tokens, Table *table) {
  Parser p = {
      .file = file,
      .tokens = tokens,
      .table = table,
      .i = 0,
      .ctx = ctx,
  };

  while (p.i < tokens.len) {
    KeyValue kv = {};
    if (!parse_key_value(&p, &kv)) {
      return false;
    }
    table_add(p.table, kv);
  }

  return true;
}

// TODO: check that keys are unique for each table
// TODO: values in a list have the same type? Or leave it to the user?
bool kevs_parse(Context ctx, Str file, Str content, Table *table) {
  global_ctx = ctx;

  bool ok = false;

  Tokens tokens = {};
  ok = scan(ctx, file, content, &tokens);
  if (!ok) {
    ERROR("scanner failed");
  }
  for (size_t i = 0; i < tokens.len; i++) {
    String v = string_from_str(tokens.ptr[i].value);
    INFO("%s '%s'", tokentype_str(tokens.ptr[i].type), v.ptr);
    string_free(&v);
  }

  if (ok) {
    ok = parse(ctx, file, tokens, table);
    if (!ok) {
      ERROR("parser failed");
    }
    if (!table_check(*table)) {
      ERROR("type check failed");
    }
  }

  tokens_free(&tokens);

  return ok;
}

void kevs_free(Table *self) {
  for (size_t i = 0; i < self->len; i++) {
    value_free(&self->ptr[i].val);
  }

  free(self->ptr);
  *self = (Table){};
}

void kevs_dump(Table self) {
  for (size_t i = 0; i < self.len; i++) {
    const KeyValue kv = self.ptr[i];

    String k = string_from_str(kv.key);

    switch (kv.val.tag) {
    case kValueTagTable: {
      printf("%s %s\n", k.ptr, valuetag_str(kv.val.tag));
      kevs_dump(kv.val.data.table);
    } break;

    case kValueTagList: {
      printf("%s %s\n", k.ptr, valuetag_str(kv.val.tag));
      list_dump(kv.val.data.list);
    } break;

    case kValueTagString: {
      String s = string_from_str(kv.val.data.string);
      printf("%s %s '%s'\n", k.ptr, valuetag_str(kv.val.tag), s.ptr);
      string_free(&s);
    } break;

    case kValueTagBoolean: {
      printf("%s %s %s\n", k.ptr, valuetag_str(kv.val.tag),
             (kv.val.data.boolean ? "true" : "false"));
    } break;

    case kValueTagInteger: {
      printf("%s %s %ld\n", k.ptr, valuetag_str(kv.val.tag),
             kv.val.data.integer);
    } break;

    default: {
      printf("%s %s\n", k.ptr, valuetag_str(kv.val.tag));
    } break;
    }

    string_free(&k);
  }
}
