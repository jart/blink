#ifndef BLINK_ASSERT_H_
#define BLINK_ASSERT_H_
#include <assert.h>

#include "blink/builtin.h"

#if __clang__ + __INTEL_COMPILER + _MSC_VER + 0 || \
    (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 405
#define unassert(x)                  \
  do {                               \
    if (__builtin_expect(!(x), 0)) { \
      __builtin_unreachable();       \
    }                                \
  } while (0)
#else
#define unassert(x) assert(x)
#endif

#endif /* BLINK_ASSERT_H_ */
