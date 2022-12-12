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
#include "blink/endian.h"
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/mop.h"

void OpXchgGbEb(P) {
  u8 *q;
  q = ByteRexrReg(m, rde);
  if (!IsModrmRegister(rde)) {
    *q =
        atomic_exchange_explicit((atomic_uchar *)ComputeReserveAddressWrite1(A),
                                 *q, memory_order_acq_rel);
  } else {
    u8 *p;
    u8 x, y;
    p = ByteRexbRm(m, rde);
    x = Get8(q);
    y = Load8(p);
    Put8(q, y);
    Store8(p, x);
  }
}

void OpXchgGvqpEvqp(P) {
  u8 *q = RegRexrReg(m, rde);
  u8 *p = GetModrmRegisterWordPointerWriteOszRexw(A);
  if (Rexw(rde)) {
    if (!IsModrmRegister(rde) && !((intptr_t)p & 7)) {
      atomic_store_explicit(
          (atomic_ulong *)q,
          atomic_exchange_explicit(
              (atomic_ulong *)p,
              atomic_load_explicit((atomic_ulong *)q, memory_order_relaxed),
              memory_order_acq_rel),
          memory_order_relaxed);
    } else {
      u64 x, y;
      x = Read64(q);
      y = Load64(p);
      Write64(q, y);
      Store64(p, x);
    }
  } else if (!Osz(rde)) {
    if (!IsModrmRegister(rde) && !((intptr_t)p & 3)) {
      atomic_store_explicit(
          (atomic_uint *)q,
          atomic_exchange_explicit(
              (atomic_uint *)p,
              atomic_load_explicit((atomic_uint *)q, memory_order_relaxed),
              memory_order_acq_rel),
          memory_order_relaxed);
    } else {
      u32 x, y;
      x = Read32(q);
      y = Load32(p);
      Write32(q, y);
      Store32(p, x);
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
      x = Read16(q);
      y = Load16(p);
      Write16(q, y);
      Store16(p, x);
    }
    Write16(q + 4, 0);
  }
}
