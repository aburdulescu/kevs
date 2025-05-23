#include "kevs.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline bool is_digit(char c) { return c >= '0' && c <= '9'; }

static inline char lower(char c) { return (char)(c | ('x' - 'X')); }

static inline bool is_letter(char c) {
  return lower(c) >= 'a' && lower(c) <= 'z';
}

static bool is_identifier(KevsStr s) {
  char c = s.ptr[0];
  if (c != '_' && !is_letter(c)) {
    return false;
  }
  for (size_t i = 1; i < s.len; i++) {
    if (!is_digit(s.ptr[i]) && !is_letter(s.ptr[i]) && s.ptr[i] != '_') {
      return false;
    }
  }
  return true;
}

const char *kevs_valuekind_str(KevsValueKind v) {
  switch (v) {
  case KevsValueKindUndefined:
    return "undefined";
  case KevsValueKindString:
    return "string";
  case KevsValueKindInteger:
    return "integer";
  case KevsValueKindBoolean:
    return "boolean";
  case KevsValueKindList:
    return "list";
  case KevsValueKindTable:
    return "table";
  default:
    return "unknown";
  }
}

typedef struct {
  char *ptr;
  size_t cap;
  size_t len;
} String;

static void string_reserve(String *self, size_t cap) {
  char *ptr = malloc(cap + 1);
  assert(ptr != NULL);
  self->cap = cap;
  self->ptr = ptr;
  self->ptr[self->len] = 0;
}

static void string_append(String *self, char v) {
  assert(self->len < self->cap);
  self->ptr[self->len] = v;
  self->len += 1;
  self->ptr[self->len] = 0;
}

KevsStr kevs_str_from_cstr(const char *s) {
  assert(s != NULL);
  KevsStr self = {
      .ptr = s,
      .len = strlen(s),
  };
  return self;
}

char *kevs_str_dup(KevsStr self) {
  char *ptr = malloc(self.len + 1);
  ptr[self.len] = 0;
  memcpy(ptr, self.ptr, self.len);
  return ptr;
}

static bool str_starts_with_char(KevsStr self, char c) {
  if (self.len < 1) {
    return false;
  }
  return self.ptr[0] == c;
}

int str_index_char(KevsStr self, char c) {
  const char *ptr = memchr(self.ptr, c, self.len);
  if (ptr == NULL) {
    return -1;
  }
  return (int)(ptr - self.ptr);
}

static size_t str_count_char(KevsStr self, char c) {
  size_t count = 0;
  for (size_t i = 0; i < self.len; i++) {
    if (self.ptr[i] == c) {
      count++;
    }
  }
  return count;
}

static int str_index_any(KevsStr self, KevsStr chars, char *c) {
  for (size_t i = 0; i < self.len; i++) {
    const int j = str_index_char(chars, self.ptr[i]);
    if (j != -1) {
      *c = chars.ptr[j];
      return (int)i;
    }
  }
  return -1;
}

static bool str_equals(KevsStr self, KevsStr other) {
  if (self.len != other.len) {
    return false;
  }
  return memcmp(self.ptr, other.ptr, self.len) == 0;
}

static bool str_equals_char(KevsStr self, char c) {
  if (self.len != 1) {
    return false;
  }
  return self.ptr[0] == c;
}

KevsStr str_slice_low(KevsStr self, size_t low) {
  assert(self.ptr != NULL);
  assert(self.len != 0);
  assert(low <= self.len);
  self.len -= low;
  self.ptr += low;
  return self;
}

KevsStr str_slice(KevsStr self, size_t low, size_t high) {
  assert(self.ptr != NULL);
  assert(self.len != 0);
  assert(low <= self.len);
  assert(high <= self.len);
  self.len = (self.ptr + high) - (self.ptr + low);
  self.ptr += low;
  return self;
}

KevsStr str_trim_left(KevsStr self, KevsStr cutset) {
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

KevsStr str_trim_right(KevsStr self, KevsStr cutset) {
  for (int i = (int)self.len - 1; i >= 0; i--) {
    if (str_index_char(cutset, self.ptr[i]) == -1) {
      break;
    }
    self.len--;
  }
  return self;
}

static KevsError str_to_uint(KevsStr self, uint64_t base, uint64_t *out) {
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

KevsError str_to_int(KevsStr self, uint64_t base, int64_t *out) {
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
  KevsError err = str_to_uint(self, base, &un);
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

static KevsError str_norm(KevsStr self, char **out) {
  String dst = {};
  string_reserve(&dst, self.len);

  // TODO: change this from char by char to memchr?

  for (size_t i = 0; i < self.len;) {
    if (self.ptr[i] == '\\') {
      i++;
      switch (self.ptr[i]) {
      case 'a': {
        string_append(&dst, '\a');
        i++;
      } break;
      case 'b': {
        string_append(&dst, '\b');
        i++;
      } break;
      case 'f': {
        string_append(&dst, '\f');
        i++;
      } break;
      case 'n': {
        string_append(&dst, '\n');
        i++;
      } break;
      case 'r': {
        string_append(&dst, '\r');
        i++;
      } break;
      case 't': {
        string_append(&dst, '\t');
        i++;
      } break;
      case 'v': {
        string_append(&dst, '\v');
        i++;
      } break;
      case '"': {
        string_append(&dst, '"');
        i++;
      } break;
      case '\\': {
        string_append(&dst, '\\');
        i++;
      } break;
      case 'u': {
        i++;

        if ((i + 4) > self.len) {
          free(dst.ptr);
          return "\\u must be followed by 4 hex digits: \\uXXXX";
        }

        uint64_t code = 0;
        const KevsError err = str_to_uint(str_slice(self, i, i + 4), 16, &code);
        if (err != NULL) {
          free(dst.ptr);
          return err;
        }
        i += 4;

        char utf8[4] = {};
        const int n = ucs_to_utf8(code, utf8);
        if (n == 0) {
          free(dst.ptr);
          return "could not encode Unicode code point to UTF-8";
        }

        for (int i = 0; i < n; i++) {
          string_append(&dst, utf8[i]);
        }
      } break;
      case 'U': {
        i++;

        if ((i + 8) > self.len) {
          free(dst.ptr);
          return "\\U must be followed by 8 hex digits: \\UXXXXXXXX";
        }

        uint64_t code = 0;
        const KevsError err = str_to_uint(str_slice(self, i, i + 8), 16, &code);
        if (err != NULL) {
          free(dst.ptr);
          return err;
        }
        i += 8;

        char utf8[4] = {};
        const int n = ucs_to_utf8(code, utf8);
        if (n == 0) {
          free(dst.ptr);
          return "could not encode Unicode code point to UTF-8";
        }

        for (int i = 0; i < n; i++) {
          string_append(&dst, utf8[i]);
        }
      } break;
      default: {
        free(dst.ptr);
        return "unknown escape sequence";
      }
      }
    } else {
      string_append(&dst, self.ptr[i]);
      i++;
    }
  }

  // TODO: here we might be wasting memory
  // so check if dst.len==dst.cap, if not resize memory chunk

  *out = dst.ptr;

  return NULL;
}

const char *tokenkind_str(KevsTokenKind v) {
  switch (v) {
  case KevsTokenKindUndefined:
    return "undefined";
  case KevsTokenKindKey:
    return "key";
  case KevsTokenKindDelim:
    return "delim";
  case KevsTokenKindValue:
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

static void tokens_reserve(KevsTokens *self, size_t cap) {
  self->cap = cap;
  KevsToken *ptr = realloc(self->ptr, cap * sizeof(KevsToken));
  assert(ptr != NULL);
  self->ptr = ptr;
}

static void tokens_append(KevsTokens *self, KevsToken v) {
  if (self->len == self->cap) {
    tokens_reserve(self, (self->cap + 1) * 2);
  }
  memcpy(self->ptr + self->len, &v, sizeof(v));
  self->len += 1;
}

static void list_free(KevsList *self);

static void value_free(KevsValue *self) {
  switch (self->kind) {
  case KevsValueKindString:
    free(self->data.string);
    break;

  case KevsValueKindList:
    list_free(&self->data.list);
    break;

  case KevsValueKindTable:
    kevs_free(&self->data.table);
    break;

  default:
    break;
  }

  *self = (KevsValue){};
}

static void list_free(KevsList *self) {
  for (size_t i = 0; i < self->len; i++) {
    value_free(&self->ptr[i]);
  }

  free(self->ptr);
  *self = (KevsList){};
}

typedef struct {
  KevsOpts opts;
  KevsTokens *tokens;
  int line;
  char *err_buf;
  size_t err_buf_len;
  KevsStr content;
} Scanner;

static bool scan_key_value(Scanner *self);
static bool scan_value(Scanner *self);

static void scan_errorf(const Scanner *self, const char *fmt, ...) {
  char *ptr = self->err_buf;
  size_t len = self->err_buf_len;

  int n = 0;

  if (self->opts.errors_with_file_and_line) {
    n = snprintf(ptr, len, "%s:%d: ", self->opts.file.ptr, self->line);
    assert(n >= 0);
    assert((size_t)n < len);
    ptr += n;
    len -= n;
  }

  n = snprintf(ptr, len, "scan: ");
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

  if (self->opts.abort_on_error) {
    printf("%s\n", self->err_buf);
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
  self->content = str_trim_left(self->content, kevs_str_from_cstr(spaces));
}

static void scanner_append(Scanner *self, KevsTokenKind kind, size_t end) {
  KevsStr val = str_slice(self->content, 0, end);
  val = str_trim_right(val, kevs_str_from_cstr(spaces));

  const KevsToken t = {
      .kind = kind,
      .value = val,
      .line = self->line,
  };

  tokens_append(self->tokens, t);

  scanner_advance(self, end);
}

static void scanner_append_delim(Scanner *self) {
  const KevsToken t = {
      .kind = KevsTokenKindDelim,
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
  const int i = str_index_any(self->content, kevs_str_from_cstr("=;\n"), &c);
  if (c != kKeyValSep) {
    scan_errorf(self, "key-value pair is missing separator");
    return false;
  }
  scanner_append(self, KevsTokenKindKey, i);
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
  KevsStr s = str_slice_low(self->content, 1);

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
  const size_t end = s.ptr - self->content.ptr - 1;

  // +1 for leading quote
  scanner_append(self, KevsTokenKindValue, end + 1);

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
  scanner_append(self, KevsTokenKindValue, end + 2);

  // count newlines in raw string to keep line count accurate
  self->line +=
      (int)str_count_char(self->tokens->ptr[self->tokens->len - 1].value, '\n');

  return true;
}

static bool scan_int_or_bool_value(Scanner *self) {
  // search for all possible value endings
  // if semicolon(or none of them) is not found => error
  char c = 0;
  const int i = str_index_any(self->content, kevs_str_from_cstr(";]}\n"), &c);
  if (c != kKeyValEnd) {
    scan_errorf(self, "integer or boolean value does not end with semicolon");
    return false;
  }
  scanner_append(self, KevsTokenKindValue, i);
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

KevsError scan(KevsTokens *tokens, KevsStr content, char *err_buf,
               size_t err_buf_len, KevsOpts opts) {
  Scanner s = {
      .opts = opts,
      .tokens = tokens,
      .line = 1,
      .err_buf = err_buf,
      .err_buf_len = err_buf_len,
      .content = content,
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
      return s.err_buf;
    }
  }
  return NULL;
}

static void list_reserve(KevsList *self, size_t cap) {
  self->cap = cap;
  KevsValue *ptr = realloc(self->ptr, cap * sizeof(KevsValue));
  assert(ptr != NULL);
  self->ptr = ptr;
}

static void list_append(KevsList *self, KevsValue v) {
  if (self->len == self->cap) {
    list_reserve(self, (self->cap + 1) * 2);
  }
  memcpy(self->ptr + self->len, &v, sizeof(v));
  self->len += 1;
}

static void table_reserve(KevsTable *self, size_t cap) {
  self->cap = cap;
  KevsKeyValue *ptr = realloc(self->ptr, cap * sizeof(KevsKeyValue));
  assert(ptr != NULL);
  self->ptr = ptr;
}

static void table_append(KevsTable *self, KevsKeyValue v) {
  if (self->len == self->cap) {
    table_reserve(self, (self->cap + 1) * 2);
  }
  memcpy(self->ptr + self->len, &v, sizeof(v));
  self->len += 1;
}

typedef struct {
  KevsOpts opts;
  KevsTokens tokens;
  KevsTable *table;
  size_t i;
  char *err_buf;
  size_t err_buf_len;
  KevsStr content;
} Parser;

static bool parse_value(Parser *self, KevsValue *out);
static bool parse_key_value(Parser *self, KevsTable parent, KevsKeyValue *out);

static KevsToken parser_get(const Parser *self) {
  return self->tokens.ptr[self->i];
}

static void parser_pop(Parser *self) { self->i++; }

static void parse_errorf(const Parser *self, const char *fmt, ...) {
  char *ptr = self->err_buf;
  size_t len = self->err_buf_len;

  int n = 0;

  if (self->opts.errors_with_file_and_line) {
    n = snprintf(ptr, len, "%s:%d: ", self->opts.file.ptr,
                 parser_get(self).line);
    assert(n >= 0);
    assert((size_t)n < len);
    ptr += n;
    len -= n;
  }

  n = snprintf(ptr, len, "parse: ");
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

  if (self->opts.abort_on_error) {
    printf("%s\n", self->err_buf);
    abort();
  }
}

static bool parser_expect(const Parser *self, KevsTokenKind kind) {
  if (self->i >= self->tokens.len) {
    parse_errorf(self, "expected token '%s', have nothing",
                 tokenkind_str(kind));
    return false;
  }
  return parser_get(self).kind == kind;
}

static bool parser_expect_delim(const Parser *self, char delim) {
  if (!parser_expect(self, KevsTokenKindDelim)) {
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

static bool parse_list_value(Parser *self, KevsValue *out) {
  out->kind = KevsValueKindList;

  parser_pop(self);

  while (true) {
    if (parse_delim(self, kListEnd)) {
      return true;
    }

    KevsValue v = {};
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

static bool parse_table_value(Parser *self, KevsValue *out) {
  out->kind = KevsValueKindTable;

  parser_pop(self);

  while (true) {
    if (parse_delim(self, kTableEnd)) {
      return true;
    }

    KevsKeyValue kv = {};
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

static bool parse_simple_value(Parser *self, KevsValue *out) {
  if (!parser_expect(self, KevsTokenKindValue)) {
    parse_errorf(self, "expected value token");
    return false;
  }

  const KevsStr val = parser_get(self).value;

  bool ok = true;

  if (str_starts_with_char(val, kStringBegin)) {
    char *data = NULL;
    KevsError err = str_norm(str_slice(val, 1, val.len - 1), &data);
    if (err != NULL) {
      parse_errorf(self, "could not normalize string: %s", err);
      free(data);
      return false;
    }
    out->kind = KevsValueKindString;
    out->data.string = data;

  } else if (str_starts_with_char(val, kRawStringBegin)) {
    out->kind = KevsValueKindString;
    out->data.string = kevs_str_dup(str_slice(val, 1, val.len - 1));

  } else if (str_equals(val, kevs_str_from_cstr("true"))) {
    out->kind = KevsValueKindBoolean;
    out->data.boolean = true;

  } else if (str_equals(val, kevs_str_from_cstr("false"))) {
    out->kind = KevsValueKindBoolean;
    out->data.boolean = false;

  } else {
    int64_t i = 0;
    KevsError err = str_to_int(val, 0, &i);
    if (err != NULL) {
      char *s = kevs_str_dup(val);
      parse_errorf(self, "value '%s' is not an integer: %s", s, err);
      free(s);
      ok = false;
    } else {
      out->kind = KevsValueKindInteger;
      out->data.integer = i;
    }
  }

  parser_pop(self);

  return ok;
}

static bool parse_value(Parser *self, KevsValue *out) {
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

static bool parse_key(Parser *self, KevsTable parent, KevsStr *key) {
  if (!parser_expect(self, KevsTokenKindKey)) {
    parse_errorf(self, "expected key token");
    return false;
  }

  const KevsToken tok = parser_get(self);

  if (!is_identifier(tok.value)) {
    char *s = kevs_str_dup(tok.value);
    parse_errorf(self, "key is not a valid identifier: '%s'", s);
    free(s);
    return false;
  }

  // check if key is unique
  for (size_t i = 0; i < parent.len; i++) {
    if (str_equals(parent.ptr[i].key, tok.value)) {
      char *s = kevs_str_dup(tok.value);
      parse_errorf(self, "key '%s' is not unique for current table", s);
      free(s);
      return false;
    }
  }

  *key = tok.value;

  parser_pop(self);

  return true;
}

static bool parse_key_value(Parser *self, KevsTable parent, KevsKeyValue *out) {
  KevsStr key = {};
  if (!parse_key(self, parent, &key)) {
    return false;
  }

  if (!parse_delim(self, kKeyValSep)) {
    parse_errorf(self, "missing key value separator");
    return false;
  }

  KevsValue val = {};
  if (!parse_value(self, &val)) {
    return false;
  }

  out->key = key;
  out->val = val;

  return true;
}

KevsError parse(KevsTable *table, KevsStr content, char *err_buf,
                size_t err_buf_len, KevsOpts opts, KevsTokens tokens) {
  Parser p = {
      .opts = opts,
      .tokens = tokens,
      .table = table,
      .i = 0,
      .err_buf = err_buf,
      .err_buf_len = err_buf_len,
      .content = content,
  };

  while (p.i < tokens.len) {
    KevsKeyValue kv = {};
    if (!parse_key_value(&p, *table, &kv)) {
      return err_buf;
    }
    table_append(p.table, kv);
  }

  return NULL;
}

KevsError kevs_parse(KevsTable *table, KevsStr content, char *err_buf,
                     size_t err_buf_len, KevsOpts opts) {
  assert(err_buf_len != 0);
  assert(err_buf != NULL);

  KevsTokens tokens = {};

  KevsError err = scan(&tokens, content, err_buf, err_buf_len, opts);
  if (err == NULL) {
    err = parse(table, content, err_buf, err_buf_len, opts, tokens);
  }

  free(tokens.ptr);

  return err;
}

void kevs_free(KevsTable *self) {
  for (size_t i = 0; i < self->len; i++) {
    value_free(&self->ptr[i].val);
  }

  free(self->ptr);
  *self = (KevsTable){};
}

static bool value_is(KevsValue self, KevsValueKind kind) {
  return self.kind == kind;
}

static KevsError table_get(KevsTable self, const char *key, KevsValue *val) {
  KevsStr key_str = kevs_str_from_cstr(key);
  for (size_t i = 0; i < self.len; i++) {
    if (str_equals(self.ptr[i].key, key_str)) {
      *val = self.ptr[i].val;
      return NULL;
    }
  }
  return "key not found";
}

KevsError kevs_table_string(KevsTable self, const char *key, char **out) {
  KevsValue val = {};
  KevsError err = table_get(self, key, &val);
  if (err != NULL) {
    return err;
  }
  if (!value_is(val, KevsValueKindString)) {
    return "value is not string";
  }
  *out = val.data.string;
  return NULL;
}

KevsError kevs_table_int(KevsTable self, const char *key, int64_t *out) {
  KevsValue val = {};
  KevsError err = table_get(self, key, &val);
  if (err != NULL) {
    return err;
  }
  if (!value_is(val, KevsValueKindInteger)) {
    return "value is not integer";
  }
  *out = val.data.integer;
  return NULL;
}

KevsError kevs_table_bool(KevsTable self, const char *key, bool *out) {
  KevsValue val = {};
  KevsError err = table_get(self, key, &val);
  if (err != NULL) {
    return err;
  }
  if (!value_is(val, KevsValueKindBoolean)) {
    return "value is not boolean";
  }
  *out = val.data.boolean;
  return NULL;
}

KevsError kevs_table_list(KevsTable self, const char *key, KevsList *out) {
  KevsValue val = {};
  KevsError err = table_get(self, key, &val);
  if (err != NULL) {
    return err;
  }
  if (!value_is(val, KevsValueKindList)) {
    return "value is not list";
  }
  *out = val.data.list;
  return NULL;
}

KevsError kevs_table_table(KevsTable self, const char *key, KevsTable *out) {
  KevsValue val = {};
  KevsError err = table_get(self, key, &val);
  if (err != NULL) {
    return err;
  }
  if (!value_is(val, KevsValueKindTable)) {
    return "value is not table";
  }
  *out = val.data.table;
  return NULL;
}

bool kevs_table_has(KevsTable self, const char *key) {
  KevsStr key_str = kevs_str_from_cstr(key);
  for (size_t i = 0; i < self.len; i++) {
    if (str_equals(self.ptr[i].key, key_str)) {
      return true;
    }
  }
  return false;
}

static KevsError list_get(KevsList self, size_t i, KevsValue *val) {
  if (i >= self.len) {
    return "index out of bounds";
  }
  *val = self.ptr[i];
  return NULL;
}

KevsError kevs_list_string(KevsList self, size_t i, char **out) {
  KevsValue val = {};
  KevsError err = list_get(self, i, &val);
  if (err != NULL) {
    return err;
  }
  if (!value_is(val, KevsValueKindString)) {
    return "value is not string";
  }
  *out = val.data.string;
  return NULL;
}

KevsError kevs_list_int(KevsList self, size_t i, int64_t *out) {
  KevsValue val = {};
  KevsError err = list_get(self, i, &val);
  if (err != NULL) {
    return err;
  }
  if (!value_is(val, KevsValueKindInteger)) {
    return "value is not integer";
  }
  *out = val.data.integer;
  return NULL;
}

KevsError kevs_list_bool(KevsList self, size_t i, bool *out) {
  KevsValue val = {};
  KevsError err = list_get(self, i, &val);
  if (err != NULL) {
    return err;
  }
  if (!value_is(val, KevsValueKindBoolean)) {
    return "value is not boolean";
  }
  *out = val.data.boolean;
  return NULL;
}

KevsError kevs_list_list(KevsList self, size_t i, KevsList *out) {
  KevsValue val = {};
  KevsError err = list_get(self, i, &val);
  if (err != NULL) {
    return err;
  }
  if (!value_is(val, KevsValueKindList)) {
    return "value is not list";
  }
  *out = val.data.list;
  return NULL;
}

KevsError kevs_list_table(KevsList self, size_t i, KevsTable *out) {
  KevsValue val = {};
  KevsError err = list_get(self, i, &val);
  if (err != NULL) {
    return err;
  }
  if (!value_is(val, KevsValueKindTable)) {
    return "value is not table";
  }
  *out = val.data.table;
  return NULL;
}
