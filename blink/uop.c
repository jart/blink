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
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/jit.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/modrm.h"
#include "blink/mop.h"
#include "blink/pun.h"
#include "blink/stats.h"
#include "blink/swap.h"
#include "blink/types.h"
#include "blink/x86.h"

/**
 * @fileoverview X86 Micro-Operations w/ Printf RPN Glue-Generating DSL.
 */

#define kMaxOps 256

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
MICRO_OP static struct Xmm GetReg128(struct Machine *m, long i) {
  return (struct Xmm){Get64(m->xmm[i]), Get64(m->xmm[i] + 8)};
}
typedef u64 (*getreg_f)(struct Machine *, long);
static const getreg_f kGetReg[] = {GetReg8, GetReg16,   //
                                   GetReg32, GetReg64,  //
                                   (getreg_f)GetReg128};

////////////////////////////////////////////////////////////////////////////////
// WRITING TO REGISTER FILE

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
MICRO_OP static void PutReg128(u64 x, u64 y, long dont_clobber_r1,
                               struct Machine *m, long i) {
  Put64(m->xmm[i], x);
  Put64(m->xmm[i] + 8, y);
}
typedef void (*putreg_f)(struct Machine *, long, u64);
static const putreg_f kPutReg[] = {PutReg8, PutReg16,   //
                                   PutReg32, PutReg64,  //
                                   (putreg_f)PutReg128};

////////////////////////////////////////////////////////////////////////////////
// MEMORY OPERATIONS

typedef i64 (*load_f)(const u8 *);
typedef void (*store_f)(u8 *, u64);

MICRO_OP static u8 *ResolveHost(i64 v) {
  return ToHost(v);
}

MICRO_OP static u8 *GetXmmPtr(struct Machine *m, long i) {
  return m->xmm[i];
}

#if defined(__x86_64__) && defined(TRIVIALLY_RELOCATABLE)
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
MICRO_OP static struct Xmm NativeLoad128(u8 *p) {
  return (struct Xmm){
      ((const u64 *)p)[0],
      ((const u64 *)p)[1],
  };
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

MICRO_OP static struct Xmm Load128(u8 *p) {
  return (struct Xmm){Read64(p), Read64(p + 8)};
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

#ifdef HAVE_INT128
MICRO_OP void Mulx64(u64 x,              //
                     struct Machine *m,  //
                     long vreg,          //
                     long rexrreg) {
  unsigned __int128 z;
  z = (unsigned __int128)x * Get64(m->dx);
  Put64(m->weg[vreg], z);
  Put64(m->weg[rexrreg], z >> 64);
}
#endif

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

MICRO_OP void FastRet(struct Machine *m) {
  u64 v = Get64(m->sp);
  Put64(m->sp, v + 8);
  m->ip = Read64(ToHost(v));
}

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

////////////////////////////////////////////////////////////////////////////////
// ADDRESSING

MICRO_OP static i64 Seg(struct Machine *m, u64 d, long s) {
  return d + m->seg[s];
}
MICRO_OP static i64 Base(struct Machine *m, u64 d, long i) {
  return d + Get64(m->weg[i]);
}
MICRO_OP static i64 Index(struct Machine *m, u64 d, long i, int z) {
  return d + (Get64(m->weg[i]) << z);
}
MICRO_OP static i64 BaseIndex(struct Machine *m, u64 d, long b, int z, long i) {
  return d + Get64(m->weg[b]) + (Get64(m->weg[i]) << z);
}

////////////////////////////////////////////////////////////////////////////////
// FLOATING POINT

MICRO_OP void OpPsdMuld1(u8 *p, struct Machine *m, long reg) {
  union DoublePun x, y;
  y.i = Read64(p);
  x.i = Read64(m->xmm[reg]);
  x.f = x.f * y.f;
  Write64(m->xmm[reg], x.i);
}

MICRO_OP void OpPsdAddd1(u8 *p, struct Machine *m, long reg) {
  union DoublePun x, y;
  y.i = Read64(p);
  x.i = Read64(m->xmm[reg]);
  x.f = x.f + y.f;
  Write64(m->xmm[reg], x.i);
}

MICRO_OP void OpPsdSubd1(u8 *p, struct Machine *m, long reg) {
  union DoublePun x, y;
  y.i = Read64(p);
  x.i = Read64(m->xmm[reg]);
  x.f = x.f - y.f;
  Write64(m->xmm[reg], x.i);
}

MICRO_OP void OpPsdDivd1(u8 *p, struct Machine *m, long reg) {
  union DoublePun x, y;
  y.i = Read64(p);
  x.i = Read64(m->xmm[reg]);
  x.f = x.f / y.f;
  Write64(m->xmm[reg], x.i);
}

MICRO_OP void OpPsdMind1(u8 *p, struct Machine *m, long reg) {
  union DoublePun x, y;
  y.i = Read64(p);
  x.i = Read64(m->xmm[reg]);
  x.f = MIN(x.f, y.f);
  Write64(m->xmm[reg], x.i);
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
#if defined(__x86_64__)
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
#ifndef NDEBUG
  if ((Get32(p) & ~kArmDispMask) == kArmJmp) return -1;
  if ((Get32(p) & ~kArmDispMask) == kArmCall) return -1;
#endif
  return 4;
#elif defined(__x86_64__)
  struct XedDecodedInst x;
  unassert(!DecodeInstruction(&x, p, 15, XED_MODE_LONG));
#ifndef NDEBUG
  if (ClassifyOp(x.op.rde) == kOpBranching) return -1;
  if (UsesStaticMemory(x.op.rde)) return -1;
#endif
  return x.length;
#else
  __builtin_unreachable();
#endif
}

long GetMicroOpLengthImpl(void *uop) {
  long k, n = 0;
  for (;;) {
    if (IsRet((u8 *)uop + n)) return n;
    k = GetInstructionLength((u8 *)uop + n);
    if (k == -1) return -1;
    n += k;
  }
}

long GetMicroOpLength(void *uop) {
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

static _Thread_local long i;
static _Thread_local u8 stack[8];

static inline unsigned CheckBelow(unsigned x, unsigned n) {
  unassert(x < n);
  return x;
}

static unsigned JitterImpl(P, const char *fmt, va_list va, unsigned k,
                           unsigned depth) {
  unsigned c, log2sz;
  log2sz = RegLog2(rde);
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
#ifdef TRIVIALLY_RELOCATABLE
      {
        void *fun = va_arg(va, void *);
        long len = GetMicroOpLength(fun);
        if (len > 0) {
          AppendJit(m->path.jb, fun, len);
          break;
        }
        LOG_ONCE(LOGF("jit micro-operation at address %" PRIxPTR
                      " has branches or static memory references",
                      (intptr_t)fun));
        // fallthrough
      }
#endif

      case 'c':  // call
        AppendJitCall(m->path.jb, va_arg(va, void *));
        ClobberEverythingExceptResult(m);
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
        Jitter(A,
               "q"    // arg0 = machine
               "a1i"  // arg1 = register index
               "m",   // call micro-op
               log2sz ? RexrReg(rde) : kByteReg[RexRexr(rde)], kGetReg[log2sz]);
        break;

      case 'C':  // PutReg(RexrReg, <pop>)
        if (log2sz < 4) {
          ItemsRequired(1);
          Jitter(A,
                 "a2="  // arg2 = <pop>
                 "a1i"  // arg1 = register index
                 "q"    // arg0 = machine
                 "m",   // call micro-op
                 log2sz ? RexrReg(rde) : kByteReg[RexRexr(rde)],
                 kPutReg[log2sz]);
        } else {
          Jitter(A,
                 "a4i"    // arg4 = register index
                 "s0a3="  // arg3 = machine
                 ""       // arg2 = undefined
                 "r1a1="  // arg1 = res1
                 "t"      // arg0 = res0
                 "m",     // call micro-op
                 RexrReg(rde), kPutReg[log2sz]);
        }
        break;

      case 'B':  // res0 = GetRegOrMem(RexbRm)
        if (IsModrmRegister(rde)) {
          Jitter(A,
                 "a1i"  // arg1 = register index
                 "q"    // arg0 = machine
                 "m",   // call micro-op
                 log2sz ? RexbRm(rde) : kByteReg[RexRexb(rde)],
                 kGetReg[log2sz]);
        } else if (HasLinearMapping(m)) {
          Jitter(A,
                 "L"         // load effective address
                 "t"         // arg0 = virtual address
                 "m"         // call micro-op (turn virtual into pointer)
                 "t"         // arg0 = pointer
                 LOADSTORE,  // call function (read word shared memory)
                 ResolveHost, kLoad[log2sz]);
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
                 false, 1ul << log2sz, ReserveAddress, kLoad[log2sz]);
        }
        break;

      case 'D':  // PutRegOrMem(RexbRm, <pop>), e.g. c r0 D
        if (log2sz < 4) {
          ItemsRequired(1);
          if (IsModrmRegister(rde)) {
            Jitter(A,
                   "a2="  // arg2 = <pop>
                   "a1i"  // arg1 = register index
                   "q"    // arg0 = machine
                   "m",   // call micro-op
                   log2sz ? RexbRm(rde) : kByteReg[RexRexb(rde)],
                   kPutReg[log2sz]);
          } else if (HasLinearMapping(m)) {
            Jitter(A,
                   "s3="       // sav3 = <pop>
                   "L"         // load effective address
                   "t"         // arg0 = virtual address
                   "m"         // call micro-op
                   "s3a1="     // arg1 = sav3
                   "t"         // arg0 = res0
                   LOADSTORE,  // call micro-op (write word to shared memory)
                   ResolveHost, kStore[log2sz]);
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
                   true, 1ul << log2sz, ReserveAddress, kStore[log2sz]);
          }
        } else {
          if (IsModrmRegister(rde)) {
            Jitter(A,
                   "a4i"    // arg4 = index of register
                   "s0a3="  // arg3 = machine
                   ""       // arg2 = undefined
                   "r1a1="  // arg1 = res1
                   "t"      // arg0 = res0
                   "m",     // call micro-op (xmm put register)
                   RexbRm(rde), kPutReg[log2sz]);
          } else if (HasLinearMapping(m)) {
            Jitter(A,
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
                   true, 1ul << log2sz, ReserveAddress, kStore[log2sz]);
          }
        }
        break;

      case 'L':  // load effective address
        if (!SibExists(rde) && IsRipRelative(rde)) {
          Jitter(A, "r0i", disp + m->ip);  // res0 = absolute
        } else if (!SibExists(rde)) {
          Jitter(A,
                 "a2i"  // arg2 = address base register index
                 "a1i"  // arg1 = displacement
                 "q"    // arg0 = machine
                 "m",   // call micro-op
                 RexbRm(rde), disp, Base);
        } else if (!SibHasBase(rde) && !SibHasIndex(rde)) {
          Jitter(A, "r0i", disp);  // res0 = absolute
        } else if (SibHasBase(rde) && !SibHasIndex(rde)) {
          Jitter(A,
                 "a2i"  // arg2 = address base register index
                 "a1i"  // arg1 = displacement
                 "q"    // arg0 = machine
                 "m",   // call micro-op
                 RexbBase(rde), disp, Base);
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
                 "a4i"  // arg4 = address index register index
                 "a3i"  // arg3 = log2(address index scale)
                 "a2i"  // arg2 = address base register index
                 "a1i"  // arg1 = displacement
                 "q"    // arg0 = machine
                 "m",   // call micro-op
                 Rexx(rde) << 3 | SibIndex(rde), SibScale(rde), RexbBase(rde),
                 disp, BaseIndex);
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

      case 'P':  // res0 = GetXmmOrMemPointer(RexbRm)
        if (IsModrmRegister(rde)) {
          Jitter(A,
                 "a1i"  // arg1 = register index
                 "q"    // arg0 = machine
                 "m",   // call micro-op
                 RexbRm(rde), GetXmmPtr);
        } else if (HasLinearMapping(m)) {
          Jitter(A,
                 "L"   // load effective address
                 "t"   // arg0 = virtual address
                 "m",  // res0 = call micro-op (turn virtual into pointer)
                 ResolveHost);
        } else {
          Jitter(A,
                 "L"      // load effective address
                 "a3i"    // arg3 = false
                 "a2i"    // arg2 = bytes to read
                 "r0a1="  // arg1 = virtual address
                 "q"      // arg0 = machine
                 "c",     // res0 = call function (turn virtual into pointer)
                 false, 1ul << log2sz, ReserveAddress);
        }
        break;

      case 'E':  // r0 = GetReg(RexbSrm)
        Jitter(A,
               "a1i"  // arg1 = index of register
               "q"    // arg0 = machine
               "m",   // call micro-op (get register)
               log2sz ? RexbSrm(rde) : kByteReg[RexRexbSrm(rde)],
               kGetReg[WordLog2(rde)]);
        break;

      case 'F':  // PutReg(RexbSrm, <pop>)
        ItemsRequired(1);
        unassert(log2sz < 4);
        Jitter(A,
               "a2="  // arg2 = <pop>
               "a1i"  // arg1 = index of register
               "q"    // arg0 = machine
               "m",   // call micro-op
               log2sz ? RexbSrm(rde) : kByteReg[RexRexbSrm(rde)],
               kPutReg[log2sz]);
        break;

      case 'G':  // r0 = GetReg(AX)
        Jitter(A,
               "a1i"  // arg1 = index of register
               "q"    // arg0 = machine
               "m",   // call micro-op
               0, kGetReg[log2sz]);
        break;

      case 'H':  // PutReg(AX, <pop>)
        ItemsRequired(1);
        unassert(log2sz < 4);
        Jitter(A,
               "a2="  // arg2 = <pop>
               "a1i"  // arg1 = 0
               "q"    // arg0 = machine
               "m",   // call micro-op
               0, kPutReg[log2sz]);
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
#ifdef HAVE_JIT
  va_list va;
  if (!IsMakingPath(m)) return;
  va_start(va, fmt);
  JitterImpl(A, fmt, va, 0, 0);
  unassert(!i);
  va_end(va);
#endif
}
