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
#include <string.h>

#include "blink/case.h"
#include "blink/endian.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/memory.h"
#include "blink/modrm.h"
#include "blink/sse.h"

static void MmxPor(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 8; ++i) {
    x[i] |= y[i];
  }
}

static void MmxPxor(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 8; ++i) {
    x[i] ^= y[i];
  }
}

static void MmxPsubb(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 8; ++i) {
    x[i] -= y[i];
  }
}

static void MmxPaddb(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 8; ++i) {
    x[i] += y[i];
  }
}

static void MmxPand(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 8; ++i) {
    x[i] &= y[i];
  }
}

static void MmxPandn(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 8; ++i) {
    x[i] = ~x[i] & y[i];
  }
}

static void MmxPavgb(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 8; ++i) {
    x[i] = (x[i] + y[i] + 1) >> 1;
  }
}

static void MmxPabsb(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 8; ++i) {
    x[i] = ABS((i8)y[i]);
  }
}

static void MmxPminub(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 8; ++i) {
    x[i] = MIN(x[i], y[i]);
  }
}

static void MmxPmaxub(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 8; ++i) {
    x[i] = MAX(x[i], y[i]);
  }
}

static void MmxPaddusb(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 8; ++i) {
    x[i] = MIN(255, x[i] + y[i]);
  }
}

static void MmxPsubusb(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 8; ++i) {
    x[i] = MIN(255, MAX(0, x[i] - y[i]));
  }
}

static void MmxPcmpeqb(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 8; ++i) {
    x[i] = -(x[i] == y[i]);
  }
}

static void MmxPcmpgtb(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 8; ++i) {
    x[i] = -((i8)x[i] > (i8)y[i]);
  }
}

static void MmxPsubsb(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 8; ++i) {
    x[i] = MAX(-128, MIN(127, (i8)x[i] - (i8)y[i]));
  }
}

static void MmxPaddsb(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 8; ++i) {
    x[i] = MAX(-128, MIN(127, (i8)x[i] + (i8)y[i]));
  }
}

static void MmxPmulhrsw(u8 x[8], const u8 y[8]) {
  unsigned i;
  i16 a, b;
  for (i = 0; i < 4; ++i) {
    a = Read16(x + i * 2);
    b = Read16(y + i * 2);
    Write16(x + i * 2, (((a * b) >> 14) + 1) >> 1);
  }
}

static void MmxPmaddubsw(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 4; ++i) {
    Write16(x + i * 2,
            MAX(-32768, MIN(32767, (x[i * 2 + 0] * (i8)y[i * 2 + 0] +
                                    x[i * 2 + 1] * (i8)y[i * 2 + 1]))));
  }
}

static void MmxPsraw(u8 x[8], unsigned k) {
  unsigned i;
  if (k > 15) k = 15;
  for (i = 0; i < 4; ++i) {
    Write16(x + i * 2, (i16)Read16(x + i * 2) >> k);
  }
}

static void MmxPsrad(u8 x[8], unsigned k) {
  unsigned i;
  if (k > 31) k = 31;
  for (i = 0; i < 2; ++i) {
    Write32(x + i * 4, (i32)Read32(x + i * 4) >> k);
  }
}

static void MmxPsrlw(u8 x[8], unsigned k) {
  unsigned i;
  if (k < 16) {
    for (i = 0; i < 4; ++i) {
      Write16(x + i * 2, Read16(x + i * 2) >> k);
    }
  } else {
    memset(x, 0, 8);
  }
}

static void MmxPsllw(u8 x[8], unsigned k) {
  unsigned i;
  if (k <= 15) {
    for (i = 0; i < 4; ++i) {
      Write16(x + i * 2, Read16(x + i * 2) << k);
    }
  } else {
    memset(x, 0, 8);
  }
}

static void MmxPsrld(u8 x[8], unsigned k) {
  unsigned i;
  if (k <= 31) {
    for (i = 0; i < 2; ++i) {
      Write32(x + i * 4, Read32(x + i * 4) >> k);
    }
  } else {
    memset(x, 0, 8);
  }
}

static void MmxPslld(u8 x[8], unsigned k) {
  unsigned i;
  if (k <= 31) {
    for (i = 0; i < 2; ++i) {
      Write32(x + i * 4, Read32(x + i * 4) << k);
    }
  } else {
    memset(x, 0, 8);
  }
}

static void MmxPsrlq(u8 x[8], unsigned k) {
  if (k <= 63) {
    Write64(x, Read64(x) >> k);
  } else {
    memset(x, 0, 8);
  }
}

static void MmxPsllq(u8 x[8], unsigned k) {
  if (k <= 63) {
    Write64(x, Read64(x) << k);
  } else {
    memset(x, 0, 8);
  }
}

static void MmxPslldq(u8 x[8], unsigned k) {
  unsigned i;
  u8 t[8];
  if (k > 8) k = 8;
  for (i = 0; i < k; ++i) t[i] = 0;
  for (i = 0; i < 8 - k; ++i) t[k + i] = x[i];
  memcpy(x, t, 8);
}

static void MmxPsrldq(u8 x[8], unsigned k) {
  u8 t[8];
  if (k > 8) k = 8;
  memcpy(t, x + k, 8 - k);
  memset(t + (8 - k), 0, k);
  memcpy(x, t, 8);
}

static void MmxPalignr(u8 x[8], const u8 y[8], unsigned k) {
  u8 t[24];
  memcpy(t, y, 8);
  memcpy(t + 8, x, 8);
  memset(t + 16, 0, 8);
  memcpy(x, t + MIN(k, 16), 8);
}

static void MmxPsubw(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 4; ++i) {
    Write16(x + i * 2, Read16(x + i * 2) - Read16(y + i * 2));
  }
}

static void MmxPaddw(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 4; ++i) {
    Write16(x + i * 2, Read16(x + i * 2) + Read16(y + i * 2));
  }
}

static void MmxPsubd(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 2; ++i) {
    Write32(x + i * 4, Read32(x + i * 4) - Read32(y + i * 4));
  }
}

static void MmxPaddd(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 2; ++i) {
    Write32(x + i * 4, Read32(x + i * 4) + Read32(y + i * 4));
  }
}

static void MmxPaddq(u8 x[8], const u8 y[8]) {
  Write64(x, Read64(x) + Read64(y));
}

static void MmxPsubq(u8 x[8], const u8 y[8]) {
  Write64(x, Read64(x) - Read64(y));
}

static void MmxPaddsw(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 4; ++i) {
    Write16(x + i * 2, MAX(-32768, MIN(32767, ((i16)Read16(x + i * 2) +
                                               (i16)Read16(y + i * 2)))));
  }
}

static void MmxPsubsw(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 4; ++i) {
    Write16(x + i * 2, MAX(-32768, MIN(32767, ((i16)Read16(x + i * 2) -
                                               (i16)Read16(y + i * 2)))));
  }
}

static void MmxPaddusw(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 4; ++i) {
    Write16(x + i * 2, MIN(65535, Read16(x + i * 2) + Read16(y + i * 2)));
  }
}

static void MmxPsubusw(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 4; ++i) {
    Write16(x + i * 2,
            MIN(65535, MAX(0, Read16(x + i * 2) - Read16(y + i * 2))));
  }
}

static void MmxPminsw(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 4; ++i) {
    Write16(x + i * 2,
            MIN((i16)Read16(x + i * 2), (i16)Read16(y + i * 2)));
  }
}

static void MmxPmaxsw(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 4; ++i) {
    Write16(x + i * 2,
            MAX((i16)Read16(x + i * 2), (i16)Read16(y + i * 2)));
  }
}

static void MmxPackuswb(u8 x[8], const u8 y[8]) {
  unsigned i;
  u8 t[8];
  for (i = 0; i < 4; ++i) {
    t[i + 0] = MIN(255, MAX(0, (i16)Read16(x + i * 2)));
  }
  for (i = 0; i < 4; ++i) {
    t[i + 4] = MIN(255, MAX(0, (i16)Read16(y + i * 2)));
  }
  memcpy(x, t, 8);
}

static void MmxPacksswb(u8 x[8], const u8 y[8]) {
  unsigned i;
  u8 t[8];
  for (i = 0; i < 4; ++i) {
    t[i + 0] = MAX(-128, MIN(127, (i16)Read16(x + i * 2)));
  }
  for (i = 0; i < 4; ++i) {
    t[i + 4] = MAX(-128, MIN(127, (i16)Read16(y + i * 2)));
  }
  memcpy(x, t, 8);
}

static void MmxPackssdw(u8 x[8], const u8 y[8]) {
  unsigned i;
  u8 t[8];
  for (i = 0; i < 2; ++i) {
    Write16(t + i * 2 + 0, MAX(-32768, MIN(32767, (i32)Read32(x + i * 4))));
  }
  for (i = 0; i < 2; ++i) {
    Write16(t + i * 2 + 4, MAX(-32768, MIN(32767, (i32)Read32(y + i * 4))));
  }
  memcpy(x, t, 8);
}

static void MmxPcmpgtw(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 4; ++i) {
    Write16(x + i * 2,
            -((i16)Read16(x + i * 2) > (i16)Read16(y + i * 2)));
  }
}

static void MmxPcmpeqw(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 4; ++i) {
    Write16(x + i * 2, -(Read16(x + i * 2) == Read16(y + i * 2)));
  }
}

static void MmxPcmpgtd(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 2; ++i) {
    Write32(x + i * 4,
            -((i32)Read32(x + i * 4) > (i32)Read32(y + i * 4)));
  }
}

static void MmxPcmpeqd(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 2; ++i) {
    Write32(x + i * 4, -(Read32(x + i * 4) == Read32(y + i * 4)));
  }
}

static void MmxPsrawv(u8 x[8], const u8 y[8]) {
  unsigned i;
  u64 k;
  k = Read64(y);
  if (k > 15) k = 15;
  for (i = 0; i < 4; ++i) {
    Write16(x + i * 2, (i16)Read16(x + i * 2) >> k);
  }
}

static void MmxPsradv(u8 x[8], const u8 y[8]) {
  unsigned i;
  u64 k;
  k = Read64(y);
  if (k > 31) k = 31;
  for (i = 0; i < 2; ++i) {
    Write32(x + i * 4, (i32)Read32(x + i * 4) >> k);
  }
}

static void MmxPsrlwv(u8 x[8], const u8 y[8]) {
  unsigned i;
  u64 k;
  k = Read64(y);
  if (k < 16) {
    for (i = 0; i < 4; ++i) {
      Write16(x + i * 2, Read16(x + i * 2) >> k);
    }
  } else {
    memset(x, 0, 8);
  }
}

static void MmxPsllwv(u8 x[8], const u8 y[8]) {
  unsigned i;
  u64 k;
  k = Read64(y);
  if (k < 16) {
    for (i = 0; i < 4; ++i) {
      Write16(x + i * 2, Read16(x + i * 2) << k);
    }
  } else {
    memset(x, 0, 8);
  }
}

static void MmxPsrldv(u8 x[8], const u8 y[8]) {
  unsigned i;
  u64 k;
  k = Read64(y);
  if (k < 32) {
    for (i = 0; i < 2; ++i) {
      Write32(x + i * 4, Read32(x + i * 4) >> k);
    }
  } else {
    memset(x, 0, 8);
  }
}

static void MmxPslldv(u8 x[8], const u8 y[8]) {
  unsigned i;
  u64 k;
  k = Read64(y);
  if (k < 32) {
    for (i = 0; i < 2; ++i) {
      Write32(x + i * 4, Read32(x + i * 4) << k);
    }
  } else {
    memset(x, 0, 8);
  }
}

static void MmxPsrlqv(u8 x[8], const u8 y[8]) {
  u64 k;
  k = Read64(y);
  if (k < 64) {
    Write64(x, Read64(x) >> k);
  } else {
    memset(x, 0, 8);
  }
}

static void MmxPsllqv(u8 x[8], const u8 y[8]) {
  u64 k;
  k = Read64(y);
  if (k < 64) {
    Write64(x, Read64(x) << k);
  } else {
    memset(x, 0, 8);
  }
}

static void MmxPavgw(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 4; ++i) {
    Write16(x + i * 2, (Read16(x + i * 2) + Read16(y + i * 2) + 1) >> 1);
  }
}

static void MmxPsadbw(u8 x[8], const u8 y[8]) {
  unsigned i, s, t;
  for (s = i = 0; i < 4; ++i) s += ABS(x[i] - y[i]);
  for (t = 0; i < 8; ++i) t += ABS(x[i] - y[i]);
  Write32(x + 0, s);
  Write32(x + 4, t);
}

static void MmxPmaddwd(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 2; ++i) {
    Write32(x + i * 4,
            ((i16)Read16(x + i * 4 + 0) * (i16)Read16(y + i * 4 + 0) +
             (i16)Read16(x + i * 4 + 2) * (i16)Read16(y + i * 4 + 2)));
  }
}

static void MmxPmulhuw(u8 x[8], const u8 y[8]) {
  u32 v;
  unsigned i;
  for (i = 0; i < 4; ++i) {
    v = Read16(x + i * 2);
    v *= Read16(y + i * 2);
    v >>= 16;
    Write16(x + i * 2, v);
  }
}

static void MmxPmulhw(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 4; ++i) {
    Write16(x + i * 2,
            ((i16)Read16(x + i * 2) * (i16)Read16(y + i * 2)) >> 16);
  }
}

static void MmxPmuludq(u8 x[8], const u8 y[8]) {
  Write64(x, (u64)Read32(x) * Read32(y));
}

static void MmxPmullw(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 4; ++i) {
    Write16(x + i * 2, (i16)Read16(x + i * 2) * (i16)Read16(y + i * 2));
  }
}

static void MmxPmulld(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 2; ++i) {
    Write32(x + i * 4, Read32(x + i * 4) * Read32(y + i * 4));
  }
}

static void MmxPshufb(u8 x[8], const u8 y[8]) {
  unsigned i;
  u8 t[8];
  for (i = 0; i < 8; ++i) {
    t[i] = (y[i] & 128) ? 0 : x[y[i] & 7];
  }
  memcpy(x, t, 8);
}

static void MmxPsignb(u8 x[8], const u8 y[8]) {
  int v;
  unsigned i;
  for (i = 0; i < 8; ++i) {
    v = (i8)y[i];
    if (!v) {
      x[i] = 0;
    } else if (v < 0) {
      x[i] = -(i8)x[i];
    }
  }
}

static void MmxPsignw(u8 x[8], const u8 y[8]) {
  int v;
  unsigned i;
  for (i = 0; i < 4; ++i) {
    v = (i16)Read16(y + i * 2);
    if (!v) {
      Write16(x + i * 2, 0);
    } else if (v < 0) {
      Write16(x + i * 2, -(i16)Read16(x + i * 2));
    }
  }
}

static void MmxPsignd(u8 x[8], const u8 y[8]) {
  i32 v;
  unsigned i;
  for (i = 0; i < 2; ++i) {
    v = Read32(y + i * 4);
    if (!v) {
      Write32(x + i * 4, 0);
    } else if (v < 0) {
      Write32(x + i * 4, -Read32(x + i * 4));
    }
  }
}

static void MmxPabsw(u8 x[8], const u8 y[8]) {
  unsigned i;
  for (i = 0; i < 4; ++i) {
    Write16(x + i * 2, ABS((i16)Read16(y + i * 2)));
  }
}

static void MmxPabsd(u8 x[8], const u8 y[8]) {
  i32 v;
  unsigned i;
  for (i = 0; i < 2; ++i) {
    v = Read32(y + i * 4);
    Write32(x + i * 4, v >= 0 ? v : -(u32)v);
  }
}

static void MmxPhaddw(u8 x[8], const u8 y[8]) {
  u8 t[8];
  Write16(t + 0 * 2, Read16(x + 0 * 2) + Read16(x + 1 * 2));
  Write16(t + 1 * 2, Read16(x + 2 * 2) + Read16(x + 3 * 2));
  Write16(t + 2 * 2, Read16(y + 0 * 2) + Read16(y + 1 * 2));
  Write16(t + 3 * 2, Read16(y + 2 * 2) + Read16(y + 3 * 2));
  memcpy(x, t, 8);
}

static void MmxPhsubw(u8 x[8], const u8 y[8]) {
  u8 t[8];
  Write16(t + 0 * 2, Read16(x + 0 * 2) - Read16(x + 1 * 2));
  Write16(t + 1 * 2, Read16(x + 2 * 2) - Read16(x + 3 * 2));
  Write16(t + 2 * 2, Read16(y + 0 * 2) - Read16(y + 1 * 2));
  Write16(t + 3 * 2, Read16(y + 2 * 2) - Read16(y + 3 * 2));
  memcpy(x, t, 8);
}

static void MmxPhaddd(u8 x[8], const u8 y[8]) {
  u8 t[8];
  Write32(t + 0 * 4, Read32(x + 0 * 4) + Read32(x + 1 * 4));
  Write32(t + 1 * 4, Read32(y + 0 * 4) + Read32(y + 1 * 4));
  memcpy(x, t, 8);
}

static void MmxPhsubd(u8 x[8], const u8 y[8]) {
  u8 t[8];
  Write32(t + 0 * 4, Read32(x + 0 * 4) - Read32(x + 1 * 4));
  Write32(t + 1 * 4, Read32(y + 0 * 4) - Read32(y + 1 * 4));
  memcpy(x, t, 8);
}

static void MmxPhaddsw(u8 x[8], const u8 y[8]) {
  u8 t[8];
  Write16(t + 0 * 2, MAX(-32768, MIN(32767, ((i16)Read16(x + 0 * 2) +
                                             (i16)Read16(x + 1 * 2)))));
  Write16(t + 1 * 2, MAX(-32768, MIN(32767, ((i16)Read16(x + 2 * 2) +
                                             (i16)Read16(x + 3 * 2)))));
  Write16(t + 2 * 2, MAX(-32768, MIN(32767, ((i16)Read16(y + 0 * 2) +
                                             (i16)Read16(y + 1 * 2)))));
  Write16(t + 3 * 2, MAX(-32768, MIN(32767, ((i16)Read16(y + 2 * 2) +
                                             (i16)Read16(y + 3 * 2)))));
  memcpy(x, t, 8);
}

static void MmxPhsubsw(u8 x[8], const u8 y[8]) {
  u8 t[8];
  Write16(t + 0 * 2, MAX(-32768, MIN(32767, ((i16)Read16(x + 0 * 2) -
                                             (i16)Read16(x + 1 * 2)))));
  Write16(t + 1 * 2, MAX(-32768, MIN(32767, ((i16)Read16(x + 2 * 2) -
                                             (i16)Read16(x + 3 * 2)))));
  Write16(t + 2 * 2, MAX(-32768, MIN(32767, ((i16)Read16(y + 0 * 2) -
                                             (i16)Read16(x + 1 * 2)))));
  Write16(t + 3 * 2, MAX(-32768, MIN(32767, ((i16)Read16(y + 2 * 2) -
                                             (i16)Read16(y + 3 * 2)))));
  memcpy(x, t, 8);
}

static void MmxPunpcklbw(u8 x[8], const u8 y[8]) {
  x[7] = y[3];
  x[6] = x[3];
  x[5] = y[2];
  x[4] = x[2];
  x[3] = y[1];
  x[2] = x[1];
  x[1] = y[0];
  x[0] = x[0];
}

static void MmxPunpckhbw(u8 x[8], const u8 y[8]) {
  x[0] = x[4];
  x[1] = y[4];
  x[2] = x[5];
  x[3] = y[5];
  x[4] = x[6];
  x[5] = y[6];
  x[6] = x[7];
  x[7] = y[7];
}

static void MmxPunpcklwd(u8 x[8], const u8 y[8]) {
  x[6] = y[2];
  x[7] = y[3];
  x[4] = x[2];
  x[5] = x[3];
  x[2] = y[0];
  x[3] = y[1];
  x[0] = x[0];
  x[1] = x[1];
}

static void MmxPunpckldq(u8 x[8], const u8 y[8]) {
  x[4] = y[0];
  x[5] = y[1];
  x[6] = y[2];
  x[7] = y[3];
  x[0] = x[0];
  x[1] = x[1];
  x[2] = x[2];
  x[3] = x[3];
}

static void MmxPunpckhwd(u8 x[8], const u8 y[8]) {
  x[0] = x[4];
  x[1] = x[5];
  x[2] = y[4];
  x[3] = y[5];
  x[4] = x[6];
  x[5] = x[7];
  x[6] = y[6];
  x[7] = y[7];
}

static void MmxPunpckhdq(u8 x[8], const u8 y[8]) {
  x[0] = x[4];
  x[1] = x[5];
  x[2] = x[6];
  x[3] = x[7];
  x[4] = y[4];
  x[5] = y[5];
  x[6] = y[6];
  x[7] = y[7];
}

static void MmxPunpcklqdq(u8 x[8], const u8 y[8]) {
}

static void MmxPunpckhqdq(u8 x[8], const u8 y[8]) {
}

static void SsePsubb(u8 x[16], const u8 y[16]) {
  unsigned i;
  for (i = 0; i < 16; ++i) {
    x[i] -= y[i];
  }
}

static void SsePaddb(u8 x[16], const u8 y[16]) {
  unsigned i;
  for (i = 0; i < 16; ++i) {
    x[i] += y[i];
  }
}

static void SsePor(u8 x[16], const u8 y[16]) {
  unsigned i;
  for (i = 0; i < 16; ++i) {
    x[i] |= y[i];
  }
}

static void SsePxor(u8 x[16], const u8 y[16]) {
  unsigned i;
  for (i = 0; i < 16; ++i) {
    x[i] ^= y[i];
  }
}

static void SsePand(u8 x[16], const u8 y[16]) {
  unsigned i;
  for (i = 0; i < 16; ++i) {
    x[i] &= y[i];
  }
}

static void SsePandn(u8 x[16], const u8 y[16]) {
  unsigned i;
  for (i = 0; i < 16; ++i) {
    x[i] = ~x[i] & y[i];
  }
}

static void SsePcmpeqb(u8 x[16], const u8 y[16]) {
  unsigned i;
  for (i = 0; i < 16; ++i) {
    x[i] = -(x[i] == y[i]);
  }
}

static void SsePcmpgtb(u8 x[16], const u8 y[16]) {
  unsigned i;
  for (i = 0; i < 16; ++i) {
    x[i] = -((i8)x[i] > (i8)y[i]);
  }
}

static void SsePavgb(u8 x[16], const u8 y[16]) {
  unsigned i;
  for (i = 0; i < 16; ++i) {
    x[i] = (x[i] + y[i] + 1) >> 1;
  }
}

static void SsePabsb(u8 x[16], const u8 y[16]) {
  unsigned i;
  for (i = 0; i < 16; ++i) {
    x[i] = ABS((i8)y[i]);
  }
}

static void SsePminub(u8 x[16], const u8 y[16]) {
  unsigned i;
  for (i = 0; i < 16; ++i) {
    x[i] = MIN(x[i], y[i]);
  }
}

static void SsePmaxub(u8 x[16], const u8 y[16]) {
  unsigned i;
  for (i = 0; i < 16; ++i) {
    x[i] = MAX(x[i], y[i]);
  }
}

static void SsePslldq(u8 x[16], unsigned k) {
  unsigned i;
  u8 t[16];
  if (k > 16) k = 16;
  for (i = 0; i < k; ++i) t[i] = 0;
  for (i = 0; i < 16 - k; ++i) t[k + i] = x[i];
  memcpy(x, t, 16);
}

static void SsePsrldq(u8 x[16], unsigned k) {
  u8 t[16];
  if (k > 16) k = 16;
  memcpy(t, x + k, 16 - k);
  memset(t + (16 - k), 0, k);
  memcpy(x, t, 16);
}

static void SsePalignr(u8 x[16], const u8 y[16], unsigned k) {
  u8 t[48];
  memcpy(t, y, 16);
  memcpy(t + 16, x, 16);
  memset(t + 32, 0, 16);
  memcpy(x, t + MIN(k, 32), 16);
}

static void SsePsubw(u8 x[16], const u8 y[16]) {
  unsigned i;
  for (i = 0; i < 8; ++i) {
    Write16(x + i * 2, Read16(x + i * 2) - Read16(y + i * 2));
  }
}

static void SsePaddw(u8 x[16], const u8 y[16]) {
  unsigned i;
  for (i = 0; i < 8; ++i) {
    Write16(x + i * 2, Read16(x + i * 2) + Read16(y + i * 2));
  }
}

static void SsePsubd(u8 x[16], const u8 y[16]) {
  unsigned i;
  for (i = 0; i < 4; ++i) {
    Write32(x + i * 4, Read32(x + i * 4) - Read32(y + i * 4));
  }
}

static void SsePaddd(u8 x[16], const u8 y[16]) {
  unsigned i;
  for (i = 0; i < 4; ++i) {
    Write32(x + i * 4, Read32(x + i * 4) + Read32(y + i * 4));
  }
}

static void SsePaddq(u8 x[16], const u8 y[16]) {
  unsigned i;
  for (i = 0; i < 2; ++i) {
    Write64(x + i * 8, Read64(x + i * 8) + Read64(y + i * 8));
  }
}

static void SsePsubq(u8 x[16], const u8 y[16]) {
  unsigned i;
  for (i = 0; i < 2; ++i) {
    Write64(x + i * 8, Read64(x + i * 8) - Read64(y + i * 8));
  }
}

static void SsePaddusw(u8 x[16], const u8 y[16]) {
  unsigned i;
  for (i = 0; i < 8; ++i) {
    Write16(x + i * 2, MIN(65535, Read16(x + i * 2) + Read16(y + i * 2)));
  }
}

static void SsePackuswb(u8 x[16], const u8 y[16]) {
  unsigned i;
  u8 t[16];
  for (i = 0; i < 8; ++i) {
    t[i + 0] = MIN(255, MAX(0, (i16)Read16(x + i * 2)));
  }
  for (i = 0; i < 8; ++i) {
    t[i + 8] = MIN(255, MAX(0, (i16)Read16(y + i * 2)));
  }
  memcpy(x, t, 16);
}

static void SsePacksswb(u8 x[16], const u8 y[16]) {
  unsigned i;
  u8 t[16];
  for (i = 0; i < 8; ++i) {
    t[i + 0] = MAX(-128, MIN(127, (i16)Read16(x + i * 2)));
  }
  for (i = 0; i < 8; ++i) {
    t[i + 8] = MAX(-128, MIN(127, (i16)Read16(y + i * 2)));
  }
  memcpy(x, t, 16);
}

static void SsePackssdw(u8 x[16], const u8 y[16]) {
  unsigned i;
  u8 t[16];
  for (i = 0; i < 4; ++i) {
    Write16(t + i * 2 + 0, MAX(-32768, MIN(32767, (i32)Read32(x + i * 4))));
  }
  for (i = 0; i < 4; ++i) {
    Write16(t + i * 2 + 8, MAX(-32768, MIN(32767, (i32)Read32(y + i * 4))));
  }
  memcpy(x, t, 16);
}

static void SsePsadbw(u8 x[16], const u8 y[16]) {
  unsigned i, s, t;
  for (s = i = 0; i < 8; ++i) s += ABS(x[i] - y[i]);
  for (t = 0; i < 16; ++i) t += ABS(x[i] - y[i]);
  Write64(x + 0, s);
  Write64(x + 8, t);
}

static void SsePmuludq(u8 x[16], const u8 y[16]) {
  unsigned i;
  for (i = 0; i < 2; ++i) {
    Write64(x + i * 8, (u64)Read32(x + i * 8) * Read32(y + i * 8));
  }
}

static void SsePshufb(u8 x[16], const u8 y[16]) {
  unsigned i;
  u8 t[16];
  for (i = 0; i < 16; ++i) {
    t[i] = (y[i] & 128) ? 0 : x[y[i] & 15];
  }
  memcpy(x, t, 16);
}

static void SsePhaddd(u8 x[16], const u8 y[16]) {
  u8 t[16];
  Write32(t + 0 * 4, Read32(x + 0 * 4) + Read32(x + 1 * 4));
  Write32(t + 1 * 4, Read32(x + 2 * 4) + Read32(x + 3 * 4));
  Write32(t + 2 * 4, Read32(y + 0 * 4) + Read32(y + 1 * 4));
  Write32(t + 3 * 4, Read32(y + 2 * 4) + Read32(y + 3 * 4));
  memcpy(x, t, 16);
}

static void SsePhsubd(u8 x[16], const u8 y[16]) {
  u8 t[16];
  Write32(t + 0 * 4, Read32(x + 0 * 4) - Read32(x + 1 * 4));
  Write32(t + 1 * 4, Read32(x + 2 * 4) - Read32(x + 3 * 4));
  Write32(t + 2 * 4, Read32(y + 0 * 4) - Read32(y + 1 * 4));
  Write32(t + 3 * 4, Read32(y + 2 * 4) - Read32(y + 3 * 4));
  memcpy(x, t, 16);
}

static void SsePhaddw(u8 x[16], const u8 y[16]) {
  u8 t[16];
  Write16(t + 0 * 2, Read16(x + 0 * 2) + Read16(x + 1 * 2));
  Write16(t + 1 * 2, Read16(x + 2 * 2) + Read16(x + 3 * 2));
  Write16(t + 2 * 2, Read16(x + 4 * 2) + Read16(x + 5 * 2));
  Write16(t + 3 * 2, Read16(x + 6 * 2) + Read16(x + 7 * 2));
  Write16(t + 4 * 2, Read16(y + 0 * 2) + Read16(y + 1 * 2));
  Write16(t + 5 * 2, Read16(y + 2 * 2) + Read16(y + 3 * 2));
  Write16(t + 6 * 2, Read16(y + 4 * 2) + Read16(y + 5 * 2));
  Write16(t + 7 * 2, Read16(y + 6 * 2) + Read16(y + 7 * 2));
  memcpy(x, t, 16);
}

static void SsePhsubw(u8 x[16], const u8 y[16]) {
  u8 t[16];
  Write16(t + 0 * 2, Read16(x + 0 * 2) - Read16(x + 1 * 2));
  Write16(t + 1 * 2, Read16(x + 2 * 2) - Read16(x + 3 * 2));
  Write16(t + 2 * 2, Read16(x + 4 * 2) - Read16(x + 5 * 2));
  Write16(t + 3 * 2, Read16(x + 6 * 2) - Read16(x + 7 * 2));
  Write16(t + 4 * 2, Read16(y + 0 * 2) - Read16(y + 1 * 2));
  Write16(t + 5 * 2, Read16(y + 2 * 2) - Read16(y + 3 * 2));
  Write16(t + 6 * 2, Read16(y + 4 * 2) - Read16(y + 5 * 2));
  Write16(t + 7 * 2, Read16(y + 6 * 2) - Read16(y + 7 * 2));
  memcpy(x, t, 16);
}

static void SsePhaddsw(u8 x[16], const u8 y[16]) {
  u8 t[16];
  Write16(t + 0 * 2, MAX(-32768, MIN(32767, ((i16)Read16(x + 0 * 2) +
                                             (i16)Read16(x + 1 * 2)))));
  Write16(t + 1 * 2, MAX(-32768, MIN(32767, ((i16)Read16(x + 2 * 2) +
                                             (i16)Read16(x + 3 * 2)))));
  Write16(t + 2 * 2, MAX(-32768, MIN(32767, ((i16)Read16(x + 4 * 2) +
                                             (i16)Read16(x + 5 * 2)))));
  Write16(t + 3 * 2, MAX(-32768, MIN(32767, ((i16)Read16(x + 6 * 2) +
                                             (i16)Read16(x + 7 * 2)))));
  Write16(t + 4 * 2, MAX(-32768, MIN(32767, ((i16)Read16(y + 0 * 2) +
                                             (i16)Read16(y + 1 * 2)))));
  Write16(t + 5 * 2, MAX(-32768, MIN(32767, ((i16)Read16(y + 2 * 2) +
                                             (i16)Read16(y + 3 * 2)))));
  Write16(t + 6 * 2, MAX(-32768, MIN(32767, ((i16)Read16(y + 4 * 2) +
                                             (i16)Read16(y + 5 * 2)))));
  Write16(t + 7 * 2, MAX(-32768, MIN(32767, ((i16)Read16(y + 6 * 2) +
                                             (i16)Read16(y + 7 * 2)))));
  memcpy(x, t, 16);
}

static void SsePhsubsw(u8 x[16], const u8 y[16]) {
  u8 t[16];
  Write16(t + 0 * 2, MAX(-32768, MIN(32767, ((i16)Read16(x + 0 * 2) -
                                             (i16)Read16(x + 1 * 2)))));
  Write16(t + 1 * 2, MAX(-32768, MIN(32767, ((i16)Read16(x + 2 * 2) -
                                             (i16)Read16(x + 3 * 2)))));
  Write16(t + 2 * 2, MAX(-32768, MIN(32767, ((i16)Read16(x + 4 * 2) -
                                             (i16)Read16(x + 5 * 2)))));
  Write16(t + 3 * 2, MAX(-32768, MIN(32767, ((i16)Read16(x + 6 * 2) -
                                             (i16)Read16(x + 7 * 2)))));
  Write16(t + 4 * 2, MAX(-32768, MIN(32767, ((i16)Read16(y + 0 * 2) -
                                             (i16)Read16(y + 1 * 2)))));
  Write16(t + 5 * 2, MAX(-32768, MIN(32767, ((i16)Read16(y + 2 * 2) -
                                             (i16)Read16(y + 3 * 2)))));
  Write16(t + 6 * 2, MAX(-32768, MIN(32767, ((i16)Read16(y + 4 * 2) -
                                             (i16)Read16(y + 5 * 2)))));
  Write16(t + 7 * 2, MAX(-32768, MIN(32767, ((i16)Read16(y + 6 * 2) -
                                             (i16)Read16(y + 7 * 2)))));
  memcpy(x, t, 16);
}

static void SsePunpcklbw(u8 x[16], const u8 y[16]) {
  x[0xf] = y[0x7];
  x[0xe] = x[0x7];
  x[0xd] = y[0x6];
  x[0xc] = x[0x6];
  x[0xb] = y[0x5];
  x[0xa] = x[0x5];
  x[0x9] = y[0x4];
  x[0x8] = x[0x4];
  x[0x7] = y[0x3];
  x[0x6] = x[0x3];
  x[0x5] = y[0x2];
  x[0x4] = x[0x2];
  x[0x3] = y[0x1];
  x[0x2] = x[0x1];
  x[0x1] = y[0x0];
  x[0x0] = x[0x0];
}

static void SsePunpckhbw(u8 x[16], const u8 y[16]) {
  x[0x0] = x[0x8];
  x[0x1] = y[0x8];
  x[0x2] = x[0x9];
  x[0x3] = y[0x9];
  x[0x4] = x[0xa];
  x[0x5] = y[0xa];
  x[0x6] = x[0xb];
  x[0x7] = y[0xb];
  x[0x8] = x[0xc];
  x[0x9] = y[0xc];
  x[0xa] = x[0xd];
  x[0xb] = y[0xd];
  x[0xc] = x[0xe];
  x[0xd] = y[0xe];
  x[0xe] = x[0xf];
  x[0xf] = y[0xf];
}

static void SsePunpcklwd(u8 x[16], const u8 y[16]) {
  x[0xe] = y[0x6];
  x[0xf] = y[0x7];
  x[0xc] = x[0x6];
  x[0xd] = x[0x7];
  x[0xa] = y[0x4];
  x[0xb] = y[0x5];
  x[0x8] = x[0x4];
  x[0x9] = x[0x5];
  x[0x6] = y[0x2];
  x[0x7] = y[0x3];
  x[0x4] = x[0x2];
  x[0x5] = x[0x3];
  x[0x2] = y[0x0];
  x[0x3] = y[0x1];
  x[0x0] = x[0x0];
  x[0x1] = x[0x1];
}

static void SsePunpckldq(u8 x[16], const u8 y[16]) {
  x[0xc] = y[0x4];
  x[0xd] = y[0x5];
  x[0xe] = y[0x6];
  x[0xf] = y[0x7];
  x[0x8] = x[0x4];
  x[0x9] = x[0x5];
  x[0xa] = x[0x6];
  x[0xb] = x[0x7];
  x[0x4] = y[0x0];
  x[0x5] = y[0x1];
  x[0x6] = y[0x2];
  x[0x7] = y[0x3];
  x[0x0] = x[0x0];
  x[0x1] = x[0x1];
  x[0x2] = x[0x2];
  x[0x3] = x[0x3];
}

static void SsePunpckhwd(u8 x[16], const u8 y[16]) {
  x[0x0] = x[0x8];
  x[0x1] = x[0x9];
  x[0x2] = y[0x8];
  x[0x3] = y[0x9];
  x[0x4] = x[0xa];
  x[0x5] = x[0xb];
  x[0x6] = y[0xa];
  x[0x7] = y[0xb];
  x[0x8] = x[0xc];
  x[0x9] = x[0xd];
  x[0xa] = y[0xc];
  x[0xb] = y[0xd];
  x[0xc] = x[0xe];
  x[0xd] = x[0xf];
  x[0xe] = y[0xe];
  x[0xf] = y[0xf];
}

static void SsePunpckhdq(u8 x[16], const u8 y[16]) {
  x[0x0] = x[0x8];
  x[0x1] = x[0x9];
  x[0x2] = x[0xa];
  x[0x3] = x[0xb];
  x[0x4] = y[0x8];
  x[0x5] = y[0x9];
  x[0x6] = y[0xa];
  x[0x7] = y[0xb];
  x[0x8] = x[0xc];
  x[0x9] = x[0xd];
  x[0xa] = x[0xe];
  x[0xb] = x[0xf];
  x[0xc] = y[0xc];
  x[0xd] = y[0xd];
  x[0xe] = y[0xe];
  x[0xf] = y[0xf];
}

static void SsePunpcklqdq(u8 x[16], const u8 y[16]) {
  x[0x8] = y[0x0];
  x[0x9] = y[0x1];
  x[0xa] = y[0x2];
  x[0xb] = y[0x3];
  x[0xc] = y[0x4];
  x[0xd] = y[0x5];
  x[0xe] = y[0x6];
  x[0xf] = y[0x7];
  x[0x0] = x[0x0];
  x[0x1] = x[0x1];
  x[0x2] = x[0x2];
  x[0x3] = x[0x3];
  x[0x4] = x[0x4];
  x[0x5] = x[0x5];
  x[0x6] = x[0x6];
  x[0x7] = x[0x7];
}

static void SsePunpckhqdq(u8 x[16], const u8 y[16]) {
  x[0x0] = x[0x8];
  x[0x1] = x[0x9];
  x[0x2] = x[0xa];
  x[0x3] = x[0xb];
  x[0x4] = x[0xc];
  x[0x5] = x[0xd];
  x[0x6] = x[0xe];
  x[0x7] = x[0xf];
  x[0x8] = y[0x8];
  x[0x9] = y[0x9];
  x[0xa] = y[0xa];
  x[0xb] = y[0xb];
  x[0xc] = y[0xc];
  x[0xd] = y[0xd];
  x[0xe] = y[0xe];
  x[0xf] = y[0xf];
}

static void SsePsrlw(u8 x[16], unsigned k) {
  MmxPsrlw(x + 0, k);
  MmxPsrlw(x + 8, k);
}

static void SsePsraw(u8 x[16], unsigned k) {
  MmxPsraw(x + 0, k);
  MmxPsraw(x + 8, k);
}

static void SsePsllw(u8 x[16], unsigned k) {
  MmxPsllw(x + 0, k);
  MmxPsllw(x + 8, k);
}

static void SsePsrld(u8 x[16], unsigned k) {
  MmxPsrld(x + 0, k);
  MmxPsrld(x + 8, k);
}

static void SsePsrad(u8 x[16], unsigned k) {
  MmxPsrad(x + 0, k);
  MmxPsrad(x + 8, k);
}

static void SsePslld(u8 x[16], unsigned k) {
  MmxPslld(x + 0, k);
  MmxPslld(x + 8, k);
}

static void SsePsrlq(u8 x[16], unsigned k) {
  MmxPsrlq(x + 0, k);
  MmxPsrlq(x + 8, k);
}

static void SsePsllq(u8 x[16], unsigned k) {
  MmxPsllq(x + 0, k);
  MmxPsllq(x + 8, k);
}

static void SsePsubsb(u8 x[16], const u8 y[16]) {
  MmxPsubsb(x + 0, y + 0);
  MmxPsubsb(x + 8, y + 8);
}

static void SsePaddsb(u8 x[16], const u8 y[16]) {
  MmxPaddsb(x + 0, y + 0);
  MmxPaddsb(x + 8, y + 8);
}

static void SsePsubsw(u8 x[16], const u8 y[16]) {
  MmxPsubsw(x + 0, y + 0);
  MmxPsubsw(x + 8, y + 8);
}

static void SsePaddsw(u8 x[16], const u8 y[16]) {
  MmxPaddsw(x + 0, y + 0);
  MmxPaddsw(x + 8, y + 8);
}

static void SsePaddusb(u8 x[16], const u8 y[16]) {
  MmxPaddusb(x + 0, y + 0);
  MmxPaddusb(x + 8, y + 8);
}

static void SsePsubusb(u8 x[16], const u8 y[16]) {
  MmxPsubusb(x + 0, y + 0);
  MmxPsubusb(x + 8, y + 8);
}

static void SsePsubusw(u8 x[16], const u8 y[16]) {
  MmxPsubusw(x + 0, y + 0);
  MmxPsubusw(x + 8, y + 8);
}

static void SsePminsw(u8 x[16], const u8 y[16]) {
  MmxPminsw(x + 0, y + 0);
  MmxPminsw(x + 8, y + 8);
}

static void SsePmaxsw(u8 x[16], const u8 y[16]) {
  MmxPmaxsw(x + 0, y + 0);
  MmxPmaxsw(x + 8, y + 8);
}

static void SsePsignb(u8 x[16], const u8 y[16]) {
  MmxPsignb(x + 0, y + 0);
  MmxPsignb(x + 8, y + 8);
}

static void SsePsignw(u8 x[16], const u8 y[16]) {
  MmxPsignw(x + 0, y + 0);
  MmxPsignw(x + 8, y + 8);
}

static void SsePsignd(u8 x[16], const u8 y[16]) {
  MmxPsignd(x + 0, y + 0);
  MmxPsignd(x + 8, y + 8);
}

static void SsePmulhrsw(u8 x[16], const u8 y[16]) {
  MmxPmulhrsw(x + 0, y + 0);
  MmxPmulhrsw(x + 8, y + 8);
}

static void SsePabsw(u8 x[16], const u8 y[16]) {
  MmxPabsw(x + 0, y + 0);
  MmxPabsw(x + 8, y + 8);
}

static void SsePabsd(u8 x[16], const u8 y[16]) {
  MmxPabsd(x + 0, y + 0);
  MmxPabsd(x + 8, y + 8);
}

static void SsePcmpgtw(u8 x[16], const u8 y[16]) {
  MmxPcmpgtw(x + 0, y + 0);
  MmxPcmpgtw(x + 8, y + 8);
}

static void SsePcmpeqw(u8 x[16], const u8 y[16]) {
  MmxPcmpeqw(x + 0, y + 0);
  MmxPcmpeqw(x + 8, y + 8);
}

static void SsePcmpgtd(u8 x[16], const u8 y[16]) {
  MmxPcmpgtd(x + 0, y + 0);
  MmxPcmpgtd(x + 8, y + 8);
}

static void SsePcmpeqd(u8 x[16], const u8 y[16]) {
  MmxPcmpeqd(x + 0, y + 0);
  MmxPcmpeqd(x + 8, y + 8);
}

static void SsePsrawv(u8 x[16], const u8 y[16]) {
  MmxPsrawv(x + 0, y);
  MmxPsrawv(x + 8, y);
}

static void SsePsradv(u8 x[16], const u8 y[16]) {
  MmxPsradv(x + 0, y);
  MmxPsradv(x + 8, y);
}

static void SsePsrlwv(u8 x[16], const u8 y[16]) {
  MmxPsrlwv(x + 0, y);
  MmxPsrlwv(x + 8, y);
}

static void SsePsllwv(u8 x[16], const u8 y[16]) {
  MmxPsllwv(x + 0, y);
  MmxPsllwv(x + 8, y);
}

static void SsePsrldv(u8 x[16], const u8 y[16]) {
  MmxPsrldv(x + 0, y);
  MmxPsrldv(x + 8, y);
}

static void SsePslldv(u8 x[16], const u8 y[16]) {
  MmxPslldv(x + 0, y);
  MmxPslldv(x + 8, y);
}

static void SsePsrlqv(u8 x[16], const u8 y[16]) {
  MmxPsrlqv(x + 0, y);
  MmxPsrlqv(x + 8, y);
}

static void SsePsllqv(u8 x[16], const u8 y[16]) {
  MmxPsllqv(x + 0, y);
  MmxPsllqv(x + 8, y);
}

static void SsePavgw(u8 x[16], const u8 y[16]) {
  MmxPavgw(x + 0, y + 0);
  MmxPavgw(x + 8, y + 8);
}

static void SsePmaddwd(u8 x[16], const u8 y[16]) {
  MmxPmaddwd(x + 0, y + 0);
  MmxPmaddwd(x + 8, y + 8);
}

static void SsePmulhuw(u8 x[16], const u8 y[16]) {
  MmxPmulhuw(x + 0, y + 0);
  MmxPmulhuw(x + 8, y + 8);
}

static void SsePmulhw(u8 x[16], const u8 y[16]) {
  MmxPmulhw(x + 0, y + 0);
  MmxPmulhw(x + 8, y + 8);
}

static void SsePmullw(u8 x[16], const u8 y[16]) {
  MmxPmullw(x + 0, y + 0);
  MmxPmullw(x + 8, y + 8);
}

static void SsePmulld(u8 x[16], const u8 y[16]) {
  MmxPmulld(x + 0, y + 0);
  MmxPmulld(x + 8, y + 8);
}

static void SsePmaddubsw(u8 x[16], const u8 y[16]) {
  MmxPmaddubsw(x + 0, y + 0);
  MmxPmaddubsw(x + 8, y + 8);
}

static void OpPsb(struct Machine *m, u32 rde,
                  void MmxKernel(u8[8], unsigned),
                  void SseKernel(u8[16], unsigned)) {
  if (Osz(rde)) {
    SseKernel(XmmRexbRm(m, rde), m->xedd->op.uimm0);
  } else {
    MmxKernel(XmmRexbRm(m, rde), m->xedd->op.uimm0);
  }
}

void Op171(struct Machine *m, u32 rde) {
  switch (ModrmReg(rde)) {
    case 2:
      OpPsb(m, rde, MmxPsrlw, SsePsrlw);
      break;
    case 4:
      OpPsb(m, rde, MmxPsraw, SsePsraw);
      break;
    case 6:
      OpPsb(m, rde, MmxPsllw, SsePsllw);
      break;
    default:
      OpUd(m, rde);
  }
}

void Op172(struct Machine *m, u32 rde) {
  switch (ModrmReg(rde)) {
    case 2:
      OpPsb(m, rde, MmxPsrld, SsePsrld);
      break;
    case 4:
      OpPsb(m, rde, MmxPsrad, SsePsrad);
      break;
    case 6:
      OpPsb(m, rde, MmxPslld, SsePslld);
      break;
    default:
      OpUd(m, rde);
  }
}

void Op173(struct Machine *m, u32 rde) {
  switch (ModrmReg(rde)) {
    case 2:
      OpPsb(m, rde, MmxPsrlq, SsePsrlq);
      break;
    case 3:
      OpPsb(m, rde, MmxPsrldq, SsePsrldq);
      break;
    case 6:
      OpPsb(m, rde, MmxPsllq, SsePsllq);
      break;
    case 7:
      OpPsb(m, rde, MmxPslldq, SsePslldq);
      break;
    default:
      OpUd(m, rde);
  }
}

void OpSsePalignr(struct Machine *m, u32 rde) {
  if (Osz(rde)) {
    SsePalignr(XmmRexrReg(m, rde), GetModrmRegisterXmmPointerRead16(m, rde),
               m->xedd->op.uimm0);
  } else {
    MmxPalignr(XmmRexrReg(m, rde), GetModrmRegisterXmmPointerRead8(m, rde),
               m->xedd->op.uimm0);
  }
}

static void OpSse(struct Machine *m, u32 rde,
                  void MmxKernel(u8[8], const u8[8]),
                  void SseKernel(u8[16], const u8[16])) {
  if (Osz(rde)) {
    SseKernel(XmmRexrReg(m, rde), GetModrmRegisterXmmPointerRead16(m, rde));
  } else {
    MmxKernel(XmmRexrReg(m, rde), GetModrmRegisterXmmPointerRead8(m, rde));
  }
}

/* clang-format off */
void OpSsePunpcklbw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPunpcklbw, SsePunpcklbw); }
void OpSsePunpcklwd(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPunpcklwd, SsePunpcklwd); }
void OpSsePunpckldq(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPunpckldq, SsePunpckldq); }
void OpSsePacksswb(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPacksswb, SsePacksswb); }
void OpSsePcmpgtb(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPcmpgtb, SsePcmpgtb); }
void OpSsePcmpgtw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPcmpgtw, SsePcmpgtw); }
void OpSsePcmpgtd(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPcmpgtd, SsePcmpgtd); }
void OpSsePackuswb(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPackuswb, SsePackuswb); }
void OpSsePunpckhbw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPunpckhbw, SsePunpckhbw); }
void OpSsePunpckhwd(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPunpckhwd, SsePunpckhwd); }
void OpSsePunpckhdq(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPunpckhdq, SsePunpckhdq); }
void OpSsePackssdw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPackssdw, SsePackssdw); }
void OpSsePunpcklqdq(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPunpcklqdq, SsePunpcklqdq); }
void OpSsePunpckhqdq(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPunpckhqdq, SsePunpckhqdq); }
void OpSsePcmpeqb(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPcmpeqb, SsePcmpeqb); }
void OpSsePcmpeqw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPcmpeqw, SsePcmpeqw); }
void OpSsePcmpeqd(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPcmpeqd, SsePcmpeqd); }
void OpSsePsrlwv(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPsrlwv, SsePsrlwv); }
void OpSsePsrldv(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPsrldv, SsePsrldv); }
void OpSsePsrlqv(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPsrlqv, SsePsrlqv); }
void OpSsePaddq(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPaddq, SsePaddq); }
void OpSsePmullw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPmullw, SsePmullw); }
void OpSsePsubusb(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPsubusb, SsePsubusb); }
void OpSsePsubusw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPsubusw, SsePsubusw); }
void OpSsePminub(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPminub, SsePminub); }
void OpSsePand(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPand, SsePand); }
void OpSsePaddusb(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPaddusb, SsePaddusb); }
void OpSsePaddusw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPaddusw, SsePaddusw); }
void OpSsePmaxub(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPmaxub, SsePmaxub); }
void OpSsePandn(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPandn, SsePandn); }
void OpSsePavgb(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPavgb, SsePavgb); }
void OpSsePsrawv(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPsrawv, SsePsrawv); }
void OpSsePsradv(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPsradv, SsePsradv); }
void OpSsePavgw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPavgw, SsePavgw); }
void OpSsePmulhuw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPmulhuw, SsePmulhuw); }
void OpSsePmulhw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPmulhw, SsePmulhw); }
void OpSsePsubsb(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPsubsb, SsePsubsb); }
void OpSsePsubsw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPsubsw, SsePsubsw); }
void OpSsePminsw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPminsw, SsePminsw); }
void OpSsePor(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPor, SsePor); }
void OpSsePaddsb(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPaddsb, SsePaddsb); }
void OpSsePaddsw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPaddsw, SsePaddsw); }
void OpSsePmaxsw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPmaxsw, SsePmaxsw); }
void OpSsePxor(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPxor, SsePxor); }
void OpSsePsllwv(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPsllwv, SsePsllwv); }
void OpSsePslldv(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPslldv, SsePslldv); }
void OpSsePsllqv(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPsllqv, SsePsllqv); }
void OpSsePmuludq(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPmuludq, SsePmuludq); }
void OpSsePmaddwd(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPmaddwd, SsePmaddwd); }
void OpSsePsadbw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPsadbw, SsePsadbw); }
void OpSsePsubb(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPsubb, SsePsubb); }
void OpSsePsubw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPsubw, SsePsubw); }
void OpSsePsubd(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPsubd, SsePsubd); }
void OpSsePsubq(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPsubq, SsePsubq); }
void OpSsePaddb(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPaddb, SsePaddb); }
void OpSsePaddw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPaddw, SsePaddw); }
void OpSsePaddd(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPaddd, SsePaddd); }
void OpSsePshufb(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPshufb, SsePshufb); }
void OpSsePhaddw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPhaddw, SsePhaddw); }
void OpSsePhaddd(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPhaddd, SsePhaddd); }
void OpSsePhaddsw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPhaddsw, SsePhaddsw); }
void OpSsePmaddubsw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPmaddubsw, SsePmaddubsw); }
void OpSsePhsubw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPhsubw, SsePhsubw); }
void OpSsePhsubd(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPhsubd, SsePhsubd); }
void OpSsePhsubsw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPhsubsw, SsePhsubsw); }
void OpSsePsignb(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPsignb, SsePsignb); }
void OpSsePsignw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPsignw, SsePsignw); }
void OpSsePsignd(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPsignd, SsePsignd); }
void OpSsePmulhrsw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPmulhrsw, SsePmulhrsw); }
void OpSsePabsb(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPabsb, SsePabsb); }
void OpSsePabsw(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPabsw, SsePabsw); }
void OpSsePabsd(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPabsd, SsePabsd); }
void OpSsePmulld(struct Machine *m, u32 rde) { OpSse(m, rde, MmxPmulld, SsePmulld); }
/* clang-format on */
