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
  struct Dubble d;
  u64 x, y, t;
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
  int n, c;
  u64 s;
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

void OpDivAlAhAxEbSigned(struct Machine *m, u32 rde) {
  i8 y, r;
  i16 x, q;
  x = Read16(m->ax);
  y = Read8(GetModrmRegisterBytePointerRead(m, rde));
  if (!y) ThrowDivideError(m);
  if (x == INT16_MIN) ThrowDivideError(m);
  q = x / y;
  r = x % y;
  if (q != (i8)q) ThrowDivideError(m);
  m->ax[0] = q & 0xff;
  m->ax[1] = r & 0xff;
}

void OpDivAlAhAxEbUnsigned(struct Machine *m, u32 rde) {
  u8 y, r;
  u16 x, q;
  x = Read16(m->ax);
  y = Read8(GetModrmRegisterBytePointerRead(m, rde));
  if (!y) ThrowDivideError(m);
  q = x / y;
  r = x % y;
  if (q > UINT8_MAX) ThrowDivideError(m);
  m->ax[0] = q & 0xff;
  m->ax[1] = r & 0xff;
}

static void OpDivRdxRaxEvqpSigned64(struct Machine *m, u32 rde, u8 *p) {
  u64 d, r;
  struct Dubble q;
  q.lo = Read64(m->ax);
  q.hi = Read64(m->dx);
  d = Read64(p);
  if (!d) ThrowDivideError(m);
  if (!q.lo && q.hi == 0x8000000000000000) ThrowDivideError(m);
  q = DubbleIdiv(q, d, &r);
  if ((i64)q.lo < 0 && (i64)q.hi != -1) ThrowDivideError(m);
  if ((i64)q.lo >= 0 && q.hi) ThrowDivideError(m);
  Write64(m->ax, q.lo);
  Write64(m->dx, r);
}

static void OpDivRdxRaxEvqpSigned32(struct Machine *m, u32 rde, u8 *p) {
  i32 y, r;
  i64 x, q;
  x = (u64)Read32(m->dx) << 32 | Read32(m->ax);
  y = Read32(p);
  if (!y) ThrowDivideError(m);
  if (x == INT64_MIN) ThrowDivideError(m);
  q = x / y;
  r = x % y;
  if (q != (i32)q) ThrowDivideError(m);
  Write64(m->ax, q & 0xffffffff);
  Write64(m->dx, r & 0xffffffff);
}

static void OpDivRdxRaxEvqpSigned16(struct Machine *m, u32 rde, u8 *p) {
  i16 y, r;
  i32 x, q;
  x = (u32)Read16(m->dx) << 16 | Read16(m->ax);
  y = Read16(p);
  if (!y) ThrowDivideError(m);
  if (x == INT32_MIN) ThrowDivideError(m);
  q = x / y;
  r = x % y;
  if (q != (i16)q) ThrowDivideError(m);
  Write16(m->ax, q);
  Write16(m->dx, r);
}

static void OpDivRdxRaxEvqpUnsigned16(struct Machine *m, u32 rde, u8 *p) {
  u16 y, r;
  u32 x, q;
  x = (u32)Read16(m->dx) << 16 | Read16(m->ax);
  y = Read16(p);
  if (!y) ThrowDivideError(m);
  q = x / y;
  r = x % y;
  if (q > UINT16_MAX) ThrowDivideError(m);
  Write16(m->ax, q);
  Write16(m->dx, r);
}

static void OpDivRdxRaxEvqpUnsigned32(struct Machine *m, u32 rde, u8 *p) {
  u32 y, r;
  u64 x, q;
  x = (u64)Read32(m->dx) << 32 | Read32(m->ax);
  y = Read32(p);
  if (!y) ThrowDivideError(m);
  q = x / y;
  r = x % y;
  if (q > UINT32_MAX) ThrowDivideError(m);
  Write64(m->ax, q & 0xffffffff);
  Write64(m->dx, r & 0xffffffff);
}

static void OpDivRdxRaxEvqpUnsigned64(struct Machine *m, u32 rde, u8 *p) {
  u64 d, r;
  struct Dubble q;
  q.lo = Read64(m->ax);
  q.hi = Read64(m->dx);
  d = Read64(p);
  if (!d) ThrowDivideError(m);
  q = DubbleDiv(q, d, &r);
  if (q.hi) ThrowDivideError(m);
  Write64(m->ax, q.lo);
  Write64(m->dx, r);
}

void OpDivRdxRaxEvqpSigned(struct Machine *m, u32 rde) {
  u8 *p;
  p = GetModrmRegisterWordPointerReadOszRexw(m, rde);
  if (Rexw(rde)) {
    OpDivRdxRaxEvqpSigned64(m, rde, p);
  } else if (!Osz(rde)) {
    OpDivRdxRaxEvqpSigned32(m, rde, p);
  } else {
    OpDivRdxRaxEvqpSigned16(m, rde, p);
  }
}

void OpDivRdxRaxEvqpUnsigned(struct Machine *m, u32 rde) {
  u8 *p;
  p = GetModrmRegisterWordPointerReadOszRexw(m, rde);
  if (Rexw(rde)) {
    OpDivRdxRaxEvqpUnsigned64(m, rde, p);
  } else if (!Osz(rde)) {
    OpDivRdxRaxEvqpUnsigned32(m, rde, p);
  } else {
    OpDivRdxRaxEvqpUnsigned16(m, rde, p);
  }
}

void OpMulAxAlEbSigned(struct Machine *m, u32 rde) {
  i16 ax;
  u8 *p;
  unsigned of;
  p = GetModrmRegisterBytePointerRead(m, rde);
  ax = (i8)Read8(m->ax) * (i8)Read8(p);
  of = ax != (i8)ax;
  m->flags = SetFlag(m->flags, FLAGS_CF, of);
  m->flags = SetFlag(m->flags, FLAGS_OF, of);
  Write16(m->ax, ax);
}

void OpMulAxAlEbUnsigned(struct Machine *m, u32 rde) {
  int ax;
  u8 *p;
  unsigned of;
  p = GetModrmRegisterBytePointerRead(m, rde);
  ax = Read8(m->ax) * Read8(p);
  of = ax != (u8)ax;
  m->flags = SetFlag(m->flags, FLAGS_CF, of);
  m->flags = SetFlag(m->flags, FLAGS_OF, of);
  Write16(m->ax, ax);
}

void OpMulRdxRaxEvqpSigned(struct Machine *m, u32 rde) {
  u8 *p;
  unsigned of;
  i32 dxax;
  i64 edxeax;
  struct Dubble rdxrax;
  p = GetModrmRegisterWordPointerReadOszRexw(m, rde);
  if (Rexw(rde)) {
    rdxrax = DubbleImul(Read64(m->ax), Read64(p));
    of = !!(rdxrax.hi + (rdxrax.lo >> 63));
    Write64(m->ax, rdxrax.lo);
    Write64(m->dx, rdxrax.hi);
  } else if (!Osz(rde)) {
    edxeax = (i64)(i32)Read32(m->ax) * (i32)Read32(p);
    of = edxeax != (i32)edxeax;
    Write64(m->ax, edxeax);
    Write64(m->dx, edxeax >> 32);
  } else {
    dxax = (i32)(i16)Read16(m->ax) * (i16)Read16(p);
    of = dxax != (i16)dxax;
    Write16(m->ax, dxax);
    Write16(m->dx, dxax >> 16);
  }
  m->flags = SetFlag(m->flags, FLAGS_CF, of);
  m->flags = SetFlag(m->flags, FLAGS_OF, of);
}

void OpMulRdxRaxEvqpUnsigned(struct Machine *m, u32 rde) {
  u8 *p;
  unsigned of;
  u32 dxax;
  u64 edxeax;
  struct Dubble rdxrax;
  p = GetModrmRegisterWordPointerReadOszRexw(m, rde);
  if (Rexw(rde)) {
    rdxrax = DubbleMul(Read64(m->ax), Read64(p));
    of = !!rdxrax.hi;
    Write64(m->ax, rdxrax.lo);
    Write64(m->dx, rdxrax.hi);
  } else if (!Osz(rde)) {
    edxeax = (u64)Read32(m->ax) * Read32(p);
    of = (u32)edxeax != edxeax;
    Write64(m->ax, edxeax);
    Write64(m->dx, edxeax >> 32);
  } else {
    dxax = (u32)(u16)Read16(m->ax) * (u16)Read16(p);
    of = (u16)dxax != dxax;
    Write16(m->ax, dxax);
    Write16(m->dx, dxax >> 16);
  }
  m->flags = SetFlag(m->flags, FLAGS_CF, of);
  m->flags = SetFlag(m->flags, FLAGS_OF, of);
}

static void AluImul(struct Machine *m, u32 rde, u8 *a, u8 *b) {
  unsigned of;
  if (Rexw(rde)) {
    struct Dubble p;
    p = DubbleImul(Read64(a), Read64(b));
    of = !!(p.hi + (p.lo >> 63));
    Write64(RegRexrReg(m, rde), p.lo);
  } else if (!Osz(rde)) {
    i64 z;
    z = (i64)(i32)Read32(a) * (i32)Read32(b);
    of = z != (i32)z;
    Write64(RegRexrReg(m, rde), z & 0xffffffff);
  } else {
    i32 z;
    z = (i32)(i16)Read16(a) * (i16)Read16(b);
    of = z != (i16)z;
    Write16(RegRexrReg(m, rde), z);
  }
  m->flags = SetFlag(m->flags, FLAGS_CF, of);
  m->flags = SetFlag(m->flags, FLAGS_OF, of);
}

void OpImulGvqpEvqp(struct Machine *m, u32 rde) {
  AluImul(m, rde, RegRexrReg(m, rde),
          GetModrmRegisterWordPointerReadOszRexw(m, rde));
}

void OpImulGvqpEvqpImm(struct Machine *m, u32 rde) {
  u8 b[8];
  Write64(b, m->xedd->op.uimm0);
  AluImul(m, rde, GetModrmRegisterWordPointerReadOszRexw(m, rde), b);
}
