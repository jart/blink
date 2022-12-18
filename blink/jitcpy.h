#ifndef BLINK_MEMCPY_H_
#define BLINK_MEMCPY_H_
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#if defined(__GNUC__) && defined(__x86_64__)

// Musl Libc has a very slow memcpy() impl for x86
// This is the best for modern x86 when n is small
static inline void jitcpy(void *dst, const void *src, size_t n) {
  char *d;
  size_t i;
  uint64_t a, b;
  const char *s;
  d = (char *)dst;
  s = (const char *)src;
  switch (n) {
    case 0:
      return;
    case 1:
      *d = *s;
      return;
    case 2:
      __builtin_memcpy(&a, s, 2);
      __builtin_memcpy(d, &a, 2);
      return;
    case 3:
      __builtin_memcpy(&a, s, 2);
      __builtin_memcpy(&b, s + 1, 2);
      __builtin_memcpy(d, &a, 2);
      __builtin_memcpy(d + 1, &b, 2);
      return;
    case 4:
      __builtin_memcpy(&a, s, 4);
      __builtin_memcpy(d, &a, 4);
      return;
    case 5:
    case 6:
    case 7:
      __builtin_memcpy(&a, s, 4);
      __builtin_memcpy(&b, s + n - 4, 4);
      __builtin_memcpy(d, &a, 4);
      __builtin_memcpy(d + n - 4, &b, 4);
      return;
    case 8:
      __builtin_memcpy(&a, s, 8);
      __builtin_memcpy(d, &a, 8);
      return;
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
      return;
    default:
      i = 0;
      do {
        __builtin_memcpy(&a, s + i, 8);
        asm volatile("" ::: "memory");
        __builtin_memcpy(d + i, &a, 8);
      } while ((i += 8) + 8 <= n);
      for (; i < n; ++i) d[i] = s[i];
      return;
  }
}

#else
#define jitcpy(x, y, z) memcpy(x, y, z)
#endif /* GNUC && X86_64 */
#endif /* BLINK_MEMCPY_H_ */
