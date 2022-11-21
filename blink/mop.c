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

#include "blink/endian.h"
#include "blink/modrm.h"
#include "blink/mop.h"
#include "blink/swap.h"
#include "blink/tsan.h"

u64 ReadRegister(u64 rde, u8 p[8]) {
  if (Rexw(rde)) {
    return Get64(p);
  } else if (!Osz(rde)) {
    return Get32(p);
  } else {
    return Get16(p);
  }
}

i64 ReadRegisterSigned(u64 rde, u8 p[8]) {
  if (Rexw(rde)) {
    return (i64)Get64(p);
  } else if (!Osz(rde)) {
    return (i32)Get32(p);
  } else {
    return (i16)Get16(p);
  }
}

void WriteRegister(u64 rde, u8 p[8], u64 x) {
  if (Rexw(rde)) {
    Put64(p, x);
  } else if (!Osz(rde)) {
    Put64(p, x & 0xffffffff);
  } else {
    Put16(p, x);
  }
}

u64 ReadMemory(u64 rde, u8 p[8]) {
  if (Rexw(rde)) {
    return Load64(p);
  } else if (!Osz(rde)) {
    return Load32(p);
  } else {
    return Load16(p);
  }
}

u64 ReadMemorySigned(u64 rde, u8 p[8]) {
  if (Rexw(rde)) {
    return (i64)Load64(p);
  } else if (!Osz(rde)) {
    return (i32)Load32(p);
  } else {
    return (i16)Load16(p);
  }
}

void WriteMemory(u64 rde, u8 p[8], u64 x) {
  if (Rexw(rde)) {
    Store64(p, x);
  } else if (!Osz(rde)) {
    Store32(p, x);
  } else {
    Store16(p, x);
  }
}

void WriteRegisterOrMemory(u64 rde, u8 p[8], u64 x) {
  if (IsModrmRegister(rde)) {
    WriteRegister(rde, p, x);
  } else {
    WriteMemory(rde, p, x);
  }
}
