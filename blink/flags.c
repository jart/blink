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
#include "blink/builtin.h"
#include "blink/debug.h"
#include "blink/flags.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/rde.h"
#include "blink/stats.h"
#include "blink/x86.h"

bool GetParity(u8 b) {
  b ^= b >> 4;
  b ^= b >> 2;
  b ^= b >> 1;
  return ~b & 1;
}

void ImportFlags(struct Machine *m, u64 flags) {
  u64 mask = 0;
  mask |= 1 << FLAGS_CF;
  mask |= 1 << FLAGS_PF;
  mask |= 1 << FLAGS_AF;
  mask |= 1 << FLAGS_ZF;
  mask |= 1 << FLAGS_SF;
  mask |= 1 << FLAGS_TF;
  mask |= 1 << FLAGS_IF;
  mask |= 1 << FLAGS_DF;
  mask |= 1 << FLAGS_OF;
  mask |= 1 << FLAGS_NT;
  mask |= 1 << FLAGS_AC;
  mask |= 1 << FLAGS_ID;
  m->flags = (flags & mask) | (m->flags & ~mask);
  m->flags = SetFlag(m->flags, FLAGS_RF, false);
  m->flags = SetLazyParityByte(m->flags, !((m->flags >> FLAGS_PF) & 1));
}

u64 ExportFlags(u64 flags) {
  flags = SetFlag(flags, FLAGS_IOPL, 3);
  flags = SetFlag(flags, FLAGS_F1, true);
  flags = SetFlag(flags, FLAGS_F0, false);
  flags = flags & ~(1ull << FLAGS_PF);
  flags |= GetLazyParityBool(flags) << FLAGS_PF;
  return flags;
}
