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
#include "blink/memory.h"
#include "blink/modrm.h"
#include "blink/mop.h"
#include "blink/pun.h"

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

static void OpGdqpWssCvttss2si(struct Machine *m, DISPATCH_PARAMETERS) {
  i64 n;
  union FloatPun f;
  f.i = Load32(GetModrmRegisterXmmPointerRead4(m, DISPATCH_ARGUMENTS));
  n = f.f;
  if (!Rexw(rde)) n &= 0xffffffff;
  Put64(RegRexrReg(m, rde), n);
}

static void OpGdqpWsdCvttsd2si(struct Machine *m, DISPATCH_PARAMETERS) {
  i64 n;
  union DoublePun d;
  d.i = Load64(GetModrmRegisterXmmPointerRead8(m, DISPATCH_ARGUMENTS));
  n = d.f;
  if (!Rexw(rde)) n &= 0xffffffff;
  Put64(RegRexrReg(m, rde), n);
}

static void OpGdqpWssCvtss2si(struct Machine *m, DISPATCH_PARAMETERS) {
  i64 n;
  union FloatPun f;
  f.i = Load32(GetModrmRegisterXmmPointerRead4(m, DISPATCH_ARGUMENTS));
  n = rintf(f.f);
  if (!Rexw(rde)) n &= 0xffffffff;
  Put64(RegRexrReg(m, rde), n);
}

static void OpGdqpWsdCvtsd2si(struct Machine *m, DISPATCH_PARAMETERS) {
  i64 n;
  union DoublePun d;
  d.i = Load64(GetModrmRegisterXmmPointerRead8(m, DISPATCH_ARGUMENTS));
  n = SseRoundDouble(m, d.f);
  if (!Rexw(rde)) n &= 0xffffffff;
  Put64(RegRexrReg(m, rde), n);
}

static void OpVssEdqpCvtsi2ss(struct Machine *m, DISPATCH_PARAMETERS) {
  union FloatPun f;
  if (Rexw(rde)) {
    i64 n = Load64(GetModrmRegisterWordPointerRead8(m, DISPATCH_ARGUMENTS));
    f.f = n;
    Put32(XmmRexrReg(m, rde), f.i);
  } else {
    i32 n = Load32(GetModrmRegisterWordPointerRead4(m, DISPATCH_ARGUMENTS));
    f.f = n;
    Put32(XmmRexrReg(m, rde), f.i);
  }
}

static void OpVsdEdqpCvtsi2sd(struct Machine *m, DISPATCH_PARAMETERS) {
  union DoublePun d;
  if (Rexw(rde)) {
    i64 n = Load64(GetModrmRegisterWordPointerRead8(m, DISPATCH_ARGUMENTS));
    d.f = n;
    Put64(XmmRexrReg(m, rde), d.i);
  } else {
    i32 n = Load32(GetModrmRegisterWordPointerRead4(m, DISPATCH_ARGUMENTS));
    d.f = n;
    Put64(XmmRexrReg(m, rde), d.i);
  }
}

static void OpVpsQpiCvtpi2ps(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 *p;
  i32 i[2];
  union FloatPun f[2];
  p = GetModrmRegisterMmPointerRead8(m, DISPATCH_ARGUMENTS);
  i[0] = Load32(p + 0);
  i[1] = Load32(p + 4);
  f[0].f = i[0];
  f[1].f = i[1];
  Put32(XmmRexrReg(m, rde) + 0, f[0].i);
  Put32(XmmRexrReg(m, rde) + 4, f[1].i);
}

static void OpVpdQpiCvtpi2pd(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 *p;
  i32 n[2];
  union DoublePun f[2];
  p = GetModrmRegisterMmPointerRead8(m, DISPATCH_ARGUMENTS);
  n[0] = Load32(p + 0);
  n[1] = Load32(p + 4);
  f[0].f = n[0];
  f[1].f = n[1];
  Put64(XmmRexrReg(m, rde) + 0, f[0].i);
  Put64(XmmRexrReg(m, rde) + 8, f[1].i);
}

static void OpPpiWpsqCvtps2pi(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 *p;
  unsigned i;
  i32 n[2];
  union FloatPun f[2];
  p = GetModrmRegisterXmmPointerRead8(m, DISPATCH_ARGUMENTS);
  f[0].i = Load32(p + 0 * 4);
  f[1].i = Load32(p + 1 * 4);
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

static void OpPpiWpsqCvttps2pi(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 *p;
  i32 n[2];
  union FloatPun f[2];
  p = GetModrmRegisterXmmPointerRead8(m, DISPATCH_ARGUMENTS);
  f[0].i = Load32(p + 0);
  f[1].i = Load32(p + 4);
  n[0] = f[0].f;
  n[1] = f[1].f;
  Put32(MmReg(m, rde) + 0, n[0]);
  Put32(MmReg(m, rde) + 4, n[1]);
}

static void OpPpiWpdCvtpd2pi(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 *p;
  unsigned i;
  i32 n[2];
  union DoublePun d[2];
  p = GetModrmRegisterXmmPointerRead16(m, DISPATCH_ARGUMENTS);
  d[0].i = Load64(p + 0);
  d[1].i = Load64(p + 8);
  for (i = 0; i < 2; ++i) n[i] = SseRoundDouble(m, d[i].f);
  Put32(MmReg(m, rde) + 0, n[0]);
  Put32(MmReg(m, rde) + 4, n[1]);
}

static void OpPpiWpdCvttpd2pi(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 *p;
  i32 n[2];
  union DoublePun d[2];
  p = GetModrmRegisterXmmPointerRead16(m, DISPATCH_ARGUMENTS);
  d[0].i = Load64(p + 0);
  d[1].i = Load64(p + 8);
  n[0] = d[0].f;
  n[1] = d[1].f;
  Put32(MmReg(m, rde) + 0, n[0]);
  Put32(MmReg(m, rde) + 4, n[1]);
}

static void OpVpdWpsCvtps2pd(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 *p;
  union FloatPun f[2];
  union DoublePun d[2];
  p = GetModrmRegisterXmmPointerRead8(m, DISPATCH_ARGUMENTS);
  f[0].i = Load32(p + 0);
  f[1].i = Load32(p + 4);
  d[0].f = f[0].f;
  d[1].f = f[1].f;
  Put64(XmmRexrReg(m, rde) + 0, d[0].i);
  Put64(XmmRexrReg(m, rde) + 8, d[1].i);
}

static void OpVpsWpdCvtpd2ps(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 *p;
  union FloatPun f[2];
  union DoublePun d[2];
  p = GetModrmRegisterXmmPointerRead16(m, DISPATCH_ARGUMENTS);
  d[0].i = Load64(p + 0);
  d[1].i = Load64(p + 8);
  f[0].f = d[0].f;
  f[1].f = d[1].f;
  Put32(XmmRexrReg(m, rde) + 0, f[0].i);
  Put32(XmmRexrReg(m, rde) + 4, f[1].i);
}

static void OpVssWsdCvtsd2ss(struct Machine *m, DISPATCH_PARAMETERS) {
  union FloatPun f;
  union DoublePun d;
  d.i = Load64(GetModrmRegisterXmmPointerRead8(m, DISPATCH_ARGUMENTS));
  f.f = d.f;
  Put32(XmmRexrReg(m, rde), f.i);
}

static void OpVsdWssCvtss2sd(struct Machine *m, DISPATCH_PARAMETERS) {
  union FloatPun f;
  union DoublePun d;
  f.i = Load32(GetModrmRegisterXmmPointerRead4(m, DISPATCH_ARGUMENTS));
  d.f = f.f;
  Put64(XmmRexrReg(m, rde), d.i);
}

static void OpVpsWdqCvtdq2ps(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 *p;
  i32 n[4];
  union FloatPun f[4];
  p = GetModrmRegisterXmmPointerRead16(m, DISPATCH_ARGUMENTS);
  n[0] = Load32(p + 0 * 4);
  n[1] = Load32(p + 1 * 4);
  n[2] = Load32(p + 2 * 4);
  n[3] = Load32(p + 3 * 4);
  f[0].f = n[0];
  f[1].f = n[1];
  f[2].f = n[2];
  f[3].f = n[3];
  Put32(XmmRexrReg(m, rde) + 0 * 4, f[0].i);
  Put32(XmmRexrReg(m, rde) + 1 * 4, f[1].i);
  Put32(XmmRexrReg(m, rde) + 2 * 4, f[2].i);
  Put32(XmmRexrReg(m, rde) + 3 * 4, f[3].i);
}

static void OpVpdWdqCvtdq2pd(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 *p;
  i32 n[2];
  union DoublePun d[2];
  p = GetModrmRegisterXmmPointerRead8(m, DISPATCH_ARGUMENTS);
  n[0] = Load32(p + 0 * 4);
  n[1] = Load32(p + 1 * 4);
  d[0].f = n[0];
  d[1].f = n[1];
  Put64(XmmRexrReg(m, rde) + 0, d[0].i);
  Put64(XmmRexrReg(m, rde) + 8, d[1].i);
}

static void OpVdqWpsCvttps2dq(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 *p;
  i32 n[4];
  union FloatPun f[4];
  p = GetModrmRegisterXmmPointerRead16(m, DISPATCH_ARGUMENTS);
  f[0].i = Load32(p + 0 * 4);
  f[1].i = Load32(p + 1 * 4);
  f[2].i = Load32(p + 2 * 4);
  f[3].i = Load32(p + 3 * 4);
  n[0] = f[0].f;
  n[1] = f[1].f;
  n[2] = f[2].f;
  n[3] = f[3].f;
  Put32(XmmRexrReg(m, rde) + 0 * 4, n[0]);
  Put32(XmmRexrReg(m, rde) + 1 * 4, n[1]);
  Put32(XmmRexrReg(m, rde) + 2 * 4, n[2]);
  Put32(XmmRexrReg(m, rde) + 3 * 4, n[3]);
}

static void OpVdqWpsCvtps2dq(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 *p;
  unsigned i;
  i32 n[4];
  union FloatPun f[4];
  p = GetModrmRegisterXmmPointerRead16(m, DISPATCH_ARGUMENTS);
  f[0].i = Load32(p + 0 * 4);
  f[1].i = Load32(p + 1 * 4);
  f[2].i = Load32(p + 2 * 4);
  f[3].i = Load32(p + 3 * 4);
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

static void OpVdqWpdCvttpd2dq(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 *p;
  i32 n[2];
  union DoublePun d[2];
  p = GetModrmRegisterXmmPointerRead16(m, DISPATCH_ARGUMENTS);
  d[0].i = Load64(p + 0);
  d[1].i = Load64(p + 8);
  n[0] = d[0].f;
  n[1] = d[1].f;
  Put32(XmmRexrReg(m, rde) + 0, n[0]);
  Put32(XmmRexrReg(m, rde) + 4, n[1]);
}

static void OpVdqWpdCvtpd2dq(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 *p;
  i32 n[2];
  unsigned i;
  union DoublePun d[2];
  p = GetModrmRegisterXmmPointerRead16(m, DISPATCH_ARGUMENTS);
  d[0].i = Load64(p + 0);
  d[1].i = Load64(p + 8);
  for (i = 0; i < 2; ++i) n[i] = SseRoundDouble(m, d[i].f);
  Put32(XmmRexrReg(m, rde) + 0, n[0]);
  Put32(XmmRexrReg(m, rde) + 4, n[1]);
}

static void OpCvt(struct Machine *m, DISPATCH_PARAMETERS, unsigned long op) {
  switch (op | Rep(rde) | Osz(rde)) {
    case kOpCvt0f2a + 0:
      OpVpsQpiCvtpi2ps(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvt0f2a + 1:
      OpVpdQpiCvtpi2pd(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvt0f2a + 2:
      OpVsdEdqpCvtsi2sd(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvt0f2a + 3:
      OpVssEdqpCvtsi2ss(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvtt0f2c + 0:
      OpPpiWpsqCvttps2pi(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvtt0f2c + 1:
      OpPpiWpdCvttpd2pi(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvtt0f2c + 2:
      OpGdqpWsdCvttsd2si(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvtt0f2c + 3:
      OpGdqpWssCvttss2si(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvt0f2d + 0:
      OpPpiWpsqCvtps2pi(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvt0f2d + 1:
      OpPpiWpdCvtpd2pi(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvt0f2d + 2:
      OpGdqpWsdCvtsd2si(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvt0f2d + 3:
      OpGdqpWssCvtss2si(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvt0f5a + 0:
      OpVpdWpsCvtps2pd(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvt0f5a + 1:
      OpVpsWpdCvtpd2ps(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvt0f5a + 2:
      OpVssWsdCvtsd2ss(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvt0f5a + 3:
      OpVsdWssCvtss2sd(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvt0f5b + 0:
      OpVpsWdqCvtdq2ps(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvt0f5b + 1:
      OpVdqWpsCvtps2dq(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvt0f5b + 3:
      OpVdqWpsCvttps2dq(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvt0fE6 + 1:
      OpVdqWpdCvtpd2dq(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvt0fE6 + 2:
      OpVdqWpdCvttpd2dq(m, DISPATCH_ARGUMENTS);
      break;
    case kOpCvt0fE6 + 3:
      OpVpdWdqCvtdq2pd(m, DISPATCH_ARGUMENTS);
      break;
    default:
      OpUdImpl(m);
  }
}

void OpCvt0f2a(struct Machine *m, DISPATCH_PARAMETERS) {
  OpCvt(m, DISPATCH_ARGUMENTS, kOpCvt0f2a);
}

void OpCvtt0f2c(struct Machine *m, DISPATCH_PARAMETERS) {
  OpCvt(m, DISPATCH_ARGUMENTS, kOpCvtt0f2c);
}

void OpCvt0f2d(struct Machine *m, DISPATCH_PARAMETERS) {
  OpCvt(m, DISPATCH_ARGUMENTS, kOpCvt0f2d);
}

void OpCvt0f5a(struct Machine *m, DISPATCH_PARAMETERS) {
  OpCvt(m, DISPATCH_ARGUMENTS, kOpCvt0f5a);
}

void OpCvt0f5b(struct Machine *m, DISPATCH_PARAMETERS) {
  OpCvt(m, DISPATCH_ARGUMENTS, kOpCvt0f5b);
}

void OpCvt0fE6(struct Machine *m, DISPATCH_PARAMETERS) {
  OpCvt(m, DISPATCH_ARGUMENTS, kOpCvt0fE6);
}
