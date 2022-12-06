#ifndef BLINK_ENDIAN_H_
#define BLINK_ENDIAN_H_
#include "blink/builtin.h"
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

MICRO_OP_SAFE u8 Read8(const u8 *p) {
  return p[0];
}

MICRO_OP_SAFE u16 Read16(const u8 *p) {
  return p[1] << 8 | p[0];
}

MICRO_OP_SAFE u32 Read32(const u8 *p) {
  return ((u32)p[0] << 000 |  //
          (u32)p[1] << 010 |  //
          (u32)p[2] << 020 |  //
          (u32)p[3] << 030);
}

MICRO_OP_SAFE u64 Read64(const u8 *p) {
  return ((u64)p[0] << 000 |  //
          (u64)p[1] << 010 |  //
          (u64)p[2] << 020 |  //
          (u64)p[3] << 030 |  //
          (u64)p[4] << 040 |  //
          (u64)p[5] << 050 |  //
          (u64)p[6] << 060 |  //
          (u64)p[7] << 070);
}

MICRO_OP_SAFE void Write8(u8 *p, u8 v) {
  *p = v;
}

MICRO_OP_SAFE void Write16(u8 *p, u16 v) {
  p[0] = (0x00FF & v) >> 000;
  p[1] = (0xFF00 & v) >> 010;
}

MICRO_OP_SAFE void Write32(u8 *p, u32 v) {
  p[0] = (0x000000FF & v) >> 000;
  p[1] = (0x0000FF00 & v) >> 010;
  p[2] = (0x00FF0000 & v) >> 020;
  p[3] = (0xFF000000 & v) >> 030;
}

MICRO_OP_SAFE void Write64(u8 *p, u64 v) {
  p[0] = (0x00000000000000FF & v) >> 000;
  p[1] = (0x000000000000FF00 & v) >> 010;
  p[2] = (0x0000000000FF0000 & v) >> 020;
  p[3] = (0x00000000FF000000 & v) >> 030;
  p[4] = (0x000000FF00000000 & v) >> 040;
  p[5] = (0x0000FF0000000000 & v) >> 050;
  p[6] = (0x00FF000000000000 & v) >> 060;
  p[7] = (0xFF00000000000000 & v) >> 070;
}

MICRO_OP_SAFE u8 Little8(u8 x) {
  return x;
}

MICRO_OP_SAFE u16 Little16(u16 x) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  return SWAP16(x);
#else
  return x;
#endif
}

MICRO_OP_SAFE u32 Little32(u32 x) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  return SWAP32(x);
#else
  return x;
#endif
}

MICRO_OP_SAFE u64 Little64(u64 x) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  return SWAP64(x);
#else
  return x;
#endif
}

MICRO_OP_SAFE u8 Get8(const u8 *p) {
  return Little8(*(u8 *)p);
}
MICRO_OP_SAFE u16 Get16(const u8 *p) {
  return Little16(*(u16 *)p);
}
MICRO_OP_SAFE u32 Get32(const u8 *p) {
  return Little32(*(u32 *)p);
}
MICRO_OP_SAFE u64 Get64(const u8 *p) {
  return Little64(*(u64 *)p);
}

MICRO_OP_SAFE void Put8(u8 *p, u8 x) {
  *(u8 *)p = Little8(x);
}
MICRO_OP_SAFE void Put16(u8 *p, u16 x) {
  *(u16 *)p = Little16(x);
}
MICRO_OP_SAFE void Put32(u8 *p, u32 x) {
  *(u32 *)p = Little32(x);
}
MICRO_OP_SAFE void Put64(u8 *p, u64 x) {
  *(u64 *)p = Little64(x);
}

#endif /* BLINK_ENDIAN_H_ */
