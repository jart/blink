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

#if defined(__x86_64__) || defined(__i386__)
#define ORDER memory_order_relaxed
#else
#define ORDER memory_order_seq_cst
#endif

i64 Load8(const u8 p[1]) {
  i64 res;
  res = atomic_load_explicit((_Atomic(u8) *)p, ORDER);
  return res;
}

i64 Load16(const u8 p[2]) {
  i64 res;
  if (ORDER != memory_order_relaxed && !((intptr_t)p & 1)) {
    res = Little16(atomic_load_explicit((_Atomic(u16) *)p, ORDER));
  } else {
    res = Read16(p);
  }
  return res;
}

i64 Load32(const u8 p[4]) {
  i64 res;
  if (ORDER != memory_order_relaxed && !((intptr_t)p & 3)) {
    res = Little32(atomic_load_explicit((_Atomic(u32) *)p, ORDER));
  } else {
    res = Read32(p);
  }
  return res;
}

i64 Load64(const u8 p[8]) {
  i64 res;
#if LONG_BIT >= 64
  if (ORDER != memory_order_relaxed && !((intptr_t)p & 7)) {
    res = Little64(atomic_load_explicit((_Atomic(u64) *)p, ORDER));
  } else {
    res = Read64(p);
  }
#else
  res = Read64(p);
#endif
  return res;
}

void Store8(u8 p[1], u64 x) {
  atomic_store_explicit((_Atomic(u8) *)p, x, ORDER);
}

void Store16(u8 p[2], u64 x) {
  if (ORDER != memory_order_relaxed && !((intptr_t)p & 1)) {
    atomic_store_explicit((_Atomic(u16) *)p, Little16(x), ORDER);
  } else {
    Write16(p, x);
  }
}

void Store32(u8 p[4], u64 x) {
  if (ORDER != memory_order_relaxed && !((intptr_t)p & 3)) {
    atomic_store_explicit((_Atomic(u32) *)p, Little32(x), ORDER);
  } else {
    Write32(p, x);
  }
}

void Store64(u8 p[8], u64 x) {
#if LONG_BIT >= 64
  if (ORDER != memory_order_relaxed && !((intptr_t)p & 7)) {
    atomic_store_explicit((_Atomic(u64) *)p, Little64(x), ORDER);
  } else {
    Write64(p, x);
  }
#else
  Write64(p, x);
#endif
}

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
