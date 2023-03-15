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

#include "blink/builtin.h"
#include "blink/bus.h"
#include "blink/endian.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/modrm.h"
#include "blink/rde.h"
#include "blink/tsan.h"
#include "blink/x86.h"

static const u8 kStackOsz[2][3] = {{4, 4, 8}, {2, 2, 2}};
static const u8 kCallOsz[2][3] = {{4, 4, 8}, {2, 2, 8}};

static void WriteStackWord(u8 *p, u64 rde, u32 osz, u64 x) {
  IGNORE_RACES_START();
  if (osz == 8) {
    Write64(p, x);
  } else if (osz == 2) {
    Write16(p, x);
  } else {
    Write32(p, x);
  }
  IGNORE_RACES_END();
}

static u64 ReadStackWord(u8 *p, u32 osz) {
  u64 x;
  IGNORE_RACES_START();
  if (osz == 8) {
    x = Read64(p);
  } else if (osz == 2) {
    x = Read16(p);
  } else {
    x = Read32(p);
  }
  IGNORE_RACES_END();
  return x;
}

static void WriteMemWord(u8 *p, u64 rde, u32 osz, u64 x) {
  if (osz == 8) {
    Store64(p, x);
  } else if (osz == 2) {
    Store16(p, x);
  } else {
    Store32(p, x);
  }
}

static u64 ReadMemWord(u8 *p, u32 osz) {
  u64 x;
  if (osz == 8) {
    x = Load64(p);
  } else if (osz == 2) {
    x = Load16(p);
  } else {
    x = Load32(p);
  }
  return x;
}

static void PushN(P, u64 x, unsigned mode, unsigned osz) {
  u8 *w;
  u64 v;
  u8 b[8];
  void *p[2];
  switch (mode) {
    case XED_MODE_REAL:
      v = (Get32(m->sp) - osz) & 0xffff;
      Put16(m->sp, v);
      v += m->ss.base;
      break;
    case XED_MODE_LEGACY:
      v = (Get32(m->sp) - osz) & 0xffffffff;
      Put64(m->sp, v);
      v += m->ss.base;
      break;
    case XED_MODE_LONG:
      v = (Get64(m->sp) - osz) & 0xffffffffffffffff;
      Put64(m->sp, v);
      break;
    default:
      __builtin_unreachable();
  }
  w = AccessRam(m, v, osz, p, b, false);
  WriteStackWord(w, rde, osz, x);
  EndStore(m, v, osz, p, b);
}

void Push(P, u64 x) {
  PushN(A, x, Eamode(rde), kStackOsz[Osz(rde)][Mode(rde)]);
}

void OpPushZvq(P) {
  int osz = kStackOsz[Osz(rde)][Mode(rde)];
  PushN(A, ReadStackWord(RegRexbSrm(m, rde), osz), Eamode(rde), osz);
  if (IsMakingPath(m) && HasLinearMapping() && !Osz(rde)) {
    Jitter(A,
           "a1i"
           "m",
           RexbSrm(rde), FastPush);
  }
}

static u64 PopN(P, u16 extra, unsigned osz) {
  u64 v;
  u8 b[8];
  void *p[2];
  switch (Eamode(rde)) {
    case XED_MODE_LONG:
      v = Get64(m->sp);
      Put64(m->sp, v + osz + extra);
      break;
    case XED_MODE_LEGACY:
      v = Get32(m->sp);
      Put64(m->sp, (v + osz + extra) & 0xffffffff);
      v += m->ss.base;
      break;
    case XED_MODE_REAL:
      v = Get16(m->sp);
      Put16(m->sp, v + osz + extra);
      v += m->ss.base;
      break;
    default:
      __builtin_unreachable();
  }
  return ReadStackWord(AccessRam(m, v, osz, p, b, true), osz);
}

u64 Pop(P, u16 extra) {
  return PopN(A, extra, kStackOsz[Osz(rde)][Mode(rde)]);
}

void OpPopZvq(P) {
  u64 x;
  int osz;
  osz = kStackOsz[Osz(rde)][Mode(rde)];
  x = PopN(A, 0, osz);
  switch (osz) {
    case 8:
    case 4:
      Put64(RegRexbSrm(m, rde), x);
      break;
    case 2:
      Put16(RegRexbSrm(m, rde), x);
      break;
    default:
      __builtin_unreachable();
  }
  if (IsMakingPath(m) && HasLinearMapping() && !Osz(rde)) {
    Jitter(A,
           "a1i"
           "m",
           RexbSrm(rde), FastPop);
  }
}

static void OpCall(P, u64 func) {
  PushN(A, m->ip, Mode(rde), kCallOsz[Osz(rde)][Mode(rde)]);
  m->ip = func;
}

void OpCallJvds(P) {
  OpCall(A, m->ip + disp);
  if (HasLinearMapping() && IsMakingPath(m)) {
    Terminate(A, FastCall);
  }
}

static u64 LoadAddressFromMemory(P) {
  unsigned osz = kCallOsz[Osz(rde)][Mode(rde)];
  return ReadMemWord(GetModrmRegisterWordPointerRead(A, osz), osz);
}

void OpCallEq(P) {
  if (IsMakingPath(m) && HasLinearMapping() && !Osz(rde)) {
    Jitter(A,
           "z3B"    // res0 = GetRegOrMem[force64bit](RexbRm)
           "s0a1="  // arg1 = machine
           "t"      // arg0 = res0
           "m",     // call micro-op (FastCallAbs)
           FastCallAbs);
  }
  OpCall(A, LoadAddressFromMemory(A));
}

void OpJmpEq(P) {
  if (IsMakingPath(m) && HasLinearMapping() && !Osz(rde)) {
    Jitter(A,
           "z3B"    // res0 = GetRegOrMem[force64bit](RexbRm)
           "s0a1="  // arg1 = machine
           "t"      // arg0 = res0
           "m",     // call micro-op (FastJmpAbs)
           FastJmpAbs);
  }
  m->ip = LoadAddressFromMemory(A);
}

void OpLeave(P) {
  switch (Eamode(rde)) {
    case XED_MODE_LONG:
      Put64(m->sp, Get64(m->bp));
      Put64(m->bp, Pop(A, 0));
      if (HasLinearMapping() && IsMakingPath(m)) {
        Jitter(A, "m", FastLeave);
      }
      break;
    case XED_MODE_LEGACY:
      Put64(m->sp, Get32(m->bp));
      Put64(m->bp, Pop(A, 0));
      break;
    case XED_MODE_REAL:
      Put16(m->sp, Get16(m->bp));
      Put16(m->bp, Pop(A, 0));
      break;
    default:
      __builtin_unreachable();
  }
}

void OpRet(P) {
  m->ip = Pop(A, 0);
  if (IsMakingPath(m) && HasLinearMapping() && !Osz(rde)) {
#ifdef __x86_64__
    Jitter(A,
           "a1i"  // arg1 = prediction
           "m"    // call micro-op (PredictRet)
           "q",   // arg0 = machine
           m->ip, PredictRet);
    AlignJit(m->path.jb, 8, 3);
    u8 code[] = {
        0x48, 0x85, 0300 | kJitRes0 << 3 | kJitRes0,  // test %rax,%rax
        0x75, 0x05,                                   // jnz   +5
    };
#else
    Jitter(A,
           "a1i"    // arg1 = prediction
           "m"      // call micro-op (PredictRet)
           "r0a2="  // arg2 = res0
           "q",     // arg0 = machine
           m->ip, PredictRet);
    u32 code[] = {
        0xb5000000 | (8 / 4) << 5 | kJitArg2,  // cbnz x2,#8
    };
#endif
    AppendJit(m->path.jb, code, sizeof(code));
    Connect(A, m->ip, true);
    AppendJitJump(m->path.jb, (void *)m->system->ender);
    FinishPath(m);
  }
}

relegated void OpRetIw(P) {
  m->ip = Pop(A, uimm0);
}

void OpPushEvq(P) {
  unsigned osz = kStackOsz[Osz(rde)][Mode(rde)];
  Push(A, ReadMemWord(GetModrmRegisterWordPointerRead(A, osz), osz));
}

void OpPopEvq(P) {
  unsigned osz = kStackOsz[Osz(rde)][Mode(rde)];
  WriteMemWord(GetModrmRegisterWordPointerWrite(A, osz), rde, osz, Pop(A, 0));
}
