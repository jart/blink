#ifndef BLINK_ASSERT_H_
#define BLINK_ASSERT_H_
#include "blink/builtin.h"

#ifndef NDEBUG
#define unassert(x)                         \
  do {                                      \
    if (__builtin_expect(!(x), 0)) {        \
      AssertFailed(__FILE__, __LINE__, #x); \
    }                                       \
  } while (0)
#elif (__clang__ + __INTEL_COMPILER + _MSC_VER + 0 || \
       (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 405)
#define unassert(x)                  \
  do {                               \
    if (__builtin_expect(!(x), 0)) { \
      __builtin_unreachable();       \
    }                                \
  } while (0)
#else
#define unassert(x) ((void)(x))
#endif

_Noreturn void AssertFailed(const char *, int, const char *);

#endif /* BLINK_ASSERT_H_ */
