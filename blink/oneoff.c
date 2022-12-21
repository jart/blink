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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/timespec.h"
#include "blink/types.h"
#include "blink/util.h"

static u64 Vigna(u64 s[1]) {
  u64 z = (s[0] += 0x9e3779b97f4a7c15);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
  z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
  return z ^ (z >> 31);
}

_Noreturn void TerminateSignal(struct Machine *m, int sig) {
  abort();
}

u16 add16(u16 x, u16 y, bool *cf, bool *zf) {
  u16 z = x + y;
  *cf = z < y;
  *zf = !z;
  return z;
}

u8 add8(u8 x, u8 y, bool *cf, bool *zf) {
  u8 z = x + y;
  *cf = z < y;
  *zf = !z;
  return z;
}

int main(int argc, char *argv[]) {
  int x, y;
  for (x = -128; x < 127; ++x) {
    for (y = -128; y < 127; ++y) {
      u8 z1, z2;
      bool cf1, cf2;
      bool zf1, zf2;
      z1 = add8((i8)x, (i8)y, &cf1, &zf1);
      z2 = add16((i8)x, (i8)y, &cf2, &zf2);
      unassert(z1 == z2);
      unassert(cf1 == cf2);
      if (zf1 != zf2) {
        LOGF("%d %d", x, y);
      }
      unassert(zf1 == zf2);
    }
  }
  return 0;
}
