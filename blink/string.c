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
#include <stdlib.h>
#include <string.h>

#include "blink/address.h"
#include "blink/alu.h"
#include "blink/builtin.h"
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/memory.h"
#include "blink/modrm.h"
#include "blink/mop.h"
#include "blink/string.h"

static u64 ReadInt(u8 p[8], unsigned long w) {
  switch (w) {
    case 0:
      return Load8(p);
    case 1:
      return Load16(p);
    case 2:
      return Load32(p);
    case 3:
      return Load64(p);
    default:
      __builtin_unreachable();
  }
}

static void WriteInt(u8 p[8], u64 x, unsigned long w) {
  switch (w) {
    case 0:
      Store8(p, x);
      break;
    case 1:
      Store16(p, x);
      break;
    case 2:
      Store32(p, x);
      break;
    case 3:
      Store64(p, x);
      break;
    default:
      __builtin_unreachable();
  }
}

static void AddDi(struct Machine *m, u64 rde, u64 x) {
  switch (Eamode(rde)) {
    case XED_MODE_LONG:
      Put64(m->di, Get64(m->di) + x);
      return;
    case XED_MODE_LEGACY:
      Put64(m->di, (Get32(m->di) + x) & 0xffffffff);
      return;
    case XED_MODE_REAL:
      Put16(m->di, Get16(m->di) + x);
      return;
    default:
      __builtin_unreachable();
  }
}

static void AddSi(struct Machine *m, u64 rde, u64 x) {
  switch (Eamode(rde)) {
    case XED_MODE_LONG:
      Put64(m->si, Get64(m->si) + x);
      return;
    case XED_MODE_LEGACY:
      Put64(m->si, (Get32(m->si) + x) & 0xffffffff);
      return;
    case XED_MODE_REAL:
      Put16(m->si, Get16(m->si) + x);
      return;
    default:
      __builtin_unreachable();
  }
}

static u64 ReadCx(struct Machine *m, u64 rde) {
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

static u64 SubtractCx(struct Machine *m, u64 rde, u64 x) {
  u64 cx;
  cx = Get64(m->cx) - x;
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

static void StringOp(struct Machine *m, u64 rde, int op) {
  bool stop;
  void *p[2];
  unsigned n;
  i64 sgn, v;
  u8 s[3][8];
  stop = false;
  n = 1 << RegLog2(rde);
  sgn = GetFlag(m->flags, FLAGS_DF) ? -1 : 1;
  do {
    if (Rep(rde) && !ReadCx(m, rde)) break;
    switch (op) {
      case STRING_CMPS:
        kAlu[ALU_SUB][RegLog2(rde)](
            ReadInt(Load(m, AddressSi(m, rde), n, s[2]), RegLog2(rde)),
            ReadInt(Load(m, AddressDi(m, rde), n, s[1]), RegLog2(rde)),
            &m->flags);
        AddDi(m, rde, sgn * n);
        AddSi(m, rde, sgn * n);
        stop = (Rep(rde) == 2 && GetFlag(m->flags, FLAGS_ZF)) ||
               (Rep(rde) == 3 && !GetFlag(m->flags, FLAGS_ZF));
        break;
      case STRING_MOVS:
        memcpy(BeginStore(m, (v = AddressDi(m, rde)), n, p, s[0]),
               Load(m, AddressSi(m, rde), n, s[1]), n);
        AddDi(m, rde, sgn * n);
        AddSi(m, rde, sgn * n);
        EndStore(m, v, n, p, s[0]);
        break;
      case STRING_STOS:
        memcpy(BeginStore(m, (v = AddressDi(m, rde)), n, p, s[0]), m->ax, n);
        AddDi(m, rde, sgn * n);
        EndStore(m, v, n, p, s[0]);
        break;
      case STRING_LODS:
        memcpy(m->ax, Load(m, AddressSi(m, rde), n, s[1]), n);
        AddSi(m, rde, sgn * n);
        break;
      case STRING_SCAS:
        kAlu[ALU_SUB][RegLog2(rde)](
            ReadInt(Load(m, AddressDi(m, rde), n, s[1]), RegLog2(rde)),
            ReadInt(m->ax, RegLog2(rde)), &m->flags);
        AddDi(m, rde, sgn * n);
        stop = (Rep(rde) == 2 && GetFlag(m->flags, FLAGS_ZF)) ||
               (Rep(rde) == 3 && !GetFlag(m->flags, FLAGS_ZF));
        break;
      case STRING_OUTS:
        OpOut(m, Get16(m->dx),
              ReadInt(Load(m, AddressSi(m, rde), n, s[1]), RegLog2(rde)));
        AddSi(m, rde, sgn * n);
        break;
      case STRING_INS:
        WriteInt((u8 *)BeginStore(m, (v = AddressDi(m, rde)), n, p, s[0]),
                 OpIn(m, Get16(m->dx)), RegLog2(rde));
        AddDi(m, rde, sgn * n);
        EndStore(m, v, n, p, s[0]);
        break;
      default:
        abort();
    }
    if (Rep(rde)) {
      SubtractCx(m, rde, 1);
    } else {
      break;
    }
  } while (!stop);
}

static void RepMovsbEnhanced(struct Machine *m, u64 rde) {
  u8 *direal, *sireal;
  u64 diactual, siactual, cx;
  unsigned diremain, siremain, i, n;
  if ((cx = ReadCx(m, rde))) {
    do {
      diactual = AddressDi(m, rde);
      siactual = AddressSi(m, rde);
      SetWriteAddr(m, diactual, cx);
      SetReadAddr(m, siactual, cx);
      direal = ResolveAddress(m, diactual);
      sireal = ResolveAddress(m, siactual);
      diremain = 4096 - (diactual & 4095);
      siremain = 4096 - (siactual & 4095);
      n = MIN(cx, MIN(diremain, siremain));
      for (i = 0; i < n; ++i) direal[i] = sireal[i];
      AddDi(m, rde, n);
      AddSi(m, rde, n);
    } while ((cx = SubtractCx(m, rde, n)));
  }
}

static void RepStosbEnhanced(struct Machine *m, u64 rde) {
  u8 *direal;
  unsigned diremain, n;
  u64 diactual, cx;
  if ((cx = ReadCx(m, rde))) {
    do {
      diactual = AddressDi(m, rde);
      SetWriteAddr(m, diactual, cx);
      direal = ResolveAddress(m, diactual);
      diremain = 4096 - (diactual & 4095);
      n = MIN(cx, diremain);
      memset(direal, Get8(m->ax), n);
      AddDi(m, rde, n);
    } while ((cx = SubtractCx(m, rde, n)));
  }
}

void OpMovs(struct Machine *m, u64 rde) {
  StringOp(m, rde, STRING_MOVS);
}

void OpCmps(struct Machine *m, u64 rde) {
  StringOp(m, rde, STRING_CMPS);
}

void OpStos(struct Machine *m, u64 rde) {
  StringOp(m, rde, STRING_STOS);
}

void OpLods(struct Machine *m, u64 rde) {
  StringOp(m, rde, STRING_LODS);
}

void OpScas(struct Machine *m, u64 rde) {
  StringOp(m, rde, STRING_SCAS);
}

void OpIns(struct Machine *m, u64 rde) {
  StringOp(m, rde, STRING_INS);
}

void OpOuts(struct Machine *m, u64 rde) {
  StringOp(m, rde, STRING_OUTS);
}

void OpMovsb(struct Machine *m, u64 rde) {
  if (Rep(rde) && !GetFlag(m->flags, FLAGS_DF)) {
    RepMovsbEnhanced(m, rde);
  } else {
    OpMovs(m, rde);
  }
}

void OpStosb(struct Machine *m, u64 rde) {
  if (Rep(rde) && !GetFlag(m->flags, FLAGS_DF)) {
    RepStosbEnhanced(m, rde);
  } else {
    OpStos(m, rde);
  }
}
