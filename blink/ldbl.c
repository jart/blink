/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
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
#include "blink/ldbl.h"

#include "blink/endian.h"
#include "blink/macros.h"
#include "blink/pun.h"

u8 *SerializeLdbl(u8 b[10], double f) {
  int e;
  union DoublePun u = {f};
  e = (u.i >> 52) & 0x7ff;
  if (!e) {
    e = 0;
  } else if (e == 0x7ff) {
    e = 0x7fff;
  } else {
    e -= 0x3ff;
    e += 0x3fff;
  }
  Write16(b + 8, e | u.i >> 63 << 15);
  Write64(b, (u.i & 0x000fffffffffffff) << 11 | (u64) !!u.f << 63);
  return b;
}

double DeserializeLdbl(const u8 b[10]) {
  union DoublePun u;
  u.i = (u64)(MAX(-1023, MIN(1024, ((Read16(b + 8) & 0x7fff) - 0x3fff))) + 1023)
            << 52 |
        ((Read64(b) & 0x7fffffffffffffff) + (1 << (11 - 1))) >> 11 |
        (u64)(b[9] >> 7) << 63;
  return u.f;
}
