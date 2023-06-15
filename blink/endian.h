#ifndef BLINK_ENDIAN_H_
#define BLINK_ENDIAN_H_
#include "blink/builtin.h"
#include "blink/swap.h"
#include "blink/types.h"

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
  return *p;
}

MICRO_OP_SAFE u16 Get16(const u8 *p) {
#ifdef __OPTIMIZE__
  u16 x;
  __builtin_memcpy(&x, p, 16 / 8);
  return Little16(x);
#else
  return p[1] << 8 | p[0];
#endif
}

MICRO_OP_SAFE u32 Get32(const u8 *p) {
#ifdef __OPTIMIZE__
  u32 x;
  __builtin_memcpy(&x, p, 32 / 8);
  return Little32(x);
#else
  return ((u32)p[0] << 000 |  //
          (u32)p[1] << 010 |  //
          (u32)p[2] << 020 |  //
          (u32)p[3] << 030);
#endif
}

MICRO_OP_SAFE u64 Get64(const u8 *p) {
#ifdef __OPTIMIZE__
  u64 x;
  __builtin_memcpy(&x, p, 64 / 8);
  return Little64(x);
#else
  return ((u64)p[0] << 000 |  //
          (u64)p[1] << 010 |  //
          (u64)p[2] << 020 |  //
          (u64)p[3] << 030 |  //
          (u64)p[4] << 040 |  //
          (u64)p[5] << 050 |  //
          (u64)p[6] << 060 |  //
          (u64)p[7] << 070);
#endif
}

MICRO_OP_SAFE void Put8(u8 *p, u8 v) {
  *p = v;
}

MICRO_OP_SAFE void Put16(u8 *p, u16 v) {
#ifdef __OPTIMIZE__
  v = Little16(v);
  __builtin_memcpy(p, &v, 16 / 8);
#else
  p[0] = v >> 000;
  p[1] = v >> 010;
#endif
}

MICRO_OP_SAFE void Put32(u8 *p, u32 v) {
#ifdef __OPTIMIZE__
  v = Little32(v);
  __builtin_memcpy(p, &v, 32 / 8);
#else
  p[0] = v >> 000;
  p[1] = v >> 010;
  p[2] = v >> 020;
  p[3] = v >> 030;
#endif
}

MICRO_OP_SAFE void Put64(u8 *p, u64 v) {
#ifdef __OPTIMIZE__
  v = Little64(v);
  __builtin_memcpy(p, &v, 64 / 8);
#else
  p[0] = v >> 000;
  p[1] = v >> 010;
  p[2] = v >> 020;
  p[3] = v >> 030;
  p[4] = v >> 040;
  p[5] = v >> 050;
  p[6] = v >> 060;
  p[7] = v >> 070;
#endif
}

#define Read8  Get8
#define Read16 Get16
#define Read32 Get32
#define Read64 Get64

#define Write8  Put8
#define Write16 Put16
#define Write32 Put32
#define Write64 Put64

#endif /* BLINK_ENDIAN_H_ */
