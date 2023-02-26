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
#include <limits.h>

#include "blink/alu.h"
#include "blink/assert.h"
#include "blink/atomic.h"
#include "blink/builtin.h"
#include "blink/bus.h"
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/rde.h"
#include "blink/stats.h"
#include "blink/swap.h"
#include "blink/thread.h"

void LoadAluArgs(P) {
  if (IsMakingPath(m)) {
    if (IsModrmRegister(rde) && RexrReg(rde) == RexbRm(rde)) {
      Jitter(A, "A"        // res0 = GetReg(RexrReg)
                "r0a2="    // arg2 = res0
                "r0a1=");  // arg1 = res0
    } else {
      Jitter(A, "B"        // res0 = GetRegOrMem(RexbRm)
                "r0s1="    // sav1 = res0
                "A"        // res0 = GetReg(RexrReg)
                "r0a2="    // arg2 = res0
                "s1a1=");  // arg1 = sav1
    }
  }
}

void LoadAluFlipArgs(P) {
  if (IsMakingPath(m)) {
    if (IsModrmRegister(rde) && RexrReg(rde) == RexbRm(rde)) {
      Jitter(A, "A"        // res0 = GetReg(RexrReg)
                "r0a2="    // arg2 = res0
                "r0a1=");  // arg1 = res0
    } else {
      Jitter(A, "B"        // res0 = GetRegOrMem(RexbRm)
                "r0s1="    // sav1 = res0
                "A"        // res0 = GetReg(RexrReg)
                "s1a2="    // arg2 = sav1
                "r0a1=");  // arg1 = res0
    }
  }
}

void OpAlub(P) {
  u8 x, y, z, *p, *q;
  aluop_f f;
  f = kAlu[(Opcode(rde) & 070) >> 3][0];
  p = GetModrmRegisterBytePointerWrite1(A);
  q = ByteRexrReg(m, rde);
  if (Lock(rde)) {
    x = atomic_load_explicit((_Atomic(u8) *)p, memory_order_acquire);
    y = atomic_load_explicit((_Atomic(u8) *)q, memory_order_relaxed);
    y = Little8(y);
    do {
      z = Little8(f(m, Little8(x), y));
    } while (!atomic_compare_exchange_weak_explicit(
        (_Atomic(u8) *)p, &x, z, memory_order_release, memory_order_relaxed));
  } else {
    x = Load8(p);
    y = Get8(q);
    z = f(m, x, y);
    Store8(p, z);
  }
}

void OpAluw(P) {
  u8 *p, *q;
  aluop_f f;
  int t, flags;
  q = RegRexrReg(m, rde);
  t = (Opcode(rde) & 070) >> 3;
  f = kAlu[t][RegLog2(rde)];
  if (Rexw(rde)) {
    p = GetModrmRegisterWordPointerWrite8(A);
#if CAN_64BIT
    if (Lock(rde) && !((uintptr_t)p & 7)) {
      u64 x, y, z;
      x = atomic_load_explicit((_Atomic(u64) *)p, memory_order_acquire);
      y = atomic_load_explicit((_Atomic(u64) *)q, memory_order_relaxed);
      y = Little64(y);
      do {
        z = Little64(f(m, Little64(x), y));
      } while (!atomic_compare_exchange_weak_explicit((_Atomic(u64) *)p, &x, z,
                                                      memory_order_release,
                                                      memory_order_relaxed));
      return;
    }
#endif
    u64 x, y, z;
    /* The integrity of a bus lock is not affected by the alignment of
       the memory field. ──Intel V.3 §8.1.2.2 */
    if (Lock(rde)) {
      LockBus(p);
      x = Load64Unlocked(p);
      y = Get64(q);
      z = f(m, x, y);
      Store64Unlocked(p, z);
      UnlockBus(p);
    } else {
      x = Load64(p);
      y = Get64(q);
      z = f(m, x, y);
      Store64(p, z);
    }
  } else if (!Osz(rde)) {
    u32 x, y, z;
    p = GetModrmRegisterWordPointerWrite4(A);
    if (IsModrmRegister(rde)) {
      Put32(p + 4, 0);
    }
    if (Lock(rde) && !((uintptr_t)p & 3)) {
      x = atomic_load_explicit((_Atomic(u32) *)p, memory_order_acquire);
      y = atomic_load_explicit((_Atomic(u32) *)q, memory_order_relaxed);
      y = Little32(y);
      do {
        z = Little32(f(m, Little32(x), y));
      } while (!atomic_compare_exchange_weak_explicit((_Atomic(u32) *)p, &x, z,
                                                      memory_order_release,
                                                      memory_order_relaxed));
    } else {
      if (Lock(rde)) LockBus(p);
      x = Load32(p);
      y = Get32(q);
      z = f(m, x, y);
      Store32(p, z);
      if (Lock(rde)) UnlockBus(p);
    }
  } else {
    u16 x, y, z;
    p = GetModrmRegisterWordPointerWrite2(A);
    if (Lock(rde) && !((uintptr_t)p & 1)) {
      x = atomic_load_explicit((_Atomic(u16) *)p, memory_order_acquire);
      y = atomic_load_explicit((_Atomic(u16) *)q, memory_order_relaxed);
      y = Little16(y);
      do {
        z = Little16(f(m, Little16(x), y));
      } while (!atomic_compare_exchange_weak_explicit((_Atomic(u16) *)p, &x, z,
                                                      memory_order_release,
                                                      memory_order_relaxed));
    } else {
      if (Lock(rde)) LockBus(p);
      x = Load16(p);
      y = Get16(q);
      z = f(m, x, y);
      Store16(p, z);
      if (Lock(rde)) UnlockBus(p);
    }
  }
  if (IsMakingPath(m) && !Lock(rde)) {
    STATISTIC(++alu_ops);
    flags = GetNeededFlags(m, m->ip, CF | ZF | SF | OF | AF | PF);
    if (t == ALU_XOR &&          //
        RegLog2(rde) >= 2 &&     //
        IsModrmRegister(rde) &&  //
        RexrReg(rde) == RexbRm(rde)) {
      if (flags) {
        Jitter(A,
               "a1i"  // arg1 = register index
               "m",   // call micro-op
               RexrReg(rde), ZeroRegFlags);
      } else {
        Jitter(A,
               "s0a1="  // arg1 = machine
               "a0i"    // arg0 = zero
               "m",     // call micro-op
               (u64)0, kPutReg64[RexrReg(rde)]);
      }
    } else {
      LoadAluArgs(A);
      switch (flags) {
        case 0:
          STATISTIC(++alu_unflagged);
          if (GetFlagDeps(rde)) Jitter(A, "q");  // arg0 = machine
          Jitter(A,
                 "m"     // call micro-op
                 "r0D",  // PutRegOrMem(RexbRm, res0)
                 kJustAlu[t]);
          break;
        case CF:
        case ZF:
        case CF | ZF:
          STATISTIC(++alu_simplified);
          Jitter(A,
                 "q"     // arg0 = machine
                 "m"     // call micro-op
                 "r0D",  // PutRegOrMem(RexbRm, res0)
                 kAluFast[t][RegLog2(rde)]);
          break;
        default:
          Jitter(A,
                 "q"     // arg0 = machine
                 "c"     // call function
                 "r0D",  // PutRegOrMem(RexbRm, res0)
                 f);
          break;
      }
    }
  }
}
