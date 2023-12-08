/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2023 Justine Alexandra Roberts Tunney                              │
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

#include "blink/alu.h"
#include "blink/endian.h"
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/x86.h"

#ifndef DISABLE_METAL

relegated void OpIncZv(P) {
  if (!Osz(rde)) {
    Put32(RegSrm(m, rde), Inc32(m, Get32(RegSrm(m, rde)), 0));
  } else {
    Put16(RegSrm(m, rde), Inc16(m, Get16(RegSrm(m, rde)), 0));
  }
}

relegated void OpDecZv(P) {
  if (!Osz(rde)) {
    Put32(RegSrm(m, rde), Dec32(m, Get32(RegSrm(m, rde)), 0));
  } else {
    Put16(RegSrm(m, rde), Dec16(m, Get16(RegSrm(m, rde)), 0));
  }
}

static relegated void PushaCommon(struct Machine *m, void *b, size_t n) {
  u32 v;
  switch (m->mode.omode) {
    case XED_MODE_REAL:
      Put16(m->sp, (v = (Get16(m->sp) - n) & 0xffff));
      break;
    case XED_MODE_LEGACY:
      Put64(m->sp, (v = (Get32(m->sp) - n) & 0xffffffff));
      break;
    default:
      __builtin_unreachable();
  }
  CopyToUser(m, m->ss.base + v, b, n);
}

static relegated void Pushaw(P) {
  u8 b[8][2];
  memcpy(b[0], m->di, 2);
  memcpy(b[1], m->si, 2);
  memcpy(b[2], m->bp, 2);
  memcpy(b[3], m->sp, 2);
  memcpy(b[4], m->bx, 2);
  memcpy(b[5], m->dx, 2);
  memcpy(b[6], m->cx, 2);
  memcpy(b[7], m->ax, 2);
  PushaCommon(m, b, sizeof(b));
}

static relegated void Pushad(P) {
  u8 b[8][4];
  memcpy(b[0], m->di, 4);
  memcpy(b[1], m->si, 4);
  memcpy(b[2], m->bp, 4);
  memcpy(b[3], m->sp, 4);
  memcpy(b[4], m->bx, 4);
  memcpy(b[5], m->dx, 4);
  memcpy(b[6], m->cx, 4);
  memcpy(b[7], m->ax, 4);
  PushaCommon(m, b, sizeof(b));
}

static relegated void PopaCommon(struct Machine *m, void *b, size_t n) {
  u64 addr;
  switch (m->mode.omode) {
    case XED_MODE_REAL:
      addr = m->ss.base + Read16(m->sp);
      if (CopyFromUser(m, b, addr, n) == -1) {
        ThrowSegmentationFault(m, addr);
      }
      Put16(m->sp, (Get16(m->sp) + n) & 0xffff);
      break;
    case XED_MODE_LEGACY:
      addr = m->ss.base + Read32(m->sp);
      if (CopyFromUser(m, b, addr, n) == -1) {
        ThrowSegmentationFault(m, addr);
      }
      Put64(m->sp, (Get32(m->sp) + n) & 0xffffffff);
      break;
    default:
      __builtin_unreachable();
  }
}

static relegated void Popaw(P) {
  u8 b[8][2];
  PopaCommon(m, b, sizeof(b));
  memcpy(m->di, b[0], 2);
  memcpy(m->si, b[1], 2);
  memcpy(m->bp, b[2], 2);
  memcpy(m->bx, b[4], 2);
  memcpy(m->dx, b[5], 2);
  memcpy(m->cx, b[6], 2);
  memcpy(m->ax, b[7], 2);
}

static relegated void Popad(P) {
  u8 b[8][4];
  PopaCommon(m, b, sizeof(b));
  memcpy(m->di, b[0], 4);
  memcpy(m->si, b[1], 4);
  memcpy(m->bp, b[2], 4);
  memcpy(m->bx, b[4], 4);
  memcpy(m->dx, b[5], 4);
  memcpy(m->cx, b[6], 4);
  memcpy(m->ax, b[7], 4);
}

relegated void OpPusha(P) {
  switch (m->mode.omode) {
    case XED_MODE_REAL:
    case XED_MODE_LEGACY:
      if (Osz(rde)) {
        Pushaw(A);
      } else {
        Pushad(A);
      }
      break;
    case XED_MODE_LONG:
      OpUdImpl(m);
    default:
      __builtin_unreachable();
  }
}

relegated void OpPopa(P) {
  switch (m->mode.omode) {
    case XED_MODE_REAL:
    case XED_MODE_LEGACY:
      if (Osz(rde)) {
        Popaw(A);
      } else {
        Popad(A);
      }
      break;
    case XED_MODE_LONG:
      OpUdImpl(m);
    default:
      __builtin_unreachable();
  }
}

relegated void OpCallf(P) {
  Push(A, m->cs.sel);
  Push(A, m->ip);
  LongBranch(A, uimm0, disp & (Osz(rde) ? 0xffff : 0xffffffff));
}

relegated void OpRetf(P) {
  u64 ip = Pop(A, 0);
  u16 cs = Pop(A, uimm0);
  LongBranch(A, cs, ip);
}

#endif /* DISABLE_METAL */
