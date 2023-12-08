/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
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
#include "blink/assert.h"
#include "blink/builtin.h"
#include "blink/bus.h"
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/thread.h"

void OpCmpxchgEbAlGb(P) {
  u8 *p = GetModrmRegisterBytePointerWrite1(A);
  u8 *q = ByteRexrReg(m, rde);
  u8 ax = atomic_load_explicit((_Atomic(u8) *)m->ax, memory_order_relaxed);
  atomic_compare_exchange_strong_explicit(
      (_Atomic(u8) *)p, &ax,
      atomic_load_explicit((_Atomic(u8) *)q, memory_order_relaxed),
      memory_order_acq_rel, memory_order_acquire);
  Sub8(m, Get8(m->ax), Little8(ax));
  atomic_store_explicit((_Atomic(u8) *)m->ax, ax, memory_order_relaxed);
}

static void LockCmpxchgMem32(struct Machine *m, u64 rde, u8 *p) {
  u8 *q;
  int c;
  u32 x, z;
  bool didit;
  q = RegRexrReg(m, rde);
  if (!((uintptr_t)p & 3)) {
    x = atomic_load_explicit((_Atomic(u32) *)m->ax, memory_order_relaxed);
    didit = atomic_compare_exchange_strong_explicit(
        (_Atomic(u32) *)p, &x,
        atomic_load_explicit((_Atomic(u32) *)q, memory_order_relaxed),
        memory_order_acq_rel, memory_order_acquire);
    z = Get32(m->ax) - Little32(x);
    c = Get32(m->ax) < z;
    m->flags = (m->flags & ~(CF | ZF)) | c << FLAGS_CF | !z << FLAGS_ZF;
    if (!didit) Put64(m->ax, Little32(x));
  } else {
    LockBus(p);
    x = Load32(p);
    Sub32(m, Get32(m->ax), x);
    if ((didit = x == Get32(m->ax))) {
      Store32(p, Get32(q));
    } else {
      Put64(m->ax, x);
    }
    UnlockBus(p);
  }
}

static void Cmpxchg(struct Machine *m, u64 rde, u8 *p) {
  u8 *q;
  bool didit;
  q = RegRexrReg(m, rde);
  if (Rexw(rde)) {
    u64 x;
#if CAN_64BIT
    if (Lock(rde) && !((uintptr_t)p & 7)) {
      x = atomic_load_explicit((_Atomic(u64) *)m->ax, memory_order_relaxed);
      atomic_compare_exchange_strong_explicit(
          (_Atomic(u64) *)p, &x,
          atomic_load_explicit((_Atomic(u64) *)q, memory_order_relaxed),
          memory_order_acq_rel, memory_order_acquire);
      Sub64(m, Get64(m->ax), Little64(x));
      atomic_store_explicit((_Atomic(u64) *)m->ax, x, memory_order_relaxed);
      return;
    }
#endif
    if (Lock(rde)) {
      LockBus(p);
      x = Load64Unlocked(p);
      Sub64(m, Get64(m->ax), x);
      if ((didit = x == Get64(m->ax))) {
        Store64Unlocked(p, Get64(q));
      } else {
        Put64(m->ax, x);
      }
      UnlockBus(p);
    } else {
      x = Load64(p);
      Sub64(m, Get64(m->ax), x);
      if ((didit = x == Get64(m->ax))) {
        Store64(p, Get64(q));
      } else {
        Put64(m->ax, x);
      }
    }
  } else if (!Osz(rde)) {
    if (Lock(rde) && !((uintptr_t)p & 3)) {
      u32 ax =
          atomic_load_explicit((_Atomic(u32) *)m->ax, memory_order_relaxed);
      didit = atomic_compare_exchange_strong_explicit(
          (_Atomic(u32) *)p, &ax,
          atomic_load_explicit((_Atomic(u32) *)q, memory_order_relaxed),
          memory_order_acq_rel, memory_order_acquire);
      Sub32(m, Get32(m->ax), Little32(ax));
      if (!didit) Put64(m->ax, Little32(ax));
    } else {
      if (Lock(rde)) LockBus(p);
      u32 x = Load32(p);
      Sub32(m, Get32(m->ax), x);
      if ((didit = x == Get32(m->ax))) {
        Store32(p, Get32(q));
      } else {
        Put64(m->ax, x);
      }
      if (Lock(rde)) UnlockBus(p);
    }
    if (IsModrmRegister(rde)) {
      Put32(p + 4, 0);
    }
  } else {
    if (Lock(rde) && !((uintptr_t)p & 1)) {
      u16 ax =
          atomic_load_explicit((_Atomic(u16) *)m->ax, memory_order_relaxed);
      atomic_compare_exchange_strong_explicit(
          (_Atomic(u16) *)p, &ax,
          atomic_load_explicit((_Atomic(u16) *)q, memory_order_relaxed),
          memory_order_acq_rel, memory_order_acquire);
      Sub16(m, Get16(m->ax), Little16(ax));
      atomic_store_explicit((_Atomic(u16) *)m->ax, ax, memory_order_relaxed);
    } else {
      if (Lock(rde)) LockBus(p);
      u16 x = Load16(p);
      Sub16(m, Get16(m->ax), x);
      if ((didit = x == Get16(m->ax))) {
        Store16(p, Get16(q));
      } else {
        Put16(m->ax, x);
      }
      if (Lock(rde)) UnlockBus(p);
    }
  }
}

void OpCmpxchgEvqpRaxGvqp(P) {
  Cmpxchg(m, rde, GetModrmRegisterWordPointerWriteOszRexw(A));
  if (IsMakingPath(m)) {
    Jitter(A,
           "P"      // res0 = GetRegOrMemPointer(RexbRm)
           "r0a2="  // arg2 = res0
           "a1i"    // arg1 = rde
           "q"      // arg0 = m
           "c",     // call function
           rde,
           (Lock(rde) && !IsModrmRegister(rde) && !Rexw(rde) && !Osz(rde) &&
            !(GetNeededFlags(m, m->ip, CF | ZF | SF | OF | AF | PF) &
              (SF | OF | AF | PF)))
               ? LockCmpxchgMem32
               : Cmpxchg);
  }
}
