#ifndef BLINK_MEMCPY_H_
#define BLINK_MEMCPY_H_
#include <stddef.h>
#include <stdint.h>

#include "blink/builtin.h"

#if !defined(TINY) && defined(__x86_64__) && defined(__GNUC__) && \
    !defined(__COSMOPOLITAN__) && !defined(__GLIBC__)

#define memcpy(x, y, z) BetterMemcpyX86(x, y, z)

forceinline void *RepMovsb(void *di, const void *si, size_t cx) {
  asm("rep movsb"
      : "=D"(di), "=S"(si), "=c"(cx), "=m"(*(char(*)[cx])di)
      : "0"(di), "1"(si), "2"(cx), "m"(*(char(*)[cx])si));
  return di;
}

static inline void *BetterMemcpyX86(void *dst, const void *src, size_t n) {
  char *d;
  size_t i;
  uint64_t a, b;
  const char *s;
  d = dst;
  s = src;
  switch (n) {
    case 0:
      return d;
    case 1:
      *d = *s;
      return d;
    case 2:
      __builtin_memcpy(&a, s, 2);
      __builtin_memcpy(d, &a, 2);
      return d;
    case 3:
      __builtin_memcpy(&a, s, 2);
      __builtin_memcpy(&b, s + 1, 2);
      __builtin_memcpy(d, &a, 2);
      __builtin_memcpy(d + 1, &b, 2);
      return d;
    case 4:
      __builtin_memcpy(&a, s, 4);
      __builtin_memcpy(d, &a, 4);
      return d;
    case 5:
    case 6:
    case 7:
      __builtin_memcpy(&a, s, 4);
      __builtin_memcpy(&b, s + n - 4, 4);
      __builtin_memcpy(d, &a, 4);
      __builtin_memcpy(d + n - 4, &b, 4);
      return d;
    case 8:
      __builtin_memcpy(&a, s, 8);
      __builtin_memcpy(d, &a, 8);
      return d;
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
      __builtin_memcpy(&a, s, 8);
      __builtin_memcpy(&b, s + n - 8, 8);
      __builtin_memcpy(d, &a, 8);
      __builtin_memcpy(d + n - 8, &b, 8);
      return d;
    default:
      if (n <= 64) {
        i = 0;
        do {
          __builtin_memcpy(&a, s + i, 8);
          asm volatile("" ::: "memory");
          __builtin_memcpy(d + i, &a, 8);
        } while ((i += 8) + 8 <= n);
        for (; i < n; ++i) d[i] = s[i];
        return d;
      } else {
        RepMovsb(d, s, n);
        return d;
      }
  }
}

#endif /* MUSL */
#endif /* BLINK_MEMCPY_H_ */
