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

/**
 * @fileoverview SSE operations that compilers are good at vectoring.
 */

#define COPY16_X_Y_FROM_x_y(T)                                   \
  T X[8] = {(T)Get16(x),      (T)Get16(x + 2), (T)Get16(x + 4),  \
            (T)Get16(x + 6),  (T)Get16(x + 8), (T)Get16(x + 10), \
            (T)Get16(x + 12), (T)Get16(x + 14)};                 \
  T Y[8] = {(T)Get16(y),      (T)Get16(y + 2), (T)Get16(y + 4),  \
            (T)Get16(y + 6),  (T)Get16(y + 8), (T)Get16(y + 10), \
            (T)Get16(y + 12), (T)Get16(y + 14)}

#define COPY16_x_FROM_X              \
  for (unsigned i = 0; i < 8; ++i) { \
    Put16(x + i * 2, X[i]);          \
  }

static void MmxPor(u8 x[8], const u8 y[8]) {
  *(u64 *)x |= *(u64 *)y;
}

static void MmxPxor(u8 x[8], const u8 y[8]) {
  *(u64 *)x ^= *(u64 *)y;
}

static void MmxPand(u8 x[8], const u8 y[8]) {
  *(u64 *)x &= *(u64 *)y;
}

static void MmxPandn(u8 x[8], const u8 y[8]) {
  *(u64 *)x = ~*(u64 *)x & *(u64 *)y;
}

static void SsePsubw(u8 x[16], const u8 y[16]) {
  COPY16_X_Y_FROM_x_y(u16);
  for (unsigned i = 0; i < 8; ++i) {
    X[i] -= Y[i];
  }
  COPY16_x_FROM_X;
}

static void SsePaddw(u8 x[16], const u8 y[16]) {
  COPY16_X_Y_FROM_x_y(u16);
  for (unsigned i = 0; i < 8; ++i) {
    X[i] += Y[i];
  }
  COPY16_x_FROM_X;
}

static void SsePaddusw(u8 x[16], const u8 y[16]) {
  COPY16_X_Y_FROM_x_y(u16);
  for (unsigned i = 0; i < 8; ++i) {
    X[i] = MIN(65535, X[i] + Y[i]);
  }
  COPY16_x_FROM_X;
}

static void SsePhaddsw(u8 x[16], const u8 y[16]) {
  i16 t[8];
  COPY16_X_Y_FROM_x_y(i16);
  for (unsigned i = 0; i < 4; ++i) {
    t[i] = MAX(-32768, MIN(32767, X[i * 2] + X[i * 2 + 1]));
  }
  for (unsigned i = 0; i < 4; ++i) {
    t[i + 4] = MAX(-32768, MIN(32767, Y[i * 2] + Y[i * 2 + 1]));
  }
  memcpy(X, t, 16);
  COPY16_x_FROM_X;
}

static void SsePhsubsw(u8 x[16], const u8 y[16]) {
  i16 t[8];
  COPY16_X_Y_FROM_x_y(i16);
  for (unsigned i = 0; i < 4; ++i) {
    t[i] = MAX(-32768, MIN(32767, X[i * 2] - X[i * 2 + 1]));
  }
  for (unsigned i = 0; i < 4; ++i) {
    t[i + 4] = MAX(-32768, MIN(32767, Y[i * 2] - Y[i * 2 + 1]));
  }
  memcpy(X, t, 16);
  COPY16_x_FROM_X;
}

static void SsePsubsw(u8 x[16], const u8 y[16]) {
  COPY16_X_Y_FROM_x_y(i16);
  for (unsigned i = 0; i < 8; ++i) {
    X[i] = MAX(-32768, MIN(32767, X[i] - Y[i]));
  }
  COPY16_x_FROM_X;
}

static void SsePaddsw(u8 x[16], const u8 y[16]) {
  COPY16_X_Y_FROM_x_y(i16);
  for (unsigned i = 0; i < 8; ++i) {
    X[i] = MAX(-32768, MIN(32767, X[i] + Y[i]));
  }
  COPY16_x_FROM_X;
}

static void SsePcmpgtw(u8 x[16], const u8 y[16]) {
  COPY16_X_Y_FROM_x_y(i16);
  for (unsigned i = 0; i < 8; ++i) {
    X[i] = -(X[i] > Y[i]);
  }
  COPY16_x_FROM_X;
}

static void SsePcmpeqw(u8 x[16], const u8 y[16]) {
  COPY16_X_Y_FROM_x_y(i16);
  for (unsigned i = 0; i < 8; ++i) {
    X[i] = -(X[i] == Y[i]);
  }
  COPY16_x_FROM_X;
}

static void SsePavgw(u8 x[16], const u8 y[16]) {
  COPY16_X_Y_FROM_x_y(u16);
  for (unsigned i = 0; i < 8; ++i) {
    X[i] = (X[i] + Y[i] + 1) >> 1;
  }
  COPY16_x_FROM_X;
}

static void SsePmulhw(u8 x[16], const u8 y[16]) {
  COPY16_X_Y_FROM_x_y(i16);
  for (unsigned i = 0; i < 8; ++i) {
    X[i] = (X[i] * Y[i]) >> 16;
  }
  COPY16_x_FROM_X;
}

static void SsePmullw(u8 x[16], const u8 y[16]) {
  COPY16_X_Y_FROM_x_y(i16);
  for (unsigned i = 0; i < 8; ++i) {
    X[i] *= Y[i];
  }
  COPY16_x_FROM_X;
}

static void SsePsubsb(u8 x[16], const u8 y[16]) {
  i8 X[16], Y[16];
  memcpy(X, x, 16);
  memcpy(Y, y, 16);
  for (unsigned i = 0; i < 16; ++i) {
    X[i] = MAX(-128, MIN(127, X[i] - Y[i]));
  }
  memcpy(x, X, 16);
}

static void SsePaddsb(u8 x[16], const u8 y[16]) {
  i8 X[16], Y[16];
  memcpy(X, x, 16);
  memcpy(Y, y, 16);
  for (unsigned i = 0; i < 16; ++i) {
    X[i] = MAX(-128, MIN(127, X[i] + Y[i]));
  }
  memcpy(x, X, 16);
}

static void SsePaddusb(u8 x[16], const u8 y[16]) {
  u8 X[16], Y[16];
  memcpy(X, x, 16);
  memcpy(Y, y, 16);
  for (unsigned i = 0; i < 16; ++i) {
    X[i] = MIN(255, X[i] + Y[i]);
  }
  memcpy(x, X, 16);
}

static void SsePsubusb(u8 x[16], const u8 y[16]) {
  u8 X[16], Y[16];
  memcpy(X, x, 16);
  memcpy(Y, y, 16);
  for (unsigned i = 0; i < 16; ++i) {
    X[i] = MIN(255, MAX(0, X[i] - Y[i]));
  }
  memcpy(x, X, 16);
}

static void SsePsubb(u8 x[16], const u8 y[16]) {
  i8 X[16], Y[16];
  memcpy(X, x, 16);
  memcpy(Y, y, 16);
  for (unsigned i = 0; i < 16; ++i) {
    X[i] -= Y[i];
  }
  memcpy(x, X, 16);
}

static void SsePaddb(u8 x[16], const u8 y[16]) {
  i8 X[16], Y[16];
  memcpy(X, x, 16);
  memcpy(Y, y, 16);
  for (unsigned i = 0; i < 16; ++i) {
    X[i] += Y[i];
  }
  memcpy(x, X, 16);
}

static void SsePor(u8 x[16], const u8 y[16]) {
  i8 X[16], Y[16];
  memcpy(X, x, 16);
  memcpy(Y, y, 16);
  for (unsigned i = 0; i < 16; ++i) {
    X[i] |= Y[i];
  }
  memcpy(x, X, 16);
}

static void SsePxor(u8 x[16], const u8 y[16]) {
  i8 X[16], Y[16];
  memcpy(X, x, 16);
  memcpy(Y, y, 16);
  for (unsigned i = 0; i < 16; ++i) {
    X[i] ^= Y[i];
  }
  memcpy(x, X, 16);
}

static void SsePand(u8 x[16], const u8 y[16]) {
  i8 X[16], Y[16];
  memcpy(X, x, 16);
  memcpy(Y, y, 16);
  for (unsigned i = 0; i < 16; ++i) {
    X[i] &= Y[i];
  }
  memcpy(x, X, 16);
}

static void SsePandn(u8 x[16], const u8 y[16]) {
  i8 X[16], Y[16];
  memcpy(X, x, 16);
  memcpy(Y, y, 16);
  for (unsigned i = 0; i < 16; ++i) {
    X[i] = ~X[i] & Y[i];
  }
  memcpy(x, X, 16);
}

static void SsePcmpeqb(u8 x[16], const u8 y[16]) {
  i8 X[16], Y[16];
  memcpy(X, x, 16);
  memcpy(Y, y, 16);
  for (unsigned i = 0; i < 16; ++i) {
    X[i] = -(X[i] == Y[i]);
  }
  memcpy(x, X, 16);
}

static void SsePcmpgtb(u8 x[16], const u8 y[16]) {
  i8 X[16], Y[16];
  memcpy(X, x, 16);
  memcpy(Y, y, 16);
  for (unsigned i = 0; i < 16; ++i) {
    X[i] = -(X[i] > Y[i]);
  }
  memcpy(x, X, 16);
}

static void SsePavgb(u8 x[16], const u8 y[16]) {
  u8 X[16], Y[16];
  memcpy(X, x, 16);
  memcpy(Y, y, 16);
  for (unsigned i = 0; i < 16; ++i) {
    X[i] = (X[i] + Y[i] + 1) >> 1;
  }
  memcpy(x, X, 16);
}

static void SsePabsb(u8 x[16], const u8 y[16]) {
  i8 X[16], Y[16];
  memcpy(X, x, 16);
  memcpy(Y, y, 16);
  for (unsigned i = 0; i < 16; ++i) {
    X[i] = ABS((i8)Y[i]);
  }
  memcpy(x, X, 16);
}

static void SsePminub(u8 x[16], const u8 y[16]) {
  u8 X[16], Y[16];
  memcpy(X, x, 16);
  memcpy(Y, y, 16);
  for (unsigned i = 0; i < 16; ++i) {
    X[i] = MIN(X[i], Y[i]);
  }
  memcpy(x, X, 16);
}

static void SsePmaxub(u8 x[16], const u8 y[16]) {
  u8 X[16], Y[16];
  memcpy(X, x, 16);
  memcpy(Y, y, 16);
  for (unsigned i = 0; i < 16; ++i) {
    X[i] = MAX(X[i], Y[i]);
  }
  memcpy(x, X, 16);
}

// clang-format off
void OpSsePcmpgtb(P) { OpSse(A, MmxPcmpgtb, SsePcmpgtb); }
void OpSsePcmpgtw(P) { OpSse(A, MmxPcmpgtw, SsePcmpgtw); }
void OpSsePcmpeqb(P) { OpSse(A, MmxPcmpeqb, SsePcmpeqb); }
void OpSsePcmpeqw(P) { OpSse(A, MmxPcmpeqw, SsePcmpeqw); }
void OpSsePmullw(P) { OpSse(A, MmxPmullw, SsePmullw); }
void OpSsePminub(P) { OpSse(A, MmxPminub, SsePminub); }
void OpSsePand(P) { OpSse(A, MmxPand, SsePand); }
void OpSsePaddusw(P) { OpSse(A, MmxPaddusw, SsePaddusw); }
void OpSsePmaxub(P) { OpSse(A, MmxPmaxub, SsePmaxub); }
void OpSsePandn(P) { OpSse(A, MmxPandn, SsePandn); }
void OpSsePavgb(P) { OpSse(A, MmxPavgb, SsePavgb); }
void OpSsePavgw(P) { OpSse(A, MmxPavgw, SsePavgw); }
void OpSsePmulhw(P) { OpSse(A, MmxPmulhw, SsePmulhw); }
void OpSsePsubsw(P) { OpSse(A, MmxPsubsw, SsePsubsw); }
void OpSsePor(P) { OpSse(A, MmxPor, SsePor); }
void OpSsePaddsw(P) { OpSse(A, MmxPaddsw, SsePaddsw); }
void OpSsePxor(P) { OpSse(A, MmxPxor, SsePxor); }
void OpSsePsubb(P) { OpSse(A, MmxPsubb, SsePsubb); }
void OpSsePsubw(P) { OpSse(A, MmxPsubw, SsePsubw); }
void OpSsePaddb(P) { OpSse(A, MmxPaddb, SsePaddb); }
void OpSsePaddw(P) { OpSse(A, MmxPaddw, SsePaddw); }
void OpSsePhaddsw(P) { OpSse(A, MmxPhaddsw, SsePhaddsw); }
void OpSsePhsubsw(P) { OpSse(A, MmxPhsubsw, SsePhsubsw); }
void OpSsePabsb(P) { OpSse(A, MmxPabsb, SsePabsb); }
