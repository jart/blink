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
#include "blink/alu.h"
#include "blink/flags.h"
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/mop.h"
#include "blink/stats.h"

static void AluiRo(P, const aluop_f ops[4], const aluop_f fast[4]) {
  ops[RegLog2(rde)](m, ReadRegisterOrMemoryBW(rde, GetModrmReadBW(A)), uimm0);
  if (IsMakingPath(m)) {
    STATISTIC(++alu_ops);
    switch (GetNeededFlags(m, m->ip, CF | ZF | SF | OF | AF | PF)) {
      case 0:
      case CF:
      case ZF:
      case CF | ZF:
        STATISTIC(++alu_simplified);
        Jitter(A,
               "B"      // res0 = GetRegOrMem(RexbRm)
               "a2i"    // arg2 = uimm0
               "r0a1="  // arg1 = res0
               "q"      // arg0 = sav0 (machine)
               "m",     // call micro-op
               uimm0, fast[RegLog2(rde)]);
        break;
      default:
        Jitter(A,
               "B"      // res0 = GetRegOrMem(RexbRm)
               "a2i"    // arg2 = uimm0
               "r0a1="  // arg1 = res0
               "q"      // arg0 = sav0 (machine)
               "c",     // call function
               uimm0, ops[RegLog2(rde)]);
        break;
    }
  }
}

static void AluiUnlocked(P, u8 *p, aluop_f op) {
  WriteRegisterOrMemoryBW(rde, p, op(m, ReadRegisterOrMemoryBW(rde, p), uimm0));
  if (IsMakingPath(m)) {
    STATISTIC(++alu_ops);
    Jitter(A,
           "B"      // res0 = GetRegOrMem(RexbRm)
           "r0a1="  // arg1 = res0
           "a2i",   // arg2 = uimm0
           uimm0);
    switch (GetNeededFlags(m, m->ip, CF | ZF | SF | OF | AF | PF)) {
      case 0:
        STATISTIC(++alu_unflagged);
        if (GetFlagDeps(rde)) {
          Jitter(A, "q");  // arg0 = sav0 (machine)
        }
        Jitter(A,
               "m"     // call micro-op
               "r0D",  // PutRegOrMem(RexbRm, res0)
               kJustAlu[ModrmReg(rde)]);
        break;
      case CF:
      case ZF:
      case CF | ZF:
        STATISTIC(++alu_simplified);
        Jitter(A,
               "q"     // arg0 = sav0 (machine)
               "m"     // call micro-op
               "r0D",  // PutRegOrMem(RexbRm, res0)
               kAluFast[ModrmReg(rde)][RegLog2(rde)]);
        break;
      default:
        Jitter(A,
               "q"     // arg0 = sav0 (machine)
               "c"     // call function
               "r0D",  // PutRegOrMem(RexbRm, res0)
               op);
        break;
    }
  }
}

static void AluiLocked(P, u8 *p, aluop_f op) {
  switch (RegLog2(rde)) {
    case 3:
#if LONG_BIT >= 64
      if (!((intptr_t)p & 7)) {
        u64 x, z;
        x = atomic_load_explicit((_Atomic(u64) *)p, memory_order_acquire);
        do {
          z = Little64(op(m, Little64(x), uimm0));
        } while (!atomic_compare_exchange_weak_explicit((_Atomic(u64) *)p, &x,
                                                        z, memory_order_release,
                                                        memory_order_relaxed));
        return;
      }
#endif
      LockBus(p);
      Store64Unlocked(p, op(m, Load64Unlocked(p), uimm0));
      UnlockBus(p);
      break;
    case 2:
      if (!((intptr_t)p & 3)) {
        u32 x, z;
        x = atomic_load_explicit((_Atomic(u32) *)p, memory_order_acquire);
        do {
          z = Little32(op(m, Little32(x), uimm0));
        } while (!atomic_compare_exchange_weak_explicit((_Atomic(u32) *)p, &x,
                                                        z, memory_order_release,
                                                        memory_order_relaxed));
        return;
      }
      LockBus(p);
      Store32(p, op(m, Load32(p), uimm0));
      UnlockBus(p);
      break;
    case 1:
      if (!((intptr_t)p & 1)) {
        u16 x, z;
        x = atomic_load_explicit((_Atomic(u16) *)p, memory_order_acquire);
        do {
          z = Little16(op(m, Little16(x), uimm0));
        } while (!atomic_compare_exchange_weak_explicit((_Atomic(u16) *)p, &x,
                                                        z, memory_order_release,
                                                        memory_order_relaxed));
        return;
      }
      LockBus(p);
      Store16(p, op(m, Load16(p), uimm0));
      UnlockBus(p);
      break;
    case 0: {
      u8 x, z;
      x = atomic_load_explicit((_Atomic(u8) *)p, memory_order_acquire);
      do {
        z = Little8(op(m, Little8(x), uimm0));
      } while (!atomic_compare_exchange_weak_explicit(
          (_Atomic(u8) *)p, &x, z, memory_order_release, memory_order_relaxed));
      break;
    }
    default:
      __builtin_unreachable();
  }
}

static void Alui(P) {
  u8 *p;
  aluop_f op;
  p = GetModrmWriteBW(A);
  op = kAlu[ModrmReg(rde)][RegLog2(rde)];
  if (Lock(rde) && !IsModrmRegister(rde)) {
    AluiLocked(A, p, op);
  } else {
    AluiUnlocked(A, p, op);
  }
}

void OpAlui(P) {
  if (ModrmReg(rde) == ALU_CMP) {
    if (IsMakingPath(m) && FuseBranchCmp(A, true)) {
      kAlu[ALU_SUB][RegLog2(rde)](
          m, ReadRegisterOrMemoryBW(rde, GetModrmReadBW(A)), uimm0);
    } else {
      AluiRo(A, kAlu[ALU_SUB], kAluFast[ALU_SUB]);
    }
  } else {
    Alui(A);
  }
}

void OpTest(P) {
  AluiRo(A, kAlu[ALU_AND], kAluFast[ALU_AND]);
}
