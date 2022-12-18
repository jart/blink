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
#include "blink/assert.h"
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/lock.h"
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
  if (Lock(rde)) LOCK(&m->system->lock_lock);
  Write8(p, f(m, Load8(p), Get8(q)));
  if (Lock(rde)) UNLOCK(&m->system->lock_lock);
}

void OpAluw(P) {
  u8 *p, *q;
  aluop_f f;
  q = RegRexrReg(m, rde);
  f = kAlu[(Opcode(rde) & 070) >> 3][RegLog2(rde)];
  if (Rexw(rde)) {
    p = GetModrmRegisterWordPointerWrite8(A);
#if LONG_BIT == 64
    if (Lock(rde) && !((intptr_t)p & 7)) {
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
    x = Load64(p);
    y = Get64(q);
    z = f(m, x, y);
    Store64(p, z);
  } else if (!Osz(rde)) {
    u32 x, y, z;
    p = GetModrmRegisterWordPointerWrite4(A);
    if (IsModrmRegister(rde)) {
      Put32(p + 4, 0);
    }
    if (Lock(rde) && !((intptr_t)p & 3)) {
      x = atomic_load_explicit((_Atomic(u32) *)p, memory_order_acquire);
      y = atomic_load_explicit((_Atomic(u32) *)q, memory_order_relaxed);
      y = Little32(y);
      do {
        z = Little32(f(m, Little32(x), y));
      } while (!atomic_compare_exchange_weak_explicit((_Atomic(u32) *)p, &x, z,
                                                      memory_order_release,
                                                      memory_order_relaxed));
    } else {
      x = Load32(p);
      y = Get32(q);
      z = f(m, x, y);
      Store32(p, z);
    }
  } else {
    u16 x, y, z;
    if (Lock(rde)) LOCK(&m->system->lock_lock);
    p = GetModrmRegisterWordPointerWrite2(A);
    x = Load16(p);
    y = Get16(q);
    z = f(m, x, y);
    Store16(p, z);
    if (Lock(rde)) UNLOCK(&m->system->lock_lock);
  }
  if (IsMakingPath(m) && !Lock(rde)) {
    Jitter(A, "B r0s1= A r0a2= s1a1=");
    if (CanSkipFlags(m, CF | ZF | SF | OF | AF | PF)) {
      if (GetFlagDeps(rde)) Jitter(A, "s0a0=");
      Jitter(A, "m r0 D", kJustAlu[(Opcode(rde) & 070) >> 3]);
    } else {
      Jitter(A, "s0a0= c r0 D", f);
    }
  }
}
