#ifndef BLINK_LIKELY_H_
#define BLINK_LIKELY_H_
#include "blink/builtin.h"

#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#if __GNUC__ + 0 >= 9 && !defined(__chibicc__)
#define VERY_LIKELY(x) __builtin_expect_with_probability(!!(x), 1, 0.999)
#else
#define VERY_LIKELY(x) LIKELY(x)
#endif

#if __GNUC__ + 0 >= 9 && !defined(__chibicc__)
#define VERY_UNLIKELY(x) __builtin_expect_with_probability(!!(x), 0, 0.999)
#else
#define VERY_UNLIKELY(x) UNLIKELY(x)
#endif

#endif /* BLINK_LIKELY_H_ */
