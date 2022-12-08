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
#include <limits.h>

#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/mop.h"

#if LONG_BIT == 64
#if (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 406 || defined(__llvm__)
#define HAVE_INT128
#endif
#endif

struct Dubble {
  u64 lo;
  u64 hi;
};

static inline struct Dubble DubbleNeg(struct Dubble x) {
  struct Dubble d;
  d.lo = -x.lo;
  d.hi = ~(x.hi - (x.lo - 1 > x.lo));
  return d;
}

static inline struct Dubble DubbleShl(struct Dubble x) {
  struct Dubble d;
  d.lo = x.lo << 1;
  d.hi = x.hi << 1 | x.lo >> 63;
  return d;
}

static inline struct Dubble DubbleShr(struct Dubble x) {
  struct Dubble d;
  d.lo = x.lo >> 1 | x.hi << 63;
  d.hi = x.hi >> 1;
  return d;
}

static inline unsigned DubbleLte(struct Dubble a, struct Dubble b) {
  return a.hi == b.hi ? a.lo <= b.lo : a.hi <= b.hi;
}

static struct Dubble DubbleMul(u64 a, u64 b) {
  u64 x, y, t;
  struct Dubble d;
  x = (a & 0xffffffff) * (b & 0xffffffff);
  t = x >> 32;
  x &= 0xffffffff;
  t += (a >> 32) * (b & 0xffffffff);
  x += (t & 0xffffffff) << 32;
  y = t >> 32;
  t = x >> 32;
  x &= 0xffffffff;
  t += (b >> 32) * (a & 0xffffffff);
  x += (t & 0xffffffff) << 32;
  y += t >> 32;
  y += (a >> 32) * (b >> 32);
  d.lo = x;
  d.hi = y;
  return d;
}

static struct Dubble DubbleImul(u64 a, u64 b) {
  unsigned s, t;
  struct Dubble p;
  if ((s = a >> 63)) a = -a;
  if ((t = b >> 63)) b = -b;
  p = DubbleMul(a, b);
  return s ^ t ? DubbleNeg(p) : p;
}

static struct Dubble DubbleDiv(struct Dubble a, u64 b, u64 *r) {
  u64 s;
  int n, c;
  struct Dubble d, q, t;
  d.lo = b, d.hi = 0;
  q.lo = 0, q.hi = 0;
  for (n = 0; DubbleLte(d, a) && n < 128; ++n) {
    d = DubbleShl(d);
  }
  for (; n > 0; --n) {
    t = a;
    d = DubbleShr(d);
    q = DubbleShl(q);
    s = a.lo, a.lo -= d.lo + 0, c = a.lo > s;
    s = a.hi, a.hi -= d.hi + c, c = a.hi > s;
    if (c) {
      a = t;
    } else {
      q.lo++;
    }
  }
  *r = a.lo;
  return q;
}

static struct Dubble DubbleIdiv(struct Dubble a, u64 b, u64 *r) {
  unsigned s, t;
  struct Dubble q;
  if ((s = a.hi >> 63)) a = DubbleNeg(a);
  if ((t = b >> 63)) b = -b;
  q = DubbleDiv(a, b, r);
  if (s ^ t) q = DubbleNeg(q);
  if (s) *r = -*r;
  return q;
}

void OpDivAlAhAxEbSigned(P) {
  i8 y, r;
  i16 x, q;
  x = Get16(m->ax);
  y = Load8(GetModrmRegisterBytePointerRead1(A));
  if (!y) return RaiseDivideError(m);
  if (x == INT16_MIN) return RaiseDivideError(m);
  q = x / y;
  r = x % y;
  if (q != (i8)q) return RaiseDivideError(m);
  m->al = q & 0xff;
  m->ah = r & 0xff;
}

void OpDivAlAhAxEbUnsigned(P) {
  u8 y, r;
  u16 x, q;
  x = Get16(m->ax);
  y = Load8(GetModrmRegisterBytePointerRead1(A));
  if (!y) return RaiseDivideError(m);
  q = x / y;
  r = x % y;
  if (q > UINT8_MAX) return RaiseDivideError(m);
  m->al = q & 0xff;
  m->ah = r & 0xff;
}

static void OpDivRdxRaxEvqpSigned64(P, u8 *p) {
#ifdef HAVE_INT128
  i64 y, r;
  __int128 x, q;
  x = (unsigned __int128)Get64(m->dx) << 64 | Get64(m->ax);
  y = Load64(p);
  if (!y) return RaiseDivideError(m);
  if (x == (unsigned __int128)0x8000000000000000ull << 64) {
    return RaiseDivideError(m);
  }
  q = x / y;
  r = x % y;
  if (q != (i64)q) return RaiseDivideError(m);
  Put64(m->ax, q);
  Put64(m->dx, r);
#else
  u64 d, r;
  struct Dubble q;
  q.lo = Get64(m->ax);
  q.hi = Get64(m->dx);
  d = Load64(p);
  if (!d) return RaiseDivideError(m);
  if (!q.lo && q.hi == 0x8000000000000000) return RaiseDivideError(m);
  q = DubbleIdiv(q, d, &r);
  if ((i64)q.lo < 0 && (i64)q.hi != -1) return RaiseDivideError(m);
  if ((i64)q.lo >= 0 && q.hi) return RaiseDivideError(m);
  Put64(m->ax, q.lo);
  Put64(m->dx, r);
#endif
}

static void OpDivRdxRaxEvqpSigned32(P, u8 *p) {
  i32 y, r;
  i64 x, q;
  x = (u64)Get32(m->dx) << 32 | Get32(m->ax);
  y = Load32(p);
  if (!y) return RaiseDivideError(m);
  if (x == INT64_MIN) return RaiseDivideError(m);
  q = x / y;
  r = x % y;
  if (q != (i32)q) return RaiseDivideError(m);
  Put64(m->ax, q & 0xffffffff);
  Put64(m->dx, r & 0xffffffff);
}

static void OpDivRdxRaxEvqpSigned16(P, u8 *p) {
  i16 y, r;
  i32 x, q;
  x = (u32)Get16(m->dx) << 16 | Get16(m->ax);
  y = Load16(p);
  if (!y) return RaiseDivideError(m);
  if (x == INT32_MIN) return RaiseDivideError(m);
  q = x / y;
  r = x % y;
  if (q != (i16)q) return RaiseDivideError(m);
  Put16(m->ax, q);
  Put16(m->dx, r);
}

static void OpDivRdxRaxEvqpUnsigned16(P, u8 *p) {
  u16 y, r;
  u32 x, q;
  x = (u32)Get16(m->dx) << 16 | Get16(m->ax);
  y = Load16(p);
  if (!y) return RaiseDivideError(m);
  q = x / y;
  r = x % y;
  if (q > UINT16_MAX) return RaiseDivideError(m);
  Put16(m->ax, q);
  Put16(m->dx, r);
}

static void OpDivRdxRaxEvqpUnsigned32(P, u8 *p) {
  u32 y, r;
  u64 x, q;
  x = (u64)Get32(m->dx) << 32 | Get32(m->ax);
  y = Load32(p);
  if (!y) return RaiseDivideError(m);
  q = x / y;
  r = x % y;
  if (q > UINT32_MAX) return RaiseDivideError(m);
  Put64(m->ax, q & 0xffffffff);
  Put64(m->dx, r & 0xffffffff);
}

static void OpDivRdxRaxEvqpUnsigned64(P, u8 *p) {
#ifdef HAVE_INT128
  u64 y, r;
  unsigned __int128 x, q;
  x = (unsigned __int128)Get64(m->dx) << 64 | Get64(m->ax);
  y = Load64(p);
  if (!y) return RaiseDivideError(m);
  q = x / y;
  r = x % y;
  if (q > UINT64_MAX) return RaiseDivideError(m);
  Put64(m->ax, q);
  Put64(m->dx, r);
#else
  u64 d, r;
  struct Dubble q;
  q.lo = Get64(m->ax);
  q.hi = Get64(m->dx);
  d = Load64(p);
  if (!d) return RaiseDivideError(m);
  q = DubbleDiv(q, d, &r);
  if (q.hi) return RaiseDivideError(m);
  Put64(m->ax, q.lo);
  Put64(m->dx, r);
#endif
}

void OpDivRdxRaxEvqpSigned(P) {
  u8 *p = GetModrmRegisterWordPointerReadOszRexw(A);
  if (Rexw(rde)) {
    OpDivRdxRaxEvqpSigned64(A, p);
  } else if (!Osz(rde)) {
    OpDivRdxRaxEvqpSigned32(A, p);
  } else {
    OpDivRdxRaxEvqpSigned16(A, p);
  }
}

void OpDivRdxRaxEvqpUnsigned(P) {
  u8 *p = GetModrmRegisterWordPointerReadOszRexw(A);
  if (Rexw(rde)) {
    OpDivRdxRaxEvqpUnsigned64(A, p);
  } else if (!Osz(rde)) {
    OpDivRdxRaxEvqpUnsigned32(A, p);
  } else {
    OpDivRdxRaxEvqpUnsigned16(A, p);
  }
}

void OpMulAxAlEbSigned(P) {
  u8 *p;
  i16 ax;
  unsigned of;
  p = GetModrmRegisterBytePointerRead1(A);
  ax = (i8)Get8(m->ax) * (i8)Load8(p);
  of = ax != (i8)ax;
  m->flags = SetFlag(m->flags, FLAGS_CF, of);
  m->flags = SetFlag(m->flags, FLAGS_OF, of);
  Put16(m->ax, ax);
}

void OpMulAxAlEbUnsigned(P) {
  u8 *p;
  int ax;
  unsigned of;
  p = GetModrmRegisterBytePointerRead1(A);
  ax = Get8(m->ax) * Load8(p);
  of = ax != (u8)ax;
  m->flags = SetFlag(m->flags, FLAGS_CF, of);
  m->flags = SetFlag(m->flags, FLAGS_OF, of);
  Put16(m->ax, ax);
}

static void OpMulRdxRaxEvqpSigned64(struct Machine *m, i64 x) {
#ifdef HAVE_INT128
  __int128 rdxrax = (__int128)(i64)Get64(m->ax) * x;
  unsigned of = rdxrax != (i64)rdxrax;
  Put64(m->ax, rdxrax);
  Put64(m->dx, rdxrax >> 64);
#else
  struct Dubble rdxrax = DubbleImul(Get64(m->ax), x);
  unsigned of = !!(rdxrax.hi + (rdxrax.lo >> 63));
  Put64(m->ax, rdxrax.lo);
  Put64(m->dx, rdxrax.hi);
#endif
  m->flags = SetFlag(m->flags, FLAGS_CF, of);
  m->flags = SetFlag(m->flags, FLAGS_OF, of);
}

void OpMulRdxRaxEvqpSigned(P) {
  u8 *p = GetModrmRegisterWordPointerReadOszRexw(A);
  if (Rexw(rde)) {
    OpMulRdxRaxEvqpSigned64(m, Load64(p));
    Jitter(A, "B r0a1= s0a0= c", OpMulRdxRaxEvqpSigned64);
  } else if (!Osz(rde)) {
    i64 edxeax = (i64)(i32)Get32(m->ax) * (i32)Load32(p);
    unsigned of = edxeax != (i32)edxeax;
    Put64(m->ax, edxeax);
    Put64(m->dx, edxeax >> 32);
    m->flags = SetFlag(m->flags, FLAGS_CF, of);
    m->flags = SetFlag(m->flags, FLAGS_OF, of);
  } else {
    i32 dxax = (i32)(i16)Get16(m->ax) * (i16)Load16(p);
    unsigned of = dxax != (i16)dxax;
    Put16(m->ax, dxax);
    Put16(m->dx, dxax >> 16);
    m->flags = SetFlag(m->flags, FLAGS_CF, of);
    m->flags = SetFlag(m->flags, FLAGS_OF, of);
  }
}

static void OpMulRdxRaxEvqpUnsigned64(struct Machine *m, u64 x) {
#ifdef HAVE_INT128
  unsigned __int128 rdxrax = (unsigned __int128)Get64(m->ax) * x;
  unsigned of = (u64)rdxrax != rdxrax;
  Put64(m->ax, rdxrax);
  Put64(m->dx, rdxrax >> 64);
#else
  struct Dubble rdxrax = DubbleMul(Get64(m->ax), x);
  unsigned of = !!rdxrax.hi;
  Put64(m->ax, rdxrax.lo);
  Put64(m->dx, rdxrax.hi);
#endif
  m->flags = SetFlag(m->flags, FLAGS_CF, of);
  m->flags = SetFlag(m->flags, FLAGS_OF, of);
}

static void OpMulRdxRaxEvqpUnsigned32(struct Machine *m, u64 x) {
  u64 edxeax;
  unsigned of;
  edxeax = (u64)Get32(m->ax) * x;
  of = (u32)edxeax != edxeax;
  Put64(m->ax, edxeax);
  Put64(m->dx, edxeax >> 32);
  m->flags = SetFlag(m->flags, FLAGS_CF, of);
  m->flags = SetFlag(m->flags, FLAGS_OF, of);
}

void OpMulRdxRaxEvqpUnsigned(P) {
  u8 *p;
  u32 dxax;
  unsigned of;
  p = GetModrmRegisterWordPointerReadOszRexw(A);
  if (Rexw(rde)) {
    OpMulRdxRaxEvqpUnsigned64(m, Load64(p));
    Jitter(A, "B r0a1= s0a0= c", OpMulRdxRaxEvqpUnsigned64);
  } else if (!Osz(rde)) {
    OpMulRdxRaxEvqpUnsigned32(m, Load32(p));
    Jitter(A, "B r0a1= s0a0= c", OpMulRdxRaxEvqpUnsigned32);
  } else {
    dxax = (u32)(u16)Get16(m->ax) * (u16)Load16(p);
    of = (u16)dxax != dxax;
    Put16(m->ax, dxax);
    Put16(m->dx, dxax >> 16);
    m->flags = SetFlag(m->flags, FLAGS_CF, of);
    m->flags = SetFlag(m->flags, FLAGS_OF, of);
  }
}

static void AluImul(P, u8 *a, u8 *b) {
  unsigned of;
  if (Rexw(rde)) {
#ifdef HAVE_INT128
    __int128 z;
    z = (__int128)(i64)Get64(a) * (i64)Load64(b);
    of = z != (i64)z;
    Put64(RegRexrReg(m, rde), z);
#else
    struct Dubble p;
    p = DubbleImul(Get64(a), Load64(b));
    of = !!(p.hi + (p.lo >> 63));
    Put64(RegRexrReg(m, rde), p.lo);
#endif
  } else if (!Osz(rde)) {
    i64 z;
    z = (i64)(i32)Get32(a) * (i32)Load32(b);
    of = z != (i32)z;
    Put64(RegRexrReg(m, rde), z & 0xffffffff);
  } else {
    i32 z;
    z = (i32)(i16)Get16(a) * (i16)Load16(b);
    of = z != (i16)z;
    Put16(RegRexrReg(m, rde), z);
  }
  m->flags = SetFlag(m->flags, FLAGS_CF, of);
  m->flags = SetFlag(m->flags, FLAGS_OF, of);
}

void OpImulGvqpEvqp(P) {
  AluImul(A, RegRexrReg(m, rde), GetModrmRegisterWordPointerReadOszRexw(A));
}

void OpImulGvqpEvqpImm(P) {
  u8 b[8];
  Put64(b, uimm0);
  AluImul(A, GetModrmRegisterWordPointerReadOszRexw(A), b);
}
