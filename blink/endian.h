#ifndef BLINK_ENDIAN_H_
#define BLINK_ENDIAN_H_
#include "blink/swap.h"
#include "blink/types.h"

/**
 * @fileoverview Word Serialization Utilities.
 *
 * These macros interact with `u8[N]` memory storing words, which are
 * serialized in little endian byte order.
 *
 * 1. The ReadN() and WriteN() functions offer defined behavior under
 *    all circumstances, and are usually optimized into a single move
 *    operation by modern compilers.
 *
 * 2. The GetN() and PutN() functions may be used when it's known for
 *    certain that an address is aligned on its respective word size,
 *    otherwise UBSAN will complain. These functions are like atomics
 *    but they're more relaxed than memory_order_relaxed, and they're
 *    more reliable when it comes to producing tinier faster code.
 *
 * 3. The LittleN() macros should be used with C11 atomic helpers, to
 *    perform the necessary byte swap on big endian platforms.
 *
 * These functions can only be used safely on thread specific memory,
 * e.g. registers; otherwise LoadN() and StoreN() should be used.
 */

#if !defined(__GNUC__) || __SIZEOF_LONG__ < 8

static inline u8 Read8(const u8 *p) {
  return p[0];
}

static inline u16 Read16(const u8 *p) {
  return p[1] << 8 | p[0];
}

static inline u32 Read32(const u8 *p) {
  return ((u32)p[0] << 000 |  //
          (u32)p[1] << 010 |  //
          (u32)p[2] << 020 |  //
          (u32)p[3] << 030);
}

static inline u64 Read64(const u8 *p) {
  return ((u64)p[0] << 000 |  //
          (u64)p[1] << 010 |  //
          (u64)p[2] << 020 |  //
          (u64)p[3] << 030 |  //
          (u64)p[4] << 040 |  //
          (u64)p[5] << 050 |  //
          (u64)p[6] << 060 |  //
          (u64)p[7] << 070);
}

static inline void Write8(u8 *p, u8 v) {
  *p = v;
}

static inline void Write16(u8 *p, u16 v) {
  p[0] = (0x00FF & v) >> 000;
  p[1] = (0xFF00 & v) >> 010;
}

static inline void Write32(u8 *p, u32 v) {
  p[0] = (0x000000FF & v) >> 000;
  p[1] = (0x0000FF00 & v) >> 010;
  p[2] = (0x00FF0000 & v) >> 020;
  p[3] = (0xFF000000 & v) >> 030;
}

static inline void Write64(u8 *p, u64 v) {
  p[0] = (0x00000000000000FF & v) >> 000;
  p[1] = (0x000000000000FF00 & v) >> 010;
  p[2] = (0x0000000000FF0000 & v) >> 020;
  p[3] = (0x00000000FF000000 & v) >> 030;
  p[4] = (0x000000FF00000000 & v) >> 040;
  p[5] = (0x0000FF0000000000 & v) >> 050;
  p[6] = (0x00FF000000000000 & v) >> 060;
  p[7] = (0xFF00000000000000 & v) >> 070;
}

static inline u16 Little16(u16 x) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  return SWAP16(x);
#else
  return x;
#endif
}

static inline u32 Little32(u32 x) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  return SWAP32(x);
#else
  return x;
#endif
}

static inline u64 Little64(u64 x) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  return SWAP64(x);
#else
  return x;
#endif
}

#else

#define Read8(P)         \
  ({                     \
    const u8 *Ptr = (P); \
    Ptr[0];              \
  })

#define Read16(P)         \
  ({                      \
    const u8 *Ptr = (P);  \
    Ptr[1] << 8 | Ptr[0]; \
  })

#define Read32(P)                                                   \
  ({                                                                \
    const u8 *Ptr = (P);                                            \
    ((u32)Ptr[0] << 000 | (u32)Ptr[1] << 010 | (u32)Ptr[2] << 020 | \
     (u32)Ptr[3] << 030);                                           \
  })

#define Read64(P)                                                   \
  ({                                                                \
    const u8 *Ptr = (P);                                            \
    ((u64)Ptr[0] << 000 | (u64)Ptr[1] << 010 | (u64)Ptr[2] << 020 | \
     (u64)Ptr[3] << 030 | (u64)Ptr[4] << 040 | (u64)Ptr[5] << 050 | \
     (u64)Ptr[6] << 060 | (u64)Ptr[7] << 070);                      \
  })

#define Write8(P, V) \
  do {               \
    u8 Val = (V);    \
    u8 *Ptr = (P);   \
    *Ptr = Val;      \
  } while (0)

#define Write16(P, V)                           \
  do {                                          \
    int Val = (V);                              \
    u8 *Ptr = (P);                              \
    Ptr[0] = (0x00000000000000FF & Val) >> 000; \
    Ptr[1] = (0x000000000000FF00 & Val) >> 010; \
  } while (0)

#define Write32(P, V)                           \
  do {                                          \
    u32 Val = (V);                              \
    u8 *Ptr = (P);                              \
    Ptr[0] = (0x00000000000000FF & Val) >> 000; \
    Ptr[1] = (0x000000000000FF00 & Val) >> 010; \
    Ptr[2] = (0x0000000000FF0000 & Val) >> 020; \
    Ptr[3] = (0x00000000FF000000 & Val) >> 030; \
  } while (0)

#define Write64(P, V)                           \
  do {                                          \
    u64 Val = (V);                              \
    u8 *Ptr = (P);                              \
    Ptr[0] = (0x00000000000000FF & Val) >> 000; \
    Ptr[1] = (0x000000000000FF00 & Val) >> 010; \
    Ptr[2] = (0x0000000000FF0000 & Val) >> 020; \
    Ptr[3] = (0x00000000FF000000 & Val) >> 030; \
    Ptr[4] = (0x000000FF00000000 & Val) >> 040; \
    Ptr[5] = (0x0000FF0000000000 & Val) >> 050; \
    Ptr[6] = (0x00FF000000000000 & Val) >> 060; \
    Ptr[7] = (0xFF00000000000000 & Val) >> 070; \
  } while (0)

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define Little16(V) \
  ({                \
    u16 Val = (V);  \
    SWAP16(Val);    \
  })
#define Little32(V) \
  ({                \
    u32 Val = (V);  \
    SWAP32(Val);    \
  })
#define Little64(V) \
  ({                \
    u64 Val = (V);  \
    SWAP64(Val);    \
  })
#else
#define Little16(V) (V)
#define Little32(V) (V)
#define Little64(V) (V)
#endif

#endif /* GNU64 */

#define Little8(V) (V)

static inline u8 Get8(const u8 *p) {
  return Little8(*(u8 *)p);
}
static inline u16 Get16(const u8 *p) {
  return Little16(*(u16 *)p);
}
static inline u32 Get32(const u8 *p) {
  return Little32(*(u32 *)p);
}
static inline u64 Get64(const u8 *p) {
  return Little64(*(u64 *)p);
}

static inline void Put8(u8 *p, u8 x) {
  *(u8 *)p = Little8(x);
}
static inline void Put16(u8 *p, u16 x) {
  *(u16 *)p = Little16(x);
}
static inline void Put32(u8 *p, u32 x) {
  *(u32 *)p = Little32(x);
}
static inline void Put64(u8 *p, u64 x) {
  *(u64 *)p = Little64(x);
}

#endif /* BLINK_ENDIAN_H_ */
