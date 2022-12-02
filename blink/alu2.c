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
#include "blink/alu.inc"
#include "blink/assert.h"
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/log.h"
#include "blink/modrm.h"
#include "blink/mop.h"
#include "blink/swap.h"

static void Alub(P, aluop_f op) {
  u8 *p, *q;
  p = GetModrmRegisterBytePointerWrite(A);
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
    OpUdImpl(m);
#endif
  }
}

void OpAlubAdd(P) {
  Alub(A, Add8);
}

void OpAlubOr(P) {
  Alub(A, Or8);
}

void OpAlubAdc(P) {
  Alub(A, Adc8);
}

void OpAlubSbb(P) {
  Alub(A, Sbb8);
}

void OpAlubAnd(P) {
  Alub(A, And8);
}

void OpAlubSub(P) {
  Alub(A, Sub8);
}

void OpAlubXor(P) {
  Alub(A, Xor8);
}

static void OpAluwRegAdd64(P) {
  Store64(RegRexbRm(m, rde), FastAdd64(Get64(RegRexbRm(m, rde)),
                                       Get64(RegRexrReg(m, rde)), &m->flags));
}
static void OpAluwRegOr64(P) {
  Store64(RegRexbRm(m, rde), FastOr64(Get64(RegRexbRm(m, rde)),
                                      Get64(RegRexrReg(m, rde)), &m->flags));
}
static void OpAluwRegAdc64(P) {
  Store64(RegRexbRm(m, rde), FastAdc64(Get64(RegRexbRm(m, rde)),
                                       Get64(RegRexrReg(m, rde)), &m->flags));
}
static void OpAluwRegSbb64(P) {
  Store64(RegRexbRm(m, rde), FastSbb64(Get64(RegRexbRm(m, rde)),
                                       Get64(RegRexrReg(m, rde)), &m->flags));
}
static void OpAluwRegAnd64(P) {
  Store64(RegRexbRm(m, rde), FastAnd64(Get64(RegRexbRm(m, rde)),
                                       Get64(RegRexrReg(m, rde)), &m->flags));
}
static void OpAluwRegSub64(P) {
  Store64(RegRexbRm(m, rde), FastSub64(Get64(RegRexbRm(m, rde)),
                                       Get64(RegRexrReg(m, rde)), &m->flags));
}
static void OpAluwRegXor64(P) {
  Store64(RegRexbRm(m, rde), FastXor64(Get64(RegRexbRm(m, rde)),
                                       Get64(RegRexrReg(m, rde)), &m->flags));
}
const nexgen32e_f kAluReg64[] = {
    OpAluwRegAdd64,  //
    OpAluwRegOr64,   //
    OpAluwRegAdc64,  //
    OpAluwRegSbb64,  //
    OpAluwRegAnd64,  //
    OpAluwRegSub64,  //
    OpAluwRegXor64,  //
    OpAluwRegSub64,  //
};

static void OpAluwRegAdd32(P) {
  Store64(RegRexbRm(m, rde), FastAdd32(Get32(RegRexbRm(m, rde)),
                                       Get32(RegRexrReg(m, rde)), &m->flags));
}
static void OpAluwRegOr32(P) {
  Store64(RegRexbRm(m, rde), FastOr32(Get32(RegRexbRm(m, rde)),
                                      Get32(RegRexrReg(m, rde)), &m->flags));
}
static void OpAluwRegAdc32(P) {
  Store64(RegRexbRm(m, rde), FastAdc32(Get32(RegRexbRm(m, rde)),
                                       Get32(RegRexrReg(m, rde)), &m->flags));
}
static void OpAluwRegSbb32(P) {
  Store64(RegRexbRm(m, rde), FastSbb32(Get32(RegRexbRm(m, rde)),
                                       Get32(RegRexrReg(m, rde)), &m->flags));
}
static void OpAluwRegAnd32(P) {
  Store64(RegRexbRm(m, rde), FastAnd32(Get32(RegRexbRm(m, rde)),
                                       Get32(RegRexrReg(m, rde)), &m->flags));
}
static void OpAluwRegSub32(P) {
  Store64(RegRexbRm(m, rde), FastSub32(Get32(RegRexbRm(m, rde)),
                                       Get32(RegRexrReg(m, rde)), &m->flags));
}
static void OpAluwRegXor32(P) {
  Store64(RegRexbRm(m, rde), FastXor32(Get32(RegRexbRm(m, rde)),
                                       Get32(RegRexrReg(m, rde)), &m->flags));
}
const nexgen32e_f kAluReg32[] = {
    OpAluwRegAdd32,  //
    OpAluwRegOr32,   //
    OpAluwRegAdc32,  //
    OpAluwRegSbb32,  //
    OpAluwRegAnd32,  //
    OpAluwRegSub32,  //
    OpAluwRegXor32,  //
    OpAluwRegSub32,  //
};

void OpAluw(P) {
  u8 *p, *q;
  q = RegRexrReg(m, rde);
  if (Rexw(rde)) {
    p = GetModrmRegisterWordPointerWrite8(A);
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
        OpUdImpl(m);
      }
#else
      LOGF("can't %s on this platform", "lock aluq");
      OpUdImpl(m);
#endif
    } else {
      u64 x, y, z;
      x = Load64(p);
      y = Get64(q);
      z = kAlu[(Opcode(rde) & 070) >> 3][ALU_INT64](x, y, &m->flags);
      Store64(p, z);
      if (m->path.jp && IsModrmRegister(rde)) {
        AppendJitSetArg(m->path.jp, kParamRde,
                        rde & (kRexbRmMask | kRexrRegMask));
        AppendJitCall(m->path.jp, (void *)kAluReg64[(Opcode(rde) & 070) >> 3]);
      }
    }
  } else if (!Osz(rde)) {
    u32 x, y, z;
    p = GetModrmRegisterWordPointerWrite4(A);
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
        OpUdImpl(m);
      }
    } else {
      x = Load32(p);
      y = Get32(q);
      z = kAlu[(Opcode(rde) & 070) >> 3][ALU_INT32](x, y, &m->flags);
      Store32(p, z);
    }
    if (IsModrmRegister(rde)) {
      Put32(p + 4, 0);
      if (m->path.jp) {
        AppendJitSetArg(m->path.jp, kParamRde,
                        rde & (kRexbRmMask | kRexrRegMask));
        AppendJitCall(m->path.jp, (void *)kAluReg32[(Opcode(rde) & 070) >> 3]);
      }
    }
  } else {
    u16 x, y, z;
    unassert(!Lock(rde));
    p = GetModrmRegisterWordPointerWrite2(A);
    x = Load16(p);
    y = Get16(q);
    z = kAlu[(Opcode(rde) & 070) >> 3][ALU_INT16](x, y, &m->flags);
    Store16(p, z);
  }
}
