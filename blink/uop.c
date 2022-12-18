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
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/jit.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/modrm.h"
#include "blink/mop.h"
#include "blink/stats.h"
#include "blink/types.h"
#include "blink/x86.h"

/**
 * @fileoverview X86 Micro-Operations w/ Printf RPN Glue-Generating DSL.
 */

////////////////////////////////////////////////////////////////////////////////
// ACCOUNTING

MICRO_OP void CountOp(long *instructions_jitted_ptr) {
  STATISTIC(++*instructions_jitted_ptr);
}

////////////////////////////////////////////////////////////////////////////////
// READING FROM REGISTER FILE

MICRO_OP static u64 GetCl(struct Machine *m) {
  return m->cl;
}
MICRO_OP static u64 GetIp(struct Machine *m) {
  return m->ip;
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
typedef u64 (*getreg_f)(struct Machine *, long);
static const getreg_f kGetReg[] = {GetReg8, GetReg16, GetReg32, GetReg64};

////////////////////////////////////////////////////////////////////////////////
// WRITING TO REGISTER FILE

MICRO_OP static void PutReg8(struct Machine *m, long b, u64 x) {
  Put8(m->beg + b, x);
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
typedef void (*putreg_f)(struct Machine *, long, u64);
static const putreg_f kPutReg[] = {PutReg8, PutReg16, PutReg32, PutReg64};

////////////////////////////////////////////////////////////////////////////////
// READING FROM MEMORY

MICRO_OP static i64 LoadLinear8(i64 v) {
  return Load8(ToHost(v));
}
MICRO_OP static i64 LoadLinear16(i64 v) {
  return Load16(ToHost(v));
}
MICRO_OP static i64 LoadLinear32(i64 v) {
  return Load32(ToHost(v));
}
MICRO_OP static i64 LoadLinear64(i64 v) {
  return Load64(ToHost(v));
}
typedef i64 (*loadmem_f)(i64);
static const loadmem_f kLoadLinear[] = {LoadLinear8, LoadLinear16,  //
                                        LoadLinear32, LoadLinear64};

static i64 LoadReserve8(struct Machine *m, i64 v) {
  return Load8(ReserveAddress(m, v, 1, false));
}
static i64 LoadReserve16(struct Machine *m, i64 v) {
  return Load16(ReserveAddress(m, v, 2, false));
}
static i64 LoadReserve32(struct Machine *m, i64 v) {
  return Load32(ReserveAddress(m, v, 4, false));
}
static i64 LoadReserve64(struct Machine *m, i64 v) {
  return Load64(ReserveAddress(m, v, 8, false));
}
typedef i64 (*loadreserve_f)(struct Machine *, i64);
static const loadreserve_f kLoadReserve[] = {LoadReserve8, LoadReserve16,
                                             LoadReserve32, LoadReserve64};

////////////////////////////////////////////////////////////////////////////////
// WRITING TO MEMORY

MICRO_OP static void StoreLinear8(i64 v, u64 x) {
  Store8(ToHost(v), x);
}
MICRO_OP static void StoreLinear16(i64 v, u64 x) {
  Store16(ToHost(v), x);
}
MICRO_OP static void StoreLinear32(i64 v, u64 x) {
  Store32(ToHost(v), x);
}
MICRO_OP static void StoreLinear64(i64 v, u64 x) {
  Store64(ToHost(v), x);
}
typedef void (*storemem_f)(i64, u64);
static const storemem_f kStoreLinear[] = {StoreLinear8, StoreLinear16,
                                          StoreLinear32, StoreLinear64};

static void StoreReserve8(struct Machine *m, i64 v, u64 x) {
  Store8(ReserveAddress(m, v, 1, true), x);
}
static void StoreReserve16(struct Machine *m, i64 v, u64 x) {
  Store16(ReserveAddress(m, v, 2, true), x);
}
static void StoreReserve32(struct Machine *m, i64 v, u64 x) {
  Store32(ReserveAddress(m, v, 4, true), x);
}
static void StoreReserve64(struct Machine *m, i64 v, u64 x) {
  Store64(ReserveAddress(m, v, 8, true), x);
}
typedef void (*storereserve_f)(struct Machine *, i64, u64);
static const storereserve_f kStoreReserve[] = {StoreReserve8, StoreReserve16,
                                               StoreReserve32, StoreReserve64};

////////////////////////////////////////////////////////////////////////////////
// ALU MICRO-OPS

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

MICRO_OP i64 JustRol(struct Machine *m, u64 x, u64 y) {
  return x << y | x >> (64 - y);
}
MICRO_OP i64 JustRor(struct Machine *m, u64 x, u64 y) {
  return x >> y | x << (64 - y);
}
MICRO_OP i64 JustShl(struct Machine *m, u64 x, u64 y) {
  return x << y;
}
MICRO_OP i64 JustShr(struct Machine *m, u64 x, u64 y) {
  return x >> y;
}
MICRO_OP i64 JustSar(struct Machine *m, u64 x, u64 y) {
  return (i64)x >> y;
}
const aluop_f kJustBsu[8] = {
    JustRol,  //
    JustRor,  //
    0,        //
    0,        //
    JustShl,  //
    JustShr,  //
    JustShl,  //
    JustSar,  //
};

////////////////////////////////////////////////////////////////////////////////
// STACK

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

MICRO_OP void FastRet(struct Machine *m) {
  u64 v = Get64(m->sp);
  Put64(m->sp, v + 8);
  m->ip = Load64(ToHost(v));
}

////////////////////////////////////////////////////////////////////////////////
// BRANCHING

MICRO_OP void FastJmp(struct Machine *m, u64 disp) {
  m->ip += disp;
}

MICRO_OP u32 Jb(struct Machine *m) {
  return m->flags & CF;
}
MICRO_OP u32 Jae(struct Machine *m) {
  return ~m->flags & CF;
}
MICRO_OP u32 Je(struct Machine *m) {
  return m->flags & ZF;
}
MICRO_OP u32 Jne(struct Machine *m) {
  return ~m->flags & ZF;
}
MICRO_OP u32 Js(struct Machine *m) {
  return m->flags & SF;
}
MICRO_OP u32 Jns(struct Machine *m) {
  return ~m->flags & SF;
}
MICRO_OP u32 Jo(struct Machine *m) {
  return m->flags & OF;
}
MICRO_OP u32 Jno(struct Machine *m) {
  return ~m->flags & OF;
}
MICRO_OP u32 Ja(struct Machine *m) {
  return IsAbove(m);
}
MICRO_OP u32 Jbe(struct Machine *m) {
  return IsBelowOrEqual(m);
}
MICRO_OP u32 Jg(struct Machine *m) {
  return IsGreater(m);
}
MICRO_OP u32 Jge(struct Machine *m) {
  return IsGreaterOrEqual(m);
}
MICRO_OP u32 Jl(struct Machine *m) {
  return IsLess(m);
}
MICRO_OP u32 Jle(struct Machine *m) {
  return IsLessOrEqual(m);
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

MICRO_OP static i64 Seg(struct Machine *m, u64 d, long s) {
  return d + m->seg[s];
}
MICRO_OP static i64 Base(struct Machine *m, u64 d, long i) {
  return d + Get64(m->weg[i]);
}
MICRO_OP static i64 Index(struct Machine *m, u64 d, long i, int z) {
  return d + (Get64(m->weg[i]) << z);
}
MICRO_OP static i64 BaseIndex(struct Machine *m, u64 d, long b, long i, int z) {
  return d + Get64(m->weg[b]) + (Get64(m->weg[i]) << z);
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

static long GetMicroOpLength(void *p) {
  long k, n = 0;
  for (;;) {
    if (IsRet((u8 *)p + n)) return n;
    k = GetInstructionLength((u8 *)p + n);
    if (k == -1) return -1;
    n += k;
  }
}

////////////////////////////////////////////////////////////////////////////////
// PRINTF-STYLE X86 MICROCODING WITH POSTFIX NOTATION

#define ItemsRequired(n) unassert(i >= n)

static _Thread_local int i;
static _Thread_local u8 stack[8];

static inline int CheckBelow(int x, unsigned n) {
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

      case '$':  // ip
        Jitter(A, "s0a0= m", GetIp);
        break;

      case '%':  // register
        switch ((c = fmt[k++])) {
          case 'c':
            switch ((c = fmt[k++])) {
              case 'l':
                Jitter(A, "s0a0= m", GetCl);
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

      case 'x':  // sign extend
        ItemsRequired(1);
        Jitter(A, "a0= m", kSex[CheckBelow(fmt[k++] - '0', 3)]);
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

      case 'A':  // r0 = ReadReg(RexrReg)
        Jitter(A, "s0a0= a1i m",
               log2sz ? RexrReg(rde) : kByteReg[ByteRexr(rde)],
               kGetReg[log2sz]);
        break;

      case 'C':  // PutReg(RexrReg, <pop>)
        ItemsRequired(1);
        Jitter(A, "a2= a1i s0a0= m",
               log2sz ? RexrReg(rde) : kByteReg[ByteRexr(rde)],
               kPutReg[log2sz]);
        break;

      case 'B':  // r0 = ReadRegOrMem(RexbRm)
        if (IsModrmRegister(rde)) {
          Jitter(A, "a1i s0a0= m",
                 log2sz ? RexbRm(rde) : kByteReg[ByteRexb(rde)],
                 kGetReg[log2sz]);
        } else if (HasLinearMapping(m) && !Sego(rde)) {
          Jitter(A, "L r0a0= m", kLoadLinear[log2sz]);
        } else {
          Jitter(A, "L r0a1= s0a0= c", kLoadReserve[log2sz]);
        }
        break;

      case 'D':  // PutRegOrMem(RexbRm, <pop>), e.g. c r0 D
        ItemsRequired(1);
        if (IsModrmRegister(rde)) {
          Jitter(A, "a2= a1i s0a0= m",
                 log2sz ? RexbRm(rde) : kByteReg[ByteRexb(rde)],
                 kPutReg[log2sz]);
        } else if (HasLinearMapping(m)) {
          Jitter(A, "s1= L s1a1= r0a0= m", kStoreLinear[log2sz]);
        } else {
          Jitter(A, "s1= L s1a2= r0a1= s0a0= c", kStoreReserve[log2sz]);
        }
        break;

      case 'L':  // load effective address
        if (!SibExists(rde) && IsRipRelative(rde)) {
          Jitter(A, "r0i", disp + m->ip);
        } else if (!SibExists(rde)) {
          Jitter(A, "a2i a1i s0a0= m", RexbRm(rde), disp, Base);
        } else if (!SibHasBase(rde) &&  //
                   !SibHasIndex(rde)) {
          Jitter(A, "r0i", disp);
        } else if (SibHasBase(rde) &&  //
                   !SibHasIndex(rde)) {
          Jitter(A, "a2i a1i s0a0= m", RexbBase(rde), disp, Base);
        } else if (!SibHasBase(rde) &&  //
                   SibHasIndex(rde)) {
          Jitter(A, "a3i a2i a1i s0a0= m", SibScale(rde),
                 Rexx(rde) << 3 | SibIndex(rde), disp, Index);
        } else {
          Jitter(A, "a4i a3i a2i a1i s0a0= m", SibScale(rde),
                 Rexx(rde) << 3 | SibIndex(rde), RexbBase(rde), disp,
                 BaseIndex);
        }
        if (Sego(rde)) {
          Jitter(A, "a2i r0a1= s0a0= m", Sego(rde) - 1, Seg);
        }
        break;

      case 'E':  // r0 = ReadReg(RexbSrm)
        Jitter(A, "s0a0= a1i m", RexbSrm(rde), kGetReg[WordLog2(rde)]);
        break;

      case 'F':  // PutReg(RexbSrm, <pop>)
        ItemsRequired(1);
        Jitter(A, "a2= a1i s0a0= m", RexbSrm(rde), kPutReg[WordLog2(rde)]);
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
