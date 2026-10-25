#include <fenv.h>
#include <string.h>

#include "blink/endian.h"
#include "blink/fpu.h"
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/pun.h"
#include "blink/tsan.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("rounding-math", "fp-contract=off")
#else
#pragma STDC FENV_ACCESS ON
#pragma STDC FP_CONTRACT OFF
#endif

struct DotState {
  unsigned mxcsr;
  unsigned exceptions;
  unsigned width;
};

static u64 DotOperand(struct DotState *s, u64 bits) {
  unsigned fraction = s->width == 8 ? 52 : 23;
  u64 exponent = s->width == 8 ? 0x7ff : 0xff;
  u64 mantissa = bits & (((u64)1 << fraction) - 1);
  u64 e = (bits >> fraction) & exponent;
  if (!e && mantissa) {
    if (s->mxcsr & kMxcsrDaz) return bits & ((u64)1 << (s->width * 8 - 1));
    s->exceptions |= kMxcsrDe;
  }
  if (e == exponent && mantissa && !(mantissa & ((u64)1 << (fraction - 1)))) {
    s->exceptions |= kMxcsrIe;
    bits |= (u64)1 << (fraction - 1);
  }
  return bits;
}

static bool DotTraps(struct DotState *s) {
  return s->exceptions & ~(s->mxcsr >> 7) & 0x3f;
}

static u64 DotArithmetic(struct DotState *s, u64 a, u64 b, bool multiply) {
  u64 result, exponent_mask, fraction_mask, sign, quiet;
  bool a_nan, b_nan;
  int flags;
  exponent_mask = s->width == 8 ? UINT64_C(0x7ff0000000000000) : 0x7f800000;
  fraction_mask = s->width == 8 ? UINT64_C(0x000fffffffffffff) : 0x007fffff;
  sign = s->width == 8 ? UINT64_C(0x8000000000000000) : 0x80000000;
  quiet = (fraction_mask + 1) >> 1;
  a_nan = (a & exponent_mask) == exponent_mask && (a & fraction_mask);
  b_nan = (b & exponent_mask) == exponent_mask && (b & fraction_mask);
  // SSE arithmetic prefers the first NaN operand; host NaN rules differ.
  // A NaN operand also takes precedence over denormal operand handling.
  if (a_nan || b_nan) {
    if ((a_nan && !(a & quiet)) || (b_nan && !(b & quiet))) {
      s->exceptions |= kMxcsrIe;
    }
    return (a_nan ? a : b) | quiet;
  }
  a = DotOperand(s, a);
  b = DotOperand(s, b);
  if (DotTraps(s)) return 0;
  feclearexcept(FE_ALL_EXCEPT);
  if (s->width == 8) {
    union DoublePun x, y, z;
    volatile double rounded;
    x.i = a;
    y.i = b;
    rounded = multiply ? x.f * y.f : x.f + y.f;
    z.f = rounded;
    result = z.i;
  } else {
    union FloatPun x, y, z;
    volatile float rounded;
    x.i = a;
    y.i = b;
    rounded = multiply ? x.f * y.f : x.f + y.f;
    z.f = rounded;
    result = z.i;
  }
  flags = fetestexcept(FE_ALL_EXCEPT);
  if (flags & FE_INVALID) {
    s->exceptions |= kMxcsrIe;
    result = sign | exponent_mask | quiet;
  }
  if (flags & FE_OVERFLOW) s->exceptions |= kMxcsrOe;
  if (flags & FE_UNDERFLOW) s->exceptions |= kMxcsrUe;
  if (flags & FE_INEXACT) s->exceptions |= kMxcsrPe;
  if ((s->mxcsr & (kMxcsrFtz | kMxcsrUm)) == (kMxcsrFtz | kMxcsrUm) &&
      !(result & exponent_mask) && (result & fraction_mask)) {
    result &= sign;
    s->exceptions |= kMxcsrUe | kMxcsrPe;
  }
  return result;
}

void OpDot(P) {
  static const int modes[4] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
  struct DotState state = {m->mxcsr, 0, (Opcode(rde) & 1) ? 8 : 4};
  unsigned i, count = 16 / state.width;
  u8 left[16], right[16], result[16];
  u64 products[4] = {0}, sum, a, b;
  fenv_t saved;
  if (!Osz(rde) || Rep(rde)) return OpUd(A);
  IGNORE_RACES_START();
  memcpy(left, XmmRexrReg(m, rde), 16);
  memcpy(right, GetModrmRegisterXmmPointerRead16(A), 16);
  IGNORE_RACES_END();
  fegetenv(&saved);
  fesetenv(FE_DFL_ENV);
  fesetround(modes[(m->mxcsr >> 13) & 3]);
  for (i = 0; i < count; ++i) {
    if (!(uimm0 & (16 << i))) continue;
    a = state.width == 8 ? Get64(left + i * 8) : Get32(left + i * 4);
    b = state.width == 8 ? Get64(right + i * 8) : Get32(right + i * 4);
    products[i] = DotArithmetic(&state, a, b, true);
    if (DotTraps(&state)) goto done;
  }
  sum = DotArithmetic(&state, products[0], products[1], false);
  if (DotTraps(&state)) goto done;
  if (count == 4) {
    b = DotArithmetic(&state, products[2], products[3], false);
    if (DotTraps(&state)) goto done;
    sum = DotArithmetic(&state, sum, b, false);
    if (DotTraps(&state)) goto done;
  }
  for (i = 0; i < count; ++i) {
    a = uimm0 & (1 << i) ? sum : 0;
    if (state.width == 8) Put64(result + i * 8, a);
    else Put32(result + i * 4, a);
  }
done:
  fesetenv(&saved);
  m->mxcsr |= state.exceptions;
  if (DotTraps(&state)) HaltMachine(m, kMachineSimdException);
  memcpy(XmmRexrReg(m, rde), result, 16);
}
