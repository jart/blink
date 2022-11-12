#ifndef TEST_TEST_H_
#define TEST_TEST_H_
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST(GROUP, NAME)                                                    \
  void GROUP##_##NAME(void);                                                 \
  __attribute__((__constructor__)) void GROUP##_##NAME##_init(void) {        \
    g_tests.p =                                                              \
        (struct Test *)realloc(g_tests.p, ++g_tests.n * sizeof(*g_tests.p)); \
    g_tests.p[g_tests.n - 1].f = GROUP##_##NAME;                             \
  }                                                                          \
  void GROUP##_##NAME(void)

#define ASSERT_EQ(WANT, GOT, ...)                                 \
  AssertionInt64(AssertEq, true, "ASSERT_EQ", __FILE__, __LINE__, \
                 __FUNCTION__, WANT, #WANT, GOT, #GOT, " " __VA_ARGS__)
#define EXPECT_EQ(WANT, GOT, ...)                                  \
  AssertionInt64(AssertEq, false, "EXPECT_EQ", __FILE__, __LINE__, \
                 __FUNCTION__, WANT, #WANT, GOT, #GOT, " " __VA_ARGS__)
#define ASSERT_NE(WANT, GOT, ...)                                 \
  AssertionInt64(AssertNe, true, "ASSERT_NE", __FILE__, __LINE__, \
                 __FUNCTION__, WANT, #WANT, GOT, #GOT, " " __VA_ARGS__)
#define EXPECT_NE(WANT, GOT, ...)                                  \
  AssertionInt64(AssertNe, false, "EXPECT_NE", __FILE__, __LINE__, \
                 __FUNCTION__, WANT, #WANT, GOT, #GOT, " " __VA_ARGS__)
#define ASSERT_LE(WANT, GOT, ...)                                 \
  AssertionInt64(AssertLe, true, "ASSERT_LE", __FILE__, __LINE__, \
                 __FUNCTION__, WANT, #WANT, GOT, #GOT, " " __VA_ARGS__)
#define EXPECT_LE(WANT, GOT, ...)                                  \
  AssertionInt64(AssertLe, false, "EXPECT_LE", __FILE__, __LINE__, \
                 __FUNCTION__, WANT, #WANT, GOT, #GOT, " " __VA_ARGS__)
#define ASSERT_GE(WANT, GOT, ...)                                 \
  AssertionInt64(AssertGe, true, "ASSERT_GE", __FILE__, __LINE__, \
                 __FUNCTION__, WANT, #WANT, GOT, #GOT, " " __VA_ARGS__)
#define EXPECT_GE(WANT, GOT, ...)                                  \
  AssertionInt64(AssertGe, false, "EXPECT_GE", __FILE__, __LINE__, \
                 __FUNCTION__, WANT, #WANT, GOT, #GOT, " " __VA_ARGS__)
#define ASSERT_LT(WANT, GOT, ...)                                 \
  AssertionInt64(AssertLt, true, "ASSERT_LT", __FILE__, __LINE__, \
                 __FUNCTION__, WANT, #WANT, GOT, #GOT, " " __VA_ARGS__)
#define EXPECT_LT(WANT, GOT, ...)                                  \
  AssertionInt64(AssertLt, false, "EXPECT_LT", __FILE__, __LINE__, \
                 __FUNCTION__, WANT, #WANT, GOT, #GOT, " " __VA_ARGS__)
#define ASSERT_GT(WANT, GOT, ...)                                 \
  AssertionInt64(AssertGt, true, "ASSERT_GT", __FILE__, __LINE__, \
                 __FUNCTION__, WANT, #WANT, GOT, #GOT, " " __VA_ARGS__)
#define EXPECT_GT(WANT, GOT, ...)                                  \
  AssertionInt64(AssertGt, false, "EXPECT_GT", __FILE__, __LINE__, \
                 __FUNCTION__, WANT, #WANT, GOT, #GOT, " " __VA_ARGS__)
#define ASSERT_STREQ(WANT, GOT, ...)                                  \
  AssertionStr(AssertStreq, true, "ASSERT_STREQ", __FILE__, __LINE__, \
               __FUNCTION__, WANT, #WANT, GOT, #GOT, " " __VA_ARGS__)
#define EXPECT_STREQ(WANT, GOT, ...)                                   \
  AssertionStr(AssertStreq, false, "EXPECT_STREQ", __FILE__, __LINE__, \
               __FUNCTION__, WANT, #WANT, GOT, #GOT, " " __VA_ARGS__)
#define ASSERT_TRUE(GOT, ...)                                            \
  AssertionBool(true, true, __FILE__, __LINE__, __FUNCTION__, GOT, #GOT, \
                " " __VA_ARGS__)
#define EXPECT_TRUE(GOT, ...)                                             \
  AssertionBool(true, false, __FILE__, __LINE__, __FUNCTION__, GOT, #GOT, \
                " " __VA_ARGS__)
#define ASSERT_FALSE(GOT, ...)                                            \
  AssertionBool(false, true, __FILE__, __LINE__, __FUNCTION__, GOT, #GOT, \
                " " __VA_ARGS__)
#define EXPECT_FALSE(GOT, ...)                                             \
  AssertionBool(false, false, __FILE__, __LINE__, __FUNCTION__, GOT, #GOT, \
                " " __VA_ARGS__)

#define ASSERT_LDBL_EQ(WANT, GOT, ...)                                      \
  do {                                                                      \
    long double Got, Want;                                                  \
    Got = GOT;                                                              \
    Want = WANT;                                                            \
    if (isnan(Got) || isnan(Want) || fabsl(Got - Want) > 0.00000001) {      \
      fprintf(stderr, "error:%s:%d: %s() ASSERT_LDBL_EQ failed:", __FILE__, \
              __LINE__, __FUNCTION__);                                      \
      fprintf(stderr, " " __VA_ARGS__);                                     \
      fprintf(stderr,                                                       \
              "\n\twant %Lg (%s)\n"                                         \
              "\tgot  %Lg (%s)\n",                                          \
              Want, #WANT, Got, #GOT);                                      \
      exit(1);                                                              \
    }                                                                       \
  } while (0)

struct Test {
  void (*f)(void);
};

struct Tests {
  int n, f;
  struct Test *p;
} g_tests;

void SetUp(void);
void TearDown(void);

static void AssertionInt64(bool pred(int64_t, int64_t), bool isfatal,
                           const char *test, const char *file, int line,
                           const char *func, int64_t want, const char *wantstr,
                           int64_t got, const char *gotstr, const char *fmt,
                           ...) {
  va_list va;
  if (!pred(want, got)) {
    fprintf(stderr, "error:%s:%d: %s() %s failed:", file, line, func, test);
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
    fprintf(stderr,
            "\n\twant %" PRId64 " (%#" PRIx64 ") %s\n"
            "\tgot  %" PRId64 " (%#" PRIx64 ") %s\n",
            want, want, wantstr, got, got, gotstr);
    if (isfatal) {
      exit(1);
    } else {
      ++g_tests.f;
    }
  }
}

static void AssertionStr(bool pred(const char *, const char *), bool isfatal,
                         const char *test, const char *file, int line,
                         const char *func, const char *want,
                         const char *wantstr, const char *got,
                         const char *gotstr, const char *fmt, ...) {
  va_list va;
  if (!pred(want, got)) {
    fprintf(stderr, "error:%s:%d: %s() %s failed:", file, line, func, test);
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
    fprintf(stderr,
            "\n\twant %s (%s)\n"
            "\tgot  %s (%s)\n",
            want, wantstr, got, gotstr);
    if (isfatal) {
      exit(1);
    } else {
      ++g_tests.f;
    }
  }
}

static void AssertionBool(bool need, bool isfatal, const char *file, int line,
                          const char *func, bool got, const char *gotstr,
                          const char *fmt, ...) {
  va_list va;
  if (got != need) {
    fprintf(stderr, "error:%s:%d: %s() ASSERT_%s failed:", file, line, func,
            need ? "TRUE" : "FALSE");
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
    fprintf(stderr, "\n\t%s\n", gotstr);
    if (isfatal) {
      exit(1);
    } else {
      ++g_tests.f;
    }
  }
}

static bool AssertStreq(const char *want, const char *got) {
  return !strcmp(want, got);
}

static bool AssertEq(int64_t want, int64_t got) {
  return want == got;
}

static bool AssertNe(int64_t want, int64_t got) {
  return want != got;
}

static bool AssertLe(int64_t want, int64_t got) {
  return want <= got;
}

static bool AssertGe(int64_t want, int64_t got) {
  return want >= got;
}

static bool AssertLt(int64_t want, int64_t got) {
  return want < got;
}

static bool AssertGt(int64_t want, int64_t got) {
  return want > got;
}

int main(int argc, char *argv[]) {
  size_t i;
  for (i = 0; i < g_tests.n; ++i) {
    SetUp();
    g_tests.p[i].f();
    TearDown();
  }
  return g_tests.f;
}

#endif /* TEST_TEST_H_ */
