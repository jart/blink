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
#include <stdatomic.h>

#include "blink/alu.h"
#include "blink/alu.inc"
#include "blink/assert.h"
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/mop.h"
#include "blink/swap.h"

void OpAlub(P) {
  u8 *p, *q;
  aluop_f f;
  f = kAlu[(Opcode(rde) & 070) >> 3][0];
  p = GetModrmRegisterBytePointerWrite1(A);
  q = ByteRexrReg(m, rde);
  if (!Lock(rde)) {
    Store8(p, f(m, Load8(p), Get8(q)));
  } else {
#if !defined(__riscv) && !defined(__MICROBLAZE__)
    u8 x, y, z;
    x = Load8(p);
    y = Get8(q);
    do {
      z = f(m, x, y);
    } while (!atomic_compare_exchange_weak_explicit(
        (atomic_uchar *)p, &x, z, memory_order_release, memory_order_relaxed));
#else
    LOGF("can't %s on this platform", "lock alub");
    OpUdImpl(m);
#endif
  }
}

static void OpAluwRegAdd64(struct Machine *m, long rexb, long rexr) {
  Put64(m->weg[rexb],
        FastAdd64(Get64(m->weg[rexb]), Get64(m->weg[rexr]), &m->flags));
}
static void OpAluwRegOr64(struct Machine *m, long rexb, long rexr) {
  Put64(m->weg[rexb],
        FastOr64(Get64(m->weg[rexb]), Get64(m->weg[rexr]), &m->flags));
}
static void OpAluwRegAdc64(struct Machine *m, long rexb, long rexr) {
  Put64(m->weg[rexb],
        FastAdc64(Get64(m->weg[rexb]), Get64(m->weg[rexr]), &m->flags));
}
static void OpAluwRegSbb64(struct Machine *m, long rexb, long rexr) {
  Put64(m->weg[rexb],
        FastSbb64(Get64(m->weg[rexb]), Get64(m->weg[rexr]), &m->flags));
}
static void OpAluwRegAnd64(struct Machine *m, long rexb, long rexr) {
  Put64(m->weg[rexb],
        FastAnd64(Get64(m->weg[rexb]), Get64(m->weg[rexr]), &m->flags));
}
static void OpAluwRegSub64(struct Machine *m, long rexb, long rexr) {
  Put64(m->weg[rexb],
        FastSub64(Get64(m->weg[rexb]), Get64(m->weg[rexr]), &m->flags));
}
static void OpAluwRegXor64(struct Machine *m, long rexb, long rexr) {
  Put64(m->weg[rexb],
        FastXor64(Get64(m->weg[rexb]), Get64(m->weg[rexr]), &m->flags));
}
typedef void (*alureg_f)(struct Machine *, long, long);
const alureg_f kAluReg64[] = {
    OpAluwRegAdd64,  //
    OpAluwRegOr64,   //
    OpAluwRegAdc64,  //
    OpAluwRegSbb64,  //
    OpAluwRegAnd64,  //
    OpAluwRegSub64,  //
    OpAluwRegXor64,  //
    OpAluwRegSub64,  //
};

static void OpAluwRegAdd32(struct Machine *m, long rexb, long rexr) {
  Put64(m->weg[rexb],
        FastAdd32(Get32(m->weg[rexb]), Get32(m->weg[rexr]), &m->flags));
}
static void OpAluwRegOr32(struct Machine *m, long rexb, long rexr) {
  Put64(m->weg[rexb],
        FastOr32(Get32(m->weg[rexb]), Get32(m->weg[rexr]), &m->flags));
}
static void OpAluwRegAdc32(struct Machine *m, long rexb, long rexr) {
  Put64(m->weg[rexb],
        FastAdc32(Get32(m->weg[rexb]), Get32(m->weg[rexr]), &m->flags));
}
static void OpAluwRegSbb32(struct Machine *m, long rexb, long rexr) {
  Put64(m->weg[rexb],
        FastSbb32(Get32(m->weg[rexb]), Get32(m->weg[rexr]), &m->flags));
}
static void OpAluwRegAnd32(struct Machine *m, long rexb, long rexr) {
  Put64(m->weg[rexb],
        FastAnd32(Get32(m->weg[rexb]), Get32(m->weg[rexr]), &m->flags));
}
static void OpAluwRegSub32(struct Machine *m, long rexb, long rexr) {
  Put64(m->weg[rexb],
        FastSub32(Get32(m->weg[rexb]), Get32(m->weg[rexr]), &m->flags));
}
static void OpAluwRegXor32(struct Machine *m, long rexb, long rexr) {
  Put64(m->weg[rexb],
        FastXor32(Get32(m->weg[rexb]), Get32(m->weg[rexr]), &m->flags));
}
const alureg_f kAluReg32[] = {
    OpAluwRegAdd32,  //
    OpAluwRegOr32,   //
    OpAluwRegAdc32,  //
    OpAluwRegSbb32,  //
    OpAluwRegAnd32,  //
    OpAluwRegSub32,  //
    OpAluwRegXor32,  //
    OpAluwRegSub32,  //
};

void OpAluw(P) {
  u8 *p, *q;
  aluop_f f;
  q = RegRexrReg(m, rde);
  f = kAlu[(Opcode(rde) & 070) >> 3][RegLog2(rde)];

  // test for clear register idiom
  if (IsModrmRegister(rde) &&        //
      (f == Xor32 || f == Xor64) &&  //
      RegRexbRm(m, rde) == RegRexrReg(m, rde)) {
    FastZeroify(m, RexbRm(rde));
    Jitter(A, "a1i m", RexbRm(rde), FastZeroify);
    return;
  }

  if (Rexw(rde)) {
    p = GetModrmRegisterWordPointerWrite8(A);
    if (Lock(rde)) {
#if LONG_BIT == 64
      if (!((intptr_t)p & 7)) {
        u64 x, y, z;
        x = atomic_load_explicit((_Atomic(u64) *)p, memory_order_acquire);
        y = atomic_load_explicit((_Atomic(u64) *)q, memory_order_relaxed);
        y = Little64(y);
        do {
          z = Little64(f(m, Little64(x), y));
        } while (!atomic_compare_exchange_weak_explicit((_Atomic(u64) *)p, &x,
                                                        z, memory_order_release,
                                                        memory_order_relaxed));
      } else {
        LOGF("can't %s misaligned address", "lock aluq");
        OpUdImpl(m);
      }
#else
      LOGF("can't %s on this platform", "lock aluq");
      OpUdImpl(m);
#endif
      return;  // can't jit lock
    } else {
      u64 x, y, z;
      x = Load64(p);
      y = Get64(q);
      z = f(m, x, y);
      Store64(p, z);
      if (IsModrmRegister(rde)) {
        Jitter(A, "a1i a2i s0a0= c", RexbRm(rde), RexrReg(rde),
               kAluReg64[(Opcode(rde) & 070) >> 3]);
        return;
      }
    }
  } else if (!Osz(rde)) {
    u32 x, y, z;
    p = GetModrmRegisterWordPointerWrite4(A);
    if (Lock(rde)) {
      if (!((intptr_t)p & 3)) {
        x = atomic_load_explicit((_Atomic(u32) *)p, memory_order_acquire);
        y = atomic_load_explicit((_Atomic(u32) *)q, memory_order_relaxed);
        y = Little32(y);
        do {
          z = Little32(f(m, Little32(x), y));
        } while (!atomic_compare_exchange_weak_explicit((_Atomic(u32) *)p, &x,
                                                        z, memory_order_release,
                                                        memory_order_relaxed));
      } else {
        LOGF("can't %s misaligned address", "lock alul");
        OpUdImpl(m);
      }
      return;  // can't jit lock
    } else {
      x = Load32(p);
      y = Get32(q);
      z = f(m, x, y);
      Store32(p, z);
    }
    if (IsModrmRegister(rde)) {
      Put32(p + 4, 0);
      if (IsModrmRegister(rde)) {
        Jitter(A, "a1i a2i c", RexbRm(rde), RexrReg(rde),
               kAluReg32[(Opcode(rde) & 070) >> 3]);
      }
      return;
    }
  } else {
    u16 x, y, z;
    unassert(!Lock(rde));
    p = GetModrmRegisterWordPointerWrite2(A);
    x = Load16(p);
    y = Get16(q);
    z = f(m, x, y);
    Store16(p, z);
  }
  Jitter(A, "B r0s1= A r0a2= s1a1= s0a0= c r0 D", f);
}
