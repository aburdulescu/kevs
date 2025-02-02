#include "kevs.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  Arena tables;
  Arena lists;
  Arena tokens;
  Arena strings;
} Allocators;

void arena_init(Arena *self, void *ptr, size_t len) {
  *self = (Arena){};
  self->ptr = ptr;
  self->cap = len;
}

void *arena_alloc(Arena *self, size_t size) {
  assert((self->index + size) < self->cap);
  void *ptr = self->ptr + self->index;
  self->index += size;
  fprintf(stderr, "%s: ptr=%p size=%zu index=%zu\n", __FUNCTION__, ptr, size,
          self->index);
  return ptr;
}

void *arena_extend(Arena *self, void *old_ptr, size_t old_size,
                   size_t new_size) {
  assert(new_size > old_size);
  if (old_ptr == NULL) {
    return arena_alloc(self, new_size);
  }
  // check if is last alloc
  if (old_ptr == (self->ptr + self->index - old_size)) {
    // update index
    const size_t new_index = self->index - old_size + new_size;
    assert(new_index < self->cap);
    self->index = new_index;
    fprintf(stderr, "%s: ptr=%p old_size=%zu new_size=%zu index=%zu\n",
            __FUNCTION__, old_ptr, old_size, new_size, self->index);
    return old_ptr;
  } else {
    // new alloc + copy
    void *new_ptr = arena_alloc(self, new_size);
    memcpy(new_ptr, old_ptr, old_size);
    fprintf(stderr, "%s: ptr=%p old_size=%zu new_size=%zu index=%zu\n",
            __FUNCTION__, new_ptr, old_size, new_size, self->index);
    return new_ptr;
  }
}

static inline bool is_digit(char c) { return c >= '0' && c <= '9'; }

static inline char lower(char c) { return (char)(c | ('x' - 'X')); }

static inline bool is_letter(char c) {
  return lower(c) >= 'a' && lower(c) <= 'z';
}

typedef struct {
  Error err;
  size_t n;
} FormatResult;

typedef struct {
  char *ptr;
  size_t cap;
  size_t len;
} String;

static void string_reserve(String *self, size_t cap, Arena *arena) {
  char *ptr = arena_extend(arena, self->ptr, self->cap * sizeof(char) + 1,
                           cap * sizeof(char) + 1);
  assert(ptr != NULL);
  self->cap = cap;
  self->ptr = ptr;
  self->ptr[self->len] = 0;
}

static void string_append(String *self, char v, Arena *arena) {
  if (self->len == self->cap) {
    string_reserve(self, (self->cap + 1) * 2, arena);
  }
  memcpy(self->ptr + self->len, &v, sizeof(v));
  self->len += 1;
  self->ptr[self->len] = 0;
}

Str str_from_cstr(const char *s) {
  assert(s != NULL);
  Str self = {
      .ptr = s,
      .len = strlen(s),
  };
  return self;
}

char *str_dup(Str self, Arena *arena) {
  char *ptr = arena_alloc(arena, self.len + 1);
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

static Error str_to_uint(Str self, uint64_t base, uint64_t *out) {
  if (self.len == 0) {
    return "empty input";
  }

  if (base == 0) {
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
    } else {
      base = 10;
    }
  } else {
    if (base != 2 && base != 8 && base != 16) {
      return "invalid base";
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

Error str_to_int(Str self, uint64_t base, int64_t *out) {
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
  Error err = str_to_uint(self, base, &un);
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

// Convert UCS code point to UTF-8
uint8_t ucs_to_utf8(uint64_t code, char out[4]) {
  // Code points in the surrogate range are not valid for UTF-8.
  if (0xd800 <= code && code <= 0xdfff) {
    return 0;
  }

  // 0x00000000 - 0x0000007F:
  // 0xxxxxxx
  if (code <= 0x0000007F) {
    out[0] = (char)code;
    return 1;
  }

  // 0x00000080 - 0x000007FF:
  // 110xxxxx 10xxxxxx
  if (code <= 0x000007FF) {
    out[0] = (char)(0xc0 | (code >> 6));
    out[1] = (char)(0x80 | (code & 0x3f));
    return 2;
  }

  // 0x00000800 - 0x0000FFFF:
  // 1110xxxx 10xxxxxx 10xxxxxx
  if (code <= 0x0000FFFF) {
    out[0] = (char)(0xe0 | (code >> 12));
    out[1] = (char)(0x80 | ((code >> 6) & 0x3f));
    out[2] = (char)(0x80 | (code & 0x3f));
    return 3;
  }

  // 0x00010000 - 0x0010FFFF:
  // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
  if (code <= 0x0010FFFF) {
    out[0] = (char)(0xf0 | (code >> 18));
    out[1] = (char)(0x80 | ((code >> 12) & 0x3f));
    out[2] = (char)(0x80 | ((code >> 6) & 0x3f));
    out[3] = (char)(0x80 | (code & 0x3f));
    return 4;
  }

  return 0;
}

static Error str_norm(Str self, Arena *arena, char **out) {
  String dst = {};
  string_reserve(&dst, self.len, arena);

  for (size_t i = 0; i < self.len;) {
    if (self.ptr[i] == '\\') {
      i++;
      switch (self.ptr[i]) {
      case 'a': {
        string_append(&dst, '\a', arena);
        i++;
      } break;
      case 'b': {
        string_append(&dst, '\b', arena);
        i++;
      } break;
      case 'f': {
        string_append(&dst, '\f', arena);
        i++;
      } break;
      case 'n': {
        string_append(&dst, '\n', arena);
        i++;
      } break;
      case 'r': {
        string_append(&dst, '\r', arena);
        i++;
      } break;
      case 't': {
        string_append(&dst, '\t', arena);
        i++;
      } break;
      case 'v': {
        string_append(&dst, '\v', arena);
        i++;
      } break;
      case '"': {
        string_append(&dst, '"', arena);
        i++;
      } break;
      case '\\': {
        string_append(&dst, '\\', arena);
        i++;
      } break;
      case 'u': {
        i++;

        if ((i + 4) > self.len) {
          return "\\u must be followed by 4 hex digits: \\uXXXX";
        }

        uint64_t code = 0;
        const Error err = str_to_uint(str_slice(self, i, i + 4), 16, &code);
        if (err != NULL) {
          return err;
        }
        i += 4;

        char utf8[4] = {};
        const int n = ucs_to_utf8(code, utf8);
        if (n == 0) {
          return "could not encode Unicode code point to UTF-8";
        }

        for (int i = 0; i < n; i++) {
          string_append(&dst, utf8[i], arena);
        }
      } break;
      case 'U': {
        i++;

        if ((i + 8) > self.len) {
          return "\\U must be followed by 8 hex digits: \\UXXXXXXXX";
        }

        uint64_t code = 0;
        const Error err = str_to_uint(str_slice(self, i, i + 8), 16, &code);
        if (err != NULL) {
          return err;
        }
        i += 8;

        char utf8[4] = {};
        const int n = ucs_to_utf8(code, utf8);
        if (n == 0) {
          return "could not encode Unicode code point to UTF-8";
        }

        for (int i = 0; i < n; i++) {
          string_append(&dst, utf8[i], arena);
        }
      } break;
      default: {
        return "unknown escape sequence";
      }
      }
    } else {
      string_append(&dst, self.ptr[i], arena);
      i++;
    }
  }

  *out = dst.ptr;

  return NULL;
}

const char *tokentype_str(TokenType v) {
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

static void tokens_reserve(Tokens *self, size_t cap, Arena *arena) {
  Token *ptr = arena_extend(arena, self->ptr, self->cap * sizeof(Token),
                            cap * sizeof(Token));
  assert(ptr != NULL);
  self->cap = cap;
  self->ptr = ptr;
}

static void tokens_append(Tokens *self, Token v, Arena *arena) {
  if (self->len == self->cap) {
    tokens_reserve(self, (self->cap + 1) * 2, arena);
  }
  memcpy(self->ptr + self->len, &v, sizeof(v));
  self->len += 1;
}

typedef struct {
  Params params;
  Allocators *alls;
  Tokens *tokens;
  int line;
} Scanner;

static bool scan_key_value(Scanner *self);
static bool scan_value(Scanner *self);

static void scan_errorf(const Scanner *self, const char *fmt, ...) {
  char *ptr = self->params.err_buf;
  size_t len = self->params.err_buf_len;

  int n = snprintf(ptr, len, "%s:%d: error: scan: ", self->params.file.ptr,
                   self->line);

  assert(n >= 0);
  assert((size_t)n < len);
  ptr += n;
  len -= n;

  va_list args;
  va_start(args, fmt);
  n = vsnprintf(ptr, len, fmt, args);
  va_end(args);

  assert(n >= 0);
  assert((size_t)n < len);
  ptr += n;
  len -= n;

  if (self->params.abort_on_error) {
    printf("%s\n", self->params.err_buf);
    abort();
  }
}

static bool scanner_expect(Scanner *self, char c) {
  if (self->params.content.len == 0) {
    return false;
  }
  return self->params.content.ptr[0] == c;
}

static void scanner_advance(Scanner *self, size_t n) {
  self->params.content = str_slice_low(self->params.content, n);
}

static void scanner_trim_space(Scanner *self) {
  self->params.content =
      str_trim_left(self->params.content, str_from_cstr(spaces));
}

static void scanner_append(Scanner *self, TokenType type, size_t end) {
  Str val = str_slice(self->params.content, 0, end);
  val = str_trim_right(val, str_from_cstr(spaces));

  const Token t = {
      .type = type,
      .value = val,
      .line = self->line,
  };

  tokens_append(self->tokens, t, &self->alls->tokens);

  scanner_advance(self, end);
}

static void scanner_append_delim(Scanner *self) {
  const Token t = {
      .type = kTokenDelim,
      .value = str_slice(self->params.content, 0, 1),
      .line = self->line,
  };
  tokens_append(self->tokens, t, &self->alls->tokens);
  scanner_advance(self, 1);
}

static bool scan_newline(Scanner *self) {
  self->line++;
  scanner_advance(self, 1);
  return true;
}

static bool scan_comment(Scanner *self) {
  const int newline = str_index_char(self->params.content, '\n');
  if (newline == -1) {
    scan_errorf(self, "comment does not end with newline");
    return false;
  }
  scanner_advance(self, newline);
  return true;
}

static bool scan_key(Scanner *self) {
  char c = 0;
  const int end = str_index_any(self->params.content, str_from_cstr("=\n"), &c);
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
  // advance past leading quote
  Str s = str_slice_low(self->params.content, 1);

  while (true) {
    // search for trailing quote
    const int i = str_index_char(s, kStringBegin);

    if (i == -1) {
      scan_errorf(self, "string value does not end with quote");
      return false;
    }

    // get previous char
    const char prev = s.ptr[i - 1];

    // advance
    s = str_slice_low(s, i + 1);

    // stop if quote is not escaped
    if (prev != '\\') {
      break;
    }
  }

  // calculate the end, includes trailing quote
  const size_t end = s.ptr - self->params.content.ptr - 1;

  // +1 for leading quote
  scanner_append(self, kTokenValue, end + 1);

  return true;
}

static bool scan_raw_string(Scanner *self) {
  const int end =
      str_index_char(str_slice_low(self->params.content, 1), kRawStringBegin);
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
  const int end =
      str_index_any(self->params.content, str_from_cstr(";]}\n"), &c);
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
    if (self->params.content.len == 0) {
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
    if (self->params.content.len == 0) {
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

Error scan(Tokens *tokens, Params params, Allocators *alls) {
  Scanner s = {
      .params = params,
      .alls = alls,
      .tokens = tokens,
      .line = 1,
  };
  while (s.params.content.len != 0) {
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
      return s.params.err_buf;
    }
  }
  return NULL;
}

static void list_reserve(List *self, size_t cap, Arena *arena) {
  Value *ptr = arena_extend(arena, self->ptr, self->cap * sizeof(Value),
                            cap * sizeof(Value));
  assert(ptr != NULL);
  self->cap = cap;
  self->ptr = ptr;
}

static void list_append(List *self, Value v, Arena *arena) {
  if (self->len == self->cap) {
    list_reserve(self, (self->cap + 1) * 2, arena);
  }
  memcpy(self->ptr + self->len, &v, sizeof(v));
  self->len += 1;
}

static void table_reserve(Table *self, size_t cap, Arena *arena) {
  KeyValue *ptr = arena_extend(arena, self->ptr, self->cap * sizeof(KeyValue),
                               cap * sizeof(KeyValue));
  assert(ptr != NULL);
  self->cap = cap;
  self->ptr = ptr;
}

static void table_append(Table *self, KeyValue v, Arena *arena) {
  if (self->len == self->cap) {
    table_reserve(self, (self->cap + 1) * 2, arena);
  }
  memcpy(self->ptr + self->len, &v, sizeof(v));
  self->len += 1;
}

typedef struct {
  Params params;
  Tokens tokens;
  Allocators *alls;
  Table *table;
  size_t i;
} Parser;

static bool parse_value(Parser *self, Value *out);
static bool parse_key_value(Parser *self, Table parent, KeyValue *out);

static Token parser_get(const Parser *self) {
  return self->tokens.ptr[self->i];
}

static void parser_pop(Parser *self) { self->i++; }

static void parse_errorf(const Parser *self, const char *fmt, ...) {
  char *ptr = self->params.err_buf;
  size_t len = self->params.err_buf_len;

  int n = snprintf(ptr, len, "%s:%d: error: parse: ", self->params.file.ptr,
                   parser_get(self).line);
  assert(n >= 0);
  assert((size_t)n < len);
  ptr += n;
  len -= n;

  va_list args;
  va_start(args, fmt);
  n = vsnprintf(ptr, len, fmt, args);
  va_end(args);

  assert(n >= 0);
  assert((size_t)n < len);
  ptr += n;
  len -= n;

  if (self->params.abort_on_error) {
    printf("%s\n", self->params.err_buf);
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

  parser_pop(self);

  while (true) {
    if (parse_delim(self, kListEnd)) {
      return true;
    }

    Value v = {};
    if (!parse_value(self, &v)) {
      return false;
    }
    list_append(&out->data.list, v, &self->alls->lists);

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
    if (!parse_key_value(self, out->data.table, &kv)) {
      return false;
    }
    table_append(&out->data.table, kv, &self->alls->tables);

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

  if (str_starts_with_char(val, kStringBegin)) {
    char *data = NULL;
    Error err =
        str_norm(str_slice(val, 1, val.len - 1), &self->alls->strings, &data);
    if (err != NULL) {
      parse_errorf(self, "could not normalize string: %s", err);
      return false;
    }
    out->tag = kValueTagString;
    out->data.string = data;
  } else if (str_starts_with_char(val, kRawStringBegin)) {
    out->tag = kValueTagString;
    out->data.string =
        str_dup(str_slice(val, 1, val.len - 1), &self->alls->strings);
  } else {
    if (str_equals(val, str_from_cstr("true"))) {
      out->tag = kValueTagBoolean;
      out->data.boolean = true;
    } else if (str_equals(val, str_from_cstr("false"))) {
      out->tag = kValueTagBoolean;
      out->data.boolean = false;
    } else {
      int64_t i = 0;
      Error err = str_to_int(val, 0, &i);
      if (err != NULL) {
        char *s = str_dup(val, &self->alls->strings);
        parse_errorf(self, "value '%s' is not an integer: %s", s, err);
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
    return false;
  }

  if (!parse_delim(self, kKeyValEnd)) {
    parse_errorf(self, "missing key value end");
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
    char *s = str_dup(tok.value, &self->alls->strings);
    parse_errorf(self, "key is not a valid identifier: '%s'", s);
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
    char *s = str_dup(temp, &self->alls->strings);
    parse_errorf(self, "key '%s' is not unique for current table", s);
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

Error parse(Table *table, Params params, Tokens tokens, Allocators *alls) {
  Parser p = {
      .params = params,
      .alls = alls,
      .tokens = tokens,
      .table = table,
      .i = 0,
  };

  while (p.i < tokens.len) {
    KeyValue kv = {};
    if (!parse_key_value(&p, *table, &kv)) {
      return params.err_buf;
    }
    table_append(p.table, kv, &alls->tables);
  }

  return NULL;
}

Error table_parse(Table *table, Params params) {
  assert(params.mem_buf_len != 0);
  assert(params.mem_buf != NULL);
  assert(params.err_buf_len != 0);
  assert(params.err_buf != NULL);

  Allocators alls = {};
  {
    const size_t n = params.mem_buf_len / 4;
    arena_init(&alls.tables, params.mem_buf + n * 1, n);
    arena_init(&alls.lists, params.mem_buf + n * 2, n);
    arena_init(&alls.tokens, params.mem_buf + n * 3, n);
    arena_init(&alls.strings, params.mem_buf + n * 4, n);
  }

  Tokens tokens = {};

  Error err = scan(&tokens, params, &alls);
  if (err == NULL) {
    err = parse(table, params, tokens, &alls);
  }

  return err;
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

Error table_string(Table self, const char *key, char **out) {
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

Error list_string(List self, size_t i, char **out) {
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
