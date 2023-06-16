#ifndef BLINK_CHECKED_H_
#define BLINK_CHECKED_H_
#if defined(__has_include) && __has_include(<stdckdint.h>)
#include <stdckdint.h>
#elif defined(NDEBUG) &&                                                  \
    ((defined(__GNUC__) && __GNUC__ >= 5 && !defined(__ICC)) ||           \
     (defined(__has_builtin) && (__has_builtin(__builtin_add_overflow) && \
                                 __has_builtin(__builtin_sub_overflow) && \
                                 __has_builtin(__builtin_mul_overflow))))

#define ckd_add(res, x, y) __builtin_add_overflow((x), (y), (res))
#define ckd_sub(res, x, y) __builtin_sub_overflow((x), (y), (res))
#define ckd_mul(res, x, y) __builtin_mul_overflow((x), (y), (res))

#else
#include <limits.h>
#include <stdint.h>

#define ckd_add(res, x, y) __ckd_arithmetic(add, res, x, y)
#define ckd_sub(res, x, y) __ckd_arithmetic(sub, res, x, y)
#define ckd_mul(res, x, y) __ckd_arithmetic(mul, res, x, y)

#define __ckd_arithmetic(op, res, x, y)                           \
  (sizeof(*(res)) == sizeof(int) ? __ckd_##op((int *)(res), x, y) \
   : sizeof(*(res)) == sizeof(long long)                          \
       ? __ckd_##op##ll((long long *)(res), x, y)                 \
       : __ckd_trap())

#define __ckd_mul_overflow(x, y, min, max) \
  if (x > 0) {                             \
    if (y > 0) {                           \
      if (x > max / y) {                   \
        return 1;                          \
      }                                    \
    } else {                               \
      if (y < min / x) {                   \
        return 1;                          \
      }                                    \
    }                                      \
  } else {                                 \
    if (y > 0) {                           \
      if (x < min / y) {                   \
        return 1;                          \
      }                                    \
    } else {                               \
      if (x && y < max / x) {              \
        return 1;                          \
      }                                    \
    }                                      \
  }                                        \
  return 0

static inline int __ckd_trap(void) {
  volatile int __x = 0;
  return 1 / __x;
}

static inline int __ckd_add(int *__z, int __x, int __y) {
  unsigned int __a, __b, __c;
  *__z = __c = (__a = __x) + (__b = __y);
  return ((__c ^ __a) & (__c ^ __b)) >> (sizeof(int) * CHAR_BIT - 1);
}
static inline int __ckd_addll(long long *__z, long long __x, long long __y) {
  unsigned long long __a, __b, __c;
  *__z = __c = (__a = __x) + (__b = __y);
  return ((__c ^ __a) & (__c ^ __b)) >> (sizeof(long long) * CHAR_BIT - 1);
}

static inline int __ckd_sub(int *__z, int __x, int __y) {
  unsigned int __a, __b, __c;
  *__z = __c = (__a = __x) - (__b = __y);
  return ((__a ^ __b) & (__c ^ __a)) >> (sizeof(int) * CHAR_BIT - 1);
}
static inline int __ckd_subll(long long *__z, long long __x, long long __y) {
  unsigned long long __a, __b, __c;
  *__z = __c = (__a = __x) - (__b = __y);
  return ((__a ^ __b) & (__c ^ __a)) >> (sizeof(long long) * CHAR_BIT - 1);
}

static inline int __ckd_mul(int *__z, int __x, int __y) {
  *__z = (unsigned)__x * (unsigned)__y;
  __ckd_mul_overflow(__x, __y, INT_MIN, INT_MAX);
}
static inline int __ckd_mulll(long long *__z, long long __x, long long __y) {
  *__z = (unsigned long long)__x * (unsigned long long)__y;
  __ckd_mul_overflow(__x, __y, LLONG_MIN, LLONG_MAX);
}

#endif
#endif /* BLINK_CHECKED_H_ */
