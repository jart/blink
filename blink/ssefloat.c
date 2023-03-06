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
#include "blink/flags.h"
#include "blink/fpu.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/modrm.h"
#include "blink/pun.h"
#include "blink/stats.h"
#include "blink/tsan.h"

static void pshufw(i16 b[4], const i16 a[4], int m) {
  i16 t[4];
  t[0] = a[(m & 0003) >> 0];
  t[1] = a[(m & 0014) >> 2];
  t[2] = a[(m & 0060) >> 4];
  t[3] = a[(m & 0300) >> 6];
  b[0] = t[0];
  b[1] = t[1];
  b[2] = t[2];
  b[3] = t[3];
}

static void pshufd(i32 b[4], const i32 a[4], int m) {
  i32 t[4];
  t[0] = a[(m & 0003) >> 0];
  t[1] = a[(m & 0014) >> 2];
  t[2] = a[(m & 0060) >> 4];
  t[3] = a[(m & 0300) >> 6];
  b[0] = t[0];
  b[1] = t[1];
  b[2] = t[2];
  b[3] = t[3];
}

static void pshuflw(i16 b[8], const i16 a[8], int m) {
  i16 t[4];
  t[0] = a[(m & 0003) >> 0];
  t[1] = a[(m & 0014) >> 2];
  t[2] = a[(m & 0060) >> 4];
  t[3] = a[(m & 0300) >> 6];
  b[0] = t[0];
  b[1] = t[1];
  b[2] = t[2];
  b[3] = t[3];
  b[4] = a[4];
  b[5] = a[5];
  b[6] = a[6];
  b[7] = a[7];
}

static void pshufhw(i16 b[8], const i16 a[8], int m) {
  i16 t[4];
  t[0] = a[4 + ((m & 0003) >> 0)];
  t[1] = a[4 + ((m & 0014) >> 2)];
  t[2] = a[4 + ((m & 0060) >> 4)];
  t[3] = a[4 + ((m & 0300) >> 6)];
  b[0] = a[0];
  b[1] = a[1];
  b[2] = a[2];
  b[3] = a[3];
  b[4] = t[0];
  b[5] = t[1];
  b[6] = t[2];
  b[7] = t[3];
}

void OpUnpcklpsd(P) {
  u8 *a, *b;
  a = XmmRexrReg(m, rde);
  b = GetModrmRegisterXmmPointerRead8(A);
  IGNORE_RACES_START();
  if (Osz(rde)) {
    memcpy(a + 8, b, 8);
  } else {
    memcpy(a + 4 * 3, b + 4, 4);
    memcpy(a + 4 * 2, a + 4, 4);
    memcpy(a + 4 * 1, b + 0, 4);
  }
  IGNORE_RACES_END();
}

void OpUnpckhpsd(P) {
  u8 *a, *b;
  a = XmmRexrReg(m, rde);
  b = GetModrmRegisterXmmPointerRead16(A);
  IGNORE_RACES_START();
  if (Osz(rde)) {
    memcpy(a + 0, b + 8, 8);
    memcpy(a + 8, b + 8, 8);
  } else {
    memcpy(a + 4 * 0, a + 4 * 2, 4);
    memcpy(a + 4 * 1, b + 4 * 2, 4);
    memcpy(a + 4 * 2, a + 4 * 3, 4);
    memcpy(a + 4 * 3, b + 4 * 3, 4);
  }
  IGNORE_RACES_END();
}

void OpPextrwGdqpUdqIb(P) {
  u8 i;
  i = uimm0;
  i &= Osz(rde) ? 7 : 3;
  Put16(RegRexrReg(m, rde), Get16(XmmRexbRm(m, rde) + i * 2));
}

void OpPinsrwVdqEwIb(P) {
  u8 i;
  i = uimm0;
  i &= Osz(rde) ? 7 : 3;
  IGNORE_RACES_START();
  Put16(XmmRexrReg(m, rde) + i * 2,
        Read16(GetModrmRegisterWordPointerRead2(A)));
  IGNORE_RACES_END();
}

void OpShuffle(P) {
  i16 q16[4];
  void *kernel;
  IGNORE_RACES_START();
  switch (Rep(rde) | Osz(rde)) {
    case 0:
      memcpy(q16, GetModrmRegisterXmmPointerRead8(A), 8);
      pshufw(q16, q16, uimm0);
      memcpy(XmmRexrReg(m, rde), q16, 8);
      kernel = 0;
      break;
    case 1:
      pshufd((i32 *)XmmRexrReg(m, rde),
             (i32 *)GetModrmRegisterXmmPointerRead16(A), uimm0);
      kernel = (void *)pshufd;
      break;
    case 2:
      pshuflw((i16 *)XmmRexrReg(m, rde),
              (i16 *)GetModrmRegisterXmmPointerRead16(A), uimm0);
      kernel = (void *)pshuflw;
      break;
    case 3:
      pshufhw((i16 *)XmmRexrReg(m, rde),
              (i16 *)GetModrmRegisterXmmPointerRead16(A), uimm0);
      kernel = (void *)pshufhw;
      break;
    default:
      __builtin_unreachable();
  }
  IGNORE_RACES_END();
  if (IsMakingPath(m) && kernel) {
    Jitter(A,
           "z4P"    // res0 = GetXmmOrMemPointer(RexbRm)
           "r0s1="  // sav1 = res0
           "z4Q"    // res0 = GetXmmPointer(RexrReg)
           "a2i"    // arg2 = uimm0
           "s1a1="  // arg1 = sav1
           "t"      // arg0 = res0
           "c",     // call function
           uimm0, kernel);
  }
}

static void Shufps(P) {
  u8 *p;
  union FloatPun x[4], y[4], z[4];
  p = GetModrmRegisterXmmPointerRead16(A);
  IGNORE_RACES_START();
  y[0].i = Read32(p + 0 * 4);
  y[1].i = Read32(p + 1 * 4);
  y[2].i = Read32(p + 2 * 4);
  y[3].i = Read32(p + 3 * 4);
  p = XmmRexrReg(m, rde);
  x[0].i = Read32(p + 0 * 4);
  x[1].i = Read32(p + 1 * 4);
  x[2].i = Read32(p + 2 * 4);
  x[3].i = Read32(p + 3 * 4);
  z[0].f = x[(uimm0 & 0003) >> 0].f;
  z[1].f = x[(uimm0 & 0014) >> 2].f;
  z[2].f = y[(uimm0 & 0060) >> 4].f;
  z[3].f = y[(uimm0 & 0300) >> 6].f;
  Write32(p + 0 * 4, z[0].i);
  Write32(p + 1 * 4, z[1].i);
  Write32(p + 2 * 4, z[2].i);
  Write32(p + 3 * 4, z[3].i);
  IGNORE_RACES_END();
}

static void Shufpd(P) {
  u8 *p;
  union DoublePun x[2], y[2], z[2];
  p = GetModrmRegisterXmmPointerRead16(A);
  IGNORE_RACES_START();
  y[0].i = Read64(p + 0 * 8);
  y[1].i = Read64(p + 1 * 8);
  p = XmmRexrReg(m, rde);
  x[0].i = Read64(p + 0 * 8);
  x[1].i = Read64(p + 1 * 8);
  z[0].f = x[(uimm0 & 0001) >> 0].f;
  z[1].f = y[(uimm0 & 0002) >> 1].f;
  Write64(p + 0 * 8, z[0].i);
  Write64(p + 1 * 8, z[1].i);
  IGNORE_RACES_END();
}

void OpShufpsd(P) {
  if (Osz(rde)) {
    Shufpd(A);
  } else {
    Shufps(A);
  }
}

static void Movmskps(P) {
  u8 *p = GetModrmRegisterXmmPointerRead16(A);
  IGNORE_RACES_START();
  Put64(RegRexrReg(m, rde), (!!(p[15] & 0x80) << 3 | !!(p[11] & 0x80) << 2 |
                             !!(p[7] & 0x80) << 1 | !!(p[3] & 0x80)));
  IGNORE_RACES_END();
}

static void Movmskpd(P) {
  u8 *p = GetModrmRegisterXmmPointerRead16(A);
  IGNORE_RACES_START();
  Put64(RegRexrReg(m, rde), !!(p[15] & 0x80) << 1 | !!(p[7] & 0x80));
  IGNORE_RACES_END();
}

void OpMovmskpsd(P) {
  if (Osz(rde)) {
    Movmskpd(A);
  } else {
    Movmskps(A);
  }
}

void OpSqrtpsd(P) {
  IGNORE_RACES_START();
  switch (Rep(rde) | Osz(rde)) {
    case 0: {
      int i;
      u8 *p;
      union FloatPun u[4];
      p = GetModrmRegisterXmmPointerRead16(A);
      u[0].i = Read32(p + 0 * 4);
      u[1].i = Read32(p + 1 * 4);
      u[2].i = Read32(p + 2 * 4);
      u[3].i = Read32(p + 3 * 4);
      for (i = 0; i < 4; ++i) u[i].f = sqrtf(u[i].f);
      p = XmmRexrReg(m, rde);
      Write32(p + 0 * 4, u[0].i);
      Write32(p + 1 * 4, u[1].i);
      Write32(p + 2 * 4, u[2].i);
      Write32(p + 3 * 4, u[3].i);
      break;
    }
    case 1: {
      int i;
      u8 *p;
      union DoublePun u[2];
      p = GetModrmRegisterXmmPointerRead16(A);
      u[0].i = Read32(p + 0 * 8);
      u[1].i = Read32(p + 1 * 8);
      for (i = 0; i < 2; ++i) u[i].f = sqrt(u[i].f);
      p = XmmRexrReg(m, rde);
      Write32(p + 0 * 8, u[0].i);
      Write32(p + 1 * 8, u[1].i);
      break;
    }
    case 2: {
      union DoublePun u;
      u.i = Read64(GetModrmRegisterXmmPointerRead8(A));
      u.f = sqrt(u.f);
      Write64(XmmRexrReg(m, rde), u.i);
      break;
    }
    case 3: {
      union FloatPun u;
      u.i = Read32(GetModrmRegisterXmmPointerRead4(A));
      u.f = sqrtf(u.f);
      Write32(XmmRexrReg(m, rde), u.i);
      break;
    }
    default:
      __builtin_unreachable();
  }
  IGNORE_RACES_END();
}

void OpRsqrtps(P) {
  IGNORE_RACES_START();
  if (Rep(rde) != 3) {
    int i;
    u8 *p;
    union FloatPun u[4];
    p = GetModrmRegisterXmmPointerRead16(A);
    u[0].i = Read32(p + 0 * 4);
    u[1].i = Read32(p + 1 * 4);
    u[2].i = Read32(p + 2 * 4);
    u[3].i = Read32(p + 3 * 4);
    for (i = 0; i < 4; ++i) u[i].f = 1.f / sqrtf(u[i].f);
    p = XmmRexrReg(m, rde);
    Write32(p + 0 * 4, u[0].i);
    Write32(p + 1 * 4, u[1].i);
    Write32(p + 2 * 4, u[2].i);
    Write32(p + 3 * 4, u[3].i);
  } else {
    union FloatPun u;
    u.i = Read32(GetModrmRegisterXmmPointerRead4(A));
    u.f = 1.f / sqrtf(u.f);
    Write32(XmmRexrReg(m, rde), u.i);
  }
  IGNORE_RACES_END();
}

void OpRcpps(P) {
  IGNORE_RACES_START();
  if (Rep(rde) != 3) {
    int i;
    u8 *p;
    union FloatPun u[4];
    p = GetModrmRegisterXmmPointerRead16(A);
    u[0].i = Read32(p + 0 * 4);
    u[1].i = Read32(p + 1 * 4);
    u[2].i = Read32(p + 2 * 4);
    u[3].i = Read32(p + 3 * 4);
    for (i = 0; i < 4; ++i) u[i].f = 1.f / u[i].f;
    p = XmmRexrReg(m, rde);
    Write32(p + 0 * 4, u[0].i);
    Write32(p + 1 * 4, u[1].i);
    Write32(p + 2 * 4, u[2].i);
    Write32(p + 3 * 4, u[3].i);
  } else {
    union FloatPun u;
    u.i = Read32(GetModrmRegisterXmmPointerRead4(A));
    u.f = 1.f / u.f;
    Write32(XmmRexrReg(m, rde), u.i);
  }
  IGNORE_RACES_END();
}

static void ComissKernel(const u8 rxr[8], const u8 reg[8], struct Machine *m) {
  bool zf, cf;
  union DoublePun xd, yd;
  m->mxcsr &= ~kMxcsrIe;
  xd.i = Read64(rxr);
  yd.i = Read64(reg);
  if (!isunordered(xd.f, yd.f)) {
    zf = xd.f == yd.f;
    cf = xd.f < yd.f;
    m->flags = SetFlag(m->flags, FLAGS_ZF, zf);
    m->flags = SetFlag(m->flags, FLAGS_CF, cf);
    m->flags = SetFlag(m->flags, FLAGS_PF, false);
    m->flags = SetFlag(m->flags, FLAGS_SF, false);
    m->flags = SetFlag(m->flags, FLAGS_OF, false);
  } else {
    m->flags = SetFlag(m->flags, FLAGS_ZF, true);
    m->flags = SetFlag(m->flags, FLAGS_CF, true);
    m->flags = SetFlag(m->flags, FLAGS_PF, true);
    m->flags = SetFlag(m->flags, FLAGS_SF, false);
    m->flags = SetFlag(m->flags, FLAGS_OF, false);
    m->mxcsr |= kMxcsrIe;
    if (!(m->mxcsr & kMxcsrIm)) {
      HaltMachine(m, kMachineSimdException);
    }
  }
}

void OpComissVsWs(P) {
  bool isucomiss;
  u8 zf, cf, pf, ie;
  isucomiss = ~Opcode(rde) & 1;
  IGNORE_RACES_START();
  if (!Osz(rde)) {
    union FloatPun xf, yf;
    xf.i = Read32(XmmRexrReg(m, rde));
    yf.i = Read32(GetModrmRegisterXmmPointerRead4(A));
    if (!isunordered(xf.f, yf.f)) {
      zf = xf.f == yf.f;
      cf = xf.f < yf.f;
      pf = false;
      ie = false;
    } else {
      zf = cf = pf = ie = true;
    }
  } else {
    union DoublePun xd, yd;
    xd.i = Read64(XmmRexrReg(m, rde));
    yd.i = Read64(GetModrmRegisterXmmPointerRead8(A));
    if (!isunordered(xd.f, yd.f)) {
      zf = xd.f == yd.f;
      cf = xd.f < yd.f;
      pf = false;
      ie = false;
    } else {
      zf = cf = pf = ie = true;
    }
    if (IsMakingPath(m) && !isucomiss) {
      Jitter(A,
             "z4P"    // res0 = GetXmmOrMemPointer(RexbRm)
             "r0s1="  // sav1 = res0
             "z4Q"    // res0 = GetXmmPointer(RexrReg)
             "s0a2="  // arg2 = sav0 (machine)
             "s1a1="  // arg1 = sav1
             "t"      // arg0 = res0
             "c",     // call function
             ComissKernel);
    }
  }
  m->flags = SetFlag(m->flags, FLAGS_ZF, zf);
  m->flags = SetFlag(m->flags, FLAGS_PF, pf);
  m->flags = SetFlag(m->flags, FLAGS_CF, cf);
  m->flags = SetFlag(m->flags, FLAGS_SF, false);
  m->flags = SetFlag(m->flags, FLAGS_OF, false);
  if (!isucomiss) {
    m->mxcsr &= ~kMxcsrIe;
    if (ie) {
      m->mxcsr |= kMxcsrIe;
      if (!(m->mxcsr & kMxcsrIm)) {
        HaltMachine(m, kMachineSimdException);
      }
    }
  }
  IGNORE_RACES_END();
}

static void OpPsd(P, float fs(float x, float y), double fd(double x, double y),
                  void s1(u8 *, struct Machine *, long),
                  void d1(u8 *, struct Machine *, long)) {
  IGNORE_RACES_START();
  if (Rep(rde) == 2) {
    d1(GetModrmRegisterXmmPointerRead8(A), m, RexrReg(rde));
    if (IsMakingPath(m)) {
      Jitter(A,
             "z4P"    // res0 = GetXmmOrMemPointer(RexbRm)
             "a2i"    // arg2 = RexrReg(rde)
             "s0a1="  // arg1 = machine
             "t"      // arg0 = res0
             "m",     // call function (d1)
             RexrReg(rde), d1);
    }
  } else if (Rep(rde) == 3) {
    union FloatPun x, y;
    y.i = Read32(GetModrmRegisterXmmPointerRead4(A));
    x.i = Read32(XmmRexrReg(m, rde));
    x.f = fs(x.f, y.f);
    Write32(XmmRexrReg(m, rde), x.i);
    if (IsMakingPath(m)) {
      Jitter(A,
             "z4P"    // res0 = GetXmmOrMemPointer(RexbRm)
             "a2i"    // arg2 = RexrReg(rde)
             "s0a1="  // arg1 = machine
             "t"      // arg0 = res0
             "m",     // call function (d1)
             RexrReg(rde), s1);
    }
  } else if (Osz(rde)) {
    u8 *p;
    union DoublePun x[2], y[2];
    p = GetModrmRegisterXmmPointerRead16(A);
    y[0].i = Read64(p + 0 * 8);
    y[1].i = Read64(p + 1 * 8);
    p = XmmRexrReg(m, rde);
    x[0].i = Read64(p + 0 * 8);
    x[1].i = Read64(p + 1 * 8);
    x[0].f = fd(x[0].f, y[0].f);
    x[1].f = fd(x[1].f, y[1].f);
    Write64(p + 0 * 8, x[0].i);
    Write64(p + 1 * 8, x[1].i);
  } else {
    u8 *p;
    union FloatPun x[4], y[4];
    p = GetModrmRegisterXmmPointerRead16(A);
    y[0].i = Read32(p + 0 * 4);
    y[1].i = Read32(p + 1 * 4);
    y[2].i = Read32(p + 2 * 4);
    y[3].i = Read32(p + 3 * 4);
    p = XmmRexrReg(m, rde);
    x[0].i = Read32(p + 0 * 4);
    x[1].i = Read32(p + 1 * 4);
    x[2].i = Read32(p + 2 * 4);
    x[3].i = Read32(p + 3 * 4);
    x[0].f = fs(x[0].f, y[0].f);
    x[1].f = fs(x[1].f, y[1].f);
    x[2].f = fs(x[2].f, y[2].f);
    x[3].f = fs(x[3].f, y[3].f);
    Write32(p + 0 * 4, x[0].i);
    Write32(p + 1 * 4, x[1].i);
    Write32(p + 2 * 4, x[2].i);
    Write32(p + 3 * 4, x[3].i);
  }
  IGNORE_RACES_END();
}

static inline float Adds(float x, float y) {
  return x + y;
}

static inline double Addd(double x, double y) {
  return x + y;
}

void OpAddpsd(P) {
  OpPsd(A, Adds, Addd, OpPsdAdds1, OpPsdAddd1);
}

static inline float Subs(float x, float y) {
  return x - y;
}

static inline double Subd(double x, double y) {
  return x - y;
}

void OpSubpsd(P) {
  OpPsd(A, Subs, Subd, OpPsdSubs1, OpPsdSubd1);
}

static inline float Muls(float x, float y) {
  return x * y;
}

static inline double Muld(double x, double y) {
  return x * y;
}

void OpMulpsd(P) {
  OpPsd(A, Muls, Muld, OpPsdMuls1, OpPsdMuld1);
}

static inline float Divs(float x, float y) {
  return x / y;
}

static inline double Divd(double x, double y) {
  return x / y;
}

void OpDivpsd(P) {
  OpPsd(A, Divs, Divd, OpPsdDivs1, OpPsdDivd1);
}

static inline float Mins(float x, float y) {
  return MIN(x, y);
}

static inline double Mind(double x, double y) {
  return MIN(x, y);
}

void OpMinpsd(P) {
  OpPsd(A, Mins, Mind, OpPsdMins1, OpPsdMind1);
}

static inline float Maxs(float x, float y) {
  return MAX(x, y);
}

static inline double Maxd(double x, double y) {
  return MAX(x, y);
}

void OpMaxpsd(P) {
  OpPsd(A, Maxs, Maxd, OpPsdMaxs1, OpPsdMaxd1);
}

static int Cmps(int imm, float x, float y) {
  switch (imm) {
    case 0:
      return x == y ? -1 : 0;
    case 1:
      return x < y ? -1 : 0;
    case 2:
      return x <= y ? -1 : 0;
    case 3:
      return isnan(x) || isnan(y) ? -1 : 0;
    case 4:
      return x != y ? -1 : 0;
    case 5:
      return x >= y ? -1 : 0;
    case 6:
      return x > y ? -1 : 0;
    case 7:
      return !(isnan(x) || isnan(y)) ? -1 : 0;
    default:
      return 0;
  }
}

static i32 Cmpd(int imm, double x, double y) {
  switch (imm) {
    case 0:
      return x == y ? -1 : 0;
    case 1:
      return x < y ? -1 : 0;
    case 2:
      return x <= y ? -1 : 0;
    case 3:
      return isnan(x) || isnan(y) ? -1 : 0;
    case 4:
      return x != y ? -1 : 0;
    case 5:
      return x >= y ? -1 : 0;
    case 6:
      return x > y ? -1 : 0;
    case 7:
      return !(isnan(x) || isnan(y)) ? -1 : 0;
    default:
      return 0;
  }
}

void OpCmppsd(P) {
  IGNORE_RACES_START();
  int imm = uimm0;
  if (Rep(rde) == 2) {
    union DoublePun x, y;
    y.i = Read64(GetModrmRegisterXmmPointerRead8(A));
    x.i = Read64(XmmRexrReg(m, rde));
    x.f = Cmpd(imm, x.f, y.f);
    Write64(XmmRexrReg(m, rde), x.i);
  } else if (Rep(rde) == 3) {
    union FloatPun x, y;
    y.i = Read32(GetModrmRegisterXmmPointerRead4(A));
    x.i = Read32(XmmRexrReg(m, rde));
    x.f = Cmps(imm, x.f, y.f);
    Write32(XmmRexrReg(m, rde), x.i);
  } else if (Osz(rde)) {
    u8 *p;
    union DoublePun x[2], y[2];
    p = GetModrmRegisterXmmPointerRead16(A);
    y[0].i = Read64(p + 0 * 8);
    y[1].i = Read64(p + 1 * 8);
    p = XmmRexrReg(m, rde);
    x[0].i = Read64(p + 0 * 8);
    x[1].i = Read64(p + 1 * 8);
    x[0].f = Cmpd(imm, x[0].f, y[0].f);
    x[1].f = Cmpd(imm, x[1].f, y[1].f);
    Write64(p + 0 * 8, x[0].i);
    Write64(p + 1 * 8, x[1].i);
  } else {
    u8 *p;
    union FloatPun x[4], y[4];
    p = GetModrmRegisterXmmPointerRead16(A);
    y[0].i = Read32(p + 0 * 4);
    y[1].i = Read32(p + 1 * 4);
    y[2].i = Read32(p + 2 * 4);
    y[3].i = Read32(p + 3 * 4);
    p = XmmRexrReg(m, rde);
    x[0].i = Read32(p + 0 * 4);
    x[1].i = Read32(p + 1 * 4);
    x[2].i = Read32(p + 2 * 4);
    x[3].i = Read32(p + 3 * 4);
    x[0].f = Cmps(imm, x[0].f, y[0].f);
    x[1].f = Cmps(imm, x[1].f, y[1].f);
    x[2].f = Cmps(imm, x[2].f, y[2].f);
    x[3].f = Cmps(imm, x[3].f, y[3].f);
    Write32(p + 0 * 4, x[0].i);
    Write32(p + 1 * 4, x[1].i);
    Write32(p + 2 * 4, x[2].i);
    Write32(p + 3 * 4, x[3].i);
  }
  IGNORE_RACES_END();
}

void OpAndpsd(P) {
  IGNORE_RACES_START();
  u64 x[2], y[2];
  memcpy(x, XmmRexrReg(m, rde), 16);
  memcpy(y, GetModrmRegisterXmmPointerRead16(A), 16);
  x[0] &= y[0];
  x[1] &= y[1];
  memcpy(XmmRexrReg(m, rde), x, 16);
  IGNORE_RACES_END();
}

void OpAndnpsd(P) {
  IGNORE_RACES_START();
  u64 x[2], y[2];
  memcpy(x, XmmRexrReg(m, rde), 16);
  memcpy(y, GetModrmRegisterXmmPointerRead16(A), 16);
  x[0] = ~x[0] & y[0];
  x[1] = ~x[1] & y[1];
  memcpy(XmmRexrReg(m, rde), x, 16);
  IGNORE_RACES_END();
}

void OpOrpsd(P) {
  IGNORE_RACES_START();
  u64 x[2], y[2];
  memcpy(x, XmmRexrReg(m, rde), 16);
  memcpy(y, GetModrmRegisterXmmPointerRead16(A), 16);
  x[0] |= y[0];
  x[1] |= y[1];
  memcpy(XmmRexrReg(m, rde), x, 16);
  IGNORE_RACES_END();
}

void OpXorpsd(P) {
  IGNORE_RACES_START();
  u64 x[2], y[2];
  memcpy(x, XmmRexrReg(m, rde), 16);
  memcpy(y, GetModrmRegisterXmmPointerRead16(A), 16);
  x[0] ^= y[0];
  x[1] ^= y[1];
  memcpy(XmmRexrReg(m, rde), x, 16);
  IGNORE_RACES_END();
}

void OpHaddpsd(P) {
  IGNORE_RACES_START();
  u8 *p;
  if (Rep(rde) == 2) {
    union FloatPun x[4], y[4], z[4];
    p = GetModrmRegisterXmmPointerRead16(A);
    y[0].i = Read32(p + 0 * 4);
    y[1].i = Read32(p + 1 * 4);
    y[2].i = Read32(p + 2 * 4);
    y[3].i = Read32(p + 3 * 4);
    p = XmmRexrReg(m, rde);
    x[0].i = Read32(p + 0 * 4);
    x[1].i = Read32(p + 1 * 4);
    x[2].i = Read32(p + 2 * 4);
    x[3].i = Read32(p + 3 * 4);
    z[0].f = x[0].f + x[1].f;
    z[1].f = x[2].f + x[3].f;
    z[2].f = y[0].f + y[1].f;
    z[3].f = y[2].f + y[3].f;
    Write32(p + 0 * 4, z[0].i);
    Write32(p + 1 * 4, z[1].i);
    Write32(p + 2 * 4, z[2].i);
    Write32(p + 3 * 4, z[3].i);
  } else if (Osz(rde)) {
    union DoublePun x[2], y[2], z[2];
    p = GetModrmRegisterXmmPointerRead16(A);
    y[0].i = Read64(p + 0 * 8);
    y[1].i = Read64(p + 1 * 8);
    p = XmmRexrReg(m, rde);
    x[0].i = Read64(p + 0 * 8);
    x[1].i = Read64(p + 1 * 8);
    z[0].f = x[0].f + x[1].f;
    z[1].f = y[0].f + y[1].f;
    Write64(p + 0 * 8, z[0].i);
    Write64(p + 1 * 8, z[1].i);
  } else {
    OpUdImpl(m);
  }
  IGNORE_RACES_END();
}

void OpHsubpsd(P) {
  IGNORE_RACES_START();
  u8 *p;
  if (Rep(rde) == 2) {
    union FloatPun x[4], y[4], z[4];
    p = GetModrmRegisterXmmPointerRead16(A);
    y[0].i = Read32(p + 0 * 4);
    y[1].i = Read32(p + 1 * 4);
    y[2].i = Read32(p + 2 * 4);
    y[3].i = Read32(p + 3 * 4);
    p = XmmRexrReg(m, rde);
    x[0].i = Read32(p + 0 * 4);
    x[1].i = Read32(p + 1 * 4);
    x[2].i = Read32(p + 2 * 4);
    x[3].i = Read32(p + 3 * 4);
    z[0].f = x[0].f - x[1].f;
    z[1].f = x[2].f - x[3].f;
    z[2].f = y[0].f - y[1].f;
    z[3].f = y[2].f - y[3].f;
    Write32(p + 0 * 4, z[0].i);
    Write32(p + 1 * 4, z[1].i);
    Write32(p + 2 * 4, z[2].i);
    Write32(p + 3 * 4, z[3].i);
  } else if (Osz(rde)) {
    union DoublePun x[2], y[2], z[2];
    p = GetModrmRegisterXmmPointerRead16(A);
    y[0].i = Read64(p + 0 * 8);
    y[1].i = Read64(p + 1 * 8);
    p = XmmRexrReg(m, rde);
    x[0].i = Read64(p + 0 * 8);
    x[1].i = Read64(p + 1 * 8);
    z[0].f = x[0].f - x[1].f;
    z[1].f = y[0].f - y[1].f;
    Write64(p + 0 * 8, z[0].i);
    Write64(p + 1 * 8, z[1].i);
  } else {
    OpUdImpl(m);
  }
  IGNORE_RACES_END();
}

void OpAddsubpsd(P) {
  IGNORE_RACES_START();
  u8 *p;
  if (Rep(rde) == 2) {
    union FloatPun x[4], y[4], z[4];
    p = GetModrmRegisterXmmPointerRead16(A);
    y[0].i = Read32(p + 0 * 4);
    y[1].i = Read32(p + 1 * 4);
    y[2].i = Read32(p + 2 * 4);
    y[3].i = Read32(p + 3 * 4);
    p = XmmRexrReg(m, rde);
    x[0].i = Read32(p + 0 * 4);
    x[1].i = Read32(p + 1 * 4);
    x[2].i = Read32(p + 2 * 4);
    x[3].i = Read32(p + 3 * 4);
    z[0].f = x[0].f - y[0].f;
    z[1].f = x[1].f + y[1].f;
    z[2].f = x[2].f - y[2].f;
    z[3].f = x[3].f + y[3].f;
    Write32(p + 0 * 4, z[0].i);
    Write32(p + 1 * 4, z[1].i);
    Write32(p + 2 * 4, z[2].i);
    Write32(p + 3 * 4, z[3].i);
  } else if (Osz(rde)) {
    union DoublePun x[2], y[2], z[2];
    p = GetModrmRegisterXmmPointerRead16(A);
    y[0].i = Read64(p + 0 * 8);
    y[1].i = Read64(p + 1 * 8);
    p = XmmRexrReg(m, rde);
    x[0].i = Read64(p + 0 * 8);
    x[1].i = Read64(p + 1 * 8);
    z[0].f = x[0].f - y[0].f;
    z[1].f = x[1].f + y[1].f;
    Write64(p + 0 * 8, z[0].i);
    Write64(p + 1 * 8, z[1].i);
  } else {
    OpUdImpl(m);
  }
  IGNORE_RACES_END();
}
