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
#include "blink/alu.h"
#include "blink/endian.h"
#include "blink/flags.h"

const aluop_f kAlu[12][4] = {
    {Add8, Add16, Add32, Add64}, {Or8, Or16, Or32, Or64},
    {Adc8, Adc16, Adc32, Adc64}, {Sbb8, Sbb16, Sbb32, Sbb64},
    {And8, And16, And32, And64}, {Sub8, Sub16, Sub32, Sub64},
    {Xor8, Xor16, Xor32, Xor64}, {Sub8, Sub16, Sub32, Sub64},
    {Not8, Not16, Not32, Not64}, {Neg8, Neg16, Neg32, Neg64},
    {Inc8, Inc16, Inc32, Inc64}, {Dec8, Dec16, Dec32, Dec64},
};

const aluop_f kBsu[8][4] = {
    {Rol8, Rol16, Rol32, Rol64}, {Ror8, Ror16, Ror32, Ror64},
    {Rcl8, Rcl16, Rcl32, Rcl64}, {Rcr8, Rcr16, Rcr32, Rcr64},
    {Shl8, Shl16, Shl32, Shl64}, {Shr8, Shr16, Shr32, Shr64},
    {Shl8, Shl16, Shl32, Shl64}, {Sar8, Sar16, Sar32, Sar64},
};

i64 Not8(u64 x, u64 y, u32 *f) {
  return ~x & 0xFF;
}

i64 Not16(u64 x, u64 y, u32 *f) {
  return ~x & 0xFFFF;
}

i64 Not32(u64 x, u64 y, u32 *f) {
  return ~x & 0xFFFFFFFF;
}

i64 Not64(u64 x, u64 y, u32 *f) {
  return ~x & 0xFFFFFFFFFFFFFFFF;
}

static i64 AluFlags(u64 x, u32 af, u32 *f, u32 of, u32 cf, u32 sf) {
  *f &= ~(1 << FLAGS_CF | 1 << FLAGS_ZF | 1 << FLAGS_SF | 1 << FLAGS_OF |
          1 << FLAGS_AF | 0xFF000000u);
  *f |= sf << FLAGS_SF | cf << FLAGS_CF | !x << FLAGS_ZF | of << FLAGS_OF |
        af << FLAGS_AF | (x & 0xFF) << 24;
  return x;
}

static i64 AluFlags8(u8 z, u32 af, u32 *f, u32 of, u32 cf) {
  return AluFlags(z, af, f, of, cf, z >> 7);
}

i64 Xor8(u64 x, u64 y, u32 *f) {
  return AluFlags8(x ^ y, 0, f, 0, 0);
}

i64 Or8(u64 x, u64 y, u32 *f) {
  return AluFlags8(x | y, 0, f, 0, 0);
}

i64 And8(u64 x, u64 y, u32 *f) {
  return AluFlags8(x & y, 0, f, 0, 0);
}

i64 Sub8(u64 x64, u64 y64, u32 *f) {
  bool cf, of, af;
  u8 x, y, z;
  x = x64;
  y = y64;
  z = x - y;
  cf = x < z;
  af = (x & 15) < (z & 15);
  of = ((z ^ x) & (x ^ y)) >> 7;
  return AluFlags8(z, af, f, of, cf);
}

i64 Add8(u64 x64, u64 y64, u32 *f) {
  bool cf, of, af;
  u8 x, y, z;
  x = x64;
  y = y64;
  z = x + y;
  cf = z < y;
  af = (z & 15) < (y & 15);
  of = ((z ^ x) & (z ^ y)) >> 7;
  return AluFlags8(z, af, f, of, cf);
}

static i64 AluFlags32(u32 z, u32 af, u32 *f, u32 of, u32 cf) {
  return AluFlags(z, af, f, of, cf, z >> 31);
}

i64 Xor32(u64 x, u64 y, u32 *f) {
  return AluFlags32(x ^ y, 0, f, 0, 0);
}

i64 Or32(u64 x, u64 y, u32 *f) {
  return AluFlags32(x | y, 0, f, 0, 0);
}

i64 And32(u64 x, u64 y, u32 *f) {
  return AluFlags32(x & y, 0, f, 0, 0);
}

i64 Sub32(u64 x64, u64 y64, u32 *f) {
  bool cf, of, af;
  u32 x, y, z;
  x = x64;
  y = y64;
  z = x - y;
  cf = x < z;
  af = (x & 15) < (z & 15);
  of = ((z ^ x) & (x ^ y)) >> 31;
  return AluFlags32(z, af, f, of, cf);
}

i64 Add32(u64 x64, u64 y64, u32 *f) {
  bool cf, of, af;
  u32 x, y, z;
  x = x64;
  y = y64;
  z = x + y;
  cf = z < y;
  af = (z & 15) < (y & 15);
  of = ((z ^ x) & (z ^ y)) >> 31;
  return AluFlags32(z, af, f, of, cf);
}

static i64 AluFlags64(u64 z, u32 af, u32 *f, u32 of, u32 cf) {
  return AluFlags(z, af, f, of, cf, z >> 63);
}

i64 Xor64(u64 x, u64 y, u32 *f) {
  return AluFlags64(x ^ y, 0, f, 0, 0);
}

i64 Or64(u64 x, u64 y, u32 *f) {
  return AluFlags64(x | y, 0, f, 0, 0);
}

i64 And64(u64 x, u64 y, u32 *f) {
  return AluFlags64(x & y, 0, f, 0, 0);
}

i64 Sub64(u64 x, u64 y, u32 *f) {
  u64 z;
  bool cf, of, af;
  z = x - y;
  cf = x < z;
  af = (x & 15) < (z & 15);
  of = ((z ^ x) & (x ^ y)) >> 63;
  return AluFlags64(z, af, f, of, cf);
}

i64 Add64(u64 x, u64 y, u32 *f) {
  u64 z;
  bool cf, of, af;
  z = x + y;
  cf = z < y;
  af = (z & 15) < (y & 15);
  of = ((z ^ x) & (z ^ y)) >> 63;
  return AluFlags64(z, af, f, of, cf);
}

i64 Adc8(u64 x64, u64 y64, u32 *f) {
  bool cf, of, af;
  u8 x, y, z, t;
  x = x64;
  y = y64;
  t = x + GetFlag(*f, FLAGS_CF);
  z = t + y;
  cf = (t < x) | (z < y);
  of = ((z ^ x) & (z ^ y)) >> 7;
  af = ((t & 15) < (x & 15)) | ((z & 15) < (y & 15));
  return AluFlags8(z, af, f, of, cf);
}

i64 Adc32(u64 x64, u64 y64, u32 *f) {
  bool cf, of, af;
  u32 x, y, z, t;
  x = x64;
  y = y64;
  t = x + GetFlag(*f, FLAGS_CF);
  z = t + y;
  cf = (t < x) | (z < y);
  of = ((z ^ x) & (z ^ y)) >> 31;
  af = ((t & 15) < (x & 15)) | ((z & 15) < (y & 15));
  return AluFlags32(z, af, f, of, cf);
}

i64 Adc64(u64 x, u64 y, u32 *f) {
  u64 z, t;
  bool cf, of, af;
  t = x + GetFlag(*f, FLAGS_CF);
  z = t + y;
  cf = (t < x) | (z < y);
  of = ((z ^ x) & (z ^ y)) >> 63;
  af = ((t & 15) < (x & 15)) | ((z & 15) < (y & 15));
  return AluFlags64(z, af, f, of, cf);
}

i64 Sbb8(u64 x64, u64 y64, u32 *f) {
  bool cf, of, af;
  u8 x, y, z, t;
  x = x64;
  y = y64;
  t = x - GetFlag(*f, FLAGS_CF);
  z = t - y;
  cf = (x < t) | (t < z);
  of = ((z ^ x) & (x ^ y)) >> 7;
  af = ((x & 15) < (t & 15)) | ((t & 15) < (z & 15));
  return AluFlags8(z, af, f, of, cf);
}

i64 Sbb32(u64 x64, u64 y64, u32 *f) {
  bool cf, of, af;
  u32 x, y, z, t;
  x = x64;
  y = y64;
  t = x - GetFlag(*f, FLAGS_CF);
  z = t - y;
  cf = (x < t) | (t < z);
  of = ((z ^ x) & (x ^ y)) >> 31;
  af = ((x & 15) < (t & 15)) | ((t & 15) < (z & 15));
  return AluFlags32(z, af, f, of, cf);
}

i64 Sbb64(u64 x, u64 y, u32 *f) {
  u64 z, t;
  bool cf, of, af;
  t = x - GetFlag(*f, FLAGS_CF);
  z = t - y;
  cf = (x < t) | (t < z);
  of = ((z ^ x) & (x ^ y)) >> 63;
  af = ((x & 15) < (t & 15)) | ((t & 15) < (z & 15));
  return AluFlags64(z, af, f, of, cf);
}

i64 Neg8(u64 x64, u64 y, u32 *f) {
  u8 x;
  bool cf, of, af;
  x = x64;
  af = cf = !!x;
  of = x == 0x80;
  x = ~x + 1;
  return AluFlags8(x, af, f, of, cf);
}

i64 Neg32(u64 x64, u64 y, u32 *f) {
  u32 x;
  bool cf, of, af;
  x = x64;
  af = cf = !!x;
  of = x == 0x80000000;
  x = ~x + 1;
  return AluFlags32(x, af, f, of, cf);
}

i64 Neg64(u64 x64, u64 y, u32 *f) {
  u64 x;
  bool cf, of, af;
  x = x64;
  af = cf = !!x;
  of = x == 0x8000000000000000;
  x = ~x + 1;
  return AluFlags64(x, af, f, of, cf);
}

static i64 BumpFlags(u64 x, u32 af, u32 *f, u32 of, u32 sf) {
  return AluFlags(x, af, f, of, GetFlag(*f, FLAGS_CF), sf);
}

i64 Dec32(u64 x64, u64 y, u32 *f) {
  u32 x, z, of, sf, af;
  x = x64;
  z = x - 1;
  sf = z >> 31;
  af = (x & 15) < (z & 15);
  of = ((z ^ x) & (x ^ 1)) >> 31;
  return BumpFlags(z, af, f, of, sf);
}

i64 Inc32(u64 x64, u64 y, u32 *f) {
  u32 x, z, of, sf, af;
  x = x64;
  z = x + 1;
  sf = z >> 31;
  af = (z & 15) < (y & 15);
  of = ((z ^ x) & (z ^ 1)) >> 31;
  return BumpFlags(z, af, f, of, sf);
}

i64 Inc64(u64 x, u64 y, u32 *f) {
  u64 z;
  u32 of, sf, af;
  z = x + 1;
  sf = z >> 63;
  af = (z & 15) < (y & 15);
  of = ((z ^ x) & (z ^ 1)) >> 63;
  return BumpFlags(z, af, f, of, sf);
}

i64 Dec64(u64 x, u64 y, u32 *f) {
  u64 z;
  u32 of, sf, af;
  z = x - 1;
  sf = z >> 63;
  af = (x & 15) < (z & 15);
  of = ((z ^ x) & (x ^ 1)) >> 63;
  return BumpFlags(z, af, f, of, sf);
}

i64 Inc8(u64 x64, u64 y, u32 *f) {
  u8 x, z;
  u32 of, sf, af;
  x = x64;
  z = x + 1;
  sf = z >> 7;
  af = (z & 15) < (y & 15);
  of = ((z ^ x) & (z ^ 1)) >> 7;
  return BumpFlags(z, af, f, of, sf);
}

i64 Dec8(u64 x64, u64 y, u32 *f) {
  u8 x, z;
  u32 of, sf, af;
  x = x64;
  z = x - 1;
  sf = z >> 7;
  af = (x & 15) < (z & 15);
  of = ((z ^ x) & (x ^ 1)) >> 7;
  return BumpFlags(z, af, f, of, sf);
}

i64 Shr8(u64 x64, u64 y, u32 *f) {
  u32 x, cf;
  x = x64 & 0xff;
  if ((y &= 31)) {
    cf = (x >> (y - 1)) & 1;
    x >>= y;
    return AluFlags8(x, 0, f, ((x << 1) ^ x) >> 7, cf);
  } else {
    return x;
  }
}

i64 Shr32(u64 x64, u64 y, u32 *f) {
  u32 cf, x = x64;
  if ((y &= 31)) {
    cf = (x >> (y - 1)) & 1;
    x >>= y;
    return AluFlags32(x, 0, f, ((x << 1) ^ x) >> 31, cf);
  } else {
    return x;
  }
}

i64 Shr64(u64 x, u64 y, u32 *f) {
  u32 cf;
  if ((y &= 63)) {
    cf = (x >> (y - 1)) & 1;
    x >>= y;
    return AluFlags64(x, 0, f, ((x << 1) ^ x) >> 63, cf);
  } else {
    return x;
  }
}

i64 Shl8(u64 x64, u64 y, u32 *f) {
  u32 x, cf;
  x = x64 & 0xff;
  if ((y &= 31)) {
    cf = (x >> ((8 - y) & 31)) & 1;
    x = (x << y) & 0xff;
    return AluFlags8(x, 0, f, (x >> 7) ^ cf, cf);
  } else {
    return x;
  }
}

i64 Shl32(u64 x64, u64 y, u32 *f) {
  u32 cf, x = x64;
  if ((y &= 31)) {
    cf = (x >> (32 - y)) & 1;
    x <<= y;
    return AluFlags32(x, 0, f, (x >> 31) ^ cf, cf);
  } else {
    return x;
  }
}

i64 Shl64(u64 x, u64 y, u32 *f) {
  u32 cf;
  if ((y &= 63)) {
    cf = (x >> (64 - y)) & 1;
    x <<= y;
    return AluFlags64(x, 0, f, (x >> 63) ^ cf, cf);
  } else {
    return x;
  }
}

i64 Sar8(u64 x64, u64 y, u32 *f) {
  u32 x, cf;
  x = x64 & 0xff;
  if ((y &= 31)) {
    cf = ((i32)(i8)x >> (y - 1)) & 1;
    x = ((i32)(i8)x >> y) & 0xff;
    return AluFlags8(x, 0, f, 0, cf);
  } else {
    return x;
  }
}

i64 Sar32(u64 x64, u64 y, u32 *f) {
  u32 cf, x = x64;
  if ((y &= 31)) {
    cf = ((i32)x >> (y - 1)) & 1;
    x = (i32)x >> y;
    return AluFlags32(x, 0, f, 0, cf);
  } else {
    return x;
  }
}

i64 Sar64(u64 x, u64 y, u32 *f) {
  u32 cf;
  if ((y &= 63)) {
    cf = ((i64)x >> (y - 1)) & 1;
    x = (i64)x >> y;
    return AluFlags64(x, 0, f, 0, cf);
  } else {
    return x;
  }
}

static i64 RotateFlags(u64 x, u32 cf, u32 *f, u32 of) {
  *f &= ~(1u << FLAGS_CF | 1u << FLAGS_OF);
  *f |= cf << FLAGS_CF | of << FLAGS_OF;
  return x;
}

i64 Rol32(u64 x64, u64 y, u32 *f) {
  u32 x = x64;
  if ((y &= 31)) {
    x = x << y | x >> (32 - y);
    return RotateFlags(x, x & 1, f, ((x >> 31) ^ x) & 1);
  } else {
    return x;
  }
}

i64 Rol64(u64 x, u64 y, u32 *f) {
  if ((y &= 63)) {
    x = x << y | x >> (64 - y);
    return RotateFlags(x, x & 1, f, ((x >> 63) ^ x) & 1);
  } else {
    return x;
  }
}

i64 Ror32(u64 x64, u64 y, u32 *f) {
  u32 x = x64;
  if ((y &= 31)) {
    x = x >> y | x << (32 - y);
    return RotateFlags(x, x >> 31, f, ((x >> 31) ^ (x >> 30)) & 1);
  } else {
    return x;
  }
}

i64 Ror64(u64 x, u64 y, u32 *f) {
  if ((y &= 63)) {
    x = x >> y | x << (64 - y);
    return RotateFlags(x, x >> 63, f, ((x >> 63) ^ (x >> 62)) & 1);
  } else {
    return x;
  }
}

i64 Rol8(u64 x64, u64 y, u32 *f) {
  u8 x = x64;
  if (y & 31) {
    if ((y &= 7)) x = x << y | x >> (8 - y);
    return RotateFlags(x, x & 1, f, ((x >> 7) ^ x) & 1);
  } else {
    return x;
  }
}

i64 Ror8(u64 x64, u64 y, u32 *f) {
  u8 x = x64;
  if (y & 31) {
    if ((y &= 7)) x = x >> y | x << (8 - y);
    return RotateFlags(x, x >> 7, f, ((x >> 7) ^ (x >> 6)) & 1);
  } else {
    return x;
  }
}

static i64 Rcr(u64 x, u64 y, u32 *f, u64 xm, u64 k) {
  u64 cf;
  u32 ct;
  x &= xm;
  if (y) {
    cf = GetFlag(*f, FLAGS_CF);
    ct = (x >> (y - 1)) & 1;
    if (y == 1) {
      x = (x >> 1 | cf << (k - 1)) & xm;
    } else {
      x = (x >> y | cf << (k - y) | x << (k + 1 - y)) & xm;
    }
    return RotateFlags(x, ct, f, (((x << 1) ^ x) >> (k - 1)) & 1);
  } else {
    return x;
  }
}

i64 Rcr8(u64 x, u64 y, u32 *f) {
  return Rcr(x, ((unsigned)y & 31) % 9, f, 0xff, 8);
}

i64 Rcr16(u64 x, u64 y, u32 *f) {
  return Rcr(x, ((unsigned)y & 31) % 17, f, 0xffff, 16);
}

i64 Rcr32(u64 x, u64 y, u32 *f) {
  return Rcr(x, y & 31, f, 0xffffffff, 32);
}

i64 Rcr64(u64 x, u64 y, u32 *f) {
  return Rcr(x, y & 63, f, 0xffffffffffffffff, 64);
}

static i64 Rcl(u64 x, u64 y, u32 *f, u64 xm, u64 k) {
  u64 cf;
  u32 ct;
  x &= xm;
  if (y) {
    cf = GetFlag(*f, FLAGS_CF);
    ct = (x >> (k - y)) & 1;
    if (y == 1) {
      x = (x << 1 | cf) & xm;
    } else {
      x = (x << y | cf << (y - 1) | x >> (k + 1 - y)) & xm;
    }
    return RotateFlags(x, ct, f, ct ^ ((x >> (k - 1)) & 1));
  } else {
    return x;
  }
}

i64 Rcl8(u64 x, u64 y, u32 *f) {
  return Rcl(x, ((unsigned)y & 31) % 9, f, 0xff, 8);
}

i64 Rcl16(u64 x, u64 y, u32 *f) {
  return Rcl(x, ((unsigned)y & 31) % 17, f, 0xffff, 16);
}

i64 Rcl32(u64 x, u64 y, u32 *f) {
  return Rcl(x, y & 31, f, 0xffffffff, 32);
}

i64 Rcl64(u64 x, u64 y, u32 *f) {
  return Rcl(x, y & 63, f, 0xffffffffffffffff, 64);
}

u64 BsuDoubleShift(int w, u64 x, u64 y, u8 b, bool isright, u32 *f) {
  bool cf, of;
  u64 s, k, m, z;
  k = 8;
  k <<= w;
  s = 1;
  s <<= k - 1;
  m = s | (s - 1);
  b &= w == 3 ? 63 : 31;
  x &= m;
  if (b) {
    if (isright) {
      z = x >> b | y << (k - b);
      cf = (x >> (b - 1)) & 1;
      of = b == 1 && (z & s) != (x & s);
    } else {
      z = x << b | y >> (k - b);
      cf = (x >> (k - b)) & 1;
      of = b == 1 && (z & s) != (x & s);
    }
    x = z;
    x &= m;
    return AluFlags(x, 0, f, of, cf, !!(x & s));
  } else {
    return x;
  }
}

static i64 AluFlags16(u16 z, u32 af, u32 *f, u32 of, u32 cf) {
  return AluFlags(z, af, f, of, cf, z >> 15);
}

i64 Xor16(u64 x, u64 y, u32 *f) {
  return AluFlags16(x ^ y, 0, f, 0, 0);
}

i64 Or16(u64 x, u64 y, u32 *f) {
  return AluFlags16(x | y, 0, f, 0, 0);
}

i64 And16(u64 x, u64 y, u32 *f) {
  return AluFlags16(x & y, 0, f, 0, 0);
}

i64 Sub16(u64 x64, u64 y64, u32 *f) {
  bool cf, of, af;
  u16 x, y, z;
  x = x64;
  y = y64;
  z = x - y;
  cf = x < z;
  af = (x & 15) < (z & 15);
  of = ((z ^ x) & (x ^ y)) >> 15;
  return AluFlags16(z, af, f, of, cf);
}

i64 Add16(u64 x64, u64 y64, u32 *f) {
  bool cf, of, af;
  u16 x, y, z;
  x = x64;
  y = y64;
  z = x + y;
  cf = z < y;
  af = (z & 15) < (y & 15);
  of = ((z ^ x) & (z ^ y)) >> 15;
  return AluFlags16(z, af, f, of, cf);
}

i64 Adc16(u64 x64, u64 y64, u32 *f) {
  bool cf, of, af;
  u16 x, y, z, t;
  x = x64;
  y = y64;
  t = x + GetFlag(*f, FLAGS_CF);
  z = t + y;
  cf = (t < x) | (z < y);
  of = ((z ^ x) & (z ^ y)) >> 15;
  af = ((t & 15) < (x & 15)) | ((z & 15) < (y & 15));
  return AluFlags16(z, af, f, of, cf);
}

i64 Sbb16(u64 x64, u64 y64, u32 *f) {
  bool cf, of, af;
  u16 x, y, z, t;
  x = x64;
  y = y64;
  t = x - GetFlag(*f, FLAGS_CF);
  z = t - y;
  cf = (x < t) | (t < z);
  of = ((z ^ x) & (x ^ y)) >> 15;
  af = ((x & 15) < (t & 15)) | ((t & 15) < (z & 15));
  return AluFlags16(z, af, f, of, cf);
}

i64 Neg16(u64 x64, u64 y, u32 *f) {
  u16 x;
  bool cf, of, af;
  x = x64;
  af = cf = !!x;
  of = x == 0x8000;
  x = ~x + 1;
  return AluFlags16(x, af, f, of, cf);
}

i64 Inc16(u64 x64, u64 y, u32 *f) {
  u16 x, z;
  u32 of, sf, af;
  x = x64;
  z = x + 1;
  sf = z >> 15;
  af = (z & 15) < (y & 15);
  of = ((z ^ x) & (z ^ 1)) >> 15;
  return BumpFlags(z, af, f, of, sf);
}

i64 Dec16(u64 x64, u64 y, u32 *f) {
  u16 x, z;
  u32 of, sf, af;
  x = x64;
  z = x - 1;
  sf = z >> 15;
  af = (x & 15) < (z & 15);
  of = ((z ^ x) & (x ^ 1)) >> 15;
  return BumpFlags(z, af, f, of, sf);
}

i64 Shr16(u64 x64, u64 y, u32 *f) {
  u32 x, cf;
  x = x64 & 0xffff;
  if ((y &= 31)) {
    cf = (x >> (y - 1)) & 1;
    x >>= y;
    return AluFlags16(x, 0, f, ((x << 1) ^ x) >> 15, cf);
  } else {
    return x;
  }
}

i64 Shl16(u64 x64, u64 y, u32 *f) {
  u32 x, cf;
  x = x64 & 0xffff;
  if ((y &= 31)) {
    cf = (x >> ((16 - y) & 31)) & 1;
    x = (x << y) & 0xffff;
    return AluFlags16(x, 0, f, (x >> 15) ^ cf, cf);
  } else {
    return x;
  }
}

i64 Sar16(u64 x64, u64 y, u32 *f) {
  u32 x, cf;
  x = x64 & 0xffff;
  if ((y &= 31)) {
    cf = ((i32)(i16)x >> (y - 1)) & 1;
    x = ((i32)(i16)x >> y) & 0xffff;
    return AluFlags16(x, 0, f, 0, cf);
  } else {
    return x;
  }
}

i64 Rol16(u64 x64, u64 y, u32 *f) {
  u16 x = x64;
  if (y & 31) {
    if ((y &= 15)) x = x << y | x >> (16 - y);
    return RotateFlags(x, x & 1, f, ((x >> 15) ^ x) & 1);
  } else {
    return x;
  }
}

i64 Ror16(u64 x64, u64 y, u32 *f) {
  u16 x = x64;
  if (y & 31) {
    if ((y &= 15)) x = x >> y | x << (16 - y);
    return RotateFlags(x, x >> 15, f, ((x >> 15) ^ (x >> 14)) & 1);
  } else {
    return x;
  }
}

void OpDas(struct Machine *m, u32 rde) {
  u8 al, af, cf;
  af = cf = 0;
  al = m->al;
  if ((al & 0x0f) > 9 || GetFlag(m->flags, FLAGS_AF)) {
    cf = m->al < 6 || GetFlag(m->flags, FLAGS_CF);
    m->al -= 0x06;
    af = 1;
  }
  if (al > 0x99 || GetFlag(m->flags, FLAGS_CF)) {
    m->al -= 0x60;
    cf = 1;
  }
  AluFlags8(m->al, af, &m->flags, 0, cf);
}

void OpAaa(struct Machine *m, u32 rde) {
  u8 af, cf;
  af = cf = 0;
  if ((m->al & 0x0f) > 9 || GetFlag(m->flags, FLAGS_AF)) {
    cf = m->al < 6 || GetFlag(m->flags, FLAGS_CF);
    Write16(m->ax, Read16(m->ax) + 0x106);
    af = cf = 1;
  }
  m->al &= 0x0f;
  AluFlags8(m->al, af, &m->flags, 0, cf);
}

void OpAas(struct Machine *m, u32 rde) {
  u8 af, cf;
  af = cf = 0;
  if ((m->al & 0x0f) > 9 || GetFlag(m->flags, FLAGS_AF)) {
    cf = m->al < 6 || GetFlag(m->flags, FLAGS_CF);
    Write16(m->ax, Read16(m->ax) - 0x106);
    af = cf = 1;
  }
  m->al &= 0x0f;
  AluFlags8(m->al, af, &m->flags, 0, cf);
}

void OpAam(struct Machine *m, u32 rde) {
  u8 i = m->xedd->op.uimm0;
  if (!i) ThrowDivideError(m);
  m->ah = m->al / i;
  m->al = m->al % i;
  AluFlags8(m->al, 0, &m->flags, 0, 0);
}

void OpAad(struct Machine *m, u32 rde) {
  u8 i = m->xedd->op.uimm0;
  Write16(m->ax, (m->ah * i + m->al) & 0xff);
  AluFlags8(m->al, 0, &m->flags, 0, 0);
}
