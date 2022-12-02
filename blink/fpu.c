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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blink/address.h"
#include "blink/builtin.h"
#include "blink/case.h"
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/fpu.h"
#include "blink/ldbl.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/memory.h"
#include "blink/modrm.h"
#include "blink/pun.h"

#define FPUREG 0
#define MEMORY 1

#define DISP(x, y, z) ((7 & (x)) << 4 | (y) << 3 | (z))

static void OnFpuStackOverflow(struct Machine *m) {
  m->fpu.sw |= kFpuSwIe | kFpuSwC1 | kFpuSwSf;
}

static double OnFpuStackUnderflow(struct Machine *m) {
  m->fpu.sw |= kFpuSwIe | kFpuSwSf;
  m->fpu.sw &= ~kFpuSwC1;
  return -NAN;
}

static double St(struct Machine *m, int i) {
  if (FpuGetTag(m, i) == kFpuTagEmpty) OnFpuStackUnderflow(m);
  return *FpuSt(m, i);
}

static double St0(struct Machine *m) {
  return St(m, 0);
}

static double St1(struct Machine *m) {
  return St(m, 1);
}

static double StRm(struct Machine *m, u64 rde) {
  return St(m, ModrmRm(rde));
}

static void FpuClearRoundup(struct Machine *m) {
  m->fpu.sw &= ~kFpuSwC1;
}

static void FpuClearOutOfRangeIndicator(struct Machine *m) {
  m->fpu.sw &= ~kFpuSwC2;
}

static void FpuSetSt0(struct Machine *m, double x) {
  *FpuSt(m, 0) = x;
}

static void FpuSetStRm(struct Machine *m, u64 rde, double x) {
  *FpuSt(m, ModrmRm(rde)) = x;
}

static void FpuSetStPop(struct Machine *m, int i, double x) {
  *FpuSt(m, i) = x;
  FpuPop(m);
}

static void FpuSetStRmPop(struct Machine *m, u64 rde, double x) {
  FpuSetStPop(m, ModrmRm(rde), x);
}

static i16 FpuGetMemoryShort(struct Machine *m) {
  u8 b[2];
  return Read16(Load(m, m->fpu.dp, 2, b));
}

static i32 FpuGetMemoryInt(struct Machine *m) {
  u8 b[4];
  return Read32(Load(m, m->fpu.dp, 4, b));
}

static i64 FpuGetMemoryLong(struct Machine *m) {
  u8 b[8];
  return Read64(Load(m, m->fpu.dp, 8, b));
}

static float FpuGetMemoryFloat(struct Machine *m) {
  union FloatPun u;
  u.i = FpuGetMemoryInt(m);
  return u.f;
}

static double FpuGetMemoryDouble(struct Machine *m) {
  union DoublePun u;
  u.i = FpuGetMemoryLong(m);
  return u.f;
}

static void FpuSetMemoryShort(struct Machine *m, i16 i) {
  void *p[2];
  u8 b[2];
  Write16(BeginStore(m, m->fpu.dp, 2, p, b), i);
  EndStore(m, m->fpu.dp, 2, p, b);
}

static void FpuSetMemoryInt(struct Machine *m, i32 i) {
  void *p[2];
  u8 b[4];
  Write32(BeginStore(m, m->fpu.dp, 4, p, b), i);
  EndStore(m, m->fpu.dp, 4, p, b);
}

static void FpuSetMemoryLong(struct Machine *m, i64 i) {
  void *p[2];
  u8 b[8];
  Write64(BeginStore(m, m->fpu.dp, 8, p, b), i);
  EndStore(m, m->fpu.dp, 8, p, b);
}

static void FpuSetMemoryFloat(struct Machine *m, float f) {
  union FloatPun u = {f};
  FpuSetMemoryInt(m, u.i);
}

static void FpuSetMemoryDouble(struct Machine *m, double f) {
  union DoublePun u = {f};
  FpuSetMemoryLong(m, u.i);
}

static double FpuGetMemoryLdbl(struct Machine *m) {
  u8 b[10];
  return DeserializeLdbl(Load(m, m->fpu.dp, 10, b));
}

static void FpuSetMemoryLdbl(struct Machine *m, double f) {
  void *p[2];
  u8 b[10], t[10];
  SerializeLdbl(b, f);
  memcpy(BeginStore(m, m->fpu.dp, 10, p, t), b, 10);
  EndStore(m, m->fpu.dp, 10, p, t);
}

static double f2xm1(double x) {
  return exp2(x) - 1;
}

static double fyl2x(double x, double y) {
  return y * log2(x);
}

static double fyl2xp1(double x, double y) {
  return y * log2(x + 1);
}

static double fscale(double significand, double exponent) {
  if (isunordered(significand, exponent)) return NAN;
  return ldexp(significand, exponent);
}

static double x87remainder(double x, double y, u32 *sw,
                           double rem(double, double), double rnd(double)) {
  int s;
  long q;
  double r;
  s = 0;
  r = rem(x, y);
  q = rnd(x / y);
  s &= ~kFpuSwC2; /* ty libm */
  if (q & 1) s |= kFpuSwC1;
  if (q & 2) s |= kFpuSwC3;
  if (q & 4) s |= kFpuSwC0;
  if (sw) *sw = s | (*sw & ~(kFpuSwC0 | kFpuSwC1 | kFpuSwC2 | kFpuSwC3));
  return r;
}

static double fprem(double dividend, double modulus, u32 *sw) {
  return x87remainder(dividend, modulus, sw, fmod, trunc);
}

static double fprem1(double dividend, double modulus, u32 *sw) {
  return x87remainder(dividend, modulus, sw, remainder, rint);
}

static double FpuAdd(struct Machine *m, double x, double y) {
  if (!isunordered(x, y)) {
    switch (isinf(y) << 1 | isinf(x)) {
      case 0:
        return x + y;
      case 1:
        return x;
      case 2:
        return y;
      case 3:
        if (signbit(x) == signbit(y)) {
          return x;
        } else {
          m->fpu.sw |= kFpuSwIe;
          return copysign(NAN, x);
        }
      default:
        __builtin_unreachable();
    }
  } else {
    return NAN;
  }
}

static double FpuSub(struct Machine *m, double x, double y) {
  if (!isunordered(x, y)) {
    switch (isinf(y) << 1 | isinf(x)) {
      case 0:
        return x - y;
      case 1:
        return -x;
      case 2:
        return y;
      case 3:
        if (signbit(x) == signbit(y)) {
          m->fpu.sw |= kFpuSwIe;
          return copysign(NAN, x);
        } else {
          return y;
        }
      default:
        __builtin_unreachable();
    }
  } else {
    return NAN;
  }
}

static double FpuMul(struct Machine *m, double x, double y) {
  if (!isunordered(x, y)) {
    if (!((isinf(x) && !y) || (isinf(y) && !x))) {
      return x * y;
    } else {
      m->fpu.sw |= kFpuSwIe;
      return -NAN;
    }
  } else {
    return NAN;
  }
}

static double FpuDiv(struct Machine *m, double x, double y) {
  if (!isunordered(x, y)) {
    if (x || y) {
      if (y) {
        return x / y;
      } else {
        m->fpu.sw |= kFpuSwZe;
        return copysign(INFINITY, x);
      }
    } else {
      m->fpu.sw |= kFpuSwIe;
      return copysign(NAN, x);
    }
  } else {
    return NAN;
  }
}

static double FpuRound(struct Machine *m, double x) {
  switch ((m->fpu.cw & kFpuCwRc) >> 10) {
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

static void FpuCompare(struct Machine *m, double y) {
  double x = St0(m);
  m->fpu.sw &= ~(kFpuSwC0 | kFpuSwC1 | kFpuSwC2 | kFpuSwC3);
  if (!isunordered(x, y)) {
    if (x < y) m->fpu.sw |= kFpuSwC0;
    if (x == y) m->fpu.sw |= kFpuSwC3;
  } else {
    m->fpu.sw |= kFpuSwC0 | kFpuSwC2 | kFpuSwC3 | kFpuSwIe;
  }
}

static void OpFxam(struct Machine *m) {
  double x;
  x = *FpuSt(m, 0);
  m->fpu.sw &= ~(kFpuSwC0 | kFpuSwC1 | kFpuSwC2 | kFpuSwC3);
  if (signbit(x)) m->fpu.sw |= kFpuSwC1;
  if (FpuGetTag(m, 0) == kFpuTagEmpty) {
    m->fpu.sw |= kFpuSwC0 | kFpuSwC3;
  } else {
    switch (fpclassify(x)) {
      case FP_NAN:
        m->fpu.sw |= kFpuSwC0;
        break;
      case FP_INFINITE:
        m->fpu.sw |= kFpuSwC0 | kFpuSwC2;
        break;
      case FP_ZERO:
        m->fpu.sw |= kFpuSwC3;
        break;
      case FP_SUBNORMAL:
        m->fpu.sw |= kFpuSwC2 | kFpuSwC3;
        break;
      case FP_NORMAL:
        m->fpu.sw |= kFpuSwC2;
        break;
      default:
        __builtin_unreachable();
    }
  }
}

static void OpFtst(struct Machine *m) {
  FpuCompare(m, 0);
}

static void OpFcmovb(struct Machine *m, u64 rde) {
  if (GetFlag(m->flags, FLAGS_CF)) {
    FpuSetSt0(m, StRm(m, rde));
  }
}

static void OpFcmove(struct Machine *m, u64 rde) {
  if (GetFlag(m->flags, FLAGS_ZF)) {
    FpuSetSt0(m, StRm(m, rde));
  }
}

static void OpFcmovbe(struct Machine *m, u64 rde) {
  if (GetFlag(m->flags, FLAGS_CF) || GetFlag(m->flags, FLAGS_ZF)) {
    FpuSetSt0(m, StRm(m, rde));
  }
}

static void OpFcmovu(struct Machine *m, u64 rde) {
  if (GetFlag(m->flags, FLAGS_PF)) {
    FpuSetSt0(m, StRm(m, rde));
  }
}

static void OpFcmovnb(struct Machine *m, u64 rde) {
  if (!GetFlag(m->flags, FLAGS_CF)) {
    FpuSetSt0(m, StRm(m, rde));
  }
}

static void OpFcmovne(struct Machine *m, u64 rde) {
  if (!GetFlag(m->flags, FLAGS_ZF)) {
    FpuSetSt0(m, StRm(m, rde));
  }
}

static void OpFcmovnbe(struct Machine *m, u64 rde) {
  if (!(GetFlag(m->flags, FLAGS_CF) || GetFlag(m->flags, FLAGS_ZF))) {
    FpuSetSt0(m, StRm(m, rde));
  }
}

static void OpFcmovnu(struct Machine *m, u64 rde) {
  if (!GetFlag(m->flags, FLAGS_PF)) {
    FpuSetSt0(m, StRm(m, rde));
  }
}

static void OpFchs(struct Machine *m) {
  FpuSetSt0(m, -St0(m));
}

static void OpFabs(struct Machine *m) {
  FpuSetSt0(m, fabs(St0(m)));
}

static void OpF2xm1(struct Machine *m) {
  FpuSetSt0(m, f2xm1(St0(m)));
}

static void OpFyl2x(struct Machine *m) {
  FpuSetStPop(m, 1, fyl2x(St0(m), St1(m)));
}

static void OpFyl2xp1(struct Machine *m) {
  FpuSetStPop(m, 1, fyl2xp1(St0(m), St1(m)));
}

static void OpFcos(struct Machine *m) {
  FpuClearOutOfRangeIndicator(m);
  FpuSetSt0(m, cos(St0(m)));
}

static void OpFsin(struct Machine *m) {
  FpuClearOutOfRangeIndicator(m);
  FpuSetSt0(m, sin(St0(m)));
}

static void OpFptan(struct Machine *m) {
  FpuClearOutOfRangeIndicator(m);
  FpuSetSt0(m, tan(St0(m)));
  FpuPush(m, 1);
}

static void OpFsincos(struct Machine *m) {
  double tsin, tcos;
  FpuClearOutOfRangeIndicator(m);
  tsin = sin(St0(m));
  tcos = cos(St0(m));
  FpuSetSt0(m, tsin);
  FpuPush(m, tcos);
}

static void OpFpatan(struct Machine *m) {
  FpuClearRoundup(m);
  FpuSetStPop(m, 1, atan2(St1(m), St0(m)));
}

static void OpFcom(struct Machine *m, u64 rde) {
  FpuCompare(m, StRm(m, rde));
}

static void OpFcomp(struct Machine *m, u64 rde) {
  FpuCompare(m, StRm(m, rde));
  FpuPop(m);
}

static void OpFaddStEst(struct Machine *m, u64 rde) {
  FpuSetSt0(m, FpuAdd(m, St0(m), StRm(m, rde)));
}

static void OpFmulStEst(struct Machine *m, u64 rde) {
  FpuSetSt0(m, FpuMul(m, St0(m), StRm(m, rde)));
}

static void OpFsubStEst(struct Machine *m, u64 rde) {
  FpuSetSt0(m, FpuSub(m, St0(m), StRm(m, rde)));
}

static void OpFsubrStEst(struct Machine *m, u64 rde) {
  FpuSetSt0(m, FpuSub(m, StRm(m, rde), St0(m)));
}

static void OpFdivStEst(struct Machine *m, u64 rde) {
  FpuSetSt0(m, FpuDiv(m, St0(m), StRm(m, rde)));
}

static void OpFdivrStEst(struct Machine *m, u64 rde) {
  FpuSetSt0(m, FpuDiv(m, StRm(m, rde), St0(m)));
}

static void OpFaddEstSt(struct Machine *m, u64 rde) {
  FpuSetStRm(m, rde, FpuAdd(m, StRm(m, rde), St0(m)));
}

static void OpFmulEstSt(struct Machine *m, u64 rde) {
  FpuSetStRm(m, rde, FpuMul(m, StRm(m, rde), St0(m)));
}

static void OpFsubEstSt(struct Machine *m, u64 rde) {
  FpuSetStRm(m, rde, FpuSub(m, St0(m), StRm(m, rde)));
}

static void OpFsubrEstSt(struct Machine *m, u64 rde) {
  FpuSetStRm(m, rde, FpuSub(m, StRm(m, rde), St0(m)));
}

static void OpFdivEstSt(struct Machine *m, u64 rde) {
  FpuSetStRm(m, rde, FpuDiv(m, StRm(m, rde), St0(m)));
}

static void OpFdivrEstSt(struct Machine *m, u64 rde) {
  FpuSetStRm(m, rde, FpuDiv(m, St0(m), StRm(m, rde)));
}

static void OpFaddp(struct Machine *m, u64 rde) {
  FpuSetStRmPop(m, rde, FpuAdd(m, St0(m), StRm(m, rde)));
}

static void OpFmulp(struct Machine *m, u64 rde) {
  FpuSetStRmPop(m, rde, FpuMul(m, St0(m), StRm(m, rde)));
}

static void OpFcompp(struct Machine *m, u64 rde) {
  OpFcomp(m, rde);
  FpuPop(m);
}

static void OpFsubp(struct Machine *m, u64 rde) {
  FpuSetStRmPop(m, rde, FpuSub(m, St0(m), StRm(m, rde)));
}

static void OpFsubrp(struct Machine *m, u64 rde) {
  FpuSetStPop(m, 1, FpuSub(m, StRm(m, rde), St0(m)));
}

static void OpFdivp(struct Machine *m, u64 rde) {
  FpuSetStRmPop(m, rde, FpuDiv(m, St0(m), StRm(m, rde)));
}

static void OpFdivrp(struct Machine *m, u64 rde) {
  FpuSetStRmPop(m, rde, FpuDiv(m, StRm(m, rde), St0(m)));
}

static void OpFadds(struct Machine *m, u64 rde) {
  FpuSetSt0(m, FpuAdd(m, St0(m), FpuGetMemoryFloat(m)));
}

static void OpFmuls(struct Machine *m, u64 rde) {
  FpuSetSt0(m, FpuMul(m, St0(m), FpuGetMemoryFloat(m)));
}

static void OpFcoms(struct Machine *m) {
  FpuCompare(m, FpuGetMemoryFloat(m));
}

static void OpFcomps(struct Machine *m) {
  OpFcoms(m);
  FpuPop(m);
}

static void OpFsubs(struct Machine *m) {
  FpuSetSt0(m, FpuSub(m, St0(m), FpuGetMemoryFloat(m)));
}

static void OpFsubrs(struct Machine *m) {
  FpuSetSt0(m, FpuSub(m, FpuGetMemoryFloat(m), St0(m)));
}

static void OpFdivs(struct Machine *m) {
  FpuSetSt0(m, FpuDiv(m, St0(m), FpuGetMemoryFloat(m)));
}

static void OpFdivrs(struct Machine *m) {
  FpuSetSt0(m, FpuDiv(m, FpuGetMemoryFloat(m), St0(m)));
}

static void OpFaddl(struct Machine *m) {
  FpuSetSt0(m, FpuAdd(m, St0(m), FpuGetMemoryDouble(m)));
}

static void OpFmull(struct Machine *m) {
  FpuSetSt0(m, FpuMul(m, St0(m), FpuGetMemoryDouble(m)));
}

static void OpFcoml(struct Machine *m) {
  FpuCompare(m, FpuGetMemoryDouble(m));
}

static void OpFcompl(struct Machine *m) {
  FpuCompare(m, FpuGetMemoryDouble(m));
  FpuPop(m);
}

static void OpFsubl(struct Machine *m) {
  FpuSetSt0(m, FpuSub(m, St0(m), FpuGetMemoryDouble(m)));
}

static void OpFsubrl(struct Machine *m) {
  FpuSetSt0(m, FpuSub(m, FpuGetMemoryDouble(m), St0(m)));
}

static void OpFdivl(struct Machine *m) {
  FpuSetSt0(m, FpuDiv(m, St0(m), FpuGetMemoryDouble(m)));
}

static void OpFdivrl(struct Machine *m) {
  FpuSetSt0(m, FpuDiv(m, FpuGetMemoryDouble(m), St0(m)));
}

static void OpFiaddl(struct Machine *m) {
  FpuSetSt0(m, FpuAdd(m, St0(m), FpuGetMemoryInt(m)));
}

static void OpFimull(struct Machine *m) {
  FpuSetSt0(m, FpuMul(m, St0(m), FpuGetMemoryInt(m)));
}

static void OpFicoml(struct Machine *m) {
  FpuCompare(m, FpuGetMemoryInt(m));
}

static void OpFicompl(struct Machine *m) {
  OpFicoml(m);
  FpuPop(m);
}

static void OpFisubl(struct Machine *m) {
  FpuSetSt0(m, FpuSub(m, St0(m), FpuGetMemoryInt(m)));
}

static void OpFisubrl(struct Machine *m) {
  FpuSetSt0(m, FpuSub(m, FpuGetMemoryInt(m), St0(m)));
}

static void OpFidivl(struct Machine *m) {
  FpuSetSt0(m, FpuDiv(m, St0(m), FpuGetMemoryInt(m)));
}

static void OpFidivrl(struct Machine *m) {
  FpuSetSt0(m, FpuDiv(m, FpuGetMemoryInt(m), St0(m)));
}

static void OpFiadds(struct Machine *m) {
  FpuSetSt0(m, FpuAdd(m, St0(m), FpuGetMemoryShort(m)));
}

static void OpFimuls(struct Machine *m) {
  FpuSetSt0(m, FpuMul(m, St0(m), FpuGetMemoryShort(m)));
}

static void OpFicoms(struct Machine *m) {
  FpuCompare(m, FpuGetMemoryShort(m));
}

static void OpFicomps(struct Machine *m) {
  OpFicoms(m);
  FpuPop(m);
}

static void OpFisubs(struct Machine *m) {
  FpuSetSt0(m, FpuSub(m, St0(m), FpuGetMemoryShort(m)));
}

static void OpFisubrs(struct Machine *m) {
  FpuSetSt0(m, FpuSub(m, FpuGetMemoryShort(m), St0(m)));
}

static void OpFidivs(struct Machine *m) {
  FpuSetSt0(m, FpuDiv(m, St0(m), FpuGetMemoryShort(m)));
}

static void OpFidivrs(struct Machine *m) {
  FpuSetSt0(m, FpuDiv(m, FpuGetMemoryShort(m), St0(m)));
}

static void OpFsqrt(struct Machine *m) {
  FpuClearRoundup(m);
  FpuSetSt0(m, sqrt(St0(m)));
}

static void OpFrndint(struct Machine *m) {
  FpuSetSt0(m, FpuRound(m, St0(m)));
}

static void OpFscale(struct Machine *m) {
  FpuClearRoundup(m);
  FpuSetSt0(m, fscale(St0(m), St1(m)));
}

static void OpFprem(struct Machine *m) {
  FpuSetSt0(m, fprem(St0(m), St1(m), &m->fpu.sw));
}

static void OpFprem1(struct Machine *m) {
  FpuSetSt0(m, fprem1(St0(m), St1(m), &m->fpu.sw));
}

static void OpFdecstp(struct Machine *m) {
  m->fpu.sw = (m->fpu.sw & ~kFpuSwSp) | ((m->fpu.sw - (1 << 11)) & kFpuSwSp);
}

static void OpFincstp(struct Machine *m) {
  m->fpu.sw = (m->fpu.sw & ~kFpuSwSp) | ((m->fpu.sw + (1 << 11)) & kFpuSwSp);
}

static void OpFxtract(struct Machine *m) {
  double x = St0(m);
  FpuSetSt0(m, logb(x));
  FpuPush(m, ldexp(x, -ilogb(x)));
}

static void OpFld(struct Machine *m, u64 rde) {
  FpuPush(m, StRm(m, rde));
}

static void OpFlds(struct Machine *m) {
  FpuPush(m, FpuGetMemoryFloat(m));
}

static void OpFsts(struct Machine *m) {
  FpuSetMemoryFloat(m, St0(m));
}

static void OpFstps(struct Machine *m) {
  OpFsts(m);
  FpuPop(m);
}

static void OpFstpt(struct Machine *m) {
  FpuSetMemoryLdbl(m, FpuPop(m));
}

static void OpFstl(struct Machine *m) {
  FpuSetMemoryDouble(m, St0(m));
}

static void OpFstpl(struct Machine *m) {
  OpFstl(m);
  FpuPop(m);
}

static void OpFst(struct Machine *m, u64 rde) {
  FpuSetStRm(m, rde, St0(m));
}

static void OpFstp(struct Machine *m, u64 rde) {
  FpuSetStRmPop(m, rde, St0(m));
}

static void OpFxch(struct Machine *m, u64 rde) {
  double t = StRm(m, rde);
  FpuSetStRm(m, rde, St0(m));
  FpuSetSt0(m, t);
}

static void OpFldcw(struct Machine *m) {
  m->fpu.cw = FpuGetMemoryShort(m);
}

static void OpFldt(struct Machine *m) {
  FpuPush(m, FpuGetMemoryLdbl(m));
}

static void OpFldl(struct Machine *m) {
  FpuPush(m, FpuGetMemoryDouble(m));
}

static double Fld1(void) {
  return 1;
}

static double Fldl2t(void) {
  return 0xd.49a784bcd1b8afep-2L; /* log₂10 */
}

static double Fldl2e(void) {
  return 0xb.8aa3b295c17f0bcp-3L; /* log₂𝑒 */
}

static double Fldpi(void) {
  return 0x1.921fb54442d1846ap+1L; /* π */
}

static double Fldlg2(void) {
  return 0x9.a209a84fbcff799p-5L; /* log₁₀2 */
}

static double Fldln2(void) {
  return 0xb.17217f7d1cf79acp-4L; /* logₑ2 */
}

static double Fldz(void) {
  return 0;
}

static void OpFldConstant(struct Machine *m, u64 rde) {
  double x;
  switch (ModrmRm(rde)) {
    CASE(0, x = Fld1());
    CASE(1, x = Fldl2t());
    CASE(2, x = Fldl2e());
    CASE(3, x = Fldpi());
    CASE(4, x = Fldlg2());
    CASE(5, x = Fldln2());
    CASE(6, x = Fldz());
    default:
      OpUdImpl(m);
  }
  FpuPush(m, x);
}

static void OpFstcw(struct Machine *m) {
  FpuSetMemoryShort(m, m->fpu.cw);
}

static void OpFilds(struct Machine *m) {
  FpuPush(m, FpuGetMemoryShort(m));
}

static void OpFildl(struct Machine *m) {
  FpuPush(m, FpuGetMemoryInt(m));
}

static void OpFildll(struct Machine *m) {
  FpuPush(m, FpuGetMemoryLong(m));
}

static void OpFisttpl(struct Machine *m) {
  FpuSetMemoryInt(m, FpuPop(m));
}

static void OpFisttpll(struct Machine *m) {
  FpuSetMemoryLong(m, FpuPop(m));
}

static void OpFisttps(struct Machine *m) {
  FpuSetMemoryShort(m, FpuPop(m));
}

static void OpFists(struct Machine *m) {
  FpuSetMemoryShort(m, FpuRound(m, St0(m)));
}

static void OpFistl(struct Machine *m) {
  FpuSetMemoryInt(m, FpuRound(m, St0(m)));
}

static void OpFistll(struct Machine *m) {
  FpuSetMemoryLong(m, FpuRound(m, St0(m)));
}

static void OpFistpl(struct Machine *m) {
  OpFistl(m);
  FpuPop(m);
}

static void OpFistpll(struct Machine *m) {
  OpFistll(m);
  FpuPop(m);
}

static void OpFistps(struct Machine *m) {
  OpFists(m);
  FpuPop(m);
}

static void OpFcomi(struct Machine *m, u64 rde) {
  double x, y;
  x = St0(m);
  y = StRm(m, rde);
  if (!isunordered(x, y)) {
    m->flags = SetFlag(m->flags, FLAGS_ZF, x == y);
    m->flags = SetFlag(m->flags, FLAGS_CF, x < y);
    m->flags = SetFlag(m->flags, FLAGS_PF, false);
  } else {
    m->fpu.sw |= kFpuSwIe;
    m->flags = SetFlag(m->flags, FLAGS_ZF, true);
    m->flags = SetFlag(m->flags, FLAGS_CF, true);
    m->flags = SetFlag(m->flags, FLAGS_PF, true);
  }
}

static void OpFucom(struct Machine *m, u64 rde) {
  FpuCompare(m, StRm(m, rde));
}

static void OpFucomp(struct Machine *m, u64 rde) {
  FpuCompare(m, StRm(m, rde));
  FpuPop(m);
}

static void OpFcomip(struct Machine *m, u64 rde) {
  OpFcomi(m, rde);
  FpuPop(m);
}

static void OpFucomi(struct Machine *m, u64 rde) {
  OpFcomi(m, rde);
}

static void OpFucomip(struct Machine *m, u64 rde) {
  OpFcomip(m, rde);
}

static void OpFfree(struct Machine *m, u64 rde) {
  FpuSetTag(m, ModrmRm(rde), kFpuTagEmpty);
}

static void OpFfreep(struct Machine *m, u64 rde) {
  if (ModrmRm(rde)) OpFfree(m, rde);
  FpuPop(m);
}

static void OpFstswMw(struct Machine *m) {
  FpuSetMemoryShort(m, m->fpu.sw);
}

static void OpFstswAx(struct Machine *m) {
  Write16(m->ax, m->fpu.sw);
}

static void SetFpuEnv(struct Machine *m, u8 p[28]) {
  Write16(p + 0, m->fpu.cw);
  Write16(p + 4, m->fpu.sw);
  Write16(p + 8, m->fpu.tw);
  Write64(p + 12, m->fpu.ip);
  Write16(p + 18, m->fpu.op);
  Write64(p + 20, m->fpu.dp);
}

static void GetFpuEnv(struct Machine *m, u8 p[28]) {
  m->fpu.cw = Read16(p + 0);
  m->fpu.sw = Read16(p + 4);
  m->fpu.tw = Read16(p + 8);
}

static void OpFstenv(struct Machine *m) {
  void *p[2];
  u8 b[28];
  SetFpuEnv(m, BeginStore(m, m->fpu.dp, sizeof(b), p, b));
  EndStore(m, m->fpu.dp, sizeof(b), p, b);
}

static void OpFldenv(struct Machine *m) {
  u8 b[28];
  GetFpuEnv(m, Load(m, m->fpu.dp, sizeof(b), b));
}

static void OpFsave(struct Machine *m) {
  int i;
  void *p[2];
  u8 *a, b[108], t[16];
  a = BeginStore(m, m->fpu.dp, sizeof(b), p, b);
  SetFpuEnv(m, a);
  memset(t, 0, sizeof(t));
  for (i = 0; i < 8; ++i) {
    SerializeLdbl(a + 28 + i * 10, *FpuSt(m, i));
  }
  EndStore(m, m->fpu.dp, sizeof(b), p, b);
  OpFinit(m);
}

static void OpFrstor(struct Machine *m) {
  int i;
  u8 *a, b[108];
  a = Load(m, m->fpu.dp, sizeof(b), b);
  GetFpuEnv(m, a);
  for (i = 0; i < 8; ++i) {
    *FpuSt(m, i) = DeserializeLdbl(a + 28 + i * 10);
  }
}

static void OpFnclex(struct Machine *m) {
  m->fpu.sw &= ~(kFpuSwIe | kFpuSwDe | kFpuSwZe | kFpuSwOe | kFpuSwUe |
                 kFpuSwPe | kFpuSwEs | kFpuSwSf | kFpuSwBf);
}

static void OpFnop(struct Machine *m) {
  /* do nothing */
}

void OpFinit(struct Machine *m) {
  m->fpu.cw = 0x037f;
  m->fpu.sw = 0;
  m->fpu.tw = -1;
}

void OpFwait(P) {
  int sw, cw;
  sw = m->fpu.sw;
  cw = m->fpu.cw;
  if (((sw & kFpuSwIe) && !(cw & kFpuCwIm)) ||
      ((sw & kFpuSwDe) && !(cw & kFpuCwDm)) ||
      ((sw & kFpuSwZe) && !(cw & kFpuCwZm)) ||
      ((sw & kFpuSwOe) && !(cw & kFpuCwOm)) ||
      ((sw & kFpuSwUe) && !(cw & kFpuCwUm)) ||
      ((sw & kFpuSwPe) && !(cw & kFpuCwPm)) ||
      ((sw & kFpuSwSf) && !(cw & kFpuCwIm))) {
    HaltMachine(m, kMachineFpuException);
  }
}

int FpuGetTag(struct Machine *m, unsigned i) {
  unsigned t;
  t = m->fpu.tw;
  i += (m->fpu.sw & kFpuSwSp) >> 11;
  i &= 7;
  i *= 2;
  t &= 3 << i;
  t >>= i;
  return t;
}

void FpuSetTag(struct Machine *m, unsigned i, unsigned t) {
  i += (m->fpu.sw & kFpuSwSp) >> 11;
  t &= 3;
  i &= 7;
  i *= 2;
  m->fpu.tw &= ~(3 << i);
  m->fpu.tw |= t << i;
}

void FpuPush(struct Machine *m, double x) {
  if (FpuGetTag(m, -1) != kFpuTagEmpty) OnFpuStackOverflow(m);
  m->fpu.sw = (m->fpu.sw & ~kFpuSwSp) | ((m->fpu.sw - (1 << 11)) & kFpuSwSp);
  *FpuSt(m, 0) = x;
  FpuSetTag(m, 0, kFpuTagValid);
}

double FpuPop(struct Machine *m) {
  double x;
  if (FpuGetTag(m, 0) != kFpuTagEmpty) {
    x = *FpuSt(m, 0);
    FpuSetTag(m, 0, kFpuTagEmpty);
  } else {
    x = OnFpuStackUnderflow(m);
  }
  m->fpu.sw = (m->fpu.sw & ~kFpuSwSp) | ((m->fpu.sw + (1 << 11)) & kFpuSwSp);
  return x;
}

void OpFpu(P) {
  unsigned op;
  bool ismemory;
  op = Opcode(rde) & 7;
  ismemory = ModrmMod(rde) != 3;
  m->fpu.ip = MaskAddress(m->mode, m->ip - Oplength(rde));
  m->fpu.op = op << 8 | ModrmMod(rde) << 6 | ModrmReg(rde) << 3 | ModrmRm(rde);
  m->fpu.dp = ismemory ? ComputeAddress(A) : 0;
  switch (DISP(op, ismemory, ModrmReg(rde))) {
    CASE(DISP(0xD8, FPUREG, 0), OpFaddStEst(m, rde));
    CASE(DISP(0xD8, FPUREG, 1), OpFmulStEst(m, rde));
    CASE(DISP(0xD8, FPUREG, 2), OpFcom(m, rde));
    CASE(DISP(0xD8, FPUREG, 3), OpFcomp(m, rde));
    CASE(DISP(0xD8, FPUREG, 4), OpFsubStEst(m, rde));
    CASE(DISP(0xD8, FPUREG, 5), OpFsubrStEst(m, rde));
    CASE(DISP(0xD8, FPUREG, 6), OpFdivStEst(m, rde));
    CASE(DISP(0xD8, FPUREG, 7), OpFdivrStEst(m, rde));
    CASE(DISP(0xD8, MEMORY, 0), OpFadds(m, rde));
    CASE(DISP(0xD8, MEMORY, 1), OpFmuls(m, rde));
    CASE(DISP(0xD8, MEMORY, 2), OpFcoms(m));
    CASE(DISP(0xD8, MEMORY, 3), OpFcomps(m));
    CASE(DISP(0xD8, MEMORY, 4), OpFsubs(m));
    CASE(DISP(0xD8, MEMORY, 5), OpFsubrs(m));
    CASE(DISP(0xD8, MEMORY, 6), OpFdivs(m));
    CASE(DISP(0xD8, MEMORY, 7), OpFdivrs(m));
    CASE(DISP(0xD9, FPUREG, 0), OpFld(m, rde));
    CASE(DISP(0xD9, FPUREG, 1), OpFxch(m, rde));
    CASE(DISP(0xD9, FPUREG, 2), OpFnop(m));
    CASE(DISP(0xD9, FPUREG, 3), OpFstp(m, rde));
    CASE(DISP(0xD9, FPUREG, 5), OpFldConstant(m, rde));
    CASE(DISP(0xD9, MEMORY, 0), OpFlds(m));
    CASE(DISP(0xD9, MEMORY, 2), OpFsts(m));
    CASE(DISP(0xD9, MEMORY, 3), OpFstps(m));
    CASE(DISP(0xD9, MEMORY, 4), OpFldenv(m));
    CASE(DISP(0xD9, MEMORY, 5), OpFldcw(m));
    CASE(DISP(0xD9, MEMORY, 6), OpFstenv(m));
    CASE(DISP(0xD9, MEMORY, 7), OpFstcw(m));
    CASE(DISP(0xDA, FPUREG, 0), OpFcmovb(m, rde));
    CASE(DISP(0xDA, FPUREG, 1), OpFcmove(m, rde));
    CASE(DISP(0xDA, FPUREG, 2), OpFcmovbe(m, rde));
    CASE(DISP(0xDA, FPUREG, 3), OpFcmovu(m, rde));
    CASE(DISP(0xDA, MEMORY, 0), OpFiaddl(m));
    CASE(DISP(0xDA, MEMORY, 1), OpFimull(m));
    CASE(DISP(0xDA, MEMORY, 2), OpFicoml(m));
    CASE(DISP(0xDA, MEMORY, 3), OpFicompl(m));
    CASE(DISP(0xDA, MEMORY, 4), OpFisubl(m));
    CASE(DISP(0xDA, MEMORY, 5), OpFisubrl(m));
    CASE(DISP(0xDA, MEMORY, 6), OpFidivl(m));
    CASE(DISP(0xDA, MEMORY, 7), OpFidivrl(m));
    CASE(DISP(0xDB, FPUREG, 0), OpFcmovnb(m, rde));
    CASE(DISP(0xDB, FPUREG, 1), OpFcmovne(m, rde));
    CASE(DISP(0xDB, FPUREG, 2), OpFcmovnbe(m, rde));
    CASE(DISP(0xDB, FPUREG, 3), OpFcmovnu(m, rde));
    CASE(DISP(0xDB, FPUREG, 5), OpFucomi(m, rde));
    CASE(DISP(0xDB, FPUREG, 6), OpFcomi(m, rde));
    CASE(DISP(0xDB, MEMORY, 0), OpFildl(m));
    CASE(DISP(0xDB, MEMORY, 1), OpFisttpl(m));
    CASE(DISP(0xDB, MEMORY, 2), OpFistl(m));
    CASE(DISP(0xDB, MEMORY, 3), OpFistpl(m));
    CASE(DISP(0xDB, MEMORY, 5), OpFldt(m));
    CASE(DISP(0xDB, MEMORY, 7), OpFstpt(m));
    CASE(DISP(0xDC, FPUREG, 0), OpFaddEstSt(m, rde));
    CASE(DISP(0xDC, FPUREG, 1), OpFmulEstSt(m, rde));
    CASE(DISP(0xDC, FPUREG, 2), OpFcom(m, rde));
    CASE(DISP(0xDC, FPUREG, 3), OpFcomp(m, rde));
    CASE(DISP(0xDC, FPUREG, 4), OpFsubEstSt(m, rde));
    CASE(DISP(0xDC, FPUREG, 5), OpFsubrEstSt(m, rde));
    CASE(DISP(0xDC, FPUREG, 6), OpFdivEstSt(m, rde));
    CASE(DISP(0xDC, FPUREG, 7), OpFdivrEstSt(m, rde));
    CASE(DISP(0xDC, MEMORY, 0), OpFaddl(m));
    CASE(DISP(0xDC, MEMORY, 1), OpFmull(m));
    CASE(DISP(0xDC, MEMORY, 2), OpFcoml(m));
    CASE(DISP(0xDC, MEMORY, 3), OpFcompl(m));
    CASE(DISP(0xDC, MEMORY, 4), OpFsubl(m));
    CASE(DISP(0xDC, MEMORY, 5), OpFsubrl(m));
    CASE(DISP(0xDC, MEMORY, 6), OpFdivl(m));
    CASE(DISP(0xDC, MEMORY, 7), OpFdivrl(m));
    CASE(DISP(0xDD, FPUREG, 0), OpFfree(m, rde));
    CASE(DISP(0xDD, FPUREG, 1), OpFxch(m, rde));
    CASE(DISP(0xDD, FPUREG, 2), OpFst(m, rde));
    CASE(DISP(0xDD, FPUREG, 3), OpFstp(m, rde));
    CASE(DISP(0xDD, FPUREG, 4), OpFucom(m, rde));
    CASE(DISP(0xDD, FPUREG, 5), OpFucomp(m, rde));
    CASE(DISP(0xDD, MEMORY, 0), OpFldl(m));
    CASE(DISP(0xDD, MEMORY, 1), OpFisttpll(m));
    CASE(DISP(0xDD, MEMORY, 2), OpFstl(m));
    CASE(DISP(0xDD, MEMORY, 3), OpFstpl(m));
    CASE(DISP(0xDD, MEMORY, 4), OpFrstor(m));
    CASE(DISP(0xDD, MEMORY, 6), OpFsave(m));
    CASE(DISP(0xDD, MEMORY, 7), OpFstswMw(m));
    CASE(DISP(0xDE, FPUREG, 0), OpFaddp(m, rde));
    CASE(DISP(0xDE, FPUREG, 1), OpFmulp(m, rde));
    CASE(DISP(0xDE, FPUREG, 2), OpFcomp(m, rde));
    CASE(DISP(0xDE, FPUREG, 3), OpFcompp(m, rde));
    CASE(DISP(0xDE, FPUREG, 4), OpFsubp(m, rde));
    CASE(DISP(0xDE, FPUREG, 5), OpFsubrp(m, rde));
    CASE(DISP(0xDE, FPUREG, 6), OpFdivp(m, rde));
    CASE(DISP(0xDE, FPUREG, 7), OpFdivrp(m, rde));
    CASE(DISP(0xDE, MEMORY, 0), OpFiadds(m));
    CASE(DISP(0xDE, MEMORY, 1), OpFimuls(m));
    CASE(DISP(0xDE, MEMORY, 2), OpFicoms(m));
    CASE(DISP(0xDE, MEMORY, 3), OpFicomps(m));
    CASE(DISP(0xDE, MEMORY, 4), OpFisubs(m));
    CASE(DISP(0xDE, MEMORY, 5), OpFisubrs(m));
    CASE(DISP(0xDE, MEMORY, 6), OpFidivs(m));
    CASE(DISP(0xDE, MEMORY, 7), OpFidivrs(m));
    CASE(DISP(0xDF, FPUREG, 0), OpFfreep(m, rde));
    CASE(DISP(0xDF, FPUREG, 1), OpFxch(m, rde));
    CASE(DISP(0xDF, FPUREG, 2), OpFstp(m, rde));
    CASE(DISP(0xDF, FPUREG, 3), OpFstp(m, rde));
    CASE(DISP(0xDF, FPUREG, 4), OpFstswAx(m));
    CASE(DISP(0xDF, FPUREG, 5), OpFucomip(m, rde));
    CASE(DISP(0xDF, FPUREG, 6), OpFcomip(m, rde));
    CASE(DISP(0xDF, MEMORY, 0), OpFilds(m));
    CASE(DISP(0xDF, MEMORY, 1), OpFisttps(m));
    CASE(DISP(0xDF, MEMORY, 2), OpFists(m));
    CASE(DISP(0xDF, MEMORY, 3), OpFistps(m));
    CASE(DISP(0xDF, MEMORY, 5), OpFildll(m));
    CASE(DISP(0xDF, MEMORY, 7), OpFistpll(m));
    case DISP(0xD9, FPUREG, 4):
      switch (ModrmRm(rde)) {
        CASE(0, OpFchs(m));
        CASE(1, OpFabs(m));
        CASE(4, OpFtst(m));
        CASE(5, OpFxam(m));
        default:
          OpUdImpl(m);
      }
      break;
    case DISP(0xD9, FPUREG, 6):
      switch (ModrmRm(rde)) {
        CASE(0, OpF2xm1(m));
        CASE(1, OpFyl2x(m));
        CASE(2, OpFptan(m));
        CASE(3, OpFpatan(m));
        CASE(4, OpFxtract(m));
        CASE(5, OpFprem1(m));
        CASE(6, OpFdecstp(m));
        CASE(7, OpFincstp(m));
        default:
          __builtin_unreachable();
      }
      break;
    case DISP(0xD9, FPUREG, 7):
      switch (ModrmRm(rde)) {
        CASE(0, OpFprem(m));
        CASE(1, OpFyl2xp1(m));
        CASE(2, OpFsqrt(m));
        CASE(3, OpFsincos(m));
        CASE(4, OpFrndint(m));
        CASE(5, OpFscale(m));
        CASE(6, OpFsin(m));
        CASE(7, OpFcos(m));
        default:
          __builtin_unreachable();
      }
      break;
    case DISP(0xDb, FPUREG, 4):
      switch (ModrmRm(rde)) {
        CASE(2, OpFnclex(m));
        CASE(3, OpFinit(m));
        default:
          OpUdImpl(m);
      }
      break;
    default:
      OpUdImpl(m);
  }
}
