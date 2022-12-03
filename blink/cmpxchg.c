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
        (atomic_uchar *)ComputeReserveAddressWrite1(A), m->ax,
        Get8(ByteRexrReg(m, rde)), memory_order_acq_rel, memory_order_acquire);
#else
    OpUdImpl(m);
#endif
  } else {
    u8 *p = ByteRexbRm(m, rde);
    u8 x = Get8(p);
    if ((didit = x == Get8(m->ax))) {
      Put8(p, Get8(ByteRexrReg(m, rde)));
    } else {
      Put8(m->ax, x);
    }
  }
  m->flags = SetFlag(m->flags, FLAGS_ZF, didit);
}

void OpCmpxchgEvqpRaxGvqp(P) {
  u8 *p, *q;
  bool didit;
  q = RegRexrReg(m, rde);
  p = GetModrmRegisterWordPointerWriteOszRexw(A);
  if (Rexw(rde)) {
    if (Lock(rde) && !((intptr_t)p & 7)) {
#if LONG_BIT == 64
      unsigned long ax =
          atomic_load_explicit((atomic_ulong *)m->ax, memory_order_relaxed);
      if (!(didit = atomic_compare_exchange_strong_explicit(
                (atomic_ulong *)p, &ax,
                atomic_load_explicit((atomic_ulong *)q, memory_order_relaxed),
                memory_order_acq_rel, memory_order_acquire))) {
        atomic_store_explicit((atomic_ulong *)m->ax, ax, memory_order_relaxed);
      }
#else
      OpUdImpl(m);
#endif
    } else {
      u64 x = Load64(p);
      if ((didit = x == Get64(m->ax))) {
        Store64(p, Get64(q));
      } else {
        Put64(m->ax, x);
      }
    }
  } else if (!Osz(rde)) {
    if (Lock(rde) && !((intptr_t)p & 3)) {
      unsigned int ax =
          atomic_load_explicit((atomic_uint *)m->ax, memory_order_relaxed);
      if (!(didit = atomic_compare_exchange_strong_explicit(
                (atomic_uint *)p, &ax,
                atomic_load_explicit((atomic_uint *)q, memory_order_relaxed),
                memory_order_acq_rel, memory_order_acquire))) {
        Put64(m->ax, Little32(ax));
      }
    } else {
      u32 x = Load32(p);
      if ((didit = x == Get32(m->ax))) {
        Store32(p, Get32(q));
      } else {
        Put64(m->ax, x);
      }
    }
    if (IsModrmRegister(rde)) {
      Put32(p + 4, 0);
    }
  } else {
    unassert(!Lock(rde));
    u16 x = Load16(p);
    if ((didit = x == Get16(m->ax))) {
      Store16(p, Get16(q));
    } else {
      Put16(m->ax, x);
    }
  }
  m->flags = SetFlag(m->flags, FLAGS_ZF, didit);
}
