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

void OpXchgGbEb(struct Machine *m, u32 rde) {
  u8 *q;
  q = ByteRexrReg(m, rde);
  if (!IsModrmRegister(rde)) {
#if !defined(__riscv) && !defined(__MICROBLAZE__)
    *q = atomic_exchange((atomic_uchar *)ComputeReserveAddressWrite1(m, rde),
                         *q);
#else
    OpUd(m, rde);
#endif
  } else {
    u8 *p;
    u8 x, y;
    p = ByteRexbRm(m, rde);
    x = Read8(q);
    y = Read8(p);
    Write8(q, y);
    Write8(p, x);
  }
}

void OpXchgGvqpEvqp(struct Machine *m, u32 rde) {
  u8 *q = RegRexrReg(m, rde);
  u8 *p = GetModrmRegisterWordPointerWriteOszRexw(m, rde);
  if (Rexw(rde)) {
    if (!IsModrmRegister(rde) && !((intptr_t)p & 7)) {
#if LONG_BIT == 64
      atomic_store_explicit(
          (atomic_ulong *)q,
          atomic_exchange(
              (atomic_ulong *)p,
              atomic_load_explicit((atomic_ulong *)q, memory_order_relaxed)),
          memory_order_relaxed);
#else
      OpUd(m, rde);
#endif
    } else {
      u64 x, y;
      x = Read64(q);
      y = Read64(p);
      Write64(q, y);
      Write64(p, x);
    }
  } else if (!Osz(rde)) {
    if (!IsModrmRegister(rde) && !((intptr_t)p & 3)) {
      atomic_store_explicit(
          (atomic_uint *)q,
          atomic_exchange(
              (atomic_uint *)p,
              atomic_load_explicit((atomic_uint *)q, memory_order_relaxed)),
          memory_order_relaxed);
    } else {
      u32 x, y;
      x = Read32(q);
      y = Read32(p);
      Write32(q, y);
      Write32(p, x);
    }
    if (IsModrmRegister(rde)) {
      Write32(p + 4, 0);
    }
  } else {
    u16 x, y;
    unassert(IsModrmRegister(rde));
    x = Read16(q);
    y = Read16(p);
    Write16(q, y);
    Write16(p, x);
  }
}
