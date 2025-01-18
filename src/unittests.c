#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "kevs.h"
#include "util.h"

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

#define INFO(...) kevs_logf("INFO ", __FUNCTION__, __LINE__, __VA_ARGS__)
#define ERROR(...) kevs_logf("ERROR", __FUNCTION__, __LINE__, __VA_ARGS__)

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

int main() {
  test_str_index_char();
  test_str_slice_low();
  test_str_slice();
  test_str_trim_left();
  test_str_trim_right();
  test_str_to_int_negative();
  test_str_to_int_positive();
  return 0;
}
