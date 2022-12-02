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

#include "blink/endian.h"
#include "blink/macros.h"
#include "blink/sse.h"

relegated void MmxPsubb(u8 x[8], const u8 y[8]) {
  for (unsigned i = 0; i < 8; ++i) {
    x[i] -= y[i];
  }
}

relegated void MmxPaddb(u8 x[8], const u8 y[8]) {
  for (unsigned i = 0; i < 8; ++i) {
    x[i] += y[i];
  }
}

relegated void MmxPavgb(u8 x[8], const u8 y[8]) {
  for (unsigned i = 0; i < 8; ++i) {
    x[i] = (x[i] + y[i] + 1) >> 1;
  }
}

relegated void MmxPabsb(u8 x[8], const u8 y[8]) {
  for (unsigned i = 0; i < 8; ++i) {
    x[i] = ABS((i8)y[i]);
  }
}

relegated void MmxPminub(u8 x[8], const u8 y[8]) {
  for (unsigned i = 0; i < 8; ++i) {
    x[i] = MIN(x[i], y[i]);
  }
}

relegated void MmxPmaxub(u8 x[8], const u8 y[8]) {
  for (unsigned i = 0; i < 8; ++i) {
    x[i] = MAX(x[i], y[i]);
  }
}

relegated void MmxPcmpeqb(u8 x[8], const u8 y[8]) {
  for (unsigned i = 0; i < 8; ++i) {
    x[i] = -(x[i] == y[i]);
  }
}

relegated void MmxPcmpgtb(u8 x[8], const u8 y[8]) {
  for (unsigned i = 0; i < 8; ++i) {
    x[i] = -((i8)x[i] > (i8)y[i]);
  }
}

relegated void MmxPsubw(u8 x[8], const u8 y[8]) {
  for (unsigned i = 0; i < 4; ++i) {
    Put16(x + i * 2, Get16(x + i * 2) - Get16(y + i * 2));
  }
}

relegated void MmxPaddw(u8 x[8], const u8 y[8]) {
  for (unsigned i = 0; i < 4; ++i) {
    Put16(x + i * 2, Get16(x + i * 2) + Get16(y + i * 2));
  }
}

relegated void MmxPaddsw(u8 x[8], const u8 y[8]) {
  for (unsigned i = 0; i < 4; ++i) {
    Put16(x + i * 2, MAX(-32768, MIN(32767, ((i16)Get16(x + i * 2) +
                                             (i16)Get16(y + i * 2)))));
  }
}

relegated void MmxPsubsw(u8 x[8], const u8 y[8]) {
  for (unsigned i = 0; i < 4; ++i) {
    Put16(x + i * 2, MAX(-32768, MIN(32767, ((i16)Get16(x + i * 2) -
                                             (i16)Get16(y + i * 2)))));
  }
}

relegated void MmxPaddusw(u8 x[8], const u8 y[8]) {
  for (unsigned i = 0; i < 4; ++i) {
    Put16(x + i * 2, MIN(65535, Get16(x + i * 2) + Get16(y + i * 2)));
  }
}

relegated void MmxPcmpgtw(u8 x[8], const u8 y[8]) {
  for (unsigned i = 0; i < 4; ++i) {
    Put16(x + i * 2, -((i16)Get16(x + i * 2) > (i16)Get16(y + i * 2)));
  }
}

relegated void MmxPcmpeqw(u8 x[8], const u8 y[8]) {
  for (unsigned i = 0; i < 4; ++i) {
    Put16(x + i * 2, -(Get16(x + i * 2) == Get16(y + i * 2)));
  }
}

relegated void MmxPavgw(u8 x[8], const u8 y[8]) {
  for (unsigned i = 0; i < 4; ++i) {
    Put16(x + i * 2, (Get16(x + i * 2) + Get16(y + i * 2) + 1) >> 1);
  }
}

relegated void MmxPmulhw(u8 x[8], const u8 y[8]) {
  for (unsigned i = 0; i < 4; ++i) {
    Put16(x + i * 2, ((i16)Get16(x + i * 2) * (i16)Get16(y + i * 2)) >> 16);
  }
}

relegated void MmxPmullw(u8 x[8], const u8 y[8]) {
  for (unsigned i = 0; i < 4; ++i) {
    Put16(x + i * 2, (i16)Get16(x + i * 2) * (i16)Get16(y + i * 2));
  }
}

static relegated int Add(int x, int y) {
  return x + y;
}

static relegated int Sub(int x, int y) {
  return x - y;
}

static relegated int Clamp16(int x) {
  return MAX(-32768, MIN(32767, x));
}

static relegated void Phsw(u8 x[8], const u8 y[8], int Op(int, int)) {
  u8 t[8];
  for (unsigned i = 0; i < 2; ++i) {
    Put16(t + i * 2, Clamp16(Op((i16)Get16(x + (i * 2) * 2),
                                (i16)Get16(x + (i * 2 + 1) * 2))));
  }
  for (unsigned i = 0; i < 2; ++i) {
    Put16(t + (2 + i) * 2, Clamp16(Op((i16)Get16(y + (i * 2) * 2),
                                      (i16)Get16(y + (i * 2 + 1) * 2))));
  }
  memcpy(x, t, 8);
}

relegated void MmxPhaddsw(u8 x[8], const u8 y[8]) {
  Phsw(x, y, Add);
}

relegated void MmxPhsubsw(u8 x[8], const u8 y[8]) {
  Phsw(x, y, Sub);
}
