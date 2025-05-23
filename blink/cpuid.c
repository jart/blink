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
#include "blink/assert.h"
#include "blink/endian.h"
#include "blink/machine.h"

#define INTEL    "GenuineIntel"
#define BLINK    "GenuineBlink"
#define LINUX_   "Linux\0\0\0\0\0\0\0"
#define FREEBSD_ "FreeBSD\0\0\0\0\0\0"
#define NETBSD_  "NetBSD\0\0\0\0\0\0"
#define OPENBSD_ "OpenBSD\0\0\0\0\0"
#define XNU_     "XNU\0\0\0\0\0\0\0\0\0"
#define WINDOWS_ "Windows\0\0\0\0\0"
#define CYGWIN_  "Cygwin\0\0\0\0\0\0"
#define HAIKU_   "Haiku\0\0\0\0\0\0\0"
#define ILLUMOS_ "illumos\0\0\0\0\0\0"
#define SOLARIS_ "Solaris\0\0\0\0\0\0"
#define SUNOS_   "SunOS\0\0\0\0\0\0\0"
#define UNKNOWN_ "Unknown\0\0\0\0\0\0"

#ifdef __COSMOPOLITAN__
#define OS                  \
  (IsLinux()     ? LINUX_   \
   : IsFreebsd() ? FREEBSD_ \
   : IsNetbsd()  ? NETBSD_  \
   : IsOpenbsd() ? OPENBSD_ \
   : IsXnu()     ? XNU_     \
   : IsWindows() ? WINDOWS_ \
                 : UNKNOWN_)
#elif defined(__linux)
#define OS LINUX_
#elif defined(__FreeBSD__)
#define OS FREEBSD_
#elif defined(__NetBSD__)
#define OS NETBSD_
#elif defined(__OpenBSD__)
#define OS OPENBSD_
#elif defined(__APPLE__)
#define OS XNU_
#elif defined(__CYGWIN__)
#define OS CYGWIN_
#elif defined(__HAIKU__)
#define OS HAIKU_
#elif defined(sun) || defined(__sun)
# if defined(__illumos__)
#define OS ILLUMOS_
# elif defined(__SVR4) || defined(__svr4__)
#define OS SOLARIS_
# else
#define OS SUNOS_
# endif
#else
#define OS UNKNOWN_
#endif

#define X86_64_  "x86_64\0\0\0\0\0\0"
#define I386_    "i386\0\0\0\0\0\0\0\0"
#define AARCH64_ "aarch64\0\0\0\0\0"
#define ARM_     "arm\0\0\0\0\0\0\0\0\0"
#define PPC64_   "ppc64\0\0\0\0\0\0\0"
#define PPC64LE_ "ppc64le\0\0\0\0\0"
#define PPC_     "ppc\0\0\0\0\0\0\0\0\0"
#define S390X_   "s390x\0\0\0\0\0\0\0"
#define RISCV64_ "riscv64\0\0\0\0\0"
#define RISCV32_ "riscv32\0\0\0\0\0"

#if defined(__x86_64__)
#define ARCH_NAME X86_64_
#elif defined(__i386__)
#define ARCH_NAME I386_
#elif defined(__aarch64__)
#define ARCH_NAME AARCH64_
#elif defined(__ARMEL__)
#define ARCH_NAME ARM_
#elif defined(__powerpc64__) && defined(__LITTLE_ENDIAN__)
#define ARCH_NAME PPC64LE_
#elif defined(__powerpc64__)
#define ARCH_NAME PPC64_
#elif defined(__powerpc__)
#define ARCH_NAME PPC_
#elif defined(__s390x__)
#define ARCH_NAME S390X_
#elif defined(__riscv) && __riscv_xlen == 64
#define ARCH_NAME RISCV64_
#elif defined(__riscv) && __riscv_xlen == 32
#define ARCH_NAME RISCV32_
#else
#define ARCH_NAME UNKNOWN_
#endif

void OpCpuid(P) {
  u32 ax, bx, cx, dx, jit;
  if (m->trapcpuid) {
    ThrowSegmentationFault(m, 0);
  }
  ax = bx = cx = dx = 0;
  switch (Get32(m->ax)) {
    case 0:
      ax = 7;
      goto vendor;
    case 0x80000000:
      ax = 0x80000001;
    vendor:
      // glibc binaries won't run unless we report blink as a
      // modern linux kernel on top of genuine intel hardware
      bx = Read32((const u8 *)INTEL + 0);
      dx = Read32((const u8 *)INTEL + 4);
      cx = Read32((const u8 *)INTEL + 8);
      break;
    case 0x40000000:
      bx = Read32((const u8 *)BLINK + 0);
      cx = Read32((const u8 *)BLINK + 4);
      dx = Read32((const u8 *)BLINK + 8);
      break;
    case 0x40031337:
      bx = Read32((const u8 *)OS + 0);
      cx = Read32((const u8 *)OS + 4);
      dx = Read32((const u8 *)OS + 8);
      break;
    case 0x40031338:
      bx = Read32((const u8 *)ARCH_NAME + 0);
      cx = Read32((const u8 *)ARCH_NAME + 4);
      dx = Read32((const u8 *)ARCH_NAME + 8);
      break;
    case 1:
      cx |= 1 << 0;    // sse3
      cx |= 1 << 1;    // pclmulqdq
      cx |= 1 << 9;    // ssse3
      cx |= 1 << 23;   // popcnt
      cx |= 1 << 30;   // rdrnd
      cx |= 0 << 25;   // aes
      cx |= 1 << 13;   // cmpxchg16b
      cx |= 1u << 31;  // hypervisor
      dx |= 1 << 4;    // tsc
      dx |= 1 << 6;    // pae
      dx |= 1 << 8;    // cmpxchg8b
      dx |= 1 << 15;   // cmov
      dx |= 1 << 19;   // clflush
      dx |= 1 << 23;   // mmx
      dx |= 1 << 24;   // fxsave
      dx |= 1 << 25;   // sse
      dx |= 1 << 26;   // sse2
      cx |= 0 << 19;   // sse4.1
      cx |= 0 << 20;   // sse4.2
#ifndef DISABLE_X87
      dx |= 1 << 0;  // fpu
#endif
      break;
    case 2:  // Cache and TLB information
      ax = 0x76035a01;
      bx = 0x00f0b0ff;
      cx = 0x00000000;
      dx = 0x00ca0000;
      break;
    case 7:
      switch (Get32(m->cx)) {
        case 0:
          bx |= 1 << 0;   // fsgsbase
          bx |= 1 << 9;   // erms
          bx |= 1 << 18;  // rdseed
          cx |= 1 << 22;  // rdpid
#ifndef DISABLE_BMI2
          bx |= 1 << 8;   // bmi2
          bx |= 1 << 19;  // adx
#endif
          break;
        default:
          break;
      }
      break;
    case 0x80000001:
      jit = !IsJitDisabled(&m->system->jit);
      cx |= 1 << 0;     // lahf
      cx |= jit << 31;  // jit
      dx |= 1 << 0;     // fpu
      dx |= 1 << 8;     // cmpxchg8b
      dx |= 1 << 11;    // syscall
      dx |= 1 << 15;    // cmov
      dx |= 1 << 20;    // nx
      dx |= 1 << 23;    // mmx
      dx |= 1 << 24;    // fxsave
      dx |= 1 << 27;    // rdtscp
      dx |= 1 << 29;    // long
      break;
    case 0x80000007:
      dx |= 1 << 8;  // invtsc
      break;
    case 4:  // cpu cache information
      // - Level 1 data 8-way 32,768 byte cache w/ 64 sets of 64 byte
      //   lines shared across 2 threads
      // - Level 1 code 8-way 32,768 byte cache w/ 64 sets of 64 byte
      //   lines shared across 2 threads
      // - Level 2 8-way 262,144 byte cache w/ 512 sets of 64 byte lines
      //   shared across 2 threads
      // - Level 3 complexly-indexed 16-way 8,388,608 byte cache w/ 8,192
      //   sets of 64 byte lines shared across 16 threads
      switch (Get32(m->cx)) {
        case 0:
          ax = 0x1c004121;
          bx = 0x01c0003f;
          cx = 0x0000003f;
          dx = 0x00000000;
          break;
        case 1:
          ax = 0x1c004122;
          bx = 0x01c0003f;
          cx = 0x0000003f;
          dx = 0x00000000;
          break;
        case 2:
          ax = 0x1c004143;
          bx = 0x01c0003f;
          cx = 0x000001ff;
          dx = 0x00000000;
          break;
        case 3:
          ax = 0x1c03c163;
          bx = 0x03c0003f;
          cx = 0x00001fff;
          dx = 0x00000006;
          break;
        default:
          break;
      }
      break;
    case 6:  // thermal and power management leaf
      ax = 0x00000077;
      bx = 0x00000002;
      cx = 0x0000000b;
      dx = 0x00000000;
      break;
    default:
      break;
  }
  Put64(m->ax, ax);
  Put64(m->bx, bx);
  Put64(m->cx, cx);
  Put64(m->dx, dx);
}
