#ifndef TEST_TEST_H_
#define TEST_TEST_H_
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST(GROUP, NAME)                                             \
  void GROUP##_##NAME(void);                                          \
  __attribute__((__constructor__)) void GROUP##_##NAME##_init(void) { \
    g_tests.p = realloc(g_tests.p, ++g_tests.n * sizeof(*g_tests.p)); \
    g_tests.p[g_tests.n - 1].f = GROUP##_##NAME;                      \
  }                                                                   \
  void GROUP##_##NAME(void)

#define EXPECT_STREQ(WANT, GOT, ...)                                   \
  do {                                                                 \
    const char *Got, *Want;                                            \
    Got = (GOT);                                                       \
    Want = (WANT);                                                     \
    if (strcmp(Want, Got)) {                                           \
      fprintf(stderr, "error:%s:%d: %s() EXPECT_EQ failed:", __FILE__, \
              __LINE__, __FUNCTION__);                                 \
      fprintf(stderr, " " __VA_ARGS__);                                \
      fprintf(stderr,                                                  \
              "\n\twant %s (%s)\n"                                     \
              "\tgot  %s (%s)\n",                                      \
              Want, #WANT, Got, #GOT);                                 \
      ++g_tests.f;                                                     \
    }                                                                  \
  } while (0)

#define EXPECT_EQ(WANT, GOT, ...)                                      \
  do {                                                                 \
    intptr_t Got, Want;                                                \
    Got = (intptr_t)(GOT);                                             \
    Want = (intptr_t)(WANT);                                           \
    if (Want != Got) {                                                 \
      fprintf(stderr, "error:%s:%d: %s() EXPECT_EQ failed:", __FILE__, \
              __LINE__, __FUNCTION__);                                 \
      fprintf(stderr, " " __VA_ARGS__);                                \
      fprintf(stderr,                                                  \
              "\n\twant %" PRIdPTR " (%#" PRIxPTR ") %s\n"             \
              "\tgot  %" PRIdPTR " (%#" PRIxPTR ") %s\n",              \
              Want, Want, #WANT, Got, Got, #GOT);                      \
      ++g_tests.f;                                                     \
    }                                                                  \
  } while (0)

#define ASSERT_EQ(WANT, GOT, ...)                                      \
  do {                                                                 \
    intptr_t Got, Want;                                                \
    Got = (intptr_t)(GOT);                                             \
    Want = (intptr_t)(WANT);                                           \
    if (Want != Got) {                                                 \
      fprintf(stderr, "error:%s:%d: %s() ASSERT_EQ failed:", __FILE__, \
              __LINE__, __FUNCTION__);                                 \
      fprintf(stderr, " " __VA_ARGS__);                                \
      fprintf(stderr,                                                  \
              "\n\twant %" PRIdPTR " (%#" PRIxPTR ") %s\n"             \
              "\tgot  %" PRIdPTR " (%#" PRIxPTR ") %s\n",              \
              Want, Want, #WANT, Got, Got, #GOT);                      \
      exit(1);                                                         \
    }                                                                  \
  } while (0)

#define EXPECT_NE(WANT, GOT, ...)                                      \
  do {                                                                 \
    intptr_t Got, Want;                                                \
    Got = (intptr_t)(GOT);                                             \
    Want = (intptr_t)(WANT);                                           \
    if (Want == Got) {                                                 \
      fprintf(stderr, "error:%s:%d: %s() EXPECT_NE failed:", __FILE__, \
              __LINE__, __FUNCTION__);                                 \
      fprintf(stderr, " " __VA_ARGS__);                                \
      fprintf(stderr,                                                  \
              "\n\twant %" PRIdPTR " (%#" PRIxPTR ") %s\n"             \
              "\tgot  %" PRIdPTR " (%#" PRIxPTR ") %s\n",              \
              Want, Want, #WANT, Got, Got, #GOT);                      \
      ++g_tests.f;                                                     \
    }                                                                  \
  } while (0)

#define ASSERT_NE(WANT, GOT, ...)                                      \
  do {                                                                 \
    intptr_t Got, Want;                                                \
    Got = (intptr_t)(GOT);                                             \
    Want = (intptr_t)(WANT);                                           \
    if (Want == Got) {                                                 \
      fprintf(stderr, "error:%s:%d: %s() ASSERT_NE failed:", __FILE__, \
              __LINE__, __FUNCTION__);                                 \
      fprintf(stderr, " " __VA_ARGS__);                                \
      fprintf(stderr,                                                  \
              "\n\twant %" PRIdPTR " (%#" PRIxPTR ") %s\n"             \
              "\tgot  %" PRIdPTR " (%#" PRIxPTR ") %s\n",              \
              Want, Want, #WANT, Got, Got, #GOT);                      \
      exit(1);                                                         \
    }                                                                  \
  } while (0)

#define ASSERT_LE(WANT, GOT, ...)                                      \
  do {                                                                 \
    intptr_t Got, Want;                                                \
    Got = (intptr_t)(GOT);                                             \
    Want = (intptr_t)(WANT);                                           \
    if (Want > Got) {                                                  \
      fprintf(stderr, "error:%s:%d: %s() ASSERT_LE failed:", __FILE__, \
              __LINE__, __FUNCTION__);                                 \
      fprintf(stderr, " " __VA_ARGS__);                                \
      fprintf(stderr,                                                  \
              "\n\t𝑥 %" PRIdPTR " (%#" PRIxPTR ") %s\n"                \
              "\t𝑦 %" PRIdPTR " (%#" PRIxPTR ") %s\n",                 \
              Want, Want, #WANT, Got, Got, #GOT);                      \
      exit(1);                                                         \
    }                                                                  \
  } while (0)

#define EXPECT_LE(WANT, GOT, ...)                                      \
  do {                                                                 \
    intptr_t Got, Want;                                                \
    Got = (intptr_t)(GOT);                                             \
    Want = (intptr_t)(WANT);                                           \
    if (Want > Got) {                                                  \
      fprintf(stderr, "error:%s:%d: %s() EXPECT_LE failed:", __FILE__, \
              __LINE__, __FUNCTION__);                                 \
      fprintf(stderr, " " __VA_ARGS__);                                \
      fprintf(stderr,                                                  \
              "\n\t𝑥 %" PRIdPTR " (%#" PRIxPTR ") %s\n"                \
              "\t𝑦 %" PRIdPTR " (%#" PRIxPTR ") %s\n",                 \
              Want, Want, #WANT, Got, Got, #GOT);                      \
      ++g_tests.f;                                                     \
    }                                                                  \
  } while (0)

#define ASSERT_GE(WANT, GOT, ...)                                      \
  do {                                                                 \
    intptr_t Got, Want;                                                \
    Got = (intptr_t)(GOT);                                             \
    Want = (intptr_t)(WANT);                                           \
    if (Want < Got) {                                                  \
      fprintf(stderr, "error:%s:%d: %s() ASSERT_GE failed:", __FILE__, \
              __LINE__, __FUNCTION__);                                 \
      fprintf(stderr, " " __VA_ARGS__);                                \
      fprintf(stderr,                                                  \
              "\n\t𝑥 %" PRIdPTR " (%#" PRIxPTR ") %s\n"                \
              "\t𝑦 %" PRIdPTR " (%#" PRIxPTR ") %s\n",                 \
              Want, Want, #WANT, Got, Got, #GOT);                      \
      exit(1);                                                         \
    }                                                                  \
  } while (0)

#define EXPECT_GE(WANT, GOT, ...)                                      \
  do {                                                                 \
    intptr_t Got, Want;                                                \
    Got = (intptr_t)(GOT);                                             \
    Want = (intptr_t)(WANT);                                           \
    if (Want < Got) {                                                  \
      fprintf(stderr, "error:%s:%d: %s() EXPECT_GE failed:", __FILE__, \
              __LINE__, __FUNCTION__);                                 \
      fprintf(stderr, " " __VA_ARGS__);                                \
      fprintf(stderr,                                                  \
              "\n\t𝑥 %" PRIdPTR " (%#" PRIxPTR ") %s\n"                \
              "\t𝑦 %" PRIdPTR " (%#" PRIxPTR ") %s\n",                 \
              Want, Want, #WANT, Got, Got, #GOT);                      \
      ++g_tests.f;                                                     \
    }                                                                  \
  } while (0)

#define ASSERT_TRUE(GOT, ...)                                            \
  do {                                                                   \
    intptr_t Got;                                                        \
    Got = (intptr_t)(GOT);                                               \
    if (!Got) {                                                          \
      fprintf(stderr, "error:%s:%d: %s() ASSERT_TRUE failed:", __FILE__, \
              __LINE__, __FUNCTION__);                                   \
      fprintf(stderr, " " __VA_ARGS__);                                  \
      fprintf(stderr, "\n\t%s\n", #GOT);                                 \
      exit(1);                                                           \
    }                                                                    \
  } while (0)

#define EXPECT_TRUE(GOT, ...)                                            \
  do {                                                                   \
    intptr_t Got;                                                        \
    Got = (intptr_t)(GOT);                                               \
    if (!Got) {                                                          \
      fprintf(stderr, "error:%s:%d: %s() EXPECT_TRUE failed:", __FILE__, \
              __LINE__, __FUNCTION__);                                   \
      fprintf(stderr, " " __VA_ARGS__);                                  \
      fprintf(stderr, "\n\t%s\n", #GOT);                                 \
      ++g_tests.f;                                                       \
    }                                                                    \
  } while (0)

#define EXPECT_FALSE(GOT, ...)                                            \
  do {                                                                    \
    intptr_t Got;                                                         \
    Got = (intptr_t)(GOT);                                                \
    if (Got) {                                                            \
      fprintf(stderr, "error:%s:%d: %s() EXPECT_FALSE failed:", __FILE__, \
              __LINE__, __FUNCTION__);                                    \
      fprintf(stderr, " " __VA_ARGS__);                                   \
      fprintf(stderr, "\n\t%s\n", #GOT);                                  \
      ++g_tests.f;                                                        \
    }                                                                     \
  } while (0)

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

struct Tests {
  int n, f;
  struct Test {
    void (*f)(void);
  } * p;
} g_tests;

void SetUp(void) __attribute__((__weak__));
void TearDown(void) __attribute__((__weak__));

int main(int argc, char *argv[]) {
  size_t i;
  for (i = 0; i < g_tests.n; ++i) {
    if (SetUp) SetUp();
    g_tests.p[i].f();
    if (TearDown) TearDown();
  }
  return g_tests.f;
}

#endif /* TEST_TEST_H_ */
