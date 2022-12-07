/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2022 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "blink/builtin.h"
#include "blink/flags.h"
#include "blink/mop.h"

/**
 * @fileoverview Fast Arithmetic-Logic Unit
 *
 * This is intended for JIT'd code paths. Since we only enable JIT when
 * the virtual machine is in long mode, there's no need to compute that
 * costly legacy auxiliary flag, since x86-64 defines away BCD support.
 */

MICRO_OP_SAFE i64 FastNot8(u64 x, u64 y, u32 *f) {
  return ~x & 0xFF;
}

MICRO_OP_SAFE i64 FastNot16(u64 x, u64 y, u32 *f) {
  return ~x & 0xFFFF;
}

MICRO_OP_SAFE i64 FastNot32(u64 x, u64 y, u32 *f) {
  return ~x & 0xFFFFFFFF;
}

MICRO_OP_SAFE i64 FastNot64(u64 x, u64 y, u32 *f) {
  return ~x & 0xFFFFFFFFFFFFFFFF;
}

MICRO_OP_SAFE i64 FastFlags(u64 x, u32 *f, u32 of, u32 cf, u32 sf) {
  *f &= ~(1 << FLAGS_CF |  //
          1 << FLAGS_ZF |  //
          1 << FLAGS_SF |  //
          1 << FLAGS_OF |  //
          0xFF000000u);
  *f |= sf << FLAGS_SF |  //
        cf << FLAGS_CF |  //
        !x << FLAGS_ZF |  //
        of << FLAGS_OF |  //
        (x & 0xFF) << 24;
  return x;
}

MICRO_OP_SAFE i64 FastFlags8(u8 z, u32 *f, u32 of, u32 cf) {
  return FastFlags(z, f, of, cf, z >> 7);
}

MICRO_OP_SAFE i64 FastXor8(u64 x, u64 y, u32 *f) {
  return FastFlags8(x ^ y, f, 0, 0);
}

MICRO_OP_SAFE i64 FastOr8(u64 x, u64 y, u32 *f) {
  return FastFlags8(x | y, f, 0, 0);
}

MICRO_OP_SAFE i64 FastAnd8(u64 x, u64 y, u32 *f) {
  return FastFlags8(x & y, f, 0, 0);
}

MICRO_OP_SAFE i64 FastSub8(u64 x64, u64 y64, u32 *f) {
  bool cf, of;
  u8 x, y, z;
  x = x64;
  y = y64;
  z = x - y;
  cf = x < z;
  of = ((z ^ x) & (x ^ y)) >> 7;
  return FastFlags8(z, f, of, cf);
}

MICRO_OP_SAFE i64 FastAdd8(u64 x64, u64 y64, u32 *f) {
  bool cf, of;
  u8 x, y, z;
  x = x64;
  y = y64;
  z = x + y;
  cf = z < y;
  of = ((z ^ x) & (z ^ y)) >> 7;
  return FastFlags8(z, f, of, cf);
}

MICRO_OP_SAFE i64 FastFlags32(u32 z, u32 *f, u32 of, u32 cf) {
  return FastFlags(z, f, of, cf, z >> 31);
}

MICRO_OP_SAFE i64 FastXor32(u64 x, u64 y, u32 *f) {
  return FastFlags32(x ^ y, f, 0, 0);
}

MICRO_OP_SAFE i64 FastOr32(u64 x, u64 y, u32 *f) {
  return FastFlags32(x | y, f, 0, 0);
}

MICRO_OP_SAFE i64 FastAnd32(u64 x, u64 y, u32 *f) {
  return FastFlags32(x & y, f, 0, 0);
}

MICRO_OP_SAFE i64 FastSub32(u64 x64, u64 y64, u32 *f) {
  bool cf, of;
  u32 x, y, z;
  x = x64;
  y = y64;
  z = x - y;
  cf = x < z;
  of = ((z ^ x) & (x ^ y)) >> 31;
  return FastFlags32(z, f, of, cf);
}

MICRO_OP_SAFE i64 FastAdd32(u64 x64, u64 y64, u32 *f) {
  bool cf, of;
  u32 x, y, z;
  x = x64;
  y = y64;
  z = x + y;
  cf = z < y;
  of = ((z ^ x) & (z ^ y)) >> 31;
  return FastFlags32(z, f, of, cf);
}

MICRO_OP_SAFE i64 FastFlags64(u64 z, u32 *f, u32 of, u32 cf) {
  return FastFlags(z, f, of, cf, z >> 63);
}

MICRO_OP_SAFE i64 FastXor64(u64 x, u64 y, u32 *f) {
  return FastFlags64(x ^ y, f, 0, 0);
}

MICRO_OP_SAFE i64 FastOr64(u64 x, u64 y, u32 *f) {
  return FastFlags64(x | y, f, 0, 0);
}

MICRO_OP_SAFE i64 FastAnd64(u64 x, u64 y, u32 *f) {
  return FastFlags64(x & y, f, 0, 0);
}

MICRO_OP_SAFE i64 FastSub64(u64 x, u64 y, u32 *f) {
  u64 z;
  bool cf, of;
  z = x - y;
  cf = x < z;
  of = ((z ^ x) & (x ^ y)) >> 63;
  return FastFlags64(z, f, of, cf);
}

MICRO_OP_SAFE i64 FastAdd64(u64 x, u64 y, u32 *f) {
  u64 z;
  bool cf, of;
  z = x + y;
  cf = z < y;
  of = ((z ^ x) & (z ^ y)) >> 63;
  return FastFlags64(z, f, of, cf);
}

MICRO_OP_SAFE i64 FastAdc8(u64 x64, u64 y64, u32 *f) {
  bool cf, of;
  u8 x, y, z, t;
  x = x64;
  y = y64;
  t = x + GetFlag(*f, FLAGS_CF);
  z = t + y;
  cf = (t < x) | (z < y);
  of = ((z ^ x) & (z ^ y)) >> 7;
  return FastFlags8(z, f, of, cf);
}

MICRO_OP_SAFE i64 FastAdc32(u64 x64, u64 y64, u32 *f) {
  bool cf, of;
  u32 x, y, z, t;
  x = x64;
  y = y64;
  t = x + GetFlag(*f, FLAGS_CF);
  z = t + y;
  cf = (t < x) | (z < y);
  of = ((z ^ x) & (z ^ y)) >> 31;
  return FastFlags32(z, f, of, cf);
}

MICRO_OP_SAFE i64 FastAdc64(u64 x, u64 y, u32 *f) {
  u64 z, t;
  bool cf, of;
  t = x + GetFlag(*f, FLAGS_CF);
  z = t + y;
  cf = (t < x) | (z < y);
  of = ((z ^ x) & (z ^ y)) >> 63;
  return FastFlags64(z, f, of, cf);
}

MICRO_OP_SAFE i64 FastSbb8(u64 x64, u64 y64, u32 *f) {
  bool cf, of;
  u8 x, y, z, t;
  x = x64;
  y = y64;
  t = x - GetFlag(*f, FLAGS_CF);
  z = t - y;
  cf = (x < t) | (t < z);
  of = ((z ^ x) & (x ^ y)) >> 7;
  return FastFlags8(z, f, of, cf);
}

MICRO_OP_SAFE i64 FastSbb32(u64 x64, u64 y64, u32 *f) {
  bool cf, of;
  u32 x, y, z, t;
  x = x64;
  y = y64;
  t = x - GetFlag(*f, FLAGS_CF);
  z = t - y;
  cf = (x < t) | (t < z);
  of = ((z ^ x) & (x ^ y)) >> 31;
  return FastFlags32(z, f, of, cf);
}

MICRO_OP_SAFE i64 FastSbb64(u64 x, u64 y, u32 *f) {
  u64 z, t;
  bool cf, of;
  t = x - GetFlag(*f, FLAGS_CF);
  z = t - y;
  cf = (x < t) | (t < z);
  of = ((z ^ x) & (x ^ y)) >> 63;
  return FastFlags64(z, f, of, cf);
}

MICRO_OP_SAFE i64 FastNeg8(u64 x64, u64 y, u32 *f) {
  u8 x;
  bool cf, of;
  x = x64;
  cf = !!x;
  of = x == 0x80;
  x = ~x + 1;
  return FastFlags8(x, f, of, cf);
}

MICRO_OP_SAFE i64 FastNeg32(u64 x64, u64 y, u32 *f) {
  u32 x;
  bool cf, of;
  x = x64;
  cf = !!x;
  of = x == 0x80000000;
  x = ~x + 1;
  return FastFlags32(x, f, of, cf);
}

MICRO_OP_SAFE i64 FastNeg64(u64 x64, u64 y, u32 *f) {
  u64 x;
  bool cf, of;
  x = x64;
  cf = !!x;
  of = x == 0x8000000000000000;
  x = ~x + 1;
  return FastFlags64(x, f, of, cf);
}

MICRO_OP_SAFE i64 BumpFlags(u64 x, u32 *f, u32 of, u32 sf) {
  return FastFlags(x, f, of, GetFlag(*f, FLAGS_CF), sf);
}

MICRO_OP_SAFE i64 FastDec32(u64 x64, u64 y, u32 *f) {
  u32 x, z, of, sf;
  x = x64;
  z = x - 1;
  sf = z >> 31;
  of = ((z ^ x) & (x ^ 1)) >> 31;
  return BumpFlags(z, f, of, sf);
}

MICRO_OP_SAFE i64 FastInc32(u64 x64, u64 y, u32 *f) {
  u32 x, z, of, sf;
  x = x64;
  z = x + 1;
  sf = z >> 31;
  of = ((z ^ x) & (z ^ 1)) >> 31;
  return BumpFlags(z, f, of, sf);
}

MICRO_OP_SAFE i64 FastInc64(u64 x, u64 y, u32 *f) {
  u64 z;
  u32 of, sf;
  z = x + 1;
  sf = z >> 63;
  of = ((z ^ x) & (z ^ 1)) >> 63;
  return BumpFlags(z, f, of, sf);
}

MICRO_OP_SAFE i64 FastDec64(u64 x, u64 y, u32 *f) {
  u64 z;
  u32 of, sf;
  z = x - 1;
  sf = z >> 63;
  of = ((z ^ x) & (x ^ 1)) >> 63;
  return BumpFlags(z, f, of, sf);
}

MICRO_OP_SAFE i64 FastInc8(u64 x64, u64 y, u32 *f) {
  u8 x, z;
  u32 of, sf;
  x = x64;
  z = x + 1;
  sf = z >> 7;
  of = ((z ^ x) & (z ^ 1)) >> 7;
  return BumpFlags(z, f, of, sf);
}

MICRO_OP_SAFE i64 FastDec8(u64 x64, u64 y, u32 *f) {
  u8 x, z;
  u32 of, sf;
  x = x64;
  z = x - 1;
  sf = z >> 7;
  of = ((z ^ x) & (x ^ 1)) >> 7;
  return BumpFlags(z, f, of, sf);
}

static inline i64 FastShr8(u64 x64, u64 y, u32 *f) {
  u32 x, cf;
  x = x64 & 0xff;
  if ((y &= 31)) {
    cf = (x >> (y - 1)) & 1;
    x >>= y;
    return FastFlags8(x, f, ((x << 1) ^ x) >> 7, cf);
  } else {
    return x;
  }
}

static inline i64 FastShr32(u64 x64, u64 y, u32 *f) {
  u32 cf, x = x64;
  if ((y &= 31)) {
    cf = (x >> (y - 1)) & 1;
    x >>= y;
    return FastFlags32(x, f, ((x << 1) ^ x) >> 31, cf);
  } else {
    return x;
  }
}

static inline i64 FastShr64(u64 x, u64 y, u32 *f) {
  u32 cf;
  if ((y &= 63)) {
    cf = (x >> (y - 1)) & 1;
    x >>= y;
    return FastFlags64(x, f, ((x << 1) ^ x) >> 63, cf);
  } else {
    return x;
  }
}

static inline i64 FastShl8(u64 x64, u64 y, u32 *f) {
  u32 x, cf;
  x = x64 & 0xff;
  if ((y &= 31)) {
    cf = (x >> ((8 - y) & 31)) & 1;
    x = (x << y) & 0xff;
    return FastFlags8(x, f, (x >> 7) ^ cf, cf);
  } else {
    return x;
  }
}

static inline i64 FastShl32(u64 x64, u64 y, u32 *f) {
  u32 cf, x = x64;
  if ((y &= 31)) {
    cf = (x >> (32 - y)) & 1;
    x <<= y;
    return FastFlags32(x, f, (x >> 31) ^ cf, cf);
  } else {
    return x;
  }
}

static inline i64 FastShl64(u64 x, u64 y, u32 *f) {
  u32 cf;
  if ((y &= 63)) {
    cf = (x >> (64 - y)) & 1;
    x <<= y;
    return FastFlags64(x, f, (x >> 63) ^ cf, cf);
  } else {
    return x;
  }
}

static inline i64 FastSar8(u64 x64, u64 y, u32 *f) {
  u32 x, cf;
  x = x64 & 0xff;
  if ((y &= 31)) {
    cf = ((i32)(i8)x >> (y - 1)) & 1;
    x = ((i32)(i8)x >> y) & 0xff;
    return FastFlags8(x, f, 0, cf);
  } else {
    return x;
  }
}

static inline i64 FastSar32(u64 x64, u64 y, u32 *f) {
  u32 cf, x = x64;
  if ((y &= 31)) {
    cf = ((i32)x >> (y - 1)) & 1;
    x = (i32)x >> y;
    return FastFlags32(x, f, 0, cf);
  } else {
    return x;
  }
}

static inline i64 FastSar64(u64 x, u64 y, u32 *f) {
  u32 cf;
  if ((y &= 63)) {
    cf = ((i64)x >> (y - 1)) & 1;
    x = (i64)x >> y;
    return FastFlags64(x, f, 0, cf);
  } else {
    return x;
  }
}

static inline i64 RotateFlags(u64 x, u32 cf, u32 *f, u32 of) {
  *f &= ~(1u << FLAGS_CF | 1u << FLAGS_OF);
  *f |= cf << FLAGS_CF | of << FLAGS_OF;
  return x;
}

static inline i64 FastRol32(u64 x64, u64 y, u32 *f) {
  u32 x = x64;
  if ((y &= 31)) {
    x = x << y | x >> (32 - y);
    return RotateFlags(x, x & 1, f, ((x >> 31) ^ x) & 1);
  } else {
    return x;
  }
}

static inline i64 FastRol64(u64 x, u64 y, u32 *f) {
  if ((y &= 63)) {
    x = x << y | x >> (64 - y);
    return RotateFlags(x, x & 1, f, ((x >> 63) ^ x) & 1);
  } else {
    return x;
  }
}

static inline i64 FastRor32(u64 x64, u64 y, u32 *f) {
  u32 x = x64;
  if ((y &= 31)) {
    x = x >> y | x << (32 - y);
    return RotateFlags(x, x >> 31, f, ((x >> 31) ^ (x >> 30)) & 1);
  } else {
    return x;
  }
}

static inline i64 FastRor64(u64 x, u64 y, u32 *f) {
  if ((y &= 63)) {
    x = x >> y | x << (64 - y);
    return RotateFlags(x, x >> 63, f, ((x >> 63) ^ (x >> 62)) & 1);
  } else {
    return x;
  }
}

static inline i64 FastRol8(u64 x64, u64 y, u32 *f) {
  u8 x = x64;
  if (y & 31) {
    if ((y &= 7)) x = x << y | x >> (8 - y);
    return RotateFlags(x, x & 1, f, ((x >> 7) ^ x) & 1);
  } else {
    return x;
  }
}

static inline i64 FastRor8(u64 x64, u64 y, u32 *f) {
  u8 x = x64;
  if (y & 31) {
    if ((y &= 7)) x = x >> y | x << (8 - y);
    return RotateFlags(x, x >> 7, f, ((x >> 7) ^ (x >> 6)) & 1);
  } else {
    return x;
  }
}

MICRO_OP_SAFE i64 FastFlags16(u16 z, u32 *f, u32 of, u32 cf) {
  return FastFlags(z, f, of, cf, z >> 15);
}

MICRO_OP_SAFE i64 FastXor16(u64 x, u64 y, u32 *f) {
  return FastFlags16(x ^ y, f, 0, 0);
}

MICRO_OP_SAFE i64 FastOr16(u64 x, u64 y, u32 *f) {
  return FastFlags16(x | y, f, 0, 0);
}

MICRO_OP_SAFE i64 FastAnd16(u64 x, u64 y, u32 *f) {
  return FastFlags16(x & y, f, 0, 0);
}

MICRO_OP_SAFE i64 FastSub16(u64 x64, u64 y64, u32 *f) {
  bool cf, of;
  u16 x, y, z;
  x = x64;
  y = y64;
  z = x - y;
  cf = x < z;
  of = ((z ^ x) & (x ^ y)) >> 15;
  return FastFlags16(z, f, of, cf);
}

MICRO_OP_SAFE i64 FastAdd16(u64 x64, u64 y64, u32 *f) {
  bool cf, of;
  u16 x, y, z;
  x = x64;
  y = y64;
  z = x + y;
  cf = z < y;
  of = ((z ^ x) & (z ^ y)) >> 15;
  return FastFlags16(z, f, of, cf);
}

MICRO_OP_SAFE i64 FastAdc16(u64 x64, u64 y64, u32 *f) {
  bool cf, of;
  u16 x, y, z, t;
  x = x64;
  y = y64;
  t = x + GetFlag(*f, FLAGS_CF);
  z = t + y;
  cf = (t < x) | (z < y);
  of = ((z ^ x) & (z ^ y)) >> 15;
  return FastFlags16(z, f, of, cf);
}

MICRO_OP_SAFE i64 FastSbb16(u64 x64, u64 y64, u32 *f) {
  bool cf, of;
  u16 x, y, z, t;
  x = x64;
  y = y64;
  t = x - GetFlag(*f, FLAGS_CF);
  z = t - y;
  cf = (x < t) | (t < z);
  of = ((z ^ x) & (x ^ y)) >> 15;
  return FastFlags16(z, f, of, cf);
}

MICRO_OP_SAFE i64 FastNeg16(u64 x64, u64 y, u32 *f) {
  u16 x;
  bool cf, of;
  x = x64;
  cf = !!x;
  of = x == 0x8000;
  x = ~x + 1;
  return FastFlags16(x, f, of, cf);
}

MICRO_OP_SAFE i64 FastInc16(u64 x64, u64 y, u32 *f) {
  u16 x, z;
  u32 of, sf;
  x = x64;
  z = x + 1;
  sf = z >> 15;
  of = ((z ^ x) & (z ^ 1)) >> 15;
  return BumpFlags(z, f, of, sf);
}

MICRO_OP_SAFE i64 FastDec16(u64 x64, u64 y, u32 *f) {
  u16 x, z;
  u32 of, sf;
  x = x64;
  z = x - 1;
  sf = z >> 15;
  of = ((z ^ x) & (x ^ 1)) >> 15;
  return BumpFlags(z, f, of, sf);
}

static inline i64 FastShr16(u64 x64, u64 y, u32 *f) {
  u32 x, cf;
  x = x64 & 0xffff;
  if ((y &= 31)) {
    cf = (x >> (y - 1)) & 1;
    x >>= y;
    return FastFlags16(x, f, ((x << 1) ^ x) >> 15, cf);
  } else {
    return x;
  }
}

static inline i64 FastShl16(u64 x64, u64 y, u32 *f) {
  u32 x, cf;
  x = x64 & 0xffff;
  if ((y &= 31)) {
    cf = (x >> ((16 - y) & 31)) & 1;
    x = (x << y) & 0xffff;
    return FastFlags16(x, f, (x >> 15) ^ cf, cf);
  } else {
    return x;
  }
}

static inline i64 FastSar16(u64 x64, u64 y, u32 *f) {
  u32 x, cf;
  x = x64 & 0xffff;
  if ((y &= 31)) {
    cf = ((i32)(i16)x >> (y - 1)) & 1;
    x = ((i32)(i16)x >> y) & 0xffff;
    return FastFlags16(x, f, 0, cf);
  } else {
    return x;
  }
}

static inline i64 FastRol16(u64 x64, u64 y, u32 *f) {
  u16 x = x64;
  if (y & 31) {
    if ((y &= 15)) x = x << y | x >> (16 - y);
    return RotateFlags(x, x & 1, f, ((x >> 15) ^ x) & 1);
  } else {
    return x;
  }
}

static inline i64 FastRor16(u64 x64, u64 y, u32 *f) {
  u16 x = x64;
  if (y & 31) {
    if ((y &= 15)) x = x >> y | x << (16 - y);
    return RotateFlags(x, x >> 15, f, ((x >> 15) ^ (x >> 14)) & 1);
  } else {
    return x;
  }
}
