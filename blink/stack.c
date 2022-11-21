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

#include "blink/address.h"
#include "blink/builtin.h"
#include "blink/endian.h"
#include "blink/macros.h"
#include "blink/memory.h"
#include "blink/modrm.h"
#include "blink/tsan.h"

static const u8 kStackOsz[2][3] = {
    {
        2,  // kStackOsz[0][XED_MODE_REAL]
        4,  // kStackOsz[0][XED_MODE_LEGACY]
        8,  // kStackOsz[0][XED_MODE_LONG]
    },
    {
        4,  // kStackOsz[1][XED_MODE_REAL]
        2,  // kStackOsz[1][XED_MODE_LEGACY]
        2,  // kStackOsz[1][XED_MODE_LONG]
    },
};

static const u8 kCallOsz[2][3] = {
    {
        2,  // kCallOsz[0][XED_MODE_REAL]
        4,  // kCallOsz[0][XED_MODE_LEGACY]
        8,  // kCallOsz[0][XED_MODE_LONG]
    },
    {
        4,  // kCallOsz[1][XED_MODE_REAL]
        2,  // kCallOsz[1][XED_MODE_LEGACY]
        8,  // kCallOsz[1][XED_MODE_LONG]
    },
};

static void WriteStackWord(u8 *p, u32 rde, u32 osz, u64 x) {
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

static void PushN(struct Machine *m, u32 rde, u64 x, unsigned mode,
                  unsigned osz) {
  u8 *w;
  u64 v;
  void *p[2];
  u8 b[8];
  switch (mode) {
    case XED_MODE_REAL:
      v = (Get32(m->sp) - osz) & 0xffff;
      Put16(m->sp, v);
      v += m->ss;
      break;
    case XED_MODE_LEGACY:
      v = (Get32(m->sp) - osz) & 0xffffffff;
      Put64(m->sp, v);
      v += m->ss;
      break;
    case XED_MODE_LONG:
      v = (Get64(m->sp) - osz) & 0xffffffffffffffff;
      Put64(m->sp, v);
      break;
    default:
      __builtin_unreachable();
  }
  w = AccessRam(m, v, osz, p, b, false);
  IGNORE_RACES_START();
  WriteStackWord(w, rde, osz, x);
  EndStore(m, v, osz, p, b);
  IGNORE_RACES_END();
}

void Push(struct Machine *m, u32 rde, u64 x) {
  PushN(m, rde, x, Eamode(rde), kStackOsz[Osz(rde)][Mode(rde)]);
}

void OpPushZvq(struct Machine *m, u32 rde) {
  unsigned osz = kStackOsz[Osz(rde)][Mode(rde)];
  PushN(m, rde, ReadStackWord(RegRexbSrm(m, rde), osz), Eamode(rde), osz);
}

static u64 PopN(struct Machine *m, u32 rde, u16 extra, unsigned osz) {
  u64 v;
  void *p[2];
  u8 b[8];
  switch (Eamode(rde)) {
    case XED_MODE_LONG:
      v = Get64(m->sp);
      Put64(m->sp, v + osz + extra);
      break;
    case XED_MODE_LEGACY:
      v = Get32(m->sp);
      Put64(m->sp, (v + osz + extra) & 0xffffffff);
      v += m->ss;
      break;
    case XED_MODE_REAL:
      v = Get32(m->sp);
      Put16(m->sp, v + osz + extra);
      v += m->ss;
      break;
    default:
      __builtin_unreachable();
  }
  return ReadStackWord(AccessRam(m, v, osz, p, b, true), osz);
}

u64 Pop(struct Machine *m, u32 rde, u16 extra) {
  return PopN(m, rde, extra, kStackOsz[Osz(rde)][Mode(rde)]);
}

void OpPopZvq(struct Machine *m, u32 rde) {
  u64 x;
  unsigned osz;
  osz = kStackOsz[Osz(rde)][Mode(rde)];
  x = PopN(m, rde, 0, osz);
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
}

static void OpCall(struct Machine *m, u32 rde, u64 func) {
  PushN(m, rde, m->ip, Mode(rde), kCallOsz[Osz(rde)][Mode(rde)]);
  m->ip = func;
}

void OpCallJvds(struct Machine *m, u32 rde) {
  OpCall(m, rde, m->ip + m->xedd->op.disp);
}

static u64 LoadAddressFromMemory(struct Machine *m, u32 rde) {
  unsigned osz;
  osz = kCallOsz[Osz(rde)][Mode(rde)];
  return ReadStackWord(GetModrmRegisterWordPointerRead(m, rde, osz), osz);
}

void OpCallEq(struct Machine *m, u32 rde) {
  OpCall(m, rde, LoadAddressFromMemory(m, rde));
}

void OpJmpEq(struct Machine *m, u32 rde) {
  m->ip = LoadAddressFromMemory(m, rde);
}

void OpLeave(struct Machine *m, u32 rde) {
  switch (Eamode(rde)) {
    case XED_MODE_LONG:
      Put64(m->sp, Get64(m->bp));
      Put64(m->bp, Pop(m, rde, 0));
      break;
    case XED_MODE_LEGACY:
      Put64(m->sp, Get32(m->bp));
      Put64(m->bp, Pop(m, rde, 0));
      break;
    case XED_MODE_REAL:
      Put16(m->sp, Get16(m->bp));
      Put16(m->bp, Pop(m, rde, 0));
      break;
    default:
      __builtin_unreachable();
  }
}

void OpRet(struct Machine *m, u32 rde) {
  m->ip = Pop(m, rde, m->xedd->op.uimm0);
}

void OpPushEvq(struct Machine *m, u32 rde) {
  unsigned osz;
  osz = kStackOsz[Osz(rde)][Mode(rde)];
  Push(m, rde,
       ReadStackWord(GetModrmRegisterWordPointerRead(m, rde, osz), osz));
}

void OpPopEvq(struct Machine *m, u32 rde) {
  unsigned osz;
  osz = kStackOsz[Osz(rde)][Mode(rde)];
  WriteStackWord(GetModrmRegisterWordPointerWrite(m, rde, osz), rde, osz,
                 Pop(m, rde, 0));
}

static void Pushaw(struct Machine *m, u32 rde) {
  u16 v;
  u8 b[8][2];
  memcpy(b[0], m->di, 2);
  memcpy(b[1], m->si, 2);
  memcpy(b[2], m->bp, 2);
  memcpy(b[3], m->sp, 2);
  memcpy(b[4], m->bx, 2);
  memcpy(b[5], m->dx, 2);
  memcpy(b[6], m->cx, 2);
  memcpy(b[7], m->ax, 2);
  Put16(m->sp, (v = (Read16(m->sp) - sizeof(b)) & 0xffff));
  VirtualRecv(m, m->ss + v, b, sizeof(b));
}

static void Pushad(struct Machine *m, u32 rde) {
  u32 v;
  u8 b[8][4];
  memcpy(b[0], m->di, 4);
  memcpy(b[1], m->si, 4);
  memcpy(b[2], m->bp, 4);
  memcpy(b[3], m->sp, 4);
  memcpy(b[4], m->bx, 4);
  memcpy(b[5], m->dx, 4);
  memcpy(b[6], m->cx, 4);
  memcpy(b[7], m->ax, 4);
  Put64(m->sp, (v = (Get32(m->sp) - sizeof(b)) & 0xffffffff));
  VirtualRecv(m, m->ss + v, b, sizeof(b));
}

static void Popaw(struct Machine *m, u32 rde) {
  u8 b[8][2];
  VirtualSend(m, b, m->ss + Read16(m->sp), sizeof(b));
  Put16(m->sp, (Get32(m->sp) + sizeof(b)) & 0xffff);
  memcpy(m->di, b[0], 2);
  memcpy(m->si, b[1], 2);
  memcpy(m->bp, b[2], 2);
  memcpy(m->sp, b[3], 2);
  memcpy(m->bx, b[4], 2);
  memcpy(m->dx, b[5], 2);
  memcpy(m->cx, b[6], 2);
  memcpy(m->ax, b[7], 2);
}

static void Popad(struct Machine *m, u32 rde) {
  u8 b[8][4];
  VirtualSend(m, b, m->ss + Get32(m->sp), sizeof(b));
  Put64(m->sp, (Get32(m->sp) + sizeof(b)) & 0xffffffff);
  memcpy(m->di, b[0], 4);
  memcpy(m->si, b[1], 4);
  memcpy(m->bp, b[2], 4);
  memcpy(m->sp, b[3], 4);
  memcpy(m->bx, b[4], 4);
  memcpy(m->dx, b[5], 4);
  memcpy(m->cx, b[6], 4);
  memcpy(m->ax, b[7], 4);
}

void OpPusha(struct Machine *m, u32 rde) {
  switch (Eamode(rde)) {
    case XED_MODE_REAL:
      Pushaw(m, rde);
      break;
    case XED_MODE_LEGACY:
      Pushad(m, rde);
      break;
    case XED_MODE_LONG:
      OpUd(m, rde);
    default:
      __builtin_unreachable();
  }
}

void OpPopa(struct Machine *m, u32 rde) {
  switch (Eamode(rde)) {
    case XED_MODE_REAL:
      Popaw(m, rde);
      break;
    case XED_MODE_LEGACY:
      Popad(m, rde);
      break;
    case XED_MODE_LONG:
      OpUd(m, rde);
    default:
      __builtin_unreachable();
  }
}

void OpCallf(struct Machine *m, u32 rde) {
  Push(m, rde, m->cs >> 4);
  Push(m, rde, m->ip);
  m->cs = m->xedd->op.uimm0 << 4;
  m->ip = m->xedd->op.disp & (Osz(rde) ? 0xffff : 0xffffffff);
  if (m->system->onlongbranch) {
    m->system->onlongbranch(m);
  }
}

void OpRetf(struct Machine *m, u32 rde) {
  m->ip = Pop(m, rde, 0);
  m->cs = Pop(m, rde, m->xedd->op.uimm0) << 4;
  if (m->system->onlongbranch) {
    m->system->onlongbranch(m);
  }
}
