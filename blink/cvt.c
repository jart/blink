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
#include <math.h>
#include <string.h>

#include "blink/builtin.h"
#include "blink/endian.h"
#include "blink/fpu.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/modrm.h"
#include "blink/mop.h"
#include "blink/pun.h"
#include "blink/stats.h"

#define kOpCvt0f2a  0
#define kOpCvtt0f2c 4
#define kOpCvt0f2d  8
#define kOpCvt0f5a  12
#define kOpCvt0f5b  16
#define kOpCvt0fE6  20

static double SseRoundDouble(struct Machine *m, double x) {
  switch ((m->mxcsr & kMxcsrRc) >> 13) {
    case 0:
      return rint(x);
    case 1:
      return floor(x);
    case 2:
      return ceil(x);
    case 3:
      return trunc(x);
    default:
      __builtin_unreachable();
  }
}

static void OpGdqpWssCvttss2si(P) {
  i64 n;
  union FloatPun f;
  f.i = Read32(GetModrmRegisterXmmPointerRead4(A));
  n = f.f;
  if (!Rexw(rde)) n &= 0xffffffff;
  Put64(RegRexrReg(m, rde), n);
}

static void OpGdqpWsdCvttsd2si(P) {
  i64 n;
  union DoublePun d;
  d.i = Read64(GetModrmRegisterXmmPointerRead8(A));
  n = d.f;
  if (!Rexw(rde)) n &= 0xffffffff;
  Put64(RegRexrReg(m, rde), n);
}

static void OpGdqpWssCvtss2si(P) {
  i64 n;
  union FloatPun f;
  f.i = Read32(GetModrmRegisterXmmPointerRead4(A));
  n = rintf(f.f);
  if (!Rexw(rde)) n &= 0xffffffff;
  Put64(RegRexrReg(m, rde), n);
}

static void OpGdqpWsdCvtsd2si(P) {
  i64 n;
  union DoublePun d;
  d.i = Read64(GetModrmRegisterXmmPointerRead8(A));
  n = SseRoundDouble(m, d.f);
  if (!Rexw(rde)) n &= 0xffffffff;
  Put64(RegRexrReg(m, rde), n);
}

static void OpVssEdqpCvtsi2ss(P) {
  union FloatPun f;
  if (Rexw(rde)) {
    i64 n = Read64(GetModrmRegisterWordPointerRead8(A));
    f.f = n;
    Put32(XmmRexrReg(m, rde), f.i);
  } else {
    i32 n = Read32(GetModrmRegisterWordPointerRead4(A));
    f.f = n;
    Put32(XmmRexrReg(m, rde), f.i);
  }
}

static void OpVsdEdqpCvtsi2sd(P) {
  union DoublePun d;
  if (Rexw(rde)) {
    d.f = (i64)Read64(GetModrmRegisterWordPointerRead8(A));
    Put64(XmmRexrReg(m, rde), d.i);
    if (IsMakingPath(m)) {
      Jitter(A,
             "z3B"    // res0 = GetRegOrMem[force64bit](RexbRm)
             "a2i"    // arg2 = RexrReg(rde)
             "s0a1="  // arg1 = machine
             "t"      // arg0 = res0
             "m",     // call function (d1)
             RexrReg(rde), Int64ToDouble);
    }
  } else {
    d.f = (i32)Read32(GetModrmRegisterWordPointerRead4(A));
    Put64(XmmRexrReg(m, rde), d.i);
    if (IsMakingPath(m)) {
      Jitter(A,
             "z2B"    // res0 = GetRegOrMem[force32bit](RexbRm)
             "a2i"    // arg2 = RexrReg(rde)
             "s0a1="  // arg1 = machine
             "t"      // arg0 = res0
             "m",     // call function (d1)
             RexrReg(rde), Int32ToDouble);
    }
  }
}

static void OpVpsQpiCvtpi2ps(P) {
  u8 *p;
  i32 i[2];
  union FloatPun f[2];
  p = GetModrmRegisterMmPointerRead8(A);
  i[0] = Read32(p + 0);
  i[1] = Read32(p + 4);
  f[0].f = i[0];
  f[1].f = i[1];
  Put32(XmmRexrReg(m, rde) + 0, f[0].i);
  Put32(XmmRexrReg(m, rde) + 4, f[1].i);
}

static void OpVpdQpiCvtpi2pd(P) {
  u8 *p;
  i32 n[2];
  union DoublePun f[2];
  p = GetModrmRegisterMmPointerRead8(A);
  n[0] = Read32(p + 0);
  n[1] = Read32(p + 4);
  f[0].f = n[0];
  f[1].f = n[1];
  Put64(XmmRexrReg(m, rde) + 0, f[0].i);
  Put64(XmmRexrReg(m, rde) + 8, f[1].i);
}

static void OpPpiWpsqCvtps2pi(P) {
  u8 *p;
  unsigned i;
  i32 n[2];
  union FloatPun f[2];
  p = GetModrmRegisterXmmPointerRead8(A);
  f[0].i = Read32(p + 0 * 4);
  f[1].i = Read32(p + 1 * 4);
  switch ((m->mxcsr & kMxcsrRc) >> 13) {
    case 0:
      for (i = 0; i < 2; ++i) n[i] = rintf(f[i].f);
      break;
    case 1:
      for (i = 0; i < 2; ++i) n[i] = floorf(f[i].f);
      break;
    case 2:
      for (i = 0; i < 2; ++i) n[i] = ceilf(f[i].f);
      break;
    case 3:
      for (i = 0; i < 2; ++i) n[i] = truncf(f[i].f);
      break;
    default:
      __builtin_unreachable();
  }
  Put32(MmReg(m, rde) + 0, n[0]);
  Put32(MmReg(m, rde) + 4, n[1]);
}

static void OpPpiWpsqCvttps2pi(P) {
  u8 *p;
  i32 n[2];
  union FloatPun f[2];
  p = GetModrmRegisterXmmPointerRead8(A);
  f[0].i = Read32(p + 0);
  f[1].i = Read32(p + 4);
  n[0] = f[0].f;
  n[1] = f[1].f;
  Put32(MmReg(m, rde) + 0, n[0]);
  Put32(MmReg(m, rde) + 4, n[1]);
}

static void OpPpiWpdCvtpd2pi(P) {
  u8 *p;
  unsigned i;
  i32 n[2];
  union DoublePun d[2];
  p = GetModrmRegisterXmmPointerRead16(A);
  d[0].i = Read64(p + 0);
  d[1].i = Read64(p + 8);
  for (i = 0; i < 2; ++i) n[i] = SseRoundDouble(m, d[i].f);
  Put32(MmReg(m, rde) + 0, n[0]);
  Put32(MmReg(m, rde) + 4, n[1]);
}

static void OpPpiWpdCvttpd2pi(P) {
  u8 *p;
  i32 n[2];
  union DoublePun d[2];
  p = GetModrmRegisterXmmPointerRead16(A);
  d[0].i = Read64(p + 0);
  d[1].i = Read64(p + 8);
  n[0] = d[0].f;
  n[1] = d[1].f;
  Put32(MmReg(m, rde) + 0, n[0]);
  Put32(MmReg(m, rde) + 4, n[1]);
}

static void OpVpdWpsCvtps2pd(P) {
  u8 *p;
  union FloatPun f[2];
  union DoublePun d[2];
  p = GetModrmRegisterXmmPointerRead8(A);
  f[0].i = Read32(p + 0);
  f[1].i = Read32(p + 4);
  d[0].f = f[0].f;
  d[1].f = f[1].f;
  Put64(XmmRexrReg(m, rde) + 0, d[0].i);
  Put64(XmmRexrReg(m, rde) + 8, d[1].i);
}

static void OpVpsWpdCvtpd2ps(P) {
  u8 *p;
  union FloatPun f[2];
  union DoublePun d[2];
  p = GetModrmRegisterXmmPointerRead16(A);
  d[0].i = Read64(p + 0);
  d[1].i = Read64(p + 8);
  f[0].f = d[0].f;
  f[1].f = d[1].f;
  Put32(XmmRexrReg(m, rde) + 0, f[0].i);
  Put32(XmmRexrReg(m, rde) + 4, f[1].i);
}

static void OpVssWsdCvtsd2ss(P) {
  union FloatPun f;
  union DoublePun d;
  d.i = Read64(GetModrmRegisterXmmPointerRead8(A));
  f.f = d.f;
  Put32(XmmRexrReg(m, rde), f.i);
}

static void OpVsdWssCvtss2sd(P) {
  union FloatPun f;
  union DoublePun d;
  f.i = Read32(GetModrmRegisterXmmPointerRead4(A));
  d.f = f.f;
  Put64(XmmRexrReg(m, rde), d.i);
}

static void OpVpsWdqCvtdq2ps(P) {
  u8 *p;
  i32 n[4];
  union FloatPun f[4];
  p = GetModrmRegisterXmmPointerRead16(A);
  n[0] = Read32(p + 0 * 4);
  n[1] = Read32(p + 1 * 4);
  n[2] = Read32(p + 2 * 4);
  n[3] = Read32(p + 3 * 4);
  f[0].f = n[0];
  f[1].f = n[1];
  f[2].f = n[2];
  f[3].f = n[3];
  Put32(XmmRexrReg(m, rde) + 0 * 4, f[0].i);
  Put32(XmmRexrReg(m, rde) + 1 * 4, f[1].i);
  Put32(XmmRexrReg(m, rde) + 2 * 4, f[2].i);
  Put32(XmmRexrReg(m, rde) + 3 * 4, f[3].i);
}

static void OpVpdWdqCvtdq2pd(P) {
  u8 *p;
  i32 n[2];
  union DoublePun d[2];
  p = GetModrmRegisterXmmPointerRead8(A);
  n[0] = Read32(p + 0 * 4);
  n[1] = Read32(p + 1 * 4);
  d[0].f = n[0];
  d[1].f = n[1];
  Put64(XmmRexrReg(m, rde) + 0, d[0].i);
  Put64(XmmRexrReg(m, rde) + 8, d[1].i);
}

static void OpVdqWpsCvttps2dq(P) {
  u8 *p;
  i32 n[4];
  union FloatPun f[4];
  p = GetModrmRegisterXmmPointerRead16(A);
  f[0].i = Read32(p + 0 * 4);
  f[1].i = Read32(p + 1 * 4);
  f[2].i = Read32(p + 2 * 4);
  f[3].i = Read32(p + 3 * 4);
  n[0] = f[0].f;
  n[1] = f[1].f;
  n[2] = f[2].f;
  n[3] = f[3].f;
  Put32(XmmRexrReg(m, rde) + 0 * 4, n[0]);
  Put32(XmmRexrReg(m, rde) + 1 * 4, n[1]);
  Put32(XmmRexrReg(m, rde) + 2 * 4, n[2]);
  Put32(XmmRexrReg(m, rde) + 3 * 4, n[3]);
}

static void OpVdqWpsCvtps2dq(P) {
  u8 *p;
  unsigned i;
  i32 n[4];
  union FloatPun f[4];
  p = GetModrmRegisterXmmPointerRead16(A);
  f[0].i = Read32(p + 0 * 4);
  f[1].i = Read32(p + 1 * 4);
  f[2].i = Read32(p + 2 * 4);
  f[3].i = Read32(p + 3 * 4);
  switch ((m->mxcsr & kMxcsrRc) >> 13) {
    case 0:
      for (i = 0; i < 4; ++i) n[i] = rintf(f[i].f);
      break;
    case 1:
      for (i = 0; i < 4; ++i) n[i] = floorf(f[i].f);
      break;
    case 2:
      for (i = 0; i < 4; ++i) n[i] = ceilf(f[i].f);
      break;
    case 3:
      for (i = 0; i < 4; ++i) n[i] = truncf(f[i].f);
      break;
    default:
      __builtin_unreachable();
  }
  Put32(XmmRexrReg(m, rde) + 0 * 4, n[0]);
  Put32(XmmRexrReg(m, rde) + 1 * 4, n[1]);
  Put32(XmmRexrReg(m, rde) + 2 * 4, n[2]);
  Put32(XmmRexrReg(m, rde) + 3 * 4, n[3]);
}

static void OpVdqWpdCvttpd2dq(P) {
  u8 *p;
  i32 n[2];
  union DoublePun d[2];
  p = GetModrmRegisterXmmPointerRead16(A);
  d[0].i = Read64(p + 0);
  d[1].i = Read64(p + 8);
  n[0] = d[0].f;
  n[1] = d[1].f;
  Put32(XmmRexrReg(m, rde) + 0, n[0]);
  Put32(XmmRexrReg(m, rde) + 4, n[1]);
}

static void OpVdqWpdCvtpd2dq(P) {
  u8 *p;
  i32 n[2];
  unsigned i;
  union DoublePun d[2];
  p = GetModrmRegisterXmmPointerRead16(A);
  d[0].i = Read64(p + 0);
  d[1].i = Read64(p + 8);
  for (i = 0; i < 2; ++i) n[i] = SseRoundDouble(m, d[i].f);
  Put32(XmmRexrReg(m, rde) + 0, n[0]);
  Put32(XmmRexrReg(m, rde) + 4, n[1]);
}

static void OpCvt(P, unsigned long op) {
  IGNORE_RACES_START();
  switch (op | Rep(rde) | Osz(rde)) {
    case kOpCvt0f2a + 0:
      OpVpsQpiCvtpi2ps(A);
      break;
    case kOpCvt0f2a + 1:
      OpVpdQpiCvtpi2pd(A);
      break;
    case kOpCvt0f2a + 2:
      OpVsdEdqpCvtsi2sd(A);  // #1 hot (6796033)
      break;
    case kOpCvt0f2a + 3:
      OpVssEdqpCvtsi2ss(A);
      break;
    case kOpCvtt0f2c + 0:
      OpPpiWpsqCvttps2pi(A);
      break;
    case kOpCvtt0f2c + 1:
      OpPpiWpdCvttpd2pi(A);
      break;
    case kOpCvtt0f2c + 2:  // #2 hot (111540)
      OpGdqpWsdCvttsd2si(A);
      break;
    case kOpCvtt0f2c + 3:
      OpGdqpWssCvttss2si(A);
      break;
    case kOpCvt0f2d + 0:
      OpPpiWpsqCvtps2pi(A);
      break;
    case kOpCvt0f2d + 1:
      OpPpiWpdCvtpd2pi(A);
      break;
    case kOpCvt0f2d + 2:
      OpGdqpWsdCvtsd2si(A);
      break;
    case kOpCvt0f2d + 3:
      OpGdqpWssCvtss2si(A);
      break;
    case kOpCvt0f5a + 0:
      OpVpdWpsCvtps2pd(A);
      break;
    case kOpCvt0f5a + 1:
      OpVpsWpdCvtpd2ps(A);
      break;
    case kOpCvt0f5a + 2:
      OpVssWsdCvtsd2ss(A);
      break;
    case kOpCvt0f5a + 3:
      OpVsdWssCvtss2sd(A);
      break;
    case kOpCvt0f5b + 0:
      OpVpsWdqCvtdq2ps(A);
      break;
    case kOpCvt0f5b + 1:
      OpVdqWpsCvtps2dq(A);
      break;
    case kOpCvt0f5b + 3:
      OpVdqWpsCvttps2dq(A);
      break;
    case kOpCvt0fE6 + 1:
      OpVdqWpdCvtpd2dq(A);
      break;
    case kOpCvt0fE6 + 2:
      OpVdqWpdCvttpd2dq(A);
      break;
    case kOpCvt0fE6 + 3:
      OpVpdWdqCvtdq2pd(A);
      break;
    default:
      OpUdImpl(m);
  }
  IGNORE_RACES_END();
}

void OpCvt0f2a(P) {
  OpCvt(A, kOpCvt0f2a);
}

void OpCvtt0f2c(P) {
  OpCvt(A, kOpCvtt0f2c);
}

void OpCvt0f2d(P) {
  OpCvt(A, kOpCvt0f2d);
}

void OpCvt0f5a(P) {
  OpCvt(A, kOpCvt0f5a);
}

void OpCvt0f5b(P) {
  OpCvt(A, kOpCvt0f5b);
}

void OpCvt0fE6(P) {
  OpCvt(A, kOpCvt0fE6);
}
