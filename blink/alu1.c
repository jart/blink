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
#include "blink/mop.h"
#include "blink/stats.h"
#include "blink/swap.h"

static void AluEb(P, aluop_f op) {
  u8 *p = GetModrmRegisterBytePointerWrite1(A);
  if (Lock(rde) && !((intptr_t)p & 1)) {
    u8 x, z;
    x = atomic_load_explicit((_Atomic(u8) *)p, memory_order_acquire);
    do {
      z = Little8(op(m, Little8(x), 0));
    } while (!atomic_compare_exchange_weak_explicit(
        (_Atomic(u8) *)p, &x, z, memory_order_release, memory_order_relaxed));
  } else {
    Store8(p, op(m, Load8(p), 0));
  }
}

void OpNotEb(P) {
  AluEb(A, Not8);
}

void OpNegEb(P) {
  AluEb(A, Neg8);
}

void Op0fe(P) {
  switch (ModrmReg(rde)) {
    case 0:
      AluEb(A, Inc8);
      break;
    case 1:
      AluEb(A, Dec8);
      break;
    default:
      OpUdImpl(m);
  }
}

static void AluEvqp(P, const aluop_f ops[4]) {
  u8 *p;
  aluop_f f;
  f = ops[RegLog2(rde)];
  if (Rexw(rde)) {
    p = GetModrmRegisterWordPointerWrite8(A);
    if (LONG_BIT == 64 && Lock(rde) && !((intptr_t)p & 7)) {
      u64 x, z;
      x = atomic_load_explicit((_Atomic(u64) *)p, memory_order_acquire);
      do {
        z = Little64(f(m, Little64(x), 0));
      } while (!atomic_compare_exchange_weak_explicit((_Atomic(u64) *)p, &x, z,
                                                      memory_order_release,
                                                      memory_order_relaxed));
    } else {
      Store64(p, f(m, Load64(p), 0));
    }
  } else if (!Osz(rde)) {
    u32 x, z;
    p = GetModrmRegisterWordPointerWrite4(A);
    if (Lock(rde) && !((intptr_t)p & 3)) {
      x = atomic_load_explicit((_Atomic(u32) *)p, memory_order_acquire);
      do {
        z = Little32(f(m, Little32(x), 0));
      } while (!atomic_compare_exchange_weak_explicit((_Atomic(u32) *)p, &x, z,
                                                      memory_order_release,
                                                      memory_order_relaxed));
    } else {
      Store32(p, f(m, Load32(p), 0));
    }
    if (IsModrmRegister(rde)) {
      Write32(p + 4, 0);
    }
  } else {
    p = GetModrmRegisterWordPointerWrite2(A);
    if (Lock(rde) && !((intptr_t)p & 1)) {
      u16 x, z;
      x = atomic_load_explicit((_Atomic(u16) *)p, memory_order_acquire);
      do {
        z = Little16(f(m, Little16(x), 0));
      } while (!atomic_compare_exchange_weak_explicit((_Atomic(u16) *)p, &x, z,
                                                      memory_order_release,
                                                      memory_order_relaxed));
    } else {
      Store16(p, f(m, Load16(p), 0));
    }
  }
  if (m->path.jb && !Lock(rde)) {
    Jitter(A, "B r0a1= s0a0= c r0 D", f);
  }
}

void OpNotEvqp(P) {
  AluEvqp(A, kAlu[ALU_NOT]);
}

void OpNegEvqp(P) {
  AluEvqp(A, kAlu[ALU_NEG]);
}

void OpIncEvqp(P) {
  AluEvqp(A, kAlu[ALU_INC]);
}

void OpDecEvqp(P) {
  AluEvqp(A, kAlu[ALU_DEC]);
}
