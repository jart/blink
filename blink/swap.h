#ifndef BLINK_SWAP_H_
#define BLINK_SWAP_H_
#include <stdint.h>

#include "blink/builtin.h"

#if __has_builtin(__builtin_bswap64) || defined(__GNUC__)
#define SWAP16(x) __builtin_bswap16(x)
#define SWAP32(x) __builtin_bswap32(x)
#define SWAP64(x) __builtin_bswap64(x)
#else
#define SWAP16(x)                                  \
  (((UINT64_C(0x00000000000000ff) & (x)) << 010) | \
   ((UINT64_C(0x000000000000ff00) & (x)) >> 010))
#define SWAP32(x)                                  \
  (((UINT64_C(0x00000000000000ff) & (x)) << 030) | \
   ((UINT64_C(0x000000000000ff00) & (x)) << 010) | \
   ((UINT64_C(0x0000000000ff0000) & (x)) >> 010) | \
   ((UINT64_C(0x00000000ff000000) & (x)) >> 030))
#define SWAP64(x)                                  \
  (((UINT64_C(0x00000000000000ff) & (x)) << 070) | \
   ((UINT64_C(0x000000000000ff00) & (x)) << 050) | \
   ((UINT64_C(0x0000000000ff0000) & (x)) << 030) | \
   ((UINT64_C(0x00000000ff000000) & (x)) << 010) | \
   ((UINT64_C(0x000000ff00000000) & (x)) >> 010) | \
   ((UINT64_C(0x0000ff0000000000) & (x)) >> 030) | \
   ((UINT64_C(0x00ff000000000000) & (x)) >> 050) | \
   ((UINT64_C(0xff00000000000000) & (x)) >> 070))
#endif

#endif /* BLINK_SWAP_H_ */
