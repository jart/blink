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
#include "blink/modrm.h"
#include "blink/swap.h"

static void Alub(struct Machine *m, uint32_t rde, aluop_f op) {
  uint8_t *p, *q;
  p = GetModrmRegisterBytePointerWrite(m, rde);
  q = ByteRexrReg(m, rde);
  if (!Lock(rde)) {
    Write8(p, op(Read8(p), Read8(q), &m->flags));
  } else {
#if !defined(__riscv) && !defined(__MICROBLAZE__)
    uint8_t x, y, z;
    x = Read8(p);
    y = Read8(q);
    do {
      z = op(x, y, &m->flags);
    } while (!atomic_compare_exchange_weak_explicit(
        (atomic_uchar *)p, &x, z, memory_order_release, memory_order_relaxed));
#else
    OpUd(m, rde);
#endif
  }
}

void OpAlubAdd(struct Machine *m, uint32_t rde) {
  Alub(m, rde, Add8);
}

void OpAlubOr(struct Machine *m, uint32_t rde) {
  Alub(m, rde, Or8);
}

void OpAlubAdc(struct Machine *m, uint32_t rde) {
  Alub(m, rde, Adc8);
}

void OpAlubSbb(struct Machine *m, uint32_t rde) {
  Alub(m, rde, Sbb8);
}

void OpAlubAnd(struct Machine *m, uint32_t rde) {
  Alub(m, rde, And8);
}

void OpAlubSub(struct Machine *m, uint32_t rde) {
  Alub(m, rde, Sub8);
}

void OpAlubXor(struct Machine *m, uint32_t rde) {
  Alub(m, rde, Xor8);
}

void OpAluw(struct Machine *m, uint32_t rde) {
  uint8_t *p, *q;
  q = RegRexrReg(m, rde);
  if (Rexw(rde)) {
    uint64_t x, y, z;
    p = GetModrmRegisterWordPointerWrite(m, rde, 8);
    if (Lock(rde) && !((intptr_t)p & 7)) {
#if LONG_BIT == 64
      x = atomic_load((atomic_ulong *)p);
      y = atomic_load_explicit((atomic_ulong *)q, memory_order_relaxed);
      y = SWAP64LE(y);
      do {
        z = kAlu[(m->xedd->op.opcode & 070) >> 3][ALU_INT64](SWAP64LE(x), y,
                                                             &m->flags);
        z = SWAP64LE(z);
      } while (!atomic_compare_exchange_weak_explicit((atomic_ulong *)p, &x, z,
                                                      memory_order_release,
                                                      memory_order_relaxed));
#else
      OpUd(m, rde);
#endif
    } else {
      x = Read64(p);
      y = Read64(q);
      z = kAlu[(m->xedd->op.opcode & 070) >> 3][ALU_INT64](x, y, &m->flags);
      Write64(p, z);
    }
  } else if (!Osz(rde)) {
    uint32_t x, y, z;
    p = GetModrmRegisterWordPointerWrite(m, rde, 4);
    if (Lock(rde) && !((intptr_t)p & 3)) {
      x = atomic_load((atomic_uint *)p);
      y = atomic_load_explicit((atomic_uint *)q, memory_order_relaxed);
      y = SWAP32LE(y);
      do {
        z = kAlu[(m->xedd->op.opcode & 070) >> 3][ALU_INT32](SWAP32LE(x), y,
                                                             &m->flags);
        z = SWAP32LE(z);
      } while (!atomic_compare_exchange_weak_explicit(
          (atomic_uint *)p, &x, z, memory_order_release, memory_order_relaxed));
    } else {
      x = Read32(p);
      y = Read32(q);
      z = kAlu[(m->xedd->op.opcode & 070) >> 3][ALU_INT32](x, y, &m->flags);
      Write32(p, z);
    }
    Write32(q + 4, 0);
    if (IsModrmRegister(rde)) {
      Write32(p + 4, 0);
    }
  } else {
    uint16_t x, y, z;
    unassert(!Lock(rde));
    p = GetModrmRegisterWordPointerWrite(m, rde, 2);
    x = Read16(p);
    y = Read16(q);
    z = kAlu[(m->xedd->op.opcode & 070) >> 3][ALU_INT16](x, y, &m->flags);
    Write16(p, z);
  }
}
