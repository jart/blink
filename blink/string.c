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
#include "blink/string.h"

#include <stdlib.h>
#include <string.h>

#include "blink/alu.h"
#include "blink/atomic.h"
#include "blink/biosrom.h"
#include "blink/builtin.h"
#include "blink/bus.h"
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/modrm.h"
#include "blink/rde.h"
#include "blink/tsan.h"
#include "blink/util.h"
#include "blink/x86.h"

static u64 ReadInt(u8 p[8], unsigned long w) {
  switch (w) {
    case 0:
      return Read8(p);
    case 1:
      return Read16(p);
    case 2:
      return Read32(p);
    case 3:
      return Read64(p);
    default:
      __builtin_unreachable();
  }
}

static void WriteInt(u8 p[8], u64 x, unsigned long w) {
  switch (w) {
    case 0:
      Write8(p, x);
      break;
    case 1:
      Write16(p, x);
      break;
    case 2:
      Write32(p, x);
      break;
    case 3:
      Write64(p, x);
      break;
    default:
      __builtin_unreachable();
  }
}

static void AddDi(P, u64 x) {
  switch (Eamode(rde)) {
    case XED_MODE_LONG:
      Put64(m->di, Get64(m->di) + x);
      break;
    case XED_MODE_LEGACY:
      Put64(m->di, (Get32(m->di) + x) & 0xffffffff);
      break;
    case XED_MODE_REAL:
      Put16(m->di, Get16(m->di) + x);
      break;
    default:
      __builtin_unreachable();
  }
}

static void AddSi(P, u64 x) {
  switch (Eamode(rde)) {
    case XED_MODE_LONG:
      Put64(m->si, Get64(m->si) + x);
      break;
    case XED_MODE_LEGACY:
      Put64(m->si, (Get32(m->si) + x) & 0xffffffff);
      break;
    case XED_MODE_REAL:
      Put16(m->si, Get16(m->si) + x);
      break;
    default:
      __builtin_unreachable();
  }
}

static u64 ReadCx(P) {
  switch (Eamode(rde)) {
    case XED_MODE_LONG:
      return Get64(m->cx);
    case XED_MODE_LEGACY:
      return Get32(m->cx);
    case XED_MODE_REAL:
      return Get16(m->cx);
    default:
      __builtin_unreachable();
  }
}

static u64 SubtractCx(P, u64 x) {
  u64 cx = Get64(m->cx) - x;
  if (Eamode(rde) != XED_MODE_REAL) {
    if (Eamode(rde) == XED_MODE_LEGACY) {
      cx &= 0xffffffff;
    }
    Put64(m->cx, cx);
  } else {
    cx &= 0xffff;
    Put16(m->cx, cx);
  }
  return cx;
}

static void StringOp(P, int op) {
  bool stop;
  unsigned n;
  i64 sgn, v;
  void *p[2];
  u8 s[3][8];
  stop = false;
  n = 1 << RegLog2(rde);
  sgn = GetFlag(m->flags, FLAGS_DF) ? -1 : 1;
  IGNORE_RACES_START();
  atomic_thread_fence(memory_order_acquire);
  do {
    if (Rep(rde) && !ReadCx(A)) break;
    switch (op) {
      case STRING_CMPS:
        kAlu[ALU_SUB][RegLog2(rde)](
            m, ReadInt(Load(m, AddressSi(A), n, s[2]), RegLog2(rde)),
            ReadInt(Load(m, AddressDi(A), n, s[1]), RegLog2(rde)));
        AddDi(A, sgn * n);
        AddSi(A, sgn * n);
        stop = (Rep(rde) == 2 && GetFlag(m->flags, FLAGS_ZF)) ||
               (Rep(rde) == 3 && !GetFlag(m->flags, FLAGS_ZF));
        break;
      case STRING_MOVS:
        memmove(BeginStore(m, (v = AddressDi(A)), n, p, s[0]),
                Load(m, AddressSi(A), n, s[1]), n);
        AddDi(A, sgn * n);
        AddSi(A, sgn * n);
        EndStore(m, v, n, p, s[0]);
        break;
      case STRING_STOS:
        memmove(BeginStore(m, (v = AddressDi(A)), n, p, s[0]), m->ax, n);
        AddDi(A, sgn * n);
        EndStore(m, v, n, p, s[0]);
        break;
      case STRING_LODS:
        memmove(m->ax, Load(m, AddressSi(A), n, s[1]), n);
        AddSi(A, sgn * n);
        break;
      case STRING_SCAS:
        kAlu[ALU_SUB][RegLog2(rde)](
            m, ReadInt(Load(m, AddressDi(A), n, s[1]), RegLog2(rde)),
            ReadInt(m->ax, RegLog2(rde)));
        AddDi(A, sgn * n);
        stop = (Rep(rde) == 2 && GetFlag(m->flags, FLAGS_ZF)) ||
               (Rep(rde) == 3 && !GetFlag(m->flags, FLAGS_ZF));
        break;
#ifndef DISABLE_METAL
      case STRING_OUTS:
        OpOut(m, Get16(m->dx),
              ReadInt(Load(m, AddressSi(A), n, s[1]), RegLog2(rde)));
        AddSi(A, sgn * n);
        break;
      case STRING_INS:
        WriteInt((u8 *)BeginStore(m, (v = AddressDi(A)), n, p, s[0]),
                 OpIn(m, Get16(m->dx)), RegLog2(rde));
        AddDi(A, sgn * n);
        EndStore(m, v, n, p, s[0]);
        break;
#endif /* DISABLE_METAL */
      default:
        Abort();
    }
    if (Rep(rde)) {
      SubtractCx(A, 1);
    } else {
      break;
    }
  } while (!stop);
  atomic_thread_fence(memory_order_release);
  IGNORE_RACES_END();
}

static void RepMovsbEnhanced(P) {
  u8 *direal, *sireal;
  u64 diactual, siactual, cx;
  u16 dilow, silow;
  long diremain, siremain, i, n;
  if ((cx = ReadCx(A))) {
    diactual = AddressDi(A);
    siactual = AddressSi(A);
    if (diactual != siactual) {
      if (!GetFlag(m->flags, FLAGS_DF)) {
        SetWriteAddr(m, diactual, cx);
        SetReadAddr(m, siactual, cx);
      } else {
        SetWriteAddr(m, diactual - cx + 1, cx);
        SetReadAddr(m, siactual - cx + 1, cx);
      }
      IGNORE_RACES_START();
      atomic_thread_fence(memory_order_acquire);
      do {
        direal = ResolveAddress(m, diactual);
        sireal = ResolveAddress(m, siactual);
        dilow = Get16(m->di);
        silow = Get16(m->si);
        if (!GetFlag(m->flags, FLAGS_DF)) {
          diremain = 4096 - (diactual & 4095);
          diremain = MIN(diremain, 65536 - dilow);
          siremain = 4096 - (siactual & 4095);
          siremain = MIN(siremain, 65536 - silow);
          n = MIN(cx, MIN(diremain, siremain));
          for (i = 0; i < n; ++i) {
            direal[i] = sireal[i];
          }
          AddDi(A, n);
          AddSi(A, n);
        } else {
          diremain = (diactual & 4095) + 1;
          diremain = MIN(diremain, (long)dilow + 1);
          siremain = (siactual & 4095) + 1;
          siremain = MIN(siremain, (long)silow + 1);
          n = MIN(cx, MIN(diremain, siremain));
          for (i = 0; i < n; ++i) {
            direal[-i] = sireal[-i];
          }
          AddDi(A, -n);
          AddSi(A, -n);
        }
        cx = SubtractCx(A, n);
        if (cx) {
          diactual = AddressDi(A);
          siactual = AddressSi(A);
        }
      } while (cx);
      atomic_thread_fence(memory_order_release);
      IGNORE_RACES_END();
    }
  }
}

static void RepStosbEnhanced(P) {
  u8 *direal;
  u64 diactual, cx;
  u16 dilow;
  unsigned diremain, n;
  if ((cx = ReadCx(A))) {
    diactual = AddressDi(A);
    SetWriteAddr(m, diactual, cx);
    IGNORE_RACES_START();
    do {
      direal = ResolveAddress(m, diactual);
      dilow = Get16(m->di);
      diremain = 4096 - (diactual & 4095);
      diremain = MIN(diremain, 65536 - dilow);
      n = MIN(cx, diremain);
      if (!IsRomAddress(m, direal)) memset(direal, m->al, n);
      AddDi(A, n);
      cx = SubtractCx(A, n);
      if (cx) diactual = AddressDi(A);
    } while (cx);
    atomic_thread_fence(memory_order_release);
    IGNORE_RACES_END();
  }
}

void OpMovs(P) {
  StringOp(A, STRING_MOVS);
}

void OpCmps(P) {
  StringOp(A, STRING_CMPS);
}

void OpStos(P) {
  StringOp(A, STRING_STOS);
}

void OpLods(P) {
  StringOp(A, STRING_LODS);
}

void OpScas(P) {
  StringOp(A, STRING_SCAS);
}

void OpIns(P) {
  StringOp(A, STRING_INS);
}

void OpOuts(P) {
  StringOp(A, STRING_OUTS);
}

void OpMovsb(P) {
  if (Rep(rde)) {
    RepMovsbEnhanced(A);
  } else {
    OpMovs(A);
  }
}

void OpStosb(P) {
  if (Rep(rde) && !GetFlag(m->flags, FLAGS_DF)) {
    RepStosbEnhanced(A);
  } else {
    OpStos(A);
  }
}
