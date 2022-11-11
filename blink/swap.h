#ifndef BLINK_SWAP_H_
#define BLINK_SWAP_H_

#ifdef __GNUC__
#define SWAP16(x) __builtin_bswap16(x)
#define SWAP32(x) __builtin_bswap32(x)
#define SWAP64(x) __builtin_bswap64(x)
#else
#define SWAP16(x)                           \
  (((0x00000000000000ffull & (x)) << 010) | \
   ((0x000000000000ff00ull & (x)) >> 010))
#define SWAP32(x)                           \
  (((0x00000000000000ffull & (x)) << 030) | \
   ((0x000000000000ff00ull & (x)) << 010) | \
   ((0x0000000000ff0000ull & (x)) >> 010) | \
   ((0x00000000ff000000ull & (x)) >> 030))
#define SWAP64(x)                           \
  (((0x00000000000000ffull & (x)) << 070) | \
   ((0x000000000000ff00ull & (x)) << 050) | \
   ((0x0000000000ff0000ull & (x)) << 030) | \
   ((0x00000000ff000000ull & (x)) << 010) | \
   ((0x000000ff00000000ull & (x)) >> 010) | \
   ((0x0000ff0000000000ull & (x)) >> 030) | \
   ((0x00ff000000000000ull & (x)) >> 050) | \
   ((0xff00000000000000ull & (x)) >> 070))
#endif

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define SWAP16LE(x) SWAP16(x)
#define SWAP32LE(x) SWAP32(x)
#define SWAP64LE(x) SWAP64(x)
#else
#define SWAP16LE(x) (x)
#define SWAP32LE(x) (x)
#define SWAP64LE(x) (x)
#endif

#endif /* BLINK_SWAP_H_ */
