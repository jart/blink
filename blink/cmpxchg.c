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
#include "blink/mop.h"

void OpCmpxchgEbAlGb(P) {
  bool didit;
  if (!IsModrmRegister(rde)) {
#if !defined(__riscv) && !defined(__MICROBLAZE__)
    didit = atomic_compare_exchange_strong_explicit(
        (atomic_uchar *)ComputeReserveAddressWrite1(A),
        m->ax, *ByteRexrReg(m, rde), memory_order_acq_rel,
        memory_order_acquire);
#else
    OpUdImpl(m);
#endif
  } else {
    u8 *p, *q;
    u64 x, y, z;
    p = ByteRexbRm(m, rde);
    q = ByteRexrReg(m, rde);
    x = Get64(p);
    y = Get8(m->ax);
    z = Get8(q);
    if ((didit = x == y)) {
      Put64(p, z);
    } else {
      Put8(q, z);
    }
  }
  m->flags = SetFlag(m->flags, FLAGS_ZF, didit);
}

void OpCmpxchgEvqpRaxGvqp(P) {
  bool didit;
  u8 *p, *q;
  q = RegRexrReg(m, rde);
  p = GetModrmRegisterWordPointerWriteOszRexw(A);
  if (Rexw(rde)) {
    if (Lock(rde) && !((intptr_t)p & 7)) {
#if LONG_BIT == 64
      didit = atomic_compare_exchange_strong_explicit(
          (atomic_ulong *)p, (unsigned long *)m->ax,
          atomic_load_explicit((atomic_ulong *)q, memory_order_relaxed),
          memory_order_acq_rel, memory_order_acquire);
#else
      OpUdImpl(m);
#endif
    } else {
      u64 x, y, z;
      x = Load64(p);
      y = Get64(m->ax);
      z = Get64(q);
      if ((didit = x == y)) {
        Store64(p, z);
      } else {
        Put64(q, z);
      }
    }
  } else if (!Osz(rde)) {
    if (Lock(rde) && !((intptr_t)p & 3)) {
      didit = atomic_compare_exchange_strong_explicit(
          (atomic_uint *)p, (unsigned int *)m->ax,
          atomic_load_explicit((atomic_uint *)q, memory_order_relaxed),
          memory_order_acq_rel, memory_order_acquire);
    } else {
      u32 x, y, z;
      x = Load32(p);
      y = Get32(m->ax);
      z = Get32(q);
      if ((didit = x == y)) {
        Store32(p, z);
      } else {
        Put32(q, z);
      }
    }
    Put32(m->ax + 4, 0);
    if (IsModrmRegister(rde)) {
      Put32(p + 4, 0);
    }
  } else {
    u16 x, y, z;
    unassert(!Lock(rde));
    x = Load16(p);
    y = Get16(m->ax);
    z = Get16(q);
    if ((didit = x == y)) {
      Store16(p, z);
    } else {
      Put16(q, z);
    }
  }
  m->flags = SetFlag(m->flags, FLAGS_ZF, didit);
}
