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
#include "blink/log.h"
#include "blink/modrm.h"
#include "blink/mop.h"
#include "blink/swap.h"

static void Alub(struct Machine *m, u64 rde, aluop_f op) {
  u8 *p, *q;
  p = GetModrmRegisterBytePointerWrite(m, rde);
  q = ByteRexrReg(m, rde);
  if (!Lock(rde)) {
    Store8(p, op(Load8(p), Get8(q), &m->flags));
  } else {
#if !defined(__riscv) && !defined(__MICROBLAZE__)
    u8 x, y, z;
    x = Load8(p);
    y = Get8(q);
    do {
      z = op(x, y, &m->flags);
    } while (!atomic_compare_exchange_weak_explicit(
        (atomic_uchar *)p, &x, z, memory_order_release, memory_order_relaxed));
#else
    LOGF("can't %s on this platform", "lock alub");
    OpUd(m, rde);
#endif
  }
}

void OpAlubAdd(struct Machine *m, u64 rde) {
  Alub(m, rde, Add8);
}

void OpAlubOr(struct Machine *m, u64 rde) {
  Alub(m, rde, Or8);
}

void OpAlubAdc(struct Machine *m, u64 rde) {
  Alub(m, rde, Adc8);
}

void OpAlubSbb(struct Machine *m, u64 rde) {
  Alub(m, rde, Sbb8);
}

void OpAlubAnd(struct Machine *m, u64 rde) {
  Alub(m, rde, And8);
}

void OpAlubSub(struct Machine *m, u64 rde) {
  Alub(m, rde, Sub8);
}

void OpAlubXor(struct Machine *m, u64 rde) {
  Alub(m, rde, Xor8);
}

void OpAluw(struct Machine *m, u64 rde) {
  u8 *p, *q;
  q = RegRexrReg(m, rde);
  if (Rexw(rde)) {
    p = GetModrmRegisterWordPointerWrite(m, rde, 8);
    if (Lock(rde)) {
#if LONG_BIT == 64
      if (!((intptr_t)p & 7)) {
        u64 x, y, z;
        x = atomic_load_explicit((_Atomic(u64) *)p, memory_order_acquire);
        y = atomic_load_explicit((_Atomic(u64) *)q, memory_order_relaxed);
        y = Little64(y);
        do {
          z = Little64(kAlu[(Opcode(rde) & 070) >> 3][ALU_INT64](Little64(x), y,
                                                                 &m->flags));
        } while (!atomic_compare_exchange_weak_explicit((_Atomic(u64) *)p, &x,
                                                        z, memory_order_release,
                                                        memory_order_relaxed));
      } else {
        LOGF("can't %s misaligned address", "lock aluq");
        OpUd(m, rde);
      }
#else
      LOGF("can't %s on this platform", "lock aluq");
      OpUd(m, rde);
#endif
    } else {
      u64 x, y, z;
      x = Load64(p);
      y = Get64(q);
      z = kAlu[(Opcode(rde) & 070) >> 3][ALU_INT64](x, y, &m->flags);
      Store64(p, z);
    }
  } else if (!Osz(rde)) {
    u32 x, y, z;
    p = GetModrmRegisterWordPointerWrite(m, rde, 4);
    if (Lock(rde)) {
      if (!((intptr_t)p & 3)) {
        x = atomic_load_explicit((_Atomic(u32) *)p, memory_order_acquire);
        y = atomic_load_explicit((_Atomic(u32) *)q, memory_order_relaxed);
        y = Little32(y);
        do {
          z = Little32(kAlu[(Opcode(rde) & 070) >> 3][ALU_INT32](Little32(x), y,
                                                                 &m->flags));
        } while (!atomic_compare_exchange_weak_explicit((_Atomic(u32) *)p, &x,
                                                        z, memory_order_release,
                                                        memory_order_relaxed));
      } else {
        LOGF("can't %s misaligned address", "lock alul");
        OpUd(m, rde);
      }
    } else {
      x = Load32(p);
      y = Get32(q);
      z = kAlu[(Opcode(rde) & 070) >> 3][ALU_INT32](x, y, &m->flags);
      Store32(p, z);
    }
    if (IsModrmRegister(rde)) {
      Put32(p + 4, 0);
    }
  } else {
    u16 x, y, z;
    unassert(!Lock(rde));
    p = GetModrmRegisterWordPointerWrite(m, rde, 2);
    x = Load16(p);
    y = Get16(q);
    z = kAlu[(Opcode(rde) & 070) >> 3][ALU_INT16](x, y, &m->flags);
    Store16(p, z);
  }
}
