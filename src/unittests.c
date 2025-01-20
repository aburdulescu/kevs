#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "kevs.h"
#include "util.h"

static void logf(const char *fn, int ln, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(stdout, "%s() %d ", fn, ln);
  vfprintf(stdout, fmt, args);
  fprintf(stdout, "\n");
  va_end(args);
}

#define INFO(...) logf(__FUNCTION__, __LINE__, __VA_ARGS__)

static void test_str_index_char() {
  Str s = str_from_cstr("0123456789");
  {
    int i = str_index_char(s, '0');
    const int expected = 0;
    INFO("want %zu, have %zu", expected, i);
    assert(i == expected);
  }
  {
    int i = str_index_char(s, '5');
    const int expected = 5;
    INFO("want %zu, have %zu", expected, i);
    assert(i == expected);
  }
  {
    int i = str_index_char(s, '9');
    const int expected = 9;
    INFO("want %zu, have %zu", expected, i);
    assert(i == expected);
  }
  {
    int i = str_index_char(s, 'x');
    const int expected = -1;
    INFO("want %zu, have %zu", expected, i);
    assert(i == expected);
  }
}

static void test_str_slice_low() {
  Str s = str_from_cstr("0123456789");

  Str s4 = str_slice_low(s, 4);

  const char *expected_ptr = s.ptr + 4;
  const size_t expected_len = 6;

  INFO("len: want %zu, have %zu", expected_len, s4.len);
  assert(s4.len == expected_len);

  INFO("ptr: want %p, have %p", expected_ptr, s4.ptr);
  assert(s4.ptr == expected_ptr);
}

static void test_str_slice() {
  Str s = str_from_cstr("0123456789");

  Str s26 = str_slice(s, 2, 6);

  const char *expected_ptr = s.ptr + 2;
  const size_t expected_len = 4;

  INFO("len: want %zu, have %zu", expected_len, s26.len);
  assert(s26.len == expected_len);

  INFO("ptr: want %p, have %p", expected_ptr, s26.ptr);
  assert(s26.ptr == expected_ptr);
}

static void test_str_trim_left() {
  Str s = str_from_cstr("  aa");

  Str t = str_trim_left(s, str_from_cstr(" "));

  const char *expected_ptr = s.ptr + 2;
  const size_t expected_len = 2;

  INFO("len: want %zu, have %zu", expected_len, t.len);
  assert(t.len == expected_len);

  INFO("ptr: want %p, have %p", expected_ptr, t.ptr);
  assert(t.ptr == expected_ptr);
}

static void test_str_trim_right() {
  Str s = str_from_cstr("aa  ");

  Str t = str_trim_right(s, str_from_cstr(" "));

  const char *expected_ptr = s.ptr;
  const size_t expected_len = 2;

  INFO("len: want %zu, have %zu", expected_len, t.len);
  assert(t.len == expected_len);

  INFO("ptr: want %p, have %p", expected_ptr, t.ptr);
  assert(t.ptr == expected_ptr);
}

static void test_str_to_int_negative() {
  const char *tests[] = {
      "",                     // empty input
      "9223372036854775808",  // > int64 max
      "9223372036854775809",  // > int64 max
      "-9223372036854775809", // < int64 min
      "-_12345",              // invalid char
      "_12345",               // invalid char
      "1__2345",              // invalid chars
      "12345_",               // invalid char
      "123%45",               // invalid char
      "12345x",               // invalid char
      "-12345x",              // invalid char
      "0wff",                 // invalid base

      // > int64 max
      "0b1000000000000000000000000000000000000000000000000000000000000000",

      // < int64 min
      "-0b1000000000000000000000000000000000000000000000000000000000000001",

      "18446744073709551616",     // > uint64 max
      "18446744073709551620",     // > uint64 max
      "0o2000000000000000000000", // > uint64 max
      "0x10000000000000000",      // > uint64 max

      // > uint64 max
      "0b10000000000000000000000000000000000000000000000000000000000000000",
  };
  const size_t tests_len = sizeof(tests) / (sizeof(tests[0]));

  for (size_t i = 0; i < tests_len; i++) {
    INFO("test #%zu: input=%s", i, tests[i]);

    int64_t v = 0;
    Error err = str_to_int(str_from_cstr(tests[i]), 0, &v);

    INFO("test #%zu: err=%s, value=%ld", i, err, v);
    assert(err != NULL);
  }
}

static void test_str_to_int_positive() {
  struct Test {
    const char *input;
    int64_t expected;
  };
  const struct Test tests[] = {
      {"0", 0},
      {"-0", 0},
      {"+0", 0},
      {"1", 1},
      {"-1", -1},
      {"+1", 1},
      {"12345", 12345},
      {"-12345", -12345},
      {"98765432100", 98765432100},
      {"-98765432100", -98765432100},
      {"0x12345", 0x12345},
      {"-0x12345", -0x12345},
      {"0o12345", 012345},
      {"-0o12345", -012345},

      // base 2
      {"0b1010", 10},
      {"0b1000000000000000", 1 << 15},
      {
          "0b111111111111111111111111111111111111111111111111111111111111"
          "111",
          (uint64_t)(((uint64_t)1) << 63) - 1,
      },
      {
          "-0b10000000000000000000000000000000000000000000000000000000000"
          "00000",
          (int64_t)(((uint64_t)1) << 63),
      },

      // base 8
      {"-0o10", -8},
      {"0o57635436545", 057635436545},
      {"0o100000000", 1 << 24},

      // base 16
      {"0x10", 16},
      {"-0x123456789abcdef", -0x123456789abcdef},
      {"0x7fffffffffffffff", (((uint64_t)1) << 63) - 1},
  };
  const size_t tests_len = sizeof(tests) / (sizeof(tests[0]));

  for (size_t i = 0; i < tests_len; i++) {
    INFO("test #%zu: input=%s, expected %ld", i, tests[i].input,
         tests[i].expected);

    int64_t v = 0;
    Error err = str_to_int(str_from_cstr(tests[i].input), 0, &v);

    INFO("test #%zu: err=%s, value=%ld", i, err, v);
    assert(err == NULL);

    INFO("test #%zu: len: want %ld, have %ld", i, tests[i].expected, v);
    assert(v == tests[i].expected);
  }
}

static void test_ucs_to_utf8() {
  const struct {
    uint64_t in;
    uint8_t n;
    char out[4];
  } tests[] = {
      // valid

      // single byte
      {0x00000000, 1, {0x00}}, // min
      {0x00000042, 1, {0x42}},
      {0x0000007f, 1, {0x7f}}, // max

      // two byte
      {0x00000080, 2, {(char)0xc2, (char)0x80}}, // min
      {0x000007ff, 2, {(char)0xdf, (char)0xbf}}, // max

      // three byte
      {0x00000800, 3, {(char)0xe0, (char)0xa0, (char)0x80}}, // min
      {0x0000d7ff, 3, {(char)0xed, (char)0x9f, (char)0xbf}}, // max < surogate
      {0x0000e000, 3, {(char)0xee, (char)0x80, (char)0x80}}, // min > surogate
      {0x0000ffff, 3, {(char)0xef, (char)0xbf, (char)0xbf}}, // max

      // four byte
      {0x00010000, 4, {(char)0xf0, (char)0x90, (char)0x80, (char)0x80}}, // min
      {0x0010ffff, 4, {(char)0xf4, (char)0x8f, (char)0xbf, (char)0xbf}}, // max

      // invalid
      {0x0000d800, 0, {}}, // low surogate start
      {0x0000dbff, 0, {}}, // low surogate end
      {0x0000dc00, 0, {}}, // high surogate start
      {0x0000dfff, 0, {}}, // high surogate end

      // edge cases
      {0x00110000, 0, {}}, // after max
  };

  const size_t tests_len = sizeof(tests) / sizeof(tests[0]);

  for (size_t i = 0; i < tests_len; i++) {
    char out[4] = {};
    const uint8_t n = ucs_to_utf8(tests[i].in, out);
    INFO("test #%zu: in=0x%08lx", i, tests[i].in);
    INFO("test #%zu: want: n=%d, out=%02x %02x %02x %02x", i,
         (uint8_t)tests[i].n, (uint8_t)tests[i].out[0],
         (uint8_t)tests[i].out[1], (uint8_t)tests[i].out[2],
         (uint8_t)tests[i].out[3]);
    INFO("test #%zu: have: n=%d, out=%02x %02x %02x %02x", i, n,
         (uint8_t)out[0], (uint8_t)out[1], (uint8_t)out[2], (uint8_t)out[3]);
    assert(n == tests[i].n);
    assert(out[0] == tests[i].out[0]);
    assert(out[1] == tests[i].out[1]);
    assert(out[2] == tests[i].out[2]);
    assert(out[3] == tests[i].out[3]);
  }
}

int main() {
  test_str_index_char();
  test_str_slice_low();
  test_str_slice();
  test_str_trim_left();
  test_str_trim_right();
  test_str_to_int_negative();
  test_str_to_int_positive();
  test_ucs_to_utf8();
  return 0;
}
