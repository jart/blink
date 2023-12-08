/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2023 Justine Alexandra Roberts Tunney                              │
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
#include "blink/bus.h"
#include "blink/flags.h"
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/rde.h"

static u64 Bts(u64 x, u64 y) {
  return x | y;
}

static u64 Btr(u64 x, u64 y) {
  return x & ~y;
}

static u64 Btc(u64 x, u64 y) {
  return (x & ~y) | (~x & y);
}

void OpBit(P) {
  u8 *p;
  int op;
  i64 bitdisp;
  unsigned bit;
  u64 v, x, y, z;
  u8 w, W[2][2] = {{2, 3}, {1, 3}};
  w = W[Osz(rde)][Rexw(rde)];
  if (Opcode(rde) == 0xBA) {
    op = ModrmReg(rde);
    bit = uimm0 & ((8 << w) - 1);
    bitdisp = 0;
  } else {
    op = (Opcode(rde) & 070) >> 3;
    bitdisp = ReadRegisterSigned(rde, RegRexrReg(m, rde));
    bit = bitdisp & ((8 << w) - 1);
    bitdisp &= -(8 << w);
    bitdisp >>= 3;
  }
  if (IsModrmRegister(rde)) {
    p = RegRexbRm(m, rde);
  } else {
    v = MaskAddress(Eamode(rde), ComputeAddress(A) + bitdisp);
    p = ReserveAddress(m, v, 1 << w, op != 4);
  }
  if (Lock(rde)) LockBus(p);
  y = 1;
  y <<= bit;
  if (Lock(rde)) {
    x = ReadMemoryUnlocked(rde, p);
  } else {
    x = ReadMemory(rde, p);
  }
  m->flags = SetFlag(m->flags, FLAGS_CF, !!(y & x));
  switch (op) {
    case 4:
      return;
    case 5:
      z = Bts(x, y);
      break;
    case 6:
      z = Btr(x, y);
      break;
    case 7:
      z = Btc(x, y);
      break;
    default:
      OpUdImpl(m);
  }
  WriteRegisterOrMemory(rde, p, z);
  if (Lock(rde)) UnlockBus(p);
}
