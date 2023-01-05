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
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/mop.h"

// todo BZHI clearing starting at bit
//      MULX flagless unsigned multiply
//      PDEP parallel bits deposit
//      PEXT parallel bits extract
//      RORX flagless rotate right logical
// todo SARX flagless shift arithmetic right
// todo SHRX flagless shift logical right
// todo SHLX flagless shift logical left

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

void Op2f5(P) {
  if (Rep(rde) == 2) {
    OpPbit(A, Pdep);
  } else if (Rep(rde) == 3) {
    OpPbit(A, Pext);
  } else {
    OpUdImpl(m);
  }
}

void OpRorx(P) {
  int i;
  u64 z;
  if (Rexw(rde)) {
    u64 x = Load64(GetModrmRegisterWordPointerRead8(A));
    i = uimm0 & 63;
    x = x >> i | x << (64 - i);
    z = x;
  } else {
    u32 x = Load32(GetModrmRegisterWordPointerRead4(A));
    i = uimm0 & 31;
    x = x >> i | x << (32 - i);
    z = x;
  }
  Put64(RegRexrReg(m, rde), z);
}
