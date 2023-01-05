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
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/modrm.h"
#include "blink/mop.h"
#include "blink/swap.h"
#include "blink/tsan.h"

#define ACQUIRE memory_order_acquire
#define RELEASE memory_order_release

/* When software uses locks or semaphores to synchronize processes,
   threads, or other code sections; Intel recommends that only one lock
   or semaphore be present within a cache line (or 128 byte sector, if
   128-byte sector is supported). In processors based on Intel NetBurst
   microarchitecture (which support 128-byte sector consisting of two
   cache lines), following this recommendation means that each lock or
   semaphore should be contained in a 128-byte block of memory that
   begins on a 128-byte boundary. The practice minimizes the bus traffic
   required to service locks. ──Intel V.3 §8.10.6.7 */
_Alignas(kSemSize) static atomic_int g_bus[kBusCount][kSemSize / sizeof(int)];

void InitBus(void) {
  int i;
  for (i = 0; i < kBusCount; ++i) {
    atomic_store_explicit(g_bus[i], 0, memory_order_relaxed);
  }
}

void LockBus(const u8 *locality) {
  /* A locked instruction is guaranteed to lock only the area of memory
     defined by the destination operand, but may be interpreted by the
     system as a lock for a larger memory area. ──Intel V.3 §8.1.2.2 */
  _Static_assert(IS2POW(kSemSize), "semaphore size must be two-power");
  _Static_assert(IS2POW(kBusCount), "virtual bus count must be two-power");
  _Static_assert(IS2POW(kBusRegion), "virtual bus region must be two-power");
  _Static_assert(kBusRegion >= 16, "virtual bus region must be at least 16");
  SpinLock(g_bus[(uintptr_t)locality / kBusRegion % kBusCount]);
}

void UnlockBus(const u8 *locality) {
  SpinUnlock(g_bus[(uintptr_t)locality / kBusRegion % kBusCount]);
}

i64 Load8(const u8 p[1]) {
  i64 z;
  z = atomic_load_explicit((_Atomic(u8) *)p, ACQUIRE);
  return z;
}

i64 Load16(const u8 p[2]) {
  i64 z;
#if (defined(__x86_64__) || defined(__i386__)) && \
    !defined(__SANITIZE_UNDEFINED__)
  z = atomic_load_explicit((_Atomic(u16) *)p, memory_order_relaxed);
#else
  if (!((intptr_t)p & 1)) {
    z = Little16(atomic_load_explicit((_Atomic(u16) *)p, ACQUIRE));
  } else {
    z = Read16(p);
  }
#endif
  return z;
}

i64 Load32(const u8 p[4]) {
  i64 z;
#if (defined(__x86_64__) || defined(__i386__)) && \
    !defined(__SANITIZE_UNDEFINED__)
  z = atomic_load_explicit((_Atomic(u32) *)p, memory_order_relaxed);
#else
  if (!((intptr_t)p & 3)) {
    z = Little32(atomic_load_explicit((_Atomic(u32) *)p, ACQUIRE));
  } else {
    z = Read32(p);
  }
#endif
  return z;
}

i64 Load64(const u8 p[8]) {
  i64 z;
#if LONG_BIT >= 64
#if defined(__x86_64__) && !defined(__SANITIZE_UNDEFINED__)
  z = atomic_load_explicit((_Atomic(u64) *)p, memory_order_relaxed);
#else
  if (!((intptr_t)p & 7)) {
    z = Little64(atomic_load_explicit((_Atomic(u64) *)p, ACQUIRE));
  } else {
    z = Read64(p);
  }
#endif
#else
  LockBus(p);
  z = Read64(p);
  UnlockBus(p);
#endif
  return z;
}

i64 Load64Unlocked(const u8 p[8]) {
  i64 z;
#if LONG_BIT >= 64
  z = Load64(p);
#else
  z = Read64(p);
#endif
  return z;
}

void Store8(u8 p[1], u64 x) {
  atomic_store_explicit((_Atomic(u8) *)p, x, RELEASE);
}

void Store16(u8 p[2], u64 x) {
#if (defined(__x86_64__) || defined(__i386__)) && \
    !defined(__SANITIZE_UNDEFINED__)
  atomic_store_explicit((_Atomic(u16) *)p, x, memory_order_relaxed);
#else
  if (!((intptr_t)p & 1)) {
    atomic_store_explicit((_Atomic(u16) *)p, Little16(x), RELEASE);
  } else {
    Write16(p, x);
  }
#endif
}

void Store32(u8 p[4], u64 x) {
#if (defined(__x86_64__) || defined(__i386__)) && \
    !defined(__SANITIZE_UNDEFINED__)
  atomic_store_explicit((_Atomic(u32) *)p, x, memory_order_relaxed);
#else
  if (!((intptr_t)p & 3)) {
    atomic_store_explicit((_Atomic(u32) *)p, Little32(x), RELEASE);
  } else {
    Write32(p, x);
  }
#endif
}

void Store64(u8 p[8], u64 x) {
#if LONG_BIT >= 64
#if defined(__x86_64__) && !defined(__SANITIZE_UNDEFINED__)
  atomic_store_explicit((_Atomic(u64) *)p, x, memory_order_relaxed);
#else
  if (!((intptr_t)p & 7)) {
    atomic_store_explicit((_Atomic(u64) *)p, Little64(x), RELEASE);
  } else {
    Write64(p, x);
  }
#endif
#else
  LockBus(p);
  Write64(p, x);
  UnlockBus(p);
#endif
}

void Store64Unlocked(u8 p[8], u64 x) {
#if LONG_BIT >= 64
  Store64(p, x);
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

u64 ReadMemoryUnlocked(u64 rde, u8 p[8]) {
  if (Rexw(rde)) {
    return Load64Unlocked(p);
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

void WriteRegisterBW(u64 rde, u8 p[8], u64 x) {
  switch (RegLog2(rde)) {
    case 3:
      Put64(p, x);
      break;
    case 2:
      Put64(p, x & 0xffffffff);
      break;
    case 0:
      Put8(p, x);
      break;
    case 1:
      Put16(p, x);
      break;
    default:
      __builtin_unreachable();
  }
}

i64 ReadRegisterBW(u64 rde, u8 p[8]) {
  switch (RegLog2(rde)) {
    case 3:
      return Get64(p);
    case 2:
      return Get32(p);
    case 0:
      return Get8(p);
    case 1:
      return Get16(p);
    default:
      __builtin_unreachable();
  }
}

i64 ReadMemoryBW(u64 rde, u8 p[8]) {
  switch (RegLog2(rde)) {
    case 3:
      return Load64(p);
    case 2:
      return Load32(p);
    case 0:
      return Load8(p);
    case 1:
      return Load16(p);
    default:
      __builtin_unreachable();
  }
}

void WriteMemoryBW(u64 rde, u8 p[8], u64 x) {
  switch (RegLog2(rde)) {
    case 3:
      Put64(p, x);
      break;
    case 2:
      Put32(p, x);
      break;
    case 0:
      Put8(p, x);
      break;
    case 1:
      Put16(p, x);
      break;
    default:
      __builtin_unreachable();
  }
}

void WriteRegisterOrMemoryBW(u64 rde, u8 p[8], u64 x) {
  if (IsModrmRegister(rde)) {
    WriteRegisterBW(rde, p, x);
  } else {
    WriteMemoryBW(rde, p, x);
  }
}

i64 ReadRegisterOrMemoryBW(u64 rde, u8 p[8]) {
  if (IsModrmRegister(rde)) {
    return ReadRegisterBW(rde, p);
  } else {
    return ReadMemoryBW(rde, p);
  }
}
