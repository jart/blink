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
#include "blink/macros.h"
#include "blink/modrm.h"
#include "blink/pun.h"

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

void OpUnpcklpsd(struct Machine *m, u32 rde) {
  u8 *a, *b;
  a = XmmRexrReg(m, rde);
  b = GetModrmRegisterXmmPointerRead8(m, rde);
  if (Osz(rde)) {
    memcpy(a + 8, b, 8);
  } else {
    memcpy(a + 4 * 3, b + 4, 4);
    memcpy(a + 4 * 2, a + 4, 4);
    memcpy(a + 4 * 1, b + 0, 4);
  }
}

void OpUnpckhpsd(struct Machine *m, u32 rde) {
  u8 *a, *b;
  a = XmmRexrReg(m, rde);
  b = GetModrmRegisterXmmPointerRead16(m, rde);
  if (Osz(rde)) {
    memcpy(a + 0, b + 8, 8);
    memcpy(a + 8, b + 8, 8);
  } else {
    memcpy(a + 4 * 0, a + 4 * 2, 4);
    memcpy(a + 4 * 1, b + 4 * 2, 4);
    memcpy(a + 4 * 2, a + 4 * 3, 4);
    memcpy(a + 4 * 3, b + 4 * 3, 4);
  }
}

void OpPextrwGdqpUdqIb(struct Machine *m, u32 rde) {
  u8 i;
  i = m->xedd->op.uimm0;
  i &= Osz(rde) ? 7 : 3;
  Put16(RegRexrReg(m, rde), Get16(XmmRexbRm(m, rde) + i * 2));
}

void OpPinsrwVdqEwIb(struct Machine *m, u32 rde) {
  u8 i;
  i = m->xedd->op.uimm0;
  i &= Osz(rde) ? 7 : 3;
  Put16(XmmRexrReg(m, rde) + i * 2,
        Read16(GetModrmRegisterWordPointerRead2(m, rde)));
}

void OpShuffle(struct Machine *m, u32 rde) {
  i16 q16[4];
  i16 x16[8];
  i32 x32[4];
  switch (m->xedd->op.rep | Osz(rde)) {
    case 0:
      memcpy(q16, GetModrmRegisterXmmPointerRead8(m, rde), 8);
      pshufw(q16, q16, m->xedd->op.uimm0);
      memcpy(XmmRexrReg(m, rde), q16, 8);
      break;
    case 1:
      memcpy(x32, GetModrmRegisterXmmPointerRead16(m, rde), 16);
      pshufd(x32, x32, m->xedd->op.uimm0);
      memcpy(XmmRexrReg(m, rde), x32, 16);
      break;
    case 2:
      memcpy(x16, GetModrmRegisterXmmPointerRead16(m, rde), 16);
      pshuflw(x16, x16, m->xedd->op.uimm0);
      memcpy(XmmRexrReg(m, rde), x16, 16);
      break;
    case 3:
      memcpy(x16, GetModrmRegisterXmmPointerRead16(m, rde), 16);
      pshufhw(x16, x16, m->xedd->op.uimm0);
      memcpy(XmmRexrReg(m, rde), x16, 16);
      break;
    default:
      __builtin_unreachable();
  }
}

static void Shufps(struct Machine *m, u32 rde) {
  u8 *p;
  union FloatPun x[4], y[4], z[4];
  p = GetModrmRegisterXmmPointerRead16(m, rde);
  y[0].i = Read32(p + 0 * 4);
  y[1].i = Read32(p + 1 * 4);
  y[2].i = Read32(p + 2 * 4);
  y[3].i = Read32(p + 3 * 4);
  p = XmmRexrReg(m, rde);
  x[0].i = Read32(p + 0 * 4);
  x[1].i = Read32(p + 1 * 4);
  x[2].i = Read32(p + 2 * 4);
  x[3].i = Read32(p + 3 * 4);
  z[0].f = y[(m->xedd->op.uimm0 & 0003) >> 0].f;
  z[1].f = y[(m->xedd->op.uimm0 & 0014) >> 2].f;
  z[2].f = x[(m->xedd->op.uimm0 & 0060) >> 4].f;
  z[3].f = x[(m->xedd->op.uimm0 & 0300) >> 6].f;
  Write32(p + 0 * 4, z[0].i);
  Write32(p + 1 * 4, z[1].i);
  Write32(p + 2 * 4, z[2].i);
  Write32(p + 3 * 4, z[3].i);
}

static void Shufpd(struct Machine *m, u32 rde) {
  u8 *p;
  union DoublePun x[2], y[2], z[2];
  p = GetModrmRegisterXmmPointerRead16(m, rde);
  y[0].i = Read64(p + 0 * 4);
  y[1].i = Read64(p + 1 * 4);
  p = XmmRexrReg(m, rde);
  x[0].i = Read64(p + 0 * 4);
  x[1].i = Read64(p + 1 * 4);
  z[0].f = y[(m->xedd->op.uimm0 & 0001) >> 0].f;
  z[1].f = x[(m->xedd->op.uimm0 & 0002) >> 1].f;
  Write64(p + 0 * 4, z[0].i);
  Write64(p + 1 * 4, z[1].i);
}

void OpShufpsd(struct Machine *m, u32 rde) {
  if (Osz(rde)) {
    Shufpd(m, rde);
  } else {
    Shufps(m, rde);
  }
}

void OpSqrtpsd(struct Machine *m, u32 rde) {
  switch (m->xedd->op.rep | Osz(rde)) {
    case 0: {
      int i;
      u8 *p;
      union FloatPun u[4];
      p = GetModrmRegisterXmmPointerRead16(m, rde);
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
      p = GetModrmRegisterXmmPointerRead16(m, rde);
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
      u.i = Read64(GetModrmRegisterXmmPointerRead8(m, rde));
      u.f = sqrt(u.f);
      Write64(XmmRexrReg(m, rde), u.i);
      break;
    }
    case 3: {
      union FloatPun u;
      u.i = Read32(GetModrmRegisterXmmPointerRead4(m, rde));
      u.f = sqrtf(u.f);
      Write32(XmmRexrReg(m, rde), u.i);
      break;
    }
    default:
      __builtin_unreachable();
  }
}

void OpRsqrtps(struct Machine *m, u32 rde) {
  if (m->xedd->op.rep != 3) {
    int i;
    u8 *p;
    union FloatPun u[4];
    p = GetModrmRegisterXmmPointerRead16(m, rde);
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
    u.i = Read32(GetModrmRegisterXmmPointerRead4(m, rde));
    u.f = 1.f / sqrtf(u.f);
    Write32(XmmRexrReg(m, rde), u.i);
  }
}

void OpRcpps(struct Machine *m, u32 rde) {
  if (m->xedd->op.rep != 3) {
    int i;
    u8 *p;
    union FloatPun u[4];
    p = GetModrmRegisterXmmPointerRead16(m, rde);
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
    u.i = Read32(GetModrmRegisterXmmPointerRead4(m, rde));
    u.f = 1.f / u.f;
    Write32(XmmRexrReg(m, rde), u.i);
  }
}

void OpComissVsWs(struct Machine *m, u32 rde) {
  u8 zf, cf, pf, ie;
  if (!Osz(rde)) {
    union FloatPun xf, yf;
    xf.i = Read32(XmmRexrReg(m, rde));
    yf.i = Read32(GetModrmRegisterXmmPointerRead4(m, rde));
    if (!isnan(xf.f) && !isnan(yf.f)) {
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
    yd.i = Read64(GetModrmRegisterXmmPointerRead8(m, rde));
    if (!isnan(xd.f) && !isnan(yd.f)) {
      zf = xd.f == yd.f;
      cf = xd.f < yd.f;
      pf = false;
      ie = false;
    } else {
      zf = cf = pf = ie = true;
    }
  }
  m->flags = SetFlag(m->flags, FLAGS_ZF, zf);
  m->flags = SetFlag(m->flags, FLAGS_PF, pf);
  m->flags = SetFlag(m->flags, FLAGS_CF, cf);
  m->flags = SetFlag(m->flags, FLAGS_SF, false);
  m->flags = SetFlag(m->flags, FLAGS_OF, false);
  if (m->xedd->op.opcode & 1) {
    m->mxcsr &= ~kMxcsrIe;
    if (ie) {
      m->mxcsr |= kMxcsrIe;
      if (!(m->mxcsr & kMxcsrIm)) {
        HaltMachine(m, kMachineSimdException);
      }
    }
  }
}

static inline void OpPsd(struct Machine *m, u32 rde, float fs(float x, float y),
                         double fd(double x, double y)) {
  if (m->xedd->op.rep == 2) {
    union DoublePun x, y;
    y.i = Read64(GetModrmRegisterXmmPointerRead8(m, rde));
    x.i = Read64(XmmRexrReg(m, rde));
    x.f = fd(x.f, y.f);
    Write64(XmmRexrReg(m, rde), x.i);
  } else if (m->xedd->op.rep == 3) {
    union FloatPun x, y;
    y.i = Read32(GetModrmRegisterXmmPointerRead4(m, rde));
    x.i = Read32(XmmRexrReg(m, rde));
    x.f = fs(x.f, y.f);
    Write32(XmmRexrReg(m, rde), x.i);
  } else if (Osz(rde)) {
    u8 *p;
    union DoublePun x[2], y[2];
    p = GetModrmRegisterXmmPointerRead16(m, rde);
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
    p = GetModrmRegisterXmmPointerRead16(m, rde);
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
}

static inline float Adds(float x, float y) {
  return x + y;
}

static inline double Addd(double x, double y) {
  return x + y;
}

void OpAddpsd(struct Machine *m, u32 rde) {
  OpPsd(m, rde, Adds, Addd);
}

static inline float Subs(float x, float y) {
  return x - y;
}

static inline double Subd(double x, double y) {
  return x - y;
}

void OpSubpsd(struct Machine *m, u32 rde) {
  OpPsd(m, rde, Subs, Subd);
}

static inline float Muls(float x, float y) {
  return x * y;
}

static inline double Muld(double x, double y) {
  return x * y;
}

void OpMulpsd(struct Machine *m, u32 rde) {
  OpPsd(m, rde, Muls, Muld);
}

static inline float Divs(float x, float y) {
  return x / y;
}

static inline double Divd(double x, double y) {
  return x / y;
}

void OpDivpsd(struct Machine *m, u32 rde) {
  OpPsd(m, rde, Divs, Divd);
}

static inline float Mins(float x, float y) {
  return MIN(x, y);
}

static inline double Mind(double x, double y) {
  return MIN(x, y);
}

void OpMinpsd(struct Machine *m, u32 rde) {
  OpPsd(m, rde, Mins, Mind);
}

static inline float Maxs(float x, float y) {
  return MAX(x, y);
}

static inline double Maxd(double x, double y) {
  return MAX(x, y);
}

void OpMaxpsd(struct Machine *m, u32 rde) {
  OpPsd(m, rde, Maxs, Maxd);
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

void OpCmppsd(struct Machine *m, u32 rde) {
  int imm = m->xedd->op.uimm0;
  if (m->xedd->op.rep == 2) {
    union DoublePun x, y;
    y.i = Read64(GetModrmRegisterXmmPointerRead8(m, rde));
    x.i = Read64(XmmRexrReg(m, rde));
    x.f = Cmpd(imm, x.f, y.f);
    Write64(XmmRexrReg(m, rde), x.i);
  } else if (m->xedd->op.rep == 3) {
    union FloatPun x, y;
    y.i = Read32(GetModrmRegisterXmmPointerRead4(m, rde));
    x.i = Read32(XmmRexrReg(m, rde));
    x.f = Cmps(imm, x.f, y.f);
    Write32(XmmRexrReg(m, rde), x.i);
  } else if (Osz(rde)) {
    u8 *p;
    union DoublePun x[2], y[2];
    p = GetModrmRegisterXmmPointerRead16(m, rde);
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
    p = GetModrmRegisterXmmPointerRead16(m, rde);
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
}

void OpAndpsd(struct Machine *m, u32 rde) {
  u64 x[2], y[2];
  memcpy(x, XmmRexrReg(m, rde), 16);
  memcpy(y, GetModrmRegisterXmmPointerRead16(m, rde), 16);
  x[0] &= y[0];
  x[1] &= y[1];
  memcpy(XmmRexrReg(m, rde), x, 16);
}

void OpAndnpsd(struct Machine *m, u32 rde) {
  u64 x[2], y[2];
  memcpy(x, XmmRexrReg(m, rde), 16);
  memcpy(y, GetModrmRegisterXmmPointerRead16(m, rde), 16);
  x[0] = ~x[0] & y[0];
  x[1] = ~x[1] & y[1];
  memcpy(XmmRexrReg(m, rde), x, 16);
}

void OpOrpsd(struct Machine *m, u32 rde) {
  u64 x[2], y[2];
  memcpy(x, XmmRexrReg(m, rde), 16);
  memcpy(y, GetModrmRegisterXmmPointerRead16(m, rde), 16);
  x[0] |= y[0];
  x[1] |= y[1];
  memcpy(XmmRexrReg(m, rde), x, 16);
}

void OpXorpsd(struct Machine *m, u32 rde) {
  u64 x[2], y[2];
  memcpy(x, XmmRexrReg(m, rde), 16);
  memcpy(y, GetModrmRegisterXmmPointerRead16(m, rde), 16);
  x[0] ^= y[0];
  x[1] ^= y[1];
  memcpy(XmmRexrReg(m, rde), x, 16);
}

void OpHaddpsd(struct Machine *m, u32 rde) {
  u8 *p;
  if (m->xedd->op.rep == 2) {
    union FloatPun x[4], y[4], z[4];
    p = GetModrmRegisterXmmPointerRead16(m, rde);
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
    p = GetModrmRegisterXmmPointerRead16(m, rde);
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
    OpUd(m, rde);
  }
}

void OpHsubpsd(struct Machine *m, u32 rde) {
  u8 *p;
  if (m->xedd->op.rep == 2) {
    union FloatPun x[4], y[4], z[4];
    p = GetModrmRegisterXmmPointerRead16(m, rde);
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
    p = GetModrmRegisterXmmPointerRead16(m, rde);
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
    OpUd(m, rde);
  }
}

void OpAddsubpsd(struct Machine *m, u32 rde) {
  u8 *p;
  if (m->xedd->op.rep == 2) {
    union FloatPun x[4], y[4], z[4];
    p = GetModrmRegisterXmmPointerRead16(m, rde);
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
    p = GetModrmRegisterXmmPointerRead16(m, rde);
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
    OpUd(m, rde);
  }
}
