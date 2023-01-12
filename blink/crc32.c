/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
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
#include "blink/endian.h"
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/mop.h"
#include "blink/swap.h"
#include "blink/types.h"

static u32 kCastagnoli[256];

static void InitializeCrc32(u32 table[256], u32 polynomial) {
  u32 d, i, r;
  for (d = 0; d < 256; ++d) {
    r = d;
    for (i = 0; i < 8; ++i) {
      r = r >> 1 ^ (r & 1 ? polynomial : 0);
    }
    table[d] = r;
  }
}

static u32 ReverseBits32(u32 x) {
  x = SWAP32(x);
  x = (x & 0xaaaaaaaa) >> 1 | (x & 0x55555555) << 1;
  x = (x & 0xcccccccc) >> 2 | (x & 0x33333333) << 2;
  x = (x & 0xf0f0f0f0) >> 4 | (x & 0x0f0f0f0f) << 4;
  return x;
}

static u32 Castagnoli(u32 h, u64 w, long n) {
  long i;
  static int once;
  if (!once) {
    InitializeCrc32(kCastagnoli, ReverseBits32(0x1edc6f41));
    once = 1;
  }
  for (i = 0; i < n; ++i) {
    h = h >> 8 ^ kCastagnoli[(h & 255) ^ (w & 255)];
    w >>= 8;
  }
  return h;
}

static void OpCrc32(P) {
  Put64(RegRexrReg(m, rde),
        Castagnoli(Get32(RegRexrReg(m, rde)),
                   ReadRegisterOrMemoryBW(rde, GetModrmReadBW(A)),
                   1 << RegLog2(rde)));
}

void Op2f01(P) {
  if (!Rep(rde) && !Osz(rde)) {
    OpUdImpl(m);  // TODO: movbe
  } else if (Rep(rde) == 2 && !Osz(rde)) {
    OpCrc32(A);
  } else {
    OpUdImpl(m);
  }
}
