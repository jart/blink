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
#include "blink/flags.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/modrm.h"

void OpCmpxchgEbAlGb(struct Machine *m, uint32_t rde) {
  bool didit;
  if (!IsModrmRegister(rde)) {
#if !defined(__riscv) && !defined(__MICROBLAZE__)
    didit = atomic_compare_exchange_strong(
        (atomic_uchar *)ComputeReserveAddressWrite1(m, rde), m->ax,
        *ByteRexrReg(m, rde));
#else
    OpUd(m, rde);
#endif
  } else {
    uint8_t *p, *q;
    uint64_t x, y, z;
    p = ByteRexbRm(m, rde);
    q = ByteRexrReg(m, rde);
    x = Read64(p);
    y = Read8(m->ax);
    z = Read8(q);
    if ((didit = x == y)) {
      Write64(p, z);
    } else {
      Write8(q, z);
    }
  }
  m->flags = SetFlag(m->flags, FLAGS_ZF, didit);
}

void OpCmpxchgEvqpRaxGvqp(struct Machine *m, uint32_t rde) {
  bool didit;
  uint8_t *p, *q;
  q = RegRexrReg(m, rde);
  p = GetModrmRegisterWordPointerWriteOszRexw(m, rde);
  if (Rexw(rde)) {
    if (Lock(rde) && !((intptr_t)p & 7)) {
#if LONG_BIT == 64
      didit = atomic_compare_exchange_strong(
          (atomic_ulong *)p, (unsigned long *)m->ax,
          atomic_load_explicit((atomic_ulong *)q, memory_order_relaxed));
#else
      OpUd(m, rde);
#endif
    } else {
      uint64_t x, y, z;
      x = Read64(p);
      y = Read64(m->ax);
      z = Read64(q);
      if ((didit = x == y)) {
        Write64(p, z);
      } else {
        Write64(q, z);
      }
    }
  } else if (!Osz(rde)) {
    if (Lock(rde) && !((intptr_t)p & 3)) {
      didit = atomic_compare_exchange_strong(
          (atomic_uint *)p, (unsigned int *)m->ax,
          atomic_load_explicit((atomic_uint *)q, memory_order_relaxed));
    } else {
      uint32_t x, y, z;
      x = Read32(p);
      y = Read32(m->ax);
      z = Read32(q);
      if ((didit = x == y)) {
        Write32(p, z);
      } else {
        Write32(q, z);
      }
    }
    Write32(q + 4, 0);
    Write32(m->ax + 4, 0);
    if (IsModrmRegister(rde)) {
      Write32(p + 4, 0);
    }
  } else {
    uint16_t x, y, z;
    unassert(!Lock(rde));
    x = Read16(p);
    y = Read16(m->ax);
    z = Read16(q);
    if ((didit = x == y)) {
      Write16(p, z);
    } else {
      Write16(q, z);
    }
  }
  m->flags = SetFlag(m->flags, FLAGS_ZF, didit);
}
