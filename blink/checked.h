#ifndef BLINK_CHECKED_H_
#define BLINK_CHECKED_H_
#include <errno.h>
#include <stdint.h>

#include "blink/builtin.h"

static long OnCheckedOverflow(void) {
  errno = EOVERFLOW;
  return -1;
}

/**
 * Computes 64-bit signed addition `*z = x + y`.
 * @return 0 on success, or -1 w/ errno
 * @raise EOVERFLOW
 */
static inline long CheckedAdd(int64_t x, int64_t y, int64_t *z) {
#if (defined(__GNUC__) && __GNUC__ >= 5 && !defined(__ICC)) || \
    __has_builtin(__builtin_add_overflow)
  if (!__builtin_add_overflow(x, y, z)) {
    return 0;
  }
#else
  uint64_t a, b, c;
  a = x, b = y, c = a + b;
  if (!(((c ^ a) & (c ^ b)) >> 63)) {
    return *z = c, 0;
  }
#endif
  return OnCheckedOverflow();
}

/**
 * Computes 64-bit signed subtraction `*z = x - y`.
 * @return 0 on success, or -1 w/ errno
 * @raise EOVERFLOW
 */
static inline long CheckedSub(int64_t x, int64_t y, int64_t *z) {
#if (defined(__GNUC__) && __GNUC__ >= 5 && !defined(__ICC)) || \
    __has_builtin(__builtin_sub_overflow)
  if (!__builtin_sub_overflow(x, y, z)) {
    return 0;
  }
#else
  uint64_t a, b, c;
  a = x, b = y, c = a - b;
  if (!(((c ^ a) & (a ^ b)) >> 63)) {
    return *z = c, 0;
  }
#endif
  return OnCheckedOverflow();
}

/**
 * Computes 64-bit unsigned multiplication `*z = x * y`.
 * @return 0 on success, or -1 w/ errno
 * @raise EOVERFLOW
 */
static inline long CheckedMul(uint64_t x, uint64_t y, uint64_t *z) {
#if (defined(__GNUC__) && __GNUC__ >= 5 && !defined(__ICC)) || \
    __has_builtin(__builtin_mul_overflow)
  // see also https://bugs.llvm.org/show_bug.cgi?id=16404
  // see also https://gcc.gnu.org/bugzilla/show_bug.cgi?id=91450
  if (!__builtin_mul_overflow(x, y, z)) {
    return 0;
  }
#else
  uint64_t t = x * y;
  if (!((x | y) & 0xffffffff && t / x != y)) {
    return *z = t, 0;
  }
#endif
  return OnCheckedOverflow();
}

#endif /* BLINK_CHECKED_H_ */
