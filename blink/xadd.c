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
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/swap.h"

void OpXaddEbGb(struct Machine *m, uint32_t rde) {
  uint8_t x, y, z, *p, *q;
  p = GetModrmRegisterBytePointerWrite(m, rde);
  q = ByteRexrReg(m, rde);
  x = Read8(p);
  y = Read8(q);
#if !defined(__riscv) && !defined(__MICROBLAZE__)
  do {
    z = Add8(x, y, &m->flags);
    Write8(q, x);
    if (!Lock(rde)) {
      *p = z;
      break;
    }
  } while (!atomic_compare_exchange_weak((atomic_uchar *)p, &x, *q));
#else
  if (!Lock(rde)) {
    z = Add8(x, y, &m->flags);
    Write8(q, x);
    Write8(p, z);
  } else {
    OpUd(m, rde);
  }
#endif
}

void OpXaddEvqpGvqp(struct Machine *m, uint32_t rde) {
  uint8_t *p, *q;
  q = RegRexrReg(m, rde);
  p = GetModrmRegisterWordPointerWriteOszRexw(m, rde);
  if (Rexw(rde)) {
    uint64_t x, y, z;
    if (Lock(rde) && !((intptr_t)p & 7)) {
#if LONG_BIT == 64
      x = atomic_load((atomic_ulong *)p);
      y = atomic_load_explicit((atomic_ulong *)q, memory_order_relaxed);
      y = SWAP64LE(y);
      do {
        atomic_store_explicit((atomic_ulong *)q, x, memory_order_relaxed);
        z = kAlu[ALU_ADD][ALU_INT64](SWAP64LE(x), y, &m->flags);
        z = SWAP64LE(z);
      } while (!atomic_compare_exchange_weak((atomic_ulong *)p, &x, z));
#else
      OpUd(m, rde);
#endif
    } else {
      x = Read64(p);
      y = Read64(q);
      z = kAlu[ALU_ADD][ALU_INT64](x, y, &m->flags);
      Write64(q, x);
      Write64(p, z);
    }
  } else if (!Osz(rde)) {
    uint32_t x, y, z;
    if (Lock(rde) && !((intptr_t)p & 3)) {
      x = atomic_load((atomic_uint *)p);
      y = atomic_load_explicit((atomic_uint *)q, memory_order_relaxed);
      y = SWAP32LE(y);
      do {
        atomic_store_explicit((atomic_uint *)q, x, memory_order_relaxed);
        z = kAlu[ALU_ADD][ALU_INT32](SWAP32LE(x), y, &m->flags);
        z = SWAP32LE(z);
      } while (!atomic_compare_exchange_weak((atomic_uint *)p, &x, z));
    } else {
      x = Read32(p);
      y = Read32(q);
      z = kAlu[ALU_ADD][ALU_INT32](x, y, &m->flags);
      Write32(q, x);
      Write32(p, z);
    }
    Write32(q + 4, 0);
    if (IsModrmRegister(rde)) {
      Write32(p + 4, 0);
    }
  } else {
    uint16_t x, y, z;
    unassert(!Lock(rde));
    x = Read16(p);
    y = Read16(q);
    z = kAlu[ALU_ADD][ALU_INT16](x, y, &m->flags);
    Write16(q, x);
    Write16(p, z);
  }
}
