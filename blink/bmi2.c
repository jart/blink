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
#include "blink/bus.h"
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/rde.h"

// BMI2
// BZHI clearing starting at bit
// MULX flagless unsigned multiply
// PDEP parallel bits deposit
// PEXT parallel bits extract
// RORX flagless rotate right logical
// SARX flagless shift arithmetic right
// SHRX flagless shift logical right
// SHLX flagless shift logical left

static u64 Pdep(u64 x, u64 mask) {
  u64 r, b;
  for (r = 0, b = 1; mask; mask >>= 1, b <<= 1) {
    if (mask & 1) {
      if (x & 1) r |= b;
      x >>= 1;
    }
  }
  return r;
}

static u64 Pext(u64 x, u64 mask) {
  u64 r, b;
  for (r = 0, b = 1; mask; mask >>= 1, x >>= 1) {
    if (mask & 1) {
      if (x & 1) r |= b;
      b <<= 1;
    }
  }
  return r;
}

static void OpPbit(P, u64 op(u64, u64)) {
  if (Rexw(rde)) {
    Put64(RegRexrReg(m, rde), op(Get64(RegVreg(m, rde)),
                                 Load64(GetModrmRegisterWordPointerRead8(A))));
  } else {
    Put64(RegRexrReg(m, rde),
          (u32)op(Get32(RegVreg(m, rde)),
                  Load32(GetModrmRegisterWordPointerRead4(A))));
  }
}

static void OpBzhi(P) {
  int i;
  u64 x;
  bool sf;
  bool cf = 0;
  i = Get8(RegVreg(m, rde));
  if (Rexw(rde)) {
    x = Load64(GetModrmRegisterWordPointerRead8(A));
    if (i < 64) {
      x &= (1ull << i) - 1;
    } else {
      cf = 1;
    }
    sf = (i64)x < 0;
  } else {
    x = Load32(GetModrmRegisterWordPointerRead4(A));
    if (i < 32) {
      x &= (1u << i) - 1;
    } else {
      cf = 1;
    }
    sf = (i32)x < 0;
  }
  Put64(RegRexrReg(m, rde), x);
  m->flags = SetFlag(m->flags, FLAGS_ZF, !x);
  m->flags = SetFlag(m->flags, FLAGS_CF, cf);
  m->flags = SetFlag(m->flags, FLAGS_SF, sf);
  m->flags = SetFlag(m->flags, FLAGS_OF, false);
}

void Op2f5(P) {
  if (Rep(rde) == 2) {
    OpPbit(A, Pdep);
  } else if (Rep(rde) == 3) {
    OpPbit(A, Pext);
  } else if (!Osz(rde)) {
    OpBzhi(A);
#ifndef TINY
  } else {
    OpUdImpl(m);
#endif
  }
}

void OpRorx(P) {
  int i;
  u64 z;
#ifndef TINY
  if (Ymm(rde)) OpUdImpl(m);
#endif
  if (Rexw(rde)) {
    u64 x = Load64(GetModrmRegisterWordPointerRead8(A));
    if ((i = uimm0 & 63)) {
      x = x >> i | x << (64 - i);
    }
    z = x;
  } else {
    u32 x = Load32(GetModrmRegisterWordPointerRead4(A));
    if ((i = uimm0 & 31)) {
      x = x >> i | x << (32 - i);
    }
    z = x;
  }
  Put64(RegRexrReg(m, rde), z);
}

static void OpShlx(P) {
  int i;
  u64 z;
  i = Get8(RegVreg(m, rde));
  if (Rexw(rde)) {
    u64 x = Load64(GetModrmRegisterWordPointerRead8(A));
    i &= 63;
    if (i) x <<= i;
    z = x;
  } else {
    u32 x = Load32(GetModrmRegisterWordPointerRead4(A));
    i &= 31;
    if (i) x <<= i;
    z = x;
  }
  Put64(RegRexrReg(m, rde), z);
}

static void OpShrx(P) {
  int i;
  u64 z;
  i = Get8(RegVreg(m, rde));
  if (Rexw(rde)) {
    u64 x = Load64(GetModrmRegisterWordPointerRead8(A));
    i &= 63;
    if (i) x >>= i;
    z = x;
  } else {
    u32 x = Load32(GetModrmRegisterWordPointerRead4(A));
    i &= 31;
    if (i) x >>= i;
    z = x;
  }
  Put64(RegRexrReg(m, rde), z);
}

static void OpSarx(P) {
  int i;
  u64 z;
  i = Get8(RegVreg(m, rde));
  if (Rexw(rde)) {
    i64 x = Load64(GetModrmRegisterWordPointerRead8(A));
    i &= 63;
    if (i) x >>= i;
    z = x;
  } else {
    i32 x = Load32(GetModrmRegisterWordPointerRead4(A));
    i &= 31;
    if (i) x >>= i;
    z = (u32)x;
  }
  Put64(RegRexrReg(m, rde), z);
}

void OpShx(P) {
#ifndef TINY
  if (Ymm(rde)) OpUdImpl(m);
#endif
  if (Osz(rde)) {
    OpShlx(A);
  } else if (Rep(rde) == 2) {
    OpShrx(A);
  } else if (Rep(rde) == 3) {
    OpSarx(A);
#ifndef TINY
  } else {
    OpUdImpl(m);
#endif
  }
}
