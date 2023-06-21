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
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>

#include "blink/alu.h"
#include "blink/assert.h"
#include "blink/builtin.h"
#include "blink/bus.h"
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/intrin.h"
#include "blink/jit.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/modrm.h"
#include "blink/pun.h"
#include "blink/rde.h"
#include "blink/stats.h"
#include "blink/swap.h"
#include "blink/thread.h"
#include "blink/types.h"
#include "blink/x86.h"
#include "blink/xmm.h"

/**
 * @fileoverview X86 Micro-Operations w/ Printf RPN Glue-Generating DSL.
 */

#define kMaxOps 256

////////////////////////////////////////////////////////////////////////////////
// ESSENTIAL

MICRO_OP static void Sax64(P) {
  Put64(m->ax, (i32)Get32(m->ax));
}
MICRO_OP static void Sax32(P) {
  Put64(m->ax, (u32)(i16)Get16(m->ax));
}
MICRO_OP static void Sax16(P) {
  Put16(m->ax, (i8)Get8(m->ax));
}
const nexgen32e_f kSax[] = {Sax16, Sax32, Sax64};

MICRO_OP i64 Not8(struct Machine *m, u64 x, u64 y) {
  return ~x & 0xFF;
}
MICRO_OP i64 Not16(struct Machine *m, u64 x, u64 y) {
  return ~x & 0xFFFF;
}
MICRO_OP i64 Not32(struct Machine *m, u64 x, u64 y) {
  return ~x & 0xFFFFFFFF;
}
MICRO_OP i64 Not64(struct Machine *m, u64 x, u64 y) {
  return ~x & 0xFFFFFFFFFFFFFFFF;
}

MICRO_OP static void Convert64(P) {
  Put64(m->dx, Get64(m->ax) & 0x8000000000000000 ? 0xffffffffffffffff : 0);
}
MICRO_OP static void Convert32(P) {
  Put64(m->dx, Get32(m->ax) & 0x80000000 ? 0xffffffff : 0);
}
MICRO_OP static void Convert16(P) {
  Put16(m->dx, Get16(m->ax) & 0x8000 ? 0xffff : 0);
}
const nexgen32e_f kConvert[] = {Convert16, Convert32, Convert64};

#ifndef DISABLE_BMI2

#if X86_INTRINSICS
MICRO_OP i64 Adcx32(u64 x, u64 y, struct Machine *m) {
  u32 z;
  _Static_assert(CF == 1, "");
  asm("btr\t$0,%1\n\t"
      "adc\t%3,%0\n\t"
      "adc\t$0,%1"
      : "=&r" (z), "+&g" (m->flags) : "%0" ((u32)x), "g" ((u32)y) : "cc");
  return z;
}
MICRO_OP i64 Adcx64(u64 x, u64 y, struct Machine *m) {
  u64 z;
  _Static_assert(CF == 1, "");
  asm("btr\t$0,%1\n\t"
      "adc\t%3,%0\n\t"
      "adc\t$0,%1"
      : "=&r" (z), "+&g" (m->flags) : "%0" (x), "g" (y) : "cc");
  return z;
}
MICRO_OP i64 Adox32(u64 x, u64 y, struct Machine *m) {
  u32 z, t;
  asm("btr\t%5,%1\n\t"
      "adc\t%4,%0\n\t"
      "sbb\t%2,%2\n\t"
      "and\t%6,%2\n\t"
      "or\t%2,%1"
      : "=&r" (z), "+&g" (m->flags), "=&r" (t)
      : "%0" ((u32)x), "g" ((u32)y), "i" (FLAGS_OF), "i" (OF)
      : "cc");
  return z;
}
MICRO_OP i64 Adox64(u64 x, u64 y, struct Machine *m) {
  u64 z;
  u32 t;
  asm("btr\t%5,%1\n\t"
      "adc\t%4,%0\n\t"
      "sbb\t%2,%2\n\t"
      "and\t%6,%2\n\t"
      "or\t%2,%1"
      : "=&r" (z), "+&g" (m->flags), "=&r" (t)
      : "%0" (x), "g" (y), "i" (FLAGS_OF), "i" (OF)
      : "cc");
  return z;
}
#elif ARM_INTRINSICS /* !X86_INTRINSICS */
MICRO_OP i64 Adcx32(u64 x, u64 y, struct Machine *m) {
  u64 f = m->flags, z;
  _Static_assert(CF == 1, "");
  asm("ror\t%1,%1,#1\n\t"
      "adds\t%1,%1,%1\n\t"
      "adcs\t%w0,%w2,%w3\n\t"
      "adc\t%1,%1,xzr"
      : "=&r" (z), "+&r" (f) : "%0" (x), "r" (y) : "cc");
  m->flags = f;
  return z;
}
MICRO_OP i64 Adcx64(u64 x, u64 y, struct Machine *m) {
  u64 f = m->flags, z;
  _Static_assert(CF == 1, "");
  asm("ror\t%1,%1,#1\n\t"
      "adds\t%1,%1,%1\n\t"
      "adcs\t%0,%2,%3\n\t"
      "adc\t%1,%1,xzr"
      : "=&r" (z), "+&r" (f) : "%0" (x), "r" (y) : "cc");
  m->flags = f;
  return z;
}
MICRO_OP i64 Adox32(u64 x, u64 y, struct Machine *m) {
  u64 f = m->flags, z;
  asm("ror\t%1,%1,%4\n\t"
      "adds\t%1,%1,%1\n\t"
      "adcs\t%w0,%w2,%w3\n\t"
      "adc\t%1,%1,xzr\n\t"
      "ror\t%1,%1,%5"
      : "=&r" (z), "+&r" (f)
      : "%0" (x), "r" (y), "i" (FLAGS_OF + 1), "i" (64 - FLAGS_OF)
      : "cc");
  m->flags = f;
  return z;
}
MICRO_OP i64 Adox64(u64 x, u64 y, struct Machine *m) {
  u64 f = m->flags, z;
  asm("ror\t%1,%1,%4\n\t"
      "adds\t%1,%1,%1\n\t"
      "adcs\t%0,%2,%3\n\t"
      "adc\t%1,%1,xzr\n\t"
      "ror\t%1,%1,%5"
      : "=&r" (z), "+&r" (f)
      : "%0" (x), "r" (y), "i" (FLAGS_OF + 1), "i" (64 - FLAGS_OF)
      : "cc");
  m->flags = f;
  return z;
}
#else /* !ARM_INTRINSICS */
MICRO_OP i64 Adcx32(u64 x, u64 y, struct Machine *m) {
  u32 t = x + !!(m->flags & CF);
  u32 z = t + y;
  int c = (t < x) | (z < y);
  m->flags = (m->flags & ~CF) | c << FLAGS_CF;
  return z;
}
MICRO_OP i64 Adcx64(u64 x, u64 y, struct Machine *m) {
  u64 t = x + !!(m->flags & CF);
  u64 z = t + y;
  int c = (t < x) | (z < y);
  m->flags = (m->flags & ~CF) | c << FLAGS_CF;
  return z;
}

MICRO_OP i64 Adox32(u64 x, u64 y, struct Machine *m) {
  u32 t = x + !!(m->flags & OF);
  u32 z = t + y;
  int c = (t < x) | (z < y);
  m->flags = (m->flags & ~OF) | c << FLAGS_OF;
  return z;
}
MICRO_OP i64 Adox64(u64 x, u64 y, struct Machine *m) {
  u64 t = x + !!(m->flags & OF);
  u64 z = t + y;
  int c = (t < x) | (z < y);
  m->flags = (m->flags & ~OF) | c << FLAGS_OF;
  return z;
}
#endif /* !ARM_INTRINSICS */

#endif /* !DISABLE_BMI2 */

////////////////////////////////////////////////////////////////////////////////
// BRANCHING

MICRO_OP void FastJmp(struct Machine *m, u64 disp) {
  m->ip += disp;
}
MICRO_OP void FastJmpAbs(u64 addr, struct Machine *m) {
  m->ip = addr;
}

MICRO_OP static u32 Jb(struct Machine *m) {
  return !!(m->flags & CF);
}
MICRO_OP static u32 Jae(struct Machine *m) {
  return !!(~m->flags & CF);
}
MICRO_OP static u32 Je(struct Machine *m) {
  return !!(m->flags & ZF);
}
MICRO_OP static u32 Jne(struct Machine *m) {
  return !!(~m->flags & ZF);
}
MICRO_OP static u32 Js(struct Machine *m) {
  return !!(m->flags & SF);
}
MICRO_OP static u32 Jns(struct Machine *m) {
  return !!(~m->flags & SF);
}
MICRO_OP static u32 Jo(struct Machine *m) {
  return !!(m->flags & OF);
}
MICRO_OP static u32 Jno(struct Machine *m) {
  return !!(~m->flags & OF);
}
MICRO_OP static u32 Ja(struct Machine *m) {
  return IsAbove(m);
}
MICRO_OP static u32 Jbe(struct Machine *m) {
  return IsBelowOrEqual(m);
}
MICRO_OP static u32 Jg(struct Machine *m) {
  return IsGreater(m);
}
MICRO_OP static u32 Jge(struct Machine *m) {
  return IsGreaterOrEqual(m);
}
MICRO_OP static u32 Jl(struct Machine *m) {
  return IsLess(m);
}
MICRO_OP static u32 Jle(struct Machine *m) {
  return IsLessOrEqual(m);
}

const cc_f kConditionCode[16] = {
    Jo,   //
    Jno,  //
    Jb,   //
    Jae,  //
    Je,   //
    Jne,  //
    Jbe,  //
    Ja,   //
    Js,   //
    Jns,  //
    0,    //
    0,    //
    Jl,   //
    Jge,  //
    Jle,  //
    Jg,   //
};

MICRO_OP u64 Pick(u32 p, u64 x, u64 y) {
  return p ? x : y;
}

////////////////////////////////////////////////////////////////////////////////
// FLOATING POINT

MICRO_OP void OpPsdMuls1(u8 *p, struct Machine *m, long reg) {
  union FloatPun x, y;
  y.i = Read32(p);
  x.i = Read32(m->xmm[reg]);
  x.f = x.f * y.f;
  Write32(m->xmm[reg], x.i);
}

MICRO_OP void OpPsdMuld1(u8 *p, struct Machine *m, long reg) {
  union DoublePun x, y;
  y.i = Read64(p);
  x.i = Read64(m->xmm[reg]);
  x.f = x.f * y.f;
  Write64(m->xmm[reg], x.i);
}

MICRO_OP void OpPsdAdds1(u8 *p, struct Machine *m, long reg) {
  union FloatPun x, y;
  y.i = Read32(p);
  x.i = Read32(m->xmm[reg]);
  x.f = x.f + y.f;
  Write32(m->xmm[reg], x.i);
}

MICRO_OP void OpPsdAddd1(u8 *p, struct Machine *m, long reg) {
  union DoublePun x, y;
  y.i = Read64(p);
  x.i = Read64(m->xmm[reg]);
  x.f = x.f + y.f;
  Write64(m->xmm[reg], x.i);
}

MICRO_OP void OpPsdSubs1(u8 *p, struct Machine *m, long reg) {
  union FloatPun x, y;
  y.i = Read32(p);
  x.i = Read32(m->xmm[reg]);
  x.f = x.f - y.f;
  Write32(m->xmm[reg], x.i);
}

MICRO_OP void OpPsdSubd1(u8 *p, struct Machine *m, long reg) {
  union DoublePun x, y;
  y.i = Read64(p);
  x.i = Read64(m->xmm[reg]);
  x.f = x.f - y.f;
  Write64(m->xmm[reg], x.i);
}

MICRO_OP void OpPsdDivs1(u8 *p, struct Machine *m, long reg) {
  union FloatPun x, y;
  y.i = Read32(p);
  x.i = Read32(m->xmm[reg]);
  x.f = x.f / y.f;
  Write32(m->xmm[reg], x.i);
}

MICRO_OP void OpPsdDivd1(u8 *p, struct Machine *m, long reg) {
  union DoublePun x, y;
  y.i = Read64(p);
  x.i = Read64(m->xmm[reg]);
  x.f = x.f / y.f;
  Write64(m->xmm[reg], x.i);
}

MICRO_OP void OpPsdMins1(u8 *p, struct Machine *m, long reg) {
  union FloatPun x, y;
  y.i = Read32(p);
  x.i = Read32(m->xmm[reg]);
  x.f = MIN(x.f, y.f);
  Write32(m->xmm[reg], x.i);
}

MICRO_OP void OpPsdMind1(u8 *p, struct Machine *m, long reg) {
  union DoublePun x, y;
  y.i = Read64(p);
  x.i = Read64(m->xmm[reg]);
  x.f = MIN(x.f, y.f);
  Write64(m->xmm[reg], x.i);
}

MICRO_OP void OpPsdMaxs1(u8 *p, struct Machine *m, long reg) {
  union FloatPun x, y;
  y.i = Read32(p);
  x.i = Read32(m->xmm[reg]);
  x.f = MAX(x.f, y.f);
  Write32(m->xmm[reg], x.i);
}

MICRO_OP void OpPsdMaxd1(u8 *p, struct Machine *m, long reg) {
  union DoublePun x, y;
  y.i = Read64(p);
  x.i = Read64(m->xmm[reg]);
  x.f = MAX(x.f, y.f);
  Write64(m->xmm[reg], x.i);
}

MICRO_OP void Int64ToDouble(i64 x, struct Machine *m, long reg) {
  union DoublePun d;
  d.f = x;
  Put64(m->xmm[reg], d.i);
}

MICRO_OP void Int32ToDouble(i32 x, struct Machine *m, long reg) {
  union DoublePun d;
  d.f = x;
  Put64(m->xmm[reg], d.i);
}

MICRO_OP void Int64ToFloat(i64 x, struct Machine *m, long reg) {
  union FloatPun d;
  d.f = x;
  Put32(m->xmm[reg], d.i);
}

MICRO_OP void Int32ToFloat(i32 x, struct Machine *m, long reg) {
  union FloatPun d;
  d.f = x;
  Put32(m->xmm[reg], d.i);
}

#ifdef HAVE_JIT

////////////////////////////////////////////////////////////////////////////////
// ACCOUNTING

MICRO_OP void CountOp(long *instructions_jitted_ptr) {
  STATISTIC(++*instructions_jitted_ptr);
}

////////////////////////////////////////////////////////////////////////////////
// PROGRAM COUNTER

MICRO_OP void AddIp(struct Machine *m, long oplen) {
  m->oplen = oplen;
  m->ip += oplen;
}

MICRO_OP void SkewIp(struct Machine *m, long oplen, long delta) {
  m->oplen = oplen;
  m->ip += delta;
}

MICRO_OP void AdvanceIp(struct Machine *m, long oplen) {
  m->ip += oplen;
}

////////////////////////////////////////////////////////////////////////////////
// READING FROM REGISTER FILE

MICRO_OP static u64 GetCl(struct Machine *m) {
  return m->cl;
}
MICRO_OP static u64 GetReg8(struct Machine *m, long b) {
  return Get8(m->beg + b);
}
MICRO_OP static u64 GetReg16(struct Machine *m, long i) {
  return Get16(m->weg[i]);
}
MICRO_OP static u64 GetReg32(struct Machine *m, long i) {
  return Get32(m->weg[i]);
}
MICRO_OP static u64 GetReg64(struct Machine *m, long i) {
  return Get64(m->weg[i]);
}
MICRO_OP static XMM_TYPE GetReg128(struct Machine *m, long i) {
  RETURN_XMM(Get64(m->xmm[i]), Get64(m->xmm[i] + 8));
}
typedef u64 (*getreg_f)(struct Machine *, long);
static const getreg_f kGetReg[] = {GetReg8, GetReg16,   //
                                   GetReg32, GetReg64,  //
                                   (getreg_f)GetReg128};

MICRO_OP static u64 GetReg32_0(struct Machine *m) {
  return Get32(m->weg[0]);
}
MICRO_OP static u64 GetReg32_1(struct Machine *m) {
  return Get32(m->weg[1]);
}
MICRO_OP static u64 GetReg32_2(struct Machine *m) {
  return Get32(m->weg[2]);
}
MICRO_OP static u64 GetReg32_3(struct Machine *m) {
  return Get32(m->weg[3]);
}
MICRO_OP static u64 GetReg32_4(struct Machine *m) {
  return Get32(m->weg[4]);
}
MICRO_OP static u64 GetReg32_5(struct Machine *m) {
  return Get32(m->weg[5]);
}
MICRO_OP static u64 GetReg32_6(struct Machine *m) {
  return Get32(m->weg[6]);
}
MICRO_OP static u64 GetReg32_7(struct Machine *m) {
  return Get32(m->weg[7]);
}
MICRO_OP static u64 GetReg32_8(struct Machine *m) {
  return Get32(m->weg[8]);
}
MICRO_OP static u64 GetReg32_9(struct Machine *m) {
  return Get32(m->weg[9]);
}
MICRO_OP static u64 GetReg32_10(struct Machine *m) {
  return Get32(m->weg[10]);
}
MICRO_OP static u64 GetReg32_11(struct Machine *m) {
  return Get32(m->weg[11]);
}
MICRO_OP static u64 GetReg32_12(struct Machine *m) {
  return Get32(m->weg[12]);
}
MICRO_OP static u64 GetReg32_13(struct Machine *m) {
  return Get32(m->weg[13]);
}
MICRO_OP static u64 GetReg32_14(struct Machine *m) {
  return Get32(m->weg[14]);
}
MICRO_OP static u64 GetReg32_15(struct Machine *m) {
  return Get32(m->weg[15]);
}
typedef u64 (*getreg32_f)(struct Machine *);
const getreg32_f kGetReg32[] = {
    GetReg32_0,   //
    GetReg32_1,   //
    GetReg32_2,   //
    GetReg32_3,   //
    GetReg32_4,   //
    GetReg32_5,   //
    GetReg32_6,   //
    GetReg32_7,   //
    GetReg32_8,   //
    GetReg32_9,   //
    GetReg32_10,  //
    GetReg32_11,  //
    GetReg32_12,  //
    GetReg32_13,  //
    GetReg32_14,  //
    GetReg32_15,  //
};

MICRO_OP static u64 GetReg64_0(struct Machine *m) {
  return Get64(m->weg[0]);
}
MICRO_OP static u64 GetReg64_1(struct Machine *m) {
  return Get64(m->weg[1]);
}
MICRO_OP static u64 GetReg64_2(struct Machine *m) {
  return Get64(m->weg[2]);
}
MICRO_OP static u64 GetReg64_3(struct Machine *m) {
  return Get64(m->weg[3]);
}
MICRO_OP static u64 GetReg64_4(struct Machine *m) {
  return Get64(m->weg[4]);
}
MICRO_OP static u64 GetReg64_5(struct Machine *m) {
  return Get64(m->weg[5]);
}
MICRO_OP static u64 GetReg64_6(struct Machine *m) {
  return Get64(m->weg[6]);
}
MICRO_OP static u64 GetReg64_7(struct Machine *m) {
  return Get64(m->weg[7]);
}
MICRO_OP static u64 GetReg64_8(struct Machine *m) {
  return Get64(m->weg[8]);
}
MICRO_OP static u64 GetReg64_9(struct Machine *m) {
  return Get64(m->weg[9]);
}
MICRO_OP static u64 GetReg64_10(struct Machine *m) {
  return Get64(m->weg[10]);
}
MICRO_OP static u64 GetReg64_11(struct Machine *m) {
  return Get64(m->weg[11]);
}
MICRO_OP static u64 GetReg64_12(struct Machine *m) {
  return Get64(m->weg[12]);
}
MICRO_OP static u64 GetReg64_13(struct Machine *m) {
  return Get64(m->weg[13]);
}
MICRO_OP static u64 GetReg64_14(struct Machine *m) {
  return Get64(m->weg[14]);
}
MICRO_OP static u64 GetReg64_15(struct Machine *m) {
  return Get64(m->weg[15]);
}
typedef u64 (*getreg64_f)(struct Machine *);
const getreg64_f kGetReg64[] = {
    GetReg64_0,   //
    GetReg64_1,   //
    GetReg64_2,   //
    GetReg64_3,   //
    GetReg64_4,   //
    GetReg64_5,   //
    GetReg64_6,   //
    GetReg64_7,   //
    GetReg64_8,   //
    GetReg64_9,   //
    GetReg64_10,  //
    GetReg64_11,  //
    GetReg64_12,  //
    GetReg64_13,  //
    GetReg64_14,  //
    GetReg64_15,  //
};

////////////////////////////////////////////////////////////////////////////////
// WRITING TO REGISTER FILE

MICRO_OP void ZeroRegFlags(struct Machine *m, long i) {
  Put64(m->weg[i], 0);
  m->flags &= ~(CF | ZF | SF | OF | AF | 0xFF000000u);
  m->flags |= 1 << FLAGS_ZF;
}

MICRO_OP static void PutReg8(struct Machine *m, long i, u64 x) {
  Put8(m->beg + i, x);
}
MICRO_OP static void PutReg16(struct Machine *m, long i, u64 x) {
  Put16(m->weg[i], x);
}
MICRO_OP static void PutReg32(struct Machine *m, long i, u64 x) {
  Put64(m->weg[i], x & 0xffffffff);
}
MICRO_OP static void PutReg64(struct Machine *m, long i, u64 x) {
  Put64(m->weg[i], x);
}
MICRO_OP static void PutReg128(u64 x, u64 y, struct Machine *m, long i) {
  Put64(m->xmm[i], x);
  Put64(m->xmm[i] + 8, y);
}
typedef void (*putreg_f)(struct Machine *, long, u64);
static const putreg_f kPutReg[] = {PutReg8, PutReg16,   //
                                   PutReg32, PutReg64,  //
                                   (putreg_f)PutReg128};

MICRO_OP static void PutReg32_0(u64 x, struct Machine *m) {
  Put64(m->weg[0], (u32)x);
}
MICRO_OP static void PutReg32_1(u64 x, struct Machine *m) {
  Put64(m->weg[1], (u32)x);
}
MICRO_OP static void PutReg32_2(u64 x, struct Machine *m) {
  Put64(m->weg[2], (u32)x);
}
MICRO_OP static void PutReg32_3(u64 x, struct Machine *m) {
  Put64(m->weg[3], (u32)x);
}
MICRO_OP static void PutReg32_4(u64 x, struct Machine *m) {
  Put64(m->weg[4], (u32)x);
}
MICRO_OP static void PutReg32_5(u64 x, struct Machine *m) {
  Put64(m->weg[5], (u32)x);
}
MICRO_OP static void PutReg32_6(u64 x, struct Machine *m) {
  Put64(m->weg[6], (u32)x);
}
MICRO_OP static void PutReg32_7(u64 x, struct Machine *m) {
  Put64(m->weg[7], (u32)x);
}
MICRO_OP static void PutReg32_8(u64 x, struct Machine *m) {
  Put64(m->weg[8], (u32)x);
}
MICRO_OP static void PutReg32_9(u64 x, struct Machine *m) {
  Put64(m->weg[9], (u32)x);
}
MICRO_OP static void PutReg32_10(u64 x, struct Machine *m) {
  Put64(m->weg[10], (u32)x);
}
MICRO_OP static void PutReg32_11(u64 x, struct Machine *m) {
  Put64(m->weg[11], (u32)x);
}
MICRO_OP static void PutReg32_12(u64 x, struct Machine *m) {
  Put64(m->weg[12], (u32)x);
}
MICRO_OP static void PutReg32_13(u64 x, struct Machine *m) {
  Put64(m->weg[13], (u32)x);
}
MICRO_OP static void PutReg32_14(u64 x, struct Machine *m) {
  Put64(m->weg[14], (u32)x);
}
MICRO_OP static void PutReg32_15(u64 x, struct Machine *m) {
  Put64(m->weg[15], (u32)x);
}
typedef void (*putreg32_f)(u64, struct Machine *);
const putreg32_f kPutReg32[] = {
    PutReg32_0,   //
    PutReg32_1,   //
    PutReg32_2,   //
    PutReg32_3,   //
    PutReg32_4,   //
    PutReg32_5,   //
    PutReg32_6,   //
    PutReg32_7,   //
    PutReg32_8,   //
    PutReg32_9,   //
    PutReg32_10,  //
    PutReg32_11,  //
    PutReg32_12,  //
    PutReg32_13,  //
    PutReg32_14,  //
    PutReg32_15,  //
};

MICRO_OP static void PutReg64_0(u64 x, struct Machine *m) {
  Put64(m->weg[0], x);
}
MICRO_OP static void PutReg64_1(u64 x, struct Machine *m) {
  Put64(m->weg[1], x);
}
MICRO_OP static void PutReg64_2(u64 x, struct Machine *m) {
  Put64(m->weg[2], x);
}
MICRO_OP static void PutReg64_3(u64 x, struct Machine *m) {
  Put64(m->weg[3], x);
}
MICRO_OP static void PutReg64_4(u64 x, struct Machine *m) {
  Put64(m->weg[4], x);
}
MICRO_OP static void PutReg64_5(u64 x, struct Machine *m) {
  Put64(m->weg[5], x);
}
MICRO_OP static void PutReg64_6(u64 x, struct Machine *m) {
  Put64(m->weg[6], x);
}
MICRO_OP static void PutReg64_7(u64 x, struct Machine *m) {
  Put64(m->weg[7], x);
}
MICRO_OP static void PutReg64_8(u64 x, struct Machine *m) {
  Put64(m->weg[8], x);
}
MICRO_OP static void PutReg64_9(u64 x, struct Machine *m) {
  Put64(m->weg[9], x);
}
MICRO_OP static void PutReg64_10(u64 x, struct Machine *m) {
  Put64(m->weg[10], x);
}
MICRO_OP static void PutReg64_11(u64 x, struct Machine *m) {
  Put64(m->weg[11], x);
}
MICRO_OP static void PutReg64_12(u64 x, struct Machine *m) {
  Put64(m->weg[12], x);
}
MICRO_OP static void PutReg64_13(u64 x, struct Machine *m) {
  Put64(m->weg[13], x);
}
MICRO_OP static void PutReg64_14(u64 x, struct Machine *m) {
  Put64(m->weg[14], x);
}
MICRO_OP static void PutReg64_15(u64 x, struct Machine *m) {
  Put64(m->weg[15], x);
}
const putreg64_f kPutReg64[] = {
    PutReg64_0,   //
    PutReg64_1,   //
    PutReg64_2,   //
    PutReg64_3,   //
    PutReg64_4,   //
    PutReg64_5,   //
    PutReg64_6,   //
    PutReg64_7,   //
    PutReg64_8,   //
    PutReg64_9,   //
    PutReg64_10,  //
    PutReg64_11,  //
    PutReg64_12,  //
    PutReg64_13,  //
    PutReg64_14,  //
    PutReg64_15,  //
};

////////////////////////////////////////////////////////////////////////////////
// MEMORY OPERATIONS

typedef i64 (*load_f)(const u8 *);
typedef void (*store_f)(u8 *, u64);

MICRO_OP static u8 *ResolveHost(i64 v) {
  return ToHost(v);
}

MICRO_OP static u8 *GetBegPtr(struct Machine *m, long i) {
  return m->beg + i;
}

MICRO_OP static u8 *GetWegPtr(struct Machine *m, long i) {
  return m->weg[i];
}

MICRO_OP static u8 *GetXmmPtr(struct Machine *m, long i) {
  return m->xmm[i];
}

MICRO_OP void MovsdWpsVpsOp(u8 *p, struct Machine *m, long reg) {
  Write64(p, Read64(m->xmm[reg]));
}

#if (defined(__x86_64__) || defined(__aarch64__)) && \
    defined(TRIVIALLY_RELOCATABLE)
#define LOADSTORE "m"

MICRO_OP static i64 NativeLoad8(const u8 *p) {
  return *p;
}
MICRO_OP static i64 NativeLoad16(const u8 *p) {
  return *(const u16 *)p;
}
MICRO_OP static i64 NativeLoad32(const u8 *p) {
  return *(const u32 *)p;
}
MICRO_OP static i64 NativeLoad64(const u8 *p) {
  return *(const u64 *)p;
}
MICRO_OP static XMM_TYPE NativeLoad128(u8 *p) {
  RETURN_XMM(((const u64 *)p)[0], ((const u64 *)p)[1]);
}

MICRO_OP static void NativeStore8(u8 *p, u64 x) {
  *p = x;
}
MICRO_OP static void NativeStore16(u8 *p, u64 x) {
  *(u16 *)p = x;
}
MICRO_OP static void NativeStore32(u8 *p, u64 x) {
  *(u32 *)p = x;
}
MICRO_OP static void NativeStore64(u8 *p, u64 x) {
  *(u64 *)p = x;
}
MICRO_OP static void NativeStore128(u8 *p, u64 x, u64 y) {
  ((u64 *)p)[0] = x;
  ((u64 *)p)[1] = y;
}

static const load_f kLoad[] = {
    (load_f)NativeLoad8,    //
    (load_f)NativeLoad16,   //
    (load_f)NativeLoad32,   //
    (load_f)NativeLoad64,   //
    (load_f)NativeLoad128,  //
};

static const store_f kStore[] = {
    (store_f)NativeStore8,    //
    (store_f)NativeStore16,   //
    (store_f)NativeStore32,   //
    (store_f)NativeStore64,   //
    (store_f)NativeStore128,  //
};

#else
#define LOADSTORE "c"

MICRO_OP static XMM_TYPE Load128(u8 *p) {
  RETURN_XMM(Read64(p), Read64(p + 8));
}

MICRO_OP static void Store128(u8 *p, u64 x, u64 y) {
  Write64(p, x);
  Write64(p + 8, y);
}

static const load_f kLoad[] = {Load8, Load16,   //
                               Load32, Load64,  //
                               (load_f)Load128};
static const store_f kStore[] = {Store8, Store16,   //
                                 Store32, Store64,  //
                                 (store_f)Store128};

#endif /* __GNUC__ */

////////////////////////////////////////////////////////////////////////////////
// ARITHMETIC

MICRO_OP i64 JustAdd(struct Machine *m, u64 x, u64 y) {
  return x + y;
}
MICRO_OP i64 JustOr(struct Machine *m, u64 x, u64 y) {
  return x | y;
}
MICRO_OP i64 JustAdc(struct Machine *m, u64 x, u64 y) {
  return x + y + !!(m->flags & CF);
}
MICRO_OP i64 JustSbb(struct Machine *m, u64 x, u64 y) {
  return x - y - !!(m->flags & CF);
}
MICRO_OP i64 JustAnd(struct Machine *m, u64 x, u64 y) {
  return x & y;
}
MICRO_OP i64 JustSub(struct Machine *m, u64 x, u64 y) {
  return x - y;
}
MICRO_OP i64 JustXor(struct Machine *m, u64 x, u64 y) {
  return x ^ y;
}
const aluop_f kJustAlu[8] = {
    JustAdd,  //
    JustOr,   //
    JustAdc,  //
    JustSbb,  //
    JustAnd,  //
    JustSub,  //
    JustXor,  //
    JustSub,  //
};

MICRO_OP i64 JustRol(u64 x, int ign1, int ign2, u64 y) {
  return x << y | x >> (64 - y);
}
MICRO_OP i64 JustRor(u64 x, int ign1, int ign2, u64 y) {
  return x >> y | x << (64 - y);
}
MICRO_OP i64 JustShl(u64 x, int ign1, int ign2, u64 y) {
  return x << y;
}
MICRO_OP i64 JustShr(u64 x, int ign1, int ign2, u64 y) {
  return x >> y;
}
MICRO_OP i64 JustSar(u64 x, int ign1, int ign2, u64 y) {
  return (i64)x >> y;
}
const aluop_f kJustBsu[8] = {
    (aluop_f)JustRol,  //
    (aluop_f)JustRor,  //
    0,                 //
    0,                 //
    (aluop_f)JustShl,  //
    (aluop_f)JustShr,  //
    (aluop_f)JustShl,  //
    (aluop_f)JustSar,  //
};

MICRO_OP i64 JustRolCl64(u64 x, struct Machine *m) {
  return x << (m->cl & 63) | x >> ((64 - m->cl) & 63);
}
MICRO_OP i64 JustRorCl64(u64 x, struct Machine *m) {
  return x >> (m->cl & 63) | x << ((64 - m->cl) & 63);
}
MICRO_OP i64 JustShlCl64(u64 x, struct Machine *m) {
  return x << (m->cl & 63);
}
MICRO_OP i64 JustShrCl64(u64 x, struct Machine *m) {
  return x >> (m->cl & 63);
}
MICRO_OP i64 JustSarCl64(u64 x, struct Machine *m) {
  return (i64)x >> (m->cl & 63);
}
const aluop_f kJustBsuCl64[8] = {
    (aluop_f)JustRolCl64,  //
    (aluop_f)JustRorCl64,  //
    0,                     //
    0,                     //
    (aluop_f)JustShlCl64,  //
    (aluop_f)JustShrCl64,  //
    (aluop_f)JustShlCl64,  //
    (aluop_f)JustSarCl64,  //
};

MICRO_OP i32 JustRolCl32(u32 x, struct Machine *m) {
  return x << (m->cl & 31) | x >> ((32 - m->cl) & 31);
}
MICRO_OP i32 JustRorCl32(u32 x, struct Machine *m) {
  return x >> (m->cl & 31) | x << ((32 - m->cl) & 31);
}
MICRO_OP i32 JustShlCl32(u32 x, struct Machine *m) {
  return x << (m->cl & 31);
}
MICRO_OP i32 JustShrCl32(u32 x, struct Machine *m) {
  return x >> (m->cl & 31);
}
MICRO_OP i32 JustSarCl32(u32 x, struct Machine *m) {
  return (i32)x >> (m->cl & 31);
}
const aluop_f kJustBsuCl32[8] = {
    (aluop_f)JustRolCl32,  //
    (aluop_f)JustRorCl32,  //
    0,                     //
    0,                     //
    (aluop_f)JustShlCl32,  //
    (aluop_f)JustShrCl32,  //
    (aluop_f)JustShlCl32,  //
    (aluop_f)JustSarCl32,  //
};

MICRO_OP i32 JustRol32(u32 x, int ign1, int ign2, u32 y) {
  return x << y | x >> (32 - y);
}
MICRO_OP i32 JustRor32(u32 x, int ign1, int ign2, u32 y) {
  return x >> y | x << (32 - y);
}
MICRO_OP i32 JustShl32(u32 x, int ign1, int ign2, u32 y) {
  return x << y;
}
MICRO_OP i32 JustShr32(u32 x, int ign1, int ign2, u32 y) {
  return x >> y;
}
MICRO_OP i32 JustSar32(u32 x, int ign1, int ign2, u32 y) {
  return (i32)x >> y;
}
const aluop_f kJustBsu32[8] = {
    (aluop_f)JustRol32,  //
    (aluop_f)JustRor32,  //
    0,                   //
    0,                   //
    (aluop_f)JustShl32,  //
    (aluop_f)JustShr32,  //
    (aluop_f)JustShl32,  //
    (aluop_f)JustSar32,  //
};

#if X86_INTRINSICS
#define ALU_FAST(M, TYPE, X, Y, OP, COMMUT)                             \
  ({                                                                    \
    TYPE Res;                                                           \
    u32 OldFlags = (M)->flags & ~(OF | SF | ZF | AF | CF);              \
    u64 NewFlags;                                                       \
    asm(OP "\t%3,%0\n\t"                                                \
        "pushfq\n\t"                                                    \
        "pop\t%1"                                                       \
        : "=&r" (Res), "=r" (NewFlags)                                  \
        : COMMUT "0" ((TYPE)(X)), "g" ((TYPE)(Y))                       \
        : "cc");                                                        \
    (M)->flags = OldFlags | ((u32)NewFlags & (OF | SF | ZF | AF | CF)); \
    Res;                                                                \
  })
#define ALU_FAST_CF(M, TYPE, X, Y, OP, COMMUT)                          \
  ({                                                                    \
    TYPE Res;                                                           \
    u32 OldFlags = (M)->flags & ~(OF | SF | ZF | AF);                   \
    u64 NewFlags;                                                       \
    asm("btr\t%5,%2\n\t"                                                \
        OP "\t%4,%0\n\t"                                                \
        "pushfq\n\t"                                                    \
        "pop\t%1"                                                       \
        : "=&r" (Res), "=r" (NewFlags), "+&r" (OldFlags)                \
        : COMMUT "0" ((TYPE)(X)), "g" ((TYPE)(Y)),                      \
          "i" (FLAGS_CF)                                                \
        : "cc");                                                        \
    (M)->flags = OldFlags | ((u32)NewFlags & (OF | SF | ZF | AF | CF)); \
    Res;                                                                \
  })
MICRO_OP static i64 FastXor64(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST(m, u64, x, y, "xor", "%");
}
MICRO_OP static i64 FastOr64(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST(m, u64, x, y, "or", "%");
}
MICRO_OP static i64 FastAnd64(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST(m, u64, x, y, "and", "%");
}
MICRO_OP static i64 FastSub64(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST(m, u64, x, y, "sub", "");
}
MICRO_OP static i64 FastAdd64(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST(m, u64, x, y, "add", "%");
}
MICRO_OP static i64 FastAdc64(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST_CF(m, u64, x, y, "adc", "%");
}
MICRO_OP static i64 FastSbb64(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST_CF(m, u64, x, y, "sbb", "");
}
MICRO_OP static i64 FastXor32(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST(m, u32, x, y, "xor", "%");
}
MICRO_OP static i64 FastOr32(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST(m, u32, x, y, "or", "%");
}
MICRO_OP static i64 FastAnd32(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST(m, u32, x, y, "and", "%");
}
MICRO_OP static i64 FastSub32(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST(m, u32, x, y, "sub", "");
}
MICRO_OP static i64 FastAdd32(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST(m, u32, x, y, "add", "%");
}
MICRO_OP static i64 FastAdc32(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST_CF(m, u32, x, y, "adc", "%");
}
MICRO_OP static i64 FastSbb32(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST_CF(m, u32, x, y, "sbb", "");
}
MICRO_OP static i64 FastXor16(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST(m, u16, x, y, "xor", "%");
}
MICRO_OP static i64 FastOr16(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST(m, u16, x, y, "or", "%");
}
MICRO_OP static i64 FastAnd16(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST(m, u16, x, y, "and", "%");
}
MICRO_OP static i64 FastSub16(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST(m, u16, x, y, "sub", "");
}
MICRO_OP static i64 FastAdd16(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST(m, u16, x, y, "add", "%");
}
MICRO_OP static i64 FastAdc16(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST_CF(m, u16, x, y, "adc", "%");
}
MICRO_OP static i64 FastSbb16(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST_CF(m, u16, x, y, "sbb", "");
}
MICRO_OP static i64 FastXor8(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST(m, u8, x, y, "xor", "%");
}
MICRO_OP static i64 FastOr8(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST(m, u8, x, y, "or", "%");
}
MICRO_OP static i64 FastAnd8(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST(m, u8, x, y, "and", "%");
}
MICRO_OP static i64 FastSub8(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST(m, u8, x, y, "sub", "");
}
MICRO_OP static i64 FastAdd8(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST(m, u8, x, y, "add", "%");
}
MICRO_OP static i64 FastAdc8(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST_CF(m, u8, x, y, "adc", "%");
}
MICRO_OP static i64 FastSbb8(struct Machine *m, u64 x, u64 y) {
  return ALU_FAST_CF(m, u8, x, y, "sbb", "");
}
#else /* !X86_INTRINSICS */
MICRO_OP static i64 FastXor64(struct Machine *m, u64 x, u64 y) {
  u64 z = x ^ y;
  m->flags = (m->flags & ~(CF | ZF)) | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastOr64(struct Machine *m, u64 x, u64 y) {
  u64 z = x | y;
  m->flags = (m->flags & ~(CF | ZF)) | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastAnd64(struct Machine *m, u64 x, u64 y) {
  u64 z = x & y;
  m->flags = (m->flags & ~(CF | ZF)) | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastSub64(struct Machine *m, u64 x, u64 y) {
  u64 z = x - y;
  int c = x < z;
  m->flags = (m->flags & ~(CF | ZF)) | c << FLAGS_CF | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastAdd64(struct Machine *m, u64 x, u64 y) {
  u64 z = x + y;
  int c = z < y;
  m->flags = (m->flags & ~(CF | ZF)) | c << FLAGS_CF | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastAdc64(struct Machine *m, u64 x, u64 y) {
  u64 t = x + !!(m->flags & CF);
  u64 z = t + y;
  int c = (t < x) | (z < y);
  m->flags = (m->flags & ~(CF | ZF)) | c << FLAGS_CF | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastSbb64(struct Machine *m, u64 x, u64 y) {
  u64 t = x - !!(m->flags & CF);
  u64 z = t - y;
  int c = (x < t) | (t < z);
  m->flags = (m->flags & ~(CF | ZF)) | c << FLAGS_CF | !z << FLAGS_ZF;
  return z;
}

MICRO_OP static i64 FastXor32(struct Machine *m, u64 x, u64 y) {
  u32 z = x ^ y;
  m->flags = (m->flags & ~(CF | ZF)) | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastOr32(struct Machine *m, u64 x, u64 y) {
  u32 z = x | y;
  m->flags = (m->flags & ~(CF | ZF)) | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastAnd32(struct Machine *m, u64 x, u64 y) {
  u32 z = x & y;
  m->flags = (m->flags & ~(CF | ZF)) | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastSub32(struct Machine *m, u64 x, u64 y) {
  u32 z = x - y;
  int c = x < z;
  m->flags = (m->flags & ~(CF | ZF)) | c << FLAGS_CF | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastAdd32(struct Machine *m, u64 x, u64 y) {
  u32 z = x + y;
  int c = z < y;
  m->flags = (m->flags & ~(CF | ZF)) | c << FLAGS_CF | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastAdc32(struct Machine *m, u64 x, u64 y) {
  u32 t = x + !!(m->flags & CF);
  u32 z = t + y;
  int c = (t < x) | (z < y);
  m->flags = (m->flags & ~(CF | ZF)) | c << FLAGS_CF | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastSbb32(struct Machine *m, u64 x, u64 y) {
  u32 t = x - !!(m->flags & CF);
  u32 z = t - y;
  int c = (x < t) | (t < z);
  m->flags = (m->flags & ~(CF | ZF)) | c << FLAGS_CF | !z << FLAGS_ZF;
  return z;
}

MICRO_OP static i64 FastXor16(struct Machine *m, u64 x, u64 y) {
  u16 z = x ^ y;
  m->flags = (m->flags & ~(CF | ZF)) | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastOr16(struct Machine *m, u64 x, u64 y) {
  u16 z = x | y;
  m->flags = (m->flags & ~(CF | ZF)) | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastAnd16(struct Machine *m, u64 x, u64 y) {
  u16 z = x & y;
  m->flags = (m->flags & ~(CF | ZF)) | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastSub16(struct Machine *m, u64 x, u64 y) {
  u16 z = x - y;
  int c = x < z;
  m->flags = (m->flags & ~(CF | ZF)) | c << FLAGS_CF | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastAdd16(struct Machine *m, u64 x, u64 y) {
  u16 z = x + y;
  int c = z < y;
  m->flags = (m->flags & ~(CF | ZF)) | c << FLAGS_CF | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastAdc16(struct Machine *m, u64 x, u64 y) {
  u16 t = x + !!(m->flags & CF);
  u16 z = t + y;
  int c = (t < x) | (z < y);
  m->flags = (m->flags & ~(CF | ZF)) | c << FLAGS_CF | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastSbb16(struct Machine *m, u64 x, u64 y) {
  u16 t = x - !!(m->flags & CF);
  u16 z = t - y;
  int c = (x < t) | (t < z);
  m->flags = (m->flags & ~(CF | ZF)) | c << FLAGS_CF | !z << FLAGS_ZF;
  return z;
}

MICRO_OP static i64 FastXor8(struct Machine *m, u64 x, u64 y) {
  u8 z = x ^ y;
  m->flags = (m->flags & ~(CF | ZF)) | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastOr8(struct Machine *m, u64 x, u64 y) {
  u8 z = x | y;
  m->flags = (m->flags & ~(CF | ZF)) | !z << FLAGS_ZF;
  return z;
}
MICRO_OP i64 FastAnd8(struct Machine *m, u64 x, u64 y) {
  u8 z = x & y;
  m->flags = (m->flags & ~(CF | ZF)) | !z << FLAGS_ZF;
  return z;
}
MICRO_OP i64 FastSub8(struct Machine *m, u64 x, u64 y) {
  u8 z = x - y;
  int c = x < z;
  m->flags = (m->flags & ~(CF | ZF)) | c << FLAGS_CF | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastAdd8(struct Machine *m, u64 x, u64 y) {
  u8 z = x + y;
  int c = z < y;
  m->flags = (m->flags & ~(CF | ZF)) | c << FLAGS_CF | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastAdc8(struct Machine *m, u64 x, u64 y) {
  u8 t = x + !!(m->flags & CF);
  u8 z = t + y;
  int c = (t < x) | (z < y);
  m->flags = (m->flags & ~(CF | ZF)) | c << FLAGS_CF | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastSbb8(struct Machine *m, u64 x, u64 y) {
  u8 t = x - !!(m->flags & CF);
  u8 z = t - y;
  int c = (x < t) | (t < z);
  m->flags = (m->flags & ~(CF | ZF)) | c << FLAGS_CF | !z << FLAGS_ZF;
  return z;
}
#endif /* !X86_INTRINSICS */

const aluop_f kAluFast[8][4] = {
    {FastAdd8, FastAdd16, FastAdd32, FastAdd64},  //
    {FastOr8, FastOr16, FastOr32, FastOr64},      //
    {FastAdc8, FastAdc16, FastAdc32, FastAdc64},  //
    {FastSbb8, FastSbb16, FastSbb32, FastSbb64},  //
    {FastAnd8, FastAnd16, FastAnd32, FastAnd64},  //
    {FastSub8, FastSub16, FastSub32, FastSub64},  //
    {FastXor8, FastXor16, FastXor32, FastXor64},  //
    {FastSub8, FastSub16, FastSub32, FastSub64},  //
};

MICRO_OP u32 JustMul32(u32 x, u32 y, struct Machine *m) {
  return x * y;
}
MICRO_OP u64 JustMul64(u64 x, u64 y, struct Machine *m) {
  return x * y;
}

MICRO_OP i32 Imul32(i32 x, i32 y, struct Machine *m) {
  int o;
  i64 z;
  z = (i64)x * y;
  o = z != (i32)z;
  m->flags = (m->flags & ~(CF | OF)) | o << FLAGS_CF | o << FLAGS_OF;
  return z;
}
#ifdef HAVE_INT128
MICRO_OP i64 Imul64(i64 x, i64 y, struct Machine *m) {
  int o;
  __int128 z;
  z = (__int128)x * y;
  o = z != (i64)z;
  m->flags = (m->flags & ~(CF | OF)) | o << FLAGS_CF | o << FLAGS_OF;
  return z;
}
MICRO_OP void JustMulAxDx(u64 x, struct Machine *m) {
  unsigned __int128 z;
  z = (unsigned __int128)x * Get64(m->ax);
  Put64(m->ax, z);
  Put64(m->dx, z >> 64);
}
MICRO_OP void MulAxDx(u64 x, struct Machine *m) {
  int o;
  unsigned __int128 z;
  z = (unsigned __int128)x * Get64(m->ax);
  o = z != (u64)z;
  m->flags = (m->flags & ~(CF | OF)) | o << FLAGS_CF | o << FLAGS_OF;
  Put64(m->ax, z);
  Put64(m->dx, z >> 64);
}
#ifndef DISABLE_BMI2
MICRO_OP void Mulx64(u64 x,              //
                     struct Machine *m,  //
                     long vreg,          //
                     long rexrreg) {
  unsigned __int128 z;
  z = (unsigned __int128)x * Get64(m->dx);
  Put64(m->weg[vreg], z);
  Put64(m->weg[rexrreg], z >> 64);
}
#endif /* !DISABLE_BMI2 */
#endif /* HAVE_INT128 */

MICRO_OP i64 JustNeg(u64 x) {
  return -x;
}

MICRO_OP i64 JustDec(u64 x) {
  return x - 1;
}

MICRO_OP static i64 FastDec64(u64 x, struct Machine *m) {
  u64 z;
  z = x - 1;
  m->flags = (m->flags & ~ZF) | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastDec32(u64 x64, struct Machine *m) {
  u32 x, z;
  x = x64;
  z = x - 1;
  m->flags = (m->flags & ~ZF) | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastDec16(u64 x64, struct Machine *m) {
  u16 x, z;
  x = x64;
  z = x - 1;
  m->flags = (m->flags & ~ZF) | !z << FLAGS_ZF;
  return z;
}
MICRO_OP static i64 FastDec8(u64 x64, struct Machine *m) {
  u8 x, z;
  x = x64;
  z = x - 1;
  m->flags = (m->flags & ~ZF) | !z << FLAGS_ZF;
  return z;
}

const aluop_f kFastDec[4] = {
    (aluop_f)FastDec8,   //
    (aluop_f)FastDec16,  //
    (aluop_f)FastDec32,  //
    (aluop_f)FastDec64,  //
};

////////////////////////////////////////////////////////////////////////////////
// STACK OPERATIONS

MICRO_OP void FastPush(struct Machine *m, long rexbsrm) {
  u64 v, x = Get64(m->weg[rexbsrm]);
  Put64(m->sp, (v = Get64(m->sp) - 8));
  Write64(ToHost(v), x);
}

MICRO_OP void FastPop(struct Machine *m, long rexbsrm) {
  u64 v = Get64(m->sp);
  Put64(m->sp, v + 8);
  Put64(m->weg[rexbsrm], Read64(ToHost(v)));
}

MICRO_OP void FastCall(struct Machine *m, u64 disp) {
  u64 v, x = m->ip + disp;
  Put64(m->sp, (v = Get64(m->sp) - 8));
  Write64(ToHost(v), m->ip);
  m->ip = x;
}

MICRO_OP void FastCallAbs(u64 x, struct Machine *m) {
  u64 v;
  Put64(m->sp, (v = Get64(m->sp) - 8));
  Write64(ToHost(v), m->ip);
  m->ip = x;
}

MICRO_OP void FastLeave(struct Machine *m) {
  u64 v = Get64(m->bp);
  Put64(m->sp, v + 8);
  Put64(m->bp, Read64(ToHost(v)));
}

MICRO_OP i64 PredictRet(struct Machine *m, i64 prediction) {
  u64 v = Get64(m->sp);
  Put64(m->sp, v + 8);
  m->ip = Read64(ToHost(v));
  return m->ip ^ prediction;
}

////////////////////////////////////////////////////////////////////////////////
// SIGN EXTENDING

MICRO_OP static u64 Sex8(u64 x) {
  return (i8)x;
}
MICRO_OP static u64 Sex16(u64 x) {
  return (i16)x;
}
MICRO_OP static u64 Sex32(u64 x) {
  return (i32)x;
}
typedef u64 (*sex_f)(u64);
static const sex_f kSex[] = {Sex8, Sex16, Sex32};

////////////////////////////////////////////////////////////////////////////////
// ADDRESSING

MICRO_OP static u64 Truncate32(u64 x) {
  return (u32)x;
}
MICRO_OP static i64 Seg(struct Machine *m, u64 d, long s) {
  return d + m->seg[s].base;
}
MICRO_OP static i64 Base(struct Machine *m, u64 d, long i) {
  return d + Get64(m->weg[i]);
}
MICRO_OP static i64 Index(struct Machine *m, u64 d, long i, int z) {
  return d + (Get64(m->weg[i]) << z);
}
MICRO_OP static i64 BaseIndex0(struct Machine *m, u64 d, long b, long i) {
  return d + Get64(m->weg[b]) + Get64(m->weg[i]);
}
MICRO_OP static i64 BaseIndex1(struct Machine *m, u64 d, long b, long i) {
  return d + Get64(m->weg[b]) + (Get64(m->weg[i]) << 1);
}
MICRO_OP static i64 BaseIndex2(struct Machine *m, u64 d, long b, long i) {
  return d + Get64(m->weg[b]) + (Get64(m->weg[i]) << 2);
}
MICRO_OP static i64 BaseIndex3(struct Machine *m, u64 d, long b, long i) {
  return d + Get64(m->weg[b]) + (Get64(m->weg[i]) << 3);
}

typedef i64 (*baseindex_f)(struct Machine *, u64, long, long);
static const baseindex_f kBaseIndex[] = {
    BaseIndex0,  //
    BaseIndex1,  //
    BaseIndex2,  //
    BaseIndex3,  //
};

////////////////////////////////////////////////////////////////////////////////
// JIT DEBUGGING UTILITIES

static pureconst bool UsesStaticMemory(u64 rde) {
  return !IsModrmRegister(rde) &&  //
         !SibExists(rde) &&        //
         IsRipRelative(rde);       //
}

static void ClobberEverythingExceptResult(struct Machine *m) {
#ifdef DEBUG
// clobber everything except result registers
#if defined(__x86_64__) && !defined(__CYGWIN__)
  AppendJitSetReg(m->path.jb, kAmdDi, 0x666);
  AppendJitSetReg(m->path.jb, kAmdSi, 0x666);
  AppendJitSetReg(m->path.jb, kAmdCx, 0x666);
  AppendJitSetReg(m->path.jb, 8, 0x666);
  AppendJitSetReg(m->path.jb, 9, 0x666);
  AppendJitSetReg(m->path.jb, 10, 0x666);
  AppendJitSetReg(m->path.jb, 11, 0x666);
#elif defined(__aarch64__)
  AppendJitSetReg(m->path.jb, 2, 0x666);
  AppendJitSetReg(m->path.jb, 3, 0x666);
  AppendJitSetReg(m->path.jb, 4, 0x666);
  AppendJitSetReg(m->path.jb, 5, 0x666);
  AppendJitSetReg(m->path.jb, 6, 0x666);
  AppendJitSetReg(m->path.jb, 7, 0x666);
  AppendJitSetReg(m->path.jb, 9, 0x666);
  AppendJitSetReg(m->path.jb, 10, 0x666);
  AppendJitSetReg(m->path.jb, 11, 0x666);
  AppendJitSetReg(m->path.jb, 12, 0x666);
  AppendJitSetReg(m->path.jb, 13, 0x666);
  AppendJitSetReg(m->path.jb, 14, 0x666);
  AppendJitSetReg(m->path.jb, 15, 0x666);
#endif
#endif
}

////////////////////////////////////////////////////////////////////////////////
// FUNCTION BODY EXTRACTOR

static bool IsRet(u8 *p) {
#if defined(__aarch64__)
  return Get32(p) == kArmRet;
#elif defined(__x86_64__)
  return *p == kAmdRet;
#else
  __builtin_unreachable();
#endif
}

static long GetInstructionLength(u8 *p) {
#if defined(__aarch64__)
  // on AArch64, do not recognize instructions which are known to be not
  // trivially relocatable i.e. opcodes which do PC-relative addressing;
  // but as exceptions, allow CBZ, CBNZ, TBZ, TBNZ, & B.cond
  u32 ins = Get32(p);
  if ((ins & ~kArmDispMask) == kArmJmp) return -1;
  if ((ins & ~kArmDispMask) == kArmCall) return -1;
  if ((ins & kArmAdrMask) == kArmAdr) return -1;
  if ((ins & kArmAdrpMask) == kArmAdrp) return -1;
  if ((ins & kArmLdrPcMask) == kArmLdrPc) return -1;
  if ((ins & kArmLdrswPcMask) == kArmLdrswPc) return -1;
  if ((ins & kArmPrfmPcMask) == kArmPrfmPc) return -1;
  return 4;
#elif defined(__x86_64__) /* !__aarch64__ */
  struct XedDecodedInst x;
  unassert(!DecodeInstruction(&x, p, 15, XED_MODE_LONG));
#ifndef NDEBUG
  if (ClassifyOp(x.op.rde) == kOpBranching) return -1;
  if (UsesStaticMemory(x.op.rde)) return -1;
#endif /* NDEBUG */
  return x.length;
#else /* !__x86_64__ */
  __builtin_unreachable();
#endif /* !__x86_64__ */
}

static long GetMicroOpLengthImpl(void *uop) {
  long k, n = 0;
  for (;;) {
    if (IsRet((u8 *)uop + n)) return n;
    k = GetInstructionLength((u8 *)uop + n);
    if (k == -1) return -1;
    n += k;
  }
}

static long GetMicroOpLength(void *uop) {
  _Static_assert(IS2POW(kMaxOps), "");
  static unsigned count;
  static void *ops[kMaxOps * 2];
  static short len[kMaxOps * 2];
  long res;
  unsigned hash, i, step;
  i = 0;
  step = 0;
  hash = ((uintptr_t)uop * 0x9e3779b1u) >> 16;
  do {
    i = (hash + step * (step + 1) / 2) & (kMaxOps - 1);
    if (ops[i] == uop) return len[i];
    ++step;
  } while (ops[i]);
  res = GetMicroOpLengthImpl(uop);
  unassert(count++ < kMaxOps);
  ops[i] = uop;
  len[i] = res;
  return res;
}

////////////////////////////////////////////////////////////////////////////////
// PRINTF-STYLE X86 MICROCODING WITH POSTFIX NOTATION

#define ItemsRequired(n) unassert(i >= n)

_Thread_local static long i;
_Thread_local static u8 stack[8];

static inline unsigned CheckBelow(unsigned x, unsigned n) {
  unassert(x < n);
  return x;
}

static void CallFunction(struct Machine *m, void *fun) {
  AppendJitCall(m->path.jb, fun);
  ClobberEverythingExceptResult(m);
}

static void CallMicroOp(struct Machine *m, void *fun) {
#ifdef TRIVIALLY_RELOCATABLE
  long len;
  if ((len = GetMicroOpLength(fun)) > 0) {
    AppendJit(m->path.jb, fun, len);
  } else {
    LOG_ONCE(LOGF("jit micro-operation at address %" PRIxPTR
                  " has branches or static memory references",
                  (uintptr_t)fun));
    CallFunction(m, fun);
  }
#else
  CallFunction(m, fun);
#endif
}

static void GetReg_32_64(struct Machine *m, void *fun) {
  AppendJitMovReg(m->path.jb, kJitArg0, kJitSav0);
  CallMicroOp(m, fun);
}

static void GetReg(P, unsigned log2sz, unsigned reg, unsigned breg) {
  switch (log2sz) {
    case 0:
      Jitter(A,
             "q"    // arg0 = machine
             "a1i"  // arg1 = register index
             "m",   // call micro-op
             (u64)kByteReg[breg], kGetReg[0]);
      break;
    case 2:
      GetReg_32_64(m, kGetReg32[reg]);
      break;
    case 3:
      GetReg_32_64(m, kGetReg64[reg]);
      break;
    default:
      Jitter(A,
             "q"    // arg0 = machine
             "a1i"  // arg1 = register index
             "m",   // call micro-op
             (u64)reg, kGetReg[log2sz]);
      break;
  }
}

static void PutReg_32_64(struct Machine *m, void *fun) {
  ItemsRequired(1);
  AppendJitMovReg(m->path.jb, kJitArg1, kJitSav0);
  AppendJitMovReg(m->path.jb, kJitArg0, stack[i - 1]);
  CallMicroOp(m, fun);
  --i;
}

static void PutReg(P, unsigned log2sz, unsigned reg, unsigned breg) {
  switch (log2sz) {
    case 0:
      ItemsRequired(1);
      Jitter(A,
             "a2="  // arg2 = <pop>
             "a1i"  // arg1 = register index
             "q"    // arg0 = machine
             "m",   // call micro-op
             (u64)kByteReg[breg], kPutReg[0]);
      break;
    case 1:
      ItemsRequired(1);
      Jitter(A,
             "a2="  // arg2 = <pop>
             "a1i"  // arg1 = register index
             "q"    // arg0 = machine
             "m",   // call micro-op
             (u64)reg, kPutReg[1]);
      break;
    case 2:
      PutReg_32_64(m, kPutReg32[reg]);
      break;
    case 3:
      PutReg_32_64(m, kPutReg64[reg]);
      break;
    case 4:
      // note: r0 == a0 on aarch64
      // note: r1 == a1 on aarch64
      // note: r1 == a2 on system five
      // note: r1 == a1 on cygwin
      Jitter(A,
             "a3i"    // arg3 = register index
             "r1a1="  // arg1 = res1
             "s0a2="  // arg2 = machine
             "t"      // arg0 = res0
             "m",     // call micro-op
             (u64)reg, kPutReg[log2sz]);
      break;
    default:
      __builtin_unreachable();
  }
}

static unsigned JitterImpl(P, const char *fmt, va_list va, unsigned k,
                           unsigned depth) {
  unsigned c, log2sz;
  log2sz = RegLog2(rde);
  LogCodOp(m, fmt);
  while ((c = fmt[k++])) {
    switch (c) {

      case ' ':  // nop
        break;

      case 'u':  // unpop
        i += 1;
        break;

      case '!':  // trap
        AppendJitTrap(m->path.jb);
        break;

      case '%':  // register
        switch ((c = fmt[k++])) {
          case 'c':
            switch ((c = fmt[k++])) {
              case 'l':
                Jitter(A,
                       "q"   // arg0 = machine
                       "m",  // call micro-op
                       GetCl);
                break;
              default:
                unassert(!"bad register");
            }
            break;
          default:
            unassert(!"bad register");
        }
        break;

      case 'm':  // micro-op
        CallMicroOp(m, va_arg(va, void *));
        break;

      case 'c':  // call
        CallFunction(m, va_arg(va, void *));
        break;

      case 'r':  // push res reg
        stack[i++] = kJitRes[CheckBelow(fmt[k++] - '0', ARRAYLEN(kJitRes))];
        break;

      case 'a':  // push arg reg
        stack[i++] = kJitArg[CheckBelow(fmt[k++] - '0', ARRAYLEN(kJitArg))];
        break;

      case 's':  // push sav reg
        stack[i++] = kJitSav[CheckBelow(fmt[k++] - '0', ARRAYLEN(kJitSav))];
        break;

      case 'i':  // set reg imm, e.g. ("a1i", 123) [mov $123,%rsi]
        ItemsRequired(1);
        AppendJitSetReg(m->path.jb, stack[--i], va_arg(va, u64));
        break;

      case '=':  // <src><dst>= mov reg, e.g. s0a0= [mov %rbx,%rdi]
        ItemsRequired(2);
        AppendJitMovReg(m->path.jb, stack[i - 1], stack[i - 2]);
        i -= 2;
        break;

      case 'q':  // s0a0= [shortcut]
        AppendJitMovReg(m->path.jb, kJitArg0, kJitSav0);
        break;

      case 't':  // r0a0= [shortcut]
        AppendJitMovReg(m->path.jb, kJitArg0, kJitRes0);
        break;

      case 'A':  // res0 = GetReg(RexrReg)
        GetReg(A, log2sz, RexrReg(rde), RexRexr(rde));
        break;

      case 'C':  // PutReg(RexrReg, <pop>)
        PutReg(A, log2sz, RexrReg(rde), RexRexr(rde));
        break;

      case 'B':  // res0 = GetRegOrMem(RexbRm)
        if (IsModrmRegister(rde)) {
          GetReg(A, log2sz, RexbRm(rde), RexRexb(rde));
        } else if (HasLinearMapping()) {
          if (!kSkew) {
            Jitter(A,
                   "L"         // load effective address
                   "t"         // arg0 = pointer
                   LOADSTORE,  // call function (read word shared memory)
                   kLoad[log2sz]);
          } else {
            Jitter(A,
                   "L"         // load effective address
                   "t"         // arg0 = virtual address
                   "m"         // call micro-op (turn virtual into pointer)
                   "t"         // arg0 = pointer
                   LOADSTORE,  // call function (read word shared memory)
                   ResolveHost, kLoad[log2sz]);
          }
        } else {
          Jitter(A,
                 "L"         // load effective address
                 "a3i"       // arg3 = false
                 "a2i"       // arg2 = bytes to read
                 "r0a1="     // arg1 = virtual address
                 "q"         // arg0 = machine
                 "c"         // call function (turn virtual into pointer)
                 "t"         // arg0 = pointer
                 LOADSTORE,  // call micro-op (read vector shared memory)
                 (u64)0, (u64)(1 << log2sz), ReserveAddress, kLoad[log2sz]);
        }
        break;

      case 'D':  // PutRegOrMem(RexbRm, <pop>), e.g. c r0 D
        if (log2sz < 4) {
          ItemsRequired(1);
          if (IsModrmRegister(rde)) {
            PutReg(A, log2sz, RexbRm(rde), RexRexb(rde));
          } else if (HasLinearMapping()) {
            if (!kSkew) {
              Jitter(A,
                     "s3="       // sav3 = <pop>
                     "L"         // load effective address
                     "s3a1="     // arg1 = sav3
                     "t"         // arg0 = res0
                     LOADSTORE,  // call micro-op (write word to shared memory)
                     kStore[log2sz]);
            } else {
              Jitter(A,
                     "s3="       // sav3 = <pop>
                     "L"         // load effective address
                     "t"         // arg0 = virtual address
                     "m"         // call micro-op
                     "s3a1="     // arg1 = sav3
                     "t"         // arg0 = res0
                     LOADSTORE,  // call micro-op (write word to shared memory)
                     ResolveHost, kStore[log2sz]);
            }
          } else {
            Jitter(A,
                   "s3="       // sav3 = <pop>
                   "L"         // load effective address
                   "a3i"       // arg3 = true
                   "a2i"       // arg2 = byte width of write operation
                   "r0a1="     // arg1 = res0
                   "q"         // arg0 = machine
                   "c"         // call function (turn virtual into pointer)
                   "s3a1="     // arg1 = sav3
                   "t"         // arg0 = res0
                   LOADSTORE,  // call function (write word to shared memory)
                   (u64)1, (u64)(1 << log2sz), ReserveAddress, kStore[log2sz]);
          }
        } else {
          if (IsModrmRegister(rde)) {
            // note: r0 == a0 on aarch64
            // note: r1 == a1 on aarch64
            // note: r1 == a2 on system five
            // note: r1 == a1 on cygwin
            Jitter(A,
                   "a3i"    // arg3 = index of register
                   "r1a1="  // arg1 = res1
                   "s0a3="  // arg2 = machine
                   "t"      // arg0 = res0
                   "m",     // call micro-op (xmm put register)
                   RexbRm(rde), kPutReg[log2sz]);
          } else if (HasLinearMapping()) {
            if (!kSkew) {
              Jitter(
                  A,
                  "r1s4="     // sav4 = res1
                  "r0s3="     // sav3 = res0
                  "L"         // load effective address
                  "s4a2="     // arg2 = sav4
                  "s3a1="     // arg1 = sav3
                  "t"         // arg0 = res0
                  LOADSTORE,  // call micro-op (store vector to shared memory)
                  kStore[log2sz]);
            } else {
              Jitter(
                  A,
                  "r1s4="     // sav4 = res1
                  "r0s3="     // sav3 = res0
                  "L"         // load effective address
                  "t"         // arg0 = virtual address
                  "m"         // call micro-op
                  "s4a2="     // arg2 = sav4
                  "s3a1="     // arg1 = sav3
                  "t"         // arg0 = res0
                  LOADSTORE,  // call micro-op (store vector to shared memory)
                  ResolveHost, kStore[log2sz]);
            }
          } else {
            Jitter(A,
                   "r1s4="     // sav4 = res1
                   "r0s3="     // sav3 = res0
                   "L"         // load effective address
                   "a3i"       // arg3 = true
                   "a2i"       // arg2 = bytes to write
                   "r0a1="     // arg1 = res0
                   "q"         // arg0 = machine
                   "c"         // call function (turn virtual into pointer)
                   "s4a2="     // arg2 = sav4
                   "s3a1="     // arg1 = sav3
                   "t"         // arg0 = res0
                   LOADSTORE,  // call micro-op (store vector to shared memory)
                   (u64)1, (u64)(1 << log2sz), ReserveAddress, kStore[log2sz]);
          }
        }
        break;

      case 'L':  // load effective address
        if (!SibExists(rde) && IsRipRelative(rde)) {
          AppendJitSetReg(m->path.jb, kJitRes0, disp + m->ip);
        } else if (!SibExists(rde)) {
          if (disp) {
            Jitter(A,
                   "a2i"  // arg2 = address base register index
                   "a1i"  // arg1 = displacement
                   "q"    // arg0 = machine
                   "m",   // call micro-op
                   RexbRm(rde), disp, Base);
          } else {
            Jitter(A,
                   "q"   // arg0 = machine
                   "m",  // call micro-op
                   kGetReg64[RexbRm(rde)]);
          }
        } else if (!SibHasBase(rde) && !SibHasIndex(rde)) {
          Jitter(A, "r0i", disp);  // res0 = absolute
        } else if (SibHasBase(rde) && !SibHasIndex(rde)) {
          if (disp) {
            AppendJitSetReg(m->path.jb, kJitArg2, RexbBase(rde));
            AppendJitSetReg(m->path.jb, kJitArg1, disp);
            AppendJitMovReg(m->path.jb, kJitArg0, kJitSav0);
            CallMicroOp(m, Base);
          } else {
            Jitter(A,
                   "q"   // arg0 = machine
                   "m",  // call micro-op
                   kGetReg64[RexbBase(rde)]);
          }
        } else if (!SibHasBase(rde) && SibHasIndex(rde)) {
          Jitter(A,
                 "a3i"  // arg3 = log2(address index scale)
                 "a2i"  // arg2 = address index register index
                 "a1i"  // arg1 = displacement
                 "q"    // arg0 = machine
                 "m",   // call micro-op
                 SibScale(rde), Rexx(rde) << 3 | SibIndex(rde), disp, Index);
        } else {
          Jitter(A,
                 "a3i"  // arg4 = address index register index
                 "a2i"  // arg2 = address base register index
                 "a1i"  // arg1 = displacement
                 "q"    // arg0 = machine
                 "m",   // call micro-op
                 Rexx(rde) << 3 | SibIndex(rde), RexbBase(rde), disp,
                 kBaseIndex[SibScale(rde)]);
        }
        if (Eamode(rde) == XED_MODE_LEGACY) {
          Jitter(A,
                 "t"   // arg0 = res0
                 "m",  // call micro-op
                 Truncate32);
        }
        if (Sego(rde)) {
          Jitter(A,
                 "a2i"    // arg2 = segment register index
                 "r0a1="  // arg1 = res0
                 "q"      // arg0 = machine
                 "m",     // call micro-op
                 Sego(rde) - 1, Seg);
        }
        break;

      case 'Q':  // res0 = GetRegPointer(RexrReg)
        Jitter(A,
               "a1i"  // arg1 = register index
               "q"    // arg0 = machine
               "m",   // call micro-op
               !log2sz ? (u64)kByteReg[RexrReg(rde)] : RexrReg(rde),
               !log2sz      ? GetBegPtr
               : log2sz < 4 ? GetWegPtr
                            : GetXmmPtr);
        break;

      case 'P':  // res0 = GetRegOrMemPointer(RexbRm)
        if (IsModrmRegister(rde)) {
          Jitter(A,
                 "a1i"  // arg1 = register index
                 "q"    // arg0 = machine
                 "m",   // call micro-op
                 !log2sz ? (u64)kByteReg[RexbRm(rde)] : RexbRm(rde),
                 !log2sz      ? GetBegPtr
                 : log2sz < 4 ? GetWegPtr
                              : GetXmmPtr);
        } else if (HasLinearMapping()) {
          if (!kSkew) {
            Jitter(A, "L");  // load effective address
          } else {
            Jitter(A,
                   "L"   // load effective address
                   "t"   // arg0 = virtual address
                   "m",  // res0 = call micro-op (turn virtual into pointer)
                   ResolveHost);
          }
        } else {
          Jitter(A,
                 "L"      // load effective address
                 "a3i"    // arg3 = false
                 "a2i"    // arg2 = bytes to read
                 "r0a1="  // arg1 = virtual address
                 "q"      // arg0 = machine
                 "c",     // res0 = call function (turn virtual into pointer)
                 (u64)0, (u64)(1 << log2sz), ReserveAddress);
        }
        break;

      case 'E':  // r0 = GetReg(RexbSrm)
        GetReg(A, log2sz, RexbSrm(rde), RexRexbSrm(rde));
        break;

      case 'F':  // PutReg(RexbSrm, <pop>)
        PutReg(A, log2sz, RexbSrm(rde), RexRexbSrm(rde));
        break;

      case 'G':  // r0 = GetReg(AX)
        GetReg(A, log2sz, 0, 0);
        break;

      case 'H':  // PutReg(AX, <pop>)
        PutReg(A, log2sz, 0, 0);
        break;

      case 'w':  // prevents byte operation
        log2sz = WordLog2(rde);
        continue;

      case 'z':  // force size
        log2sz = CheckBelow(fmt[k++] - '0', 5);
        continue;

      case 'x':  // sign extend
        ItemsRequired(1);
        unassert(log2sz < 4);
        if (log2sz < 3) {
          Jitter(A,
                 "a0="  // arg0 = machine
                 "m",   // call micro-op (sign extend)
                 kSex[log2sz]);
        } else {
          Jitter(A, "r0=");  // result = <pop>
        }
        break;

      default:
        LOGF("%s %c index %d of '%s'", "bad jitter directive", c, k - 1, fmt);
        unassert(!"bad jitter directive");
    }
    unassert(i <= ARRAYLEN(stack));
    log2sz = RegLog2(rde);
  }
  return k;
}

/**
 * Generates JIT code that implements x86 instructions.
 */
void Jitter(P, const char *fmt, ...) {
  va_list va;
  if (!IsMakingPath(m)) return;
  va_start(va, fmt);
  JitterImpl(A, fmt, va, 0, 0);
  unassert(!i);
  va_end(va);
}

#endif /* HAVE_JIT */
