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
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/machine.h"

static relegated dontinline void BcdFlags(struct Machine *m, bool af, bool cf) {
  m->flags = SetFlag(m->flags, FLAGS_CF, cf);
  m->flags = SetFlag(m->flags, FLAGS_AF, af);
  m->flags = SetFlag(m->flags, FLAGS_ZF, !m->al);
  m->flags = SetFlag(m->flags, FLAGS_SF, (i8)m->al < 0);
  m->flags = SetLazyParityByte(m->flags, m->al);
}

relegated void OpDas(P) {
  u8 al;
  bool af, cf;
  al = m->al;
  af = cf = 0;
  if ((al & 0x0f) > 9 || GetFlag(m->flags, FLAGS_AF)) {
    cf = m->al < 6 || GetFlag(m->flags, FLAGS_CF);
    m->al -= 0x06;
    af = 1;
  }
  if (al > 0x99 || GetFlag(m->flags, FLAGS_CF)) {
    m->al -= 0x60;
    cf = 1;
  }
  BcdFlags(m, af, cf);
}

relegated void OpAaa(P) {
  bool af, cf;
  af = cf = 0;
  if ((m->al & 0x0f) > 9 || GetFlag(m->flags, FLAGS_AF)) {
    cf = m->al < 6 || GetFlag(m->flags, FLAGS_CF);
    Put16(m->ax, Get16(m->ax) + 0x106);
    af = cf = 1;
  }
  m->al &= 0x0f;
  BcdFlags(m, af, cf);
}

relegated void OpAas(P) {
  bool af, cf;
  af = cf = 0;
  if ((m->al & 0x0f) > 9 || GetFlag(m->flags, FLAGS_AF)) {
    cf = m->al < 6 || GetFlag(m->flags, FLAGS_CF);
    Put16(m->ax, Get16(m->ax) - 0x106);
    af = cf = 1;
  }
  m->al &= 0x0f;
  BcdFlags(m, af, cf);
}

relegated void OpAam(P) {
  u8 imm = m->xedd->op.uimm0;
  if (!imm) RaiseDivideError(m);
  m->ah = m->al / imm;
  m->al = m->al % imm;
  BcdFlags(m, 0, 0);
}

relegated void OpAad(P) {
  u8 imm = m->xedd->op.uimm0;
  Put16(m->ax, (m->ah * imm + m->al) & 255);
  BcdFlags(m, 0, 0);
}
