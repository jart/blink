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
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/swap.h"
#include "blink/thread.h"

void OpXaddEbGb(P) {
  u8 x, y, z, *p, *q;
  p = GetModrmRegisterBytePointerWrite1(A);
  q = ByteRexrReg(m, rde);
  if (Lock(rde)) {
    x = atomic_load_explicit((_Atomic(u8) *)p, memory_order_acquire);
    y = atomic_load_explicit((_Atomic(u8) *)q, memory_order_relaxed);
    y = Little8(y);
    do {
      atomic_store_explicit((_Atomic(u8) *)q, x, memory_order_relaxed);
      z = Little8(Add8(m, Little8(x), y));
    } while (!atomic_compare_exchange_weak_explicit(
        (_Atomic(u8) *)p, &x, z, memory_order_release, memory_order_acquire));
  } else {
    x = Load8(p);
    y = Get8(q);
    z = Add8(m, x, y);
    Put8(q, x);
    Store8(p, z);
  }
}

void OpXaddEvqpGvqp(P) {
  u8 *p, *q;
  q = RegRexrReg(m, rde);
  p = GetModrmRegisterWordPointerWriteOszRexw(A);
  if (Rexw(rde)) {
    u64 x, y, z;
#if CAN_64BIT
    if (Lock(rde) && !((uintptr_t)p & 7)) {
      x = atomic_load_explicit((_Atomic(u64) *)p, memory_order_acquire);
      y = atomic_load_explicit((_Atomic(u64) *)q, memory_order_relaxed);
      y = Little64(y);
      do {
        atomic_store_explicit((_Atomic(u64) *)q, x, memory_order_relaxed);
        z = Little64(Add64(m, Little64(x), y));
      } while (!atomic_compare_exchange_weak_explicit((_Atomic(u64) *)p, &x, z,
                                                      memory_order_release,
                                                      memory_order_acquire));
      return;
    }
#endif
    if (Lock(rde)) {
      LockBus(p);
      x = Load64Unlocked(p);
      y = Get64(q);
      z = Add64(m, x, y);
      Put64(q, x);
      Store64Unlocked(p, z);
      UnlockBus(p);
    } else {
      x = Load64(p);
      y = Get64(q);
      z = Add64(m, x, y);
      Put64(q, x);
      Store64(p, z);
    }
  } else if (!Osz(rde)) {
    u32 x, y, z;
    if (Lock(rde) && !((uintptr_t)p & 3)) {
      x = atomic_load_explicit((_Atomic(u32) *)p, memory_order_acquire);
      y = atomic_load_explicit((_Atomic(u32) *)q, memory_order_relaxed);
      y = Little32(y);
      do {
        atomic_store_explicit((_Atomic(u32) *)q, x, memory_order_relaxed);
        z = Little32(Add32(m, Little32(x), y));
      } while (!atomic_compare_exchange_weak_explicit((_Atomic(u32) *)p, &x, z,
                                                      memory_order_release,
                                                      memory_order_acquire));
    } else {
      if (Lock(rde)) LockBus(p);
      x = Load32(p);
      y = Get32(q);
      z = Add32(m, x, y);
      Put32(q, x);
      Store32(p, z);
      if (Lock(rde)) UnlockBus(p);
    }
    Put32(q + 4, 0);
    if (IsModrmRegister(rde)) {
      Put32(p + 4, 0);
    }
  } else {
    u16 x, y, z;
    if (Lock(rde) && !((uintptr_t)p & 1)) {
      x = atomic_load_explicit((_Atomic(u16) *)p, memory_order_acquire);
      y = atomic_load_explicit((_Atomic(u16) *)q, memory_order_relaxed);
      y = Little16(y);
      do {
        atomic_store_explicit((_Atomic(u16) *)q, x, memory_order_relaxed);
        z = Little16(Add16(m, Little16(x), y));
      } while (!atomic_compare_exchange_weak_explicit((_Atomic(u16) *)p, &x, z,
                                                      memory_order_release,
                                                      memory_order_acquire));
    } else {
      if (Lock(rde)) LockBus(p);
      x = Load16(p);
      y = Get16(q);
      z = Add16(m, x, y);
      Put16(q, x);
      Store16(p, z);
      if (Lock(rde)) UnlockBus(p);
    }
  }
}
