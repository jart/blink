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

#include "blink/assert.h"
#include "blink/builtin.h"
#include "blink/endian.h"
#include "blink/lock.h"
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/mop.h"

void OpXchgGbEb(P) {
  u8 *p, *q, x, y;
  q = ByteRexrReg(m, rde);
  /* When a memory operand is used with the XCHG instruction, the
     processor's LOCK signal is automatically asserted. ──Intel V.1
     §7.3.1.2 */
  if (!IsModrmRegister(rde)) {
    p = ComputeReserveAddressWrite1(A);
    *q = atomic_exchange_explicit((atomic_uchar *)p, *q, memory_order_acq_rel);
  } else {
    p = ByteRexbRm(m, rde);
    x = Get8(q);
    y = Get8(p);
    Put8(q, y);
    Put8(p, x);
  }
}

void OpXchgGvqpEvqp(P) {
  u8 *q = RegRexrReg(m, rde);
  u8 *p = GetModrmRegisterWordPointerWriteOszRexw(A);
  if (Rexw(rde)) {
#if CAN_64BIT
    if (!IsModrmRegister(rde) && !((intptr_t)p & 7)) {
      atomic_store_explicit(
          (_Atomic(u64) *)q,
          atomic_exchange_explicit(
              (_Atomic(u64) *)p,
              atomic_load_explicit((_Atomic(u64) *)q, memory_order_relaxed),
              memory_order_acq_rel),
          memory_order_relaxed);
      return;
    }
#endif
    u64 x, y;
    if (IsModrmRegister(rde)) {
      x = Get64(q);
      y = Get64(p);
      Put64(q, y);
      Put64(p, x);
    } else {
      LockBus(p);
      y = Load64Unlocked(p);
      x = Get64(q);
      Put64(q, y);
      Store64Unlocked(p, x);
      UnlockBus(p);
    }
  } else if (!Osz(rde)) {
    if (!IsModrmRegister(rde) && !((intptr_t)p & 3)) {
      atomic_store_explicit(
          (_Atomic(u32) *)q,
          atomic_exchange_explicit(
              (_Atomic(u32) *)p,
              atomic_load_explicit((_Atomic(u32) *)q, memory_order_relaxed),
              memory_order_acq_rel),
          memory_order_relaxed);
    } else {
      u32 x, y;
      if (!IsModrmRegister(rde)) LockBus(p);
      y = Load32(p);
      x = Read32(q);
      Write32(q, y);
      Store32(p, x);
      if (!IsModrmRegister(rde)) UnlockBus(p);
    }
    Write32(q + 4, 0);
    if (IsModrmRegister(rde)) {
      Write32(p + 4, 0);
    }
  } else {
    if (!IsModrmRegister(rde) && !((intptr_t)p & 1)) {
      atomic_store_explicit(
          (_Atomic(u16) *)q,
          atomic_exchange_explicit(
              (_Atomic(u16) *)p,
              atomic_load_explicit((_Atomic(u16) *)q, memory_order_relaxed),
              memory_order_acq_rel),
          memory_order_relaxed);
    } else {
      u16 x, y;
      if (!IsModrmRegister(rde)) LockBus(p);
      y = Load16(p);
      x = Read16(q);
      Write16(q, y);
      Store16(p, x);
      if (!IsModrmRegister(rde)) UnlockBus(p);
    }
  }
}
