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

#include "blink/assert.h"
#include "blink/endian.h"
#include "blink/jit.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/modrm.h"
#include "blink/mop.h"
#include "blink/types.h"
#include "blink/x86.h"

/**
 * @fileoverview X86 Micro-Operations w/ Printf RPN Glue-Generating DSL.
 */

////////////////////////////////////////////////////////////////////////////////
// READING FROM REGISTER FILE

static u64 GetCl(struct Machine *m) {
  return m->cl;
}
static u64 GetReg8(struct Machine *m, long b) {
  return Get8(m->beg + b);
}
static u64 GetReg16(struct Machine *m, long i) {
  return Get16(m->weg[i]);
}
static u64 GetReg32(struct Machine *m, long i) {
  return Get32(m->weg[i]);
}
static u64 GetReg64(struct Machine *m, long i) {
  return Get64(m->weg[i]);
}
typedef u64 (*getreg_f)(struct Machine *, long);
static const getreg_f kGetReg[] = {GetReg8, GetReg16, GetReg32, GetReg64};

////////////////////////////////////////////////////////////////////////////////
// WRITING TO REGISTER FILE

static void PutReg8(struct Machine *m, long b, u64 x) {
  Put8(m->beg + b, x);
}
static void PutReg16(struct Machine *m, long i, u64 x) {
  Put16(m->weg[i], x);
}
static void PutReg32(struct Machine *m, long i, u64 x) {
  Put64(m->weg[i], x & 0xffffffff);
}
static void PutReg64(struct Machine *m, long i, u64 x) {
  Put64(m->weg[i], x);
}
typedef void (*putreg_f)(struct Machine *, long, u64);
static const putreg_f kPutReg[] = {PutReg8, PutReg16, PutReg32, PutReg64};

////////////////////////////////////////////////////////////////////////////////
// GENERALIZED FALLBACK FOR LOADING OF MODRM PARAMETER

static u64 GetRegOrMem(P) {
  if (IsByteOp(rde)) {
    return Load8(GetModrmRegisterBytePointerRead1(A));
  } else {
    return ReadMemory(rde, GetModrmRegisterWordPointerReadOszRexw(A));
  }
}

////////////////////////////////////////////////////////////////////////////////
// GENERALIZED FALLBACK FOR STORING TO MODRM PARAMETER

static void PutRegOrMem(P) {
  if (IsByteOp(rde)) {
    Store8(GetModrmRegisterBytePointerWrite1(A), uimm0);
  } else {
    WriteRegisterOrMemory(rde, GetModrmRegisterWordPointerWriteOszRexw(A),
                          uimm0);
  }
}

////////////////////////////////////////////////////////////////////////////////
// SIGN EXTENDING

static u64 Sex8(u64 x) {
  return (i8)x;
}
static u64 Sex16(u64 x) {
  return (i16)x;
}
static u64 Sex32(u64 x) {
  return (i32)x;
}
typedef u64 (*sex_f)(u64);
static const sex_f kSex[] = {Sex8, Sex16, Sex32};

////////////////////////////////////////////////////////////////////////////////
// DEVIRTUALIZED LOADING OF ABSOLUTE MEMORY PARAMETER

static u64 GetDemAbs8(u64 abs) {
  return Load8(ToHost(abs));
}
static u64 GetDemAbs16(u64 abs) {
  return Load16(ToHost(abs));
}
static u64 GetDemAbs32(u64 abs) {
  return Load32(ToHost(abs));
}
static u64 GetDemAbs64(u64 abs) {
  return Load64(ToHost(abs));
}
typedef u64 (*getdemabs_f)(u64);
static const getdemabs_f kGetDemAbs[] = {GetDemAbs8, GetDemAbs16,  //
                                         GetDemAbs32, GetDemAbs64};

////////////////////////////////////////////////////////////////////////////////
// DEVIRTUALIZED STORING TO ABSOLUTE MEMORY PARAMETER

static void PutDemAbs8(u64 abs, u64 x) {
  Store8(ToHost(abs), x);
}
static void PutDemAbs16(u64 abs, u64 x) {
  Store16(ToHost(abs), x);
}
static void PutDemAbs32(u64 abs, u64 x) {
  Store32(ToHost(abs), x);
}
static void PutDemAbs64(u64 abs, u64 x) {
  Store64(ToHost(abs), x);
}
typedef void (*putdemabs_f)(u64, u64);
static const putdemabs_f kPutDemAbs[] = {PutDemAbs8, PutDemAbs16,  //
                                         PutDemAbs32, PutDemAbs64};

////////////////////////////////////////////////////////////////////////////////
// DEVIRTUALIZED LOADING OF ±DISP(%BASE) MEMORY PARAMETER

static u64 GetDem8(struct Machine *m, u64 disp, long i) {
  return Load8(ToHost(disp + Get64(m->weg[i])));
}
static u64 GetDem16(struct Machine *m, u64 disp, long i) {
  return Load16(ToHost(disp + Get64(m->weg[i])));
}
static u64 GetDem32(struct Machine *m, u64 disp, long i) {
  return Load32(ToHost(disp + Get64(m->weg[i])));
}
static u64 GetDem64(struct Machine *m, u64 disp, long i) {
  return Load64(ToHost(disp + Get64(m->weg[i])));
}
typedef u64 (*getdem_f)(struct Machine *, u64, long);
static const getdem_f kGetDem[] = {GetDem8, GetDem16, GetDem32, GetDem64};

////////////////////////////////////////////////////////////////////////////////
// DEVIRTUALIZED STORING TO ±DISP(%BASE) MEMORY PARAMETER

static void PutDem8(struct Machine *m, u64 disp, long i, u64 x) {
  Store8(ToHost(disp + Get64(m->weg[i])), x);
}
static void PutDem16(struct Machine *m, u64 disp, long i, u64 x) {
  Store16(ToHost(disp + Get64(m->weg[i])), x);
}
static void PutDem32(struct Machine *m, u64 disp, long i, u64 x) {
  Store32(ToHost(disp + Get64(m->weg[i])), x);
}
static void PutDem64(struct Machine *m, u64 disp, long i, u64 x) {
  Store64(ToHost(disp + Get64(m->weg[i])), x);
}
typedef void (*putdem_f)(struct Machine *, u64, long, u64);
static const putdem_f kPutDem[] = {PutDem8, PutDem16, PutDem32, PutDem64};

////////////////////////////////////////////////////////////////////////////////
// DEVIRTUALIZED LOADING OF ±DISP(,%INDEX,SCALE) MEMORY PARAMETER

static u64 GetDemIndex8(struct Machine *m, u64 disp, long index, int scale) {
  return Load8(ToHost(disp + (Get64(m->weg[index]) << scale)));
}
static u64 GetDemIndex16(struct Machine *m, u64 disp, long index, int scale) {
  return Load16(ToHost(disp + (Get64(m->weg[index]) << scale)));
}
static u64 GetDemIndex32(struct Machine *m, u64 disp, long index, int scale) {
  return Load32(ToHost(disp + (Get64(m->weg[index]) << scale)));
}
static u64 GetDemIndex64(struct Machine *m, u64 disp, long index, int scale) {
  return Load64(ToHost(disp + (Get64(m->weg[index]) << scale)));
}
typedef u64 (*getdemindex_f)(struct Machine *, u64, long, int);
static const getdemindex_f kGetDemIndex[] = {GetDemIndex8, GetDemIndex16,
                                             GetDemIndex32, GetDemIndex64};

////////////////////////////////////////////////////////////////////////////////
// DEVIRTUALIZED STORING TO ±DISP(,%INDEX,SCALE) MEMORY PARAMETER

static void PutDemIndex8(struct Machine *m, u64 disp, long i, int z, u64 x) {
  Store8(ToHost(disp + (Get64(m->weg[i]) << z)), x);
}
static void PutDemIndex16(struct Machine *m, u64 disp, long i, int z, u64 x) {
  Store16(ToHost(disp + (Get64(m->weg[i]) << z)), x);
}
static void PutDemIndex32(struct Machine *m, u64 disp, long i, int z, u64 x) {
  Store32(ToHost(disp + (Get64(m->weg[i]) << z)), x);
}
static void PutDemIndex64(struct Machine *m, u64 disp, long i, int z, u64 x) {
  Store64(ToHost(disp + (Get64(m->weg[i]) << z)), x);
}
typedef void (*putdemindex_f)(struct Machine *, u64, long, int, u64);
static const putdemindex_f kPutDemIndex[] = {PutDemIndex8, PutDemIndex16,
                                             PutDemIndex32, PutDemIndex64};

////////////////////////////////////////////////////////////////////////////////
// DEVIRTUALIZED LOADING OF ±DISP(%BASE,%INDEX,SCALE) MEMORY PARAMETER

static u64 GetDemBaseIndex8(struct Machine *m, u64 d, long b, long i, int z) {
  return Load8(ToHost(d + Get64(m->weg[b]) + (Get64(m->weg[i]) << z)));
}
static u64 GetDemBaseIndex16(struct Machine *m, u64 d, long b, long i, int z) {
  return Load16(ToHost(d + Get64(m->weg[b]) + (Get64(m->weg[i]) << z)));
}
static u64 GetDemBaseIndex32(struct Machine *m, u64 d, long b, long i, int z) {
  return Load32(ToHost(d + Get64(m->weg[b]) + (Get64(m->weg[i]) << z)));
}
static u64 GetDemBaseIndex64(struct Machine *m, u64 d, long b, long i, int z) {
  return Load64(ToHost(d + Get64(m->weg[b]) + (Get64(m->weg[i]) << z)));
}
typedef u64 (*getdembaseindex_f)(struct Machine *, u64, long, long, int);
static const getdembaseindex_f kGetDemBaseIndex[] = {
    GetDemBaseIndex8, GetDemBaseIndex16, GetDemBaseIndex32, GetDemBaseIndex64};

////////////////////////////////////////////////////////////////////////////////
// DEVIRTUALIZED STORING TO ±DISP(%BASE,%INDEX,SCALE) MEMORY PARAMETER

static void PutDemBaseIndex8(struct Machine *m, u64 d, long b, long i, int z,
                             u64 x) {
  Store8(ToHost(d + Get64(m->weg[b]) + (Get64(m->weg[i]) << z)), x);
}
static void PutDemBaseIndex16(struct Machine *m, u64 d, long b, long i, int z,
                              u64 x) {
  Store16(ToHost(d + Get64(m->weg[b]) + (Get64(m->weg[i]) << z)), x);
}
static void PutDemBaseIndex32(struct Machine *m, u64 d, long b, long i, int z,
                              u64 x) {
  Store32(ToHost(d + Get64(m->weg[b]) + (Get64(m->weg[i]) << z)), x);
}
static void PutDemBaseIndex64(struct Machine *m, u64 d, long b, long i, int z,
                              u64 x) {
  Store64(ToHost(d + Get64(m->weg[b]) + (Get64(m->weg[i]) << z)), x);
}
typedef void (*putdembaseindex_f)(struct Machine *, u64, long, long, int, u64);
static const putdembaseindex_f kPutDemBaseIndex[] = {
    PutDemBaseIndex8, PutDemBaseIndex16, PutDemBaseIndex32, PutDemBaseIndex64};

////////////////////////////////////////////////////////////////////////////////
// JIT DEBUGGING UTILITIES

static void ClobberEverythingExceptResult(struct Machine *m) {
#ifdef DEBUG
// clobber everything except result registers
#ifdef __x86_64__
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
#ifdef __aarch64__
  return Get32(p) == 0xd65f03c0;
#elif defined(__x86_64__)
  return *p == 0xc3;
#else
  __builtin_unreachable();
#endif
}

static long GetInstructionLength(u8 *p) {
#ifdef __aarch64__
  return 4;
#elif defined(__x86_64__)
  struct XedDecodedInst x;
  unassert(!DecodeInstruction(&x, p, 15, XED_MODE_LONG));
  return x.length;
#else
  __builtin_unreachable();
#endif
}

static long GetLengthOfFunctionBody(void *p) {
  long n = 0;
  for (;;) {
    if (IsRet((u8 *)p + n)) return n;
    n += GetInstructionLength((u8 *)p + n);
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

      case '$':  // cl
        Jitter(A, "s0a0= m", GetCl);
        break;

      case 'c':  // call
        AppendJitCall(m->path.jb, va_arg(va, void *));
        ClobberEverythingExceptResult(m);
        break;

      case 'm': {  // inline micro-op
        void *uop = va_arg(va, void *);
        AppendJit(m->path.jb, uop, GetLengthOfFunctionBody(uop));
        break;
      }

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
          if (!SibExists(rde)) {
            if (IsRipRelative(rde)) {
              Jitter(A, "a0i m", disp + m->ip, kGetDemAbs[log2sz]);
            } else {
              Jitter(A, "a2i a1i s0a0= m", RexbRm(rde), disp, kGetDem[log2sz]);
            }
          } else if (!SibHasBase(rde) &&  //
                     !SibHasIndex(rde)) {
            Jitter(A, "a0i m", disp, kGetDemAbs[log2sz]);
          } else if (SibHasBase(rde) &&  //
                     !SibHasIndex(rde)) {
            Jitter(A, "a2i a1i s0a0= m", RexbBase(rde), disp, kGetDem[log2sz]);
          } else if (!SibHasBase(rde) &&  //
                     SibHasIndex(rde)) {
            Jitter(A, "a3i a2i a1i s0a0= m", SibScale(rde),
                   Rexx(rde) << 3 | SibIndex(rde), disp, kGetDemIndex[log2sz]);
          } else {
            Jitter(A, "a4i a3i a2i a1i s0a0= m", SibScale(rde),
                   Rexx(rde) << 3 | SibIndex(rde), RexbBase(rde), disp,
                   kGetDemBaseIndex[log2sz]);
          }
        } else {
          Jitter(A, "a2i a1i s0a0= c", disp, rde, GetRegOrMem);
        }
        break;

      case 'D':  // PutRegOrMem(RexbRm, <pop>), e.g. c r0 D
        ItemsRequired(1);
        if (IsModrmRegister(rde)) {
          Jitter(A, "a2= a1i s0a0= m",
                 log2sz ? RexbRm(rde) : kByteReg[ByteRexb(rde)],
                 kPutReg[log2sz]);
        } else if (HasLinearMapping(m) && !Sego(rde)) {
          if (!SibExists(rde)) {
            if (IsRipRelative(rde)) {
              Jitter(A, "a1= a0i m", disp + m->ip, kPutDemAbs[log2sz]);
            } else {
              Jitter(A, "a3= a2i a1i s0a0= m", RexbRm(rde), disp,
                     kPutDem[log2sz]);
            }
          } else if (!SibHasBase(rde) &&  //
                     !SibHasIndex(rde)) {
            Jitter(A, "a1= a0i m", disp, kPutDemAbs[log2sz]);
          } else if (SibHasBase(rde) &&  //
                     !SibHasIndex(rde)) {
            Jitter(A, "a3= a2i a1i s0a0= m", RexbBase(rde), disp,
                   kPutDem[log2sz]);
          } else if (!SibHasBase(rde) &&  //
                     SibHasIndex(rde)) {
            Jitter(A, "a4= a3i a2i a1i s0a0= m", SibScale(rde),
                   Rexx(rde) << 3 | SibIndex(rde), disp, kPutDemIndex[log2sz]);
          } else {
            Jitter(A, "a5= a4i a3i a2i a1i s0a0= m", SibScale(rde),
                   Rexx(rde) << 3 | SibIndex(rde), RexbBase(rde), disp,
                   kPutDemBaseIndex[log2sz]);
          }
        } else {
          Jitter(A, "s0a0= a1i a2i a3= c", rde, disp, PutRegOrMem);
        }
        break;

      case 'E':  // r0 = ReadReg(RexbSrm)
        Jitter(A, "s0a0= a1i m", RexbSrm(rde), kGetReg[WordLog2(rde)]);
        break;

      case 'F':  // PutReg(RexbSrm, <pop>)
        ItemsRequired(1);
        Jitter(A, "a2= a1i s0a0= m", RexbSrm(rde), kPutReg[WordLog2(rde)]);
        break;

      case '+':  // save state
        k = JitterImpl(A, fmt, va, k, depth + 1);
        break;

      case 'z':  // force size
        switch ((c = fmt[k++])) {
          case '-':
            log2sz = RegLog2(rde);
            break;
          case '+':
            log2sz = WordLog2(rde);
            break;
          default:
            log2sz = CheckBelow(c - '0', 4);
            break;
        }
        rde &= ~006000000000;  // RegLog2
        rde |= log2sz << 034;
        continue;

      case '-':  // restore state
        unassert(depth);
        return k;

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
  if (!m->path.jb) return;
  va_start(va, fmt);
  JitterImpl(A, fmt, va, 0, 0);
  unassert(!i);
  va_end(va);
}
