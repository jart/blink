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
#include <math.h>
#include <string.h>

#include "blink/flags.h"
#include "blink/machine.h"

#define LDBL 3
#define RINT 0

static void ResetFpu(struct Machine *m) {
  long i;
  m->fpu.sw = 0;
  m->fpu.tw = -1;
  // We diverge from System V ABI here since we don't have long double
  // support yet. We only support double precision when using x87 fpu.
  //
  //	8087 FPU Control Word
  //	 IM: Invalid Operation ───────────────┐
  //	 DM: Denormal Operand ───────────────┐│
  //	 ZM: Zero Divide ───────────────────┐││
  //	 OM: Overflow ─────────────────────┐│││
  //	 UM: Underflow ───────────────────┐││││
  //	 PM: Precision ──────────────────┐│││││
  //	 PC: Precision Control ───────┐  ││││││
  //	  {float,∅,double,long double}│  ││││││
  //	 RC: Rounding Control ──────┐ │  ││││││
  //	  {even, →-∞, →+∞, →0}      │┌┤  ││││││
  //	                           ┌┤││  ││││││
  //	                          d││││rr││││││
  m->fpu.cw = 0b0000000000000000000001001111111;
  for (i = 0; i < 8; ++i) {
    m->fpu.st[i] = -NAN;
  }
}

static void ResetSse(struct Machine *m) {
  // SSE CONTROL AND STATUS REGISTER
  // IE: Invalid Operation Flag ──────────────┐
  // DE: Denormal Flag ──────────────────────┐│
  // ZE: Divide-by-Zero Flag ───────────────┐││
  // OE: Overflow Flag ────────────────────┐│││
  // UE: Underflow Flag ──────────────────┐││││
  // PE: Precision Flag ─────────────────┐│││││
  // DAZ: Denormals Are Zeros ──────────┐││││││
  // IM: Invalid Operation Mask ───────┐│││││││
  // DM: Denormal Operation Mask ─────┐││││││││
  // ZM: Divide-by-Zero Mask ────────┐│││││││││
  // OM: Overflow Mask ─────────────┐││││││││││
  // UM: Underflow Mask ───────────┐│││││││││││
  // PM: Precision Mask ──────────┐││││││││││││
  // RC: Rounding Control ───────┐│││││││││││││
  //   {even, →-∞, →+∞, →0}      ││││││││││││││
  //                            ┌┤│││││││││││││
  //           ┌───────────────┐│││││││││││││││
  //           │   reserved    ││││││││││││││││
  m->mxcsr = 0b00000000000000000001111110000000;
  memset(m->xmm, 0, sizeof(m->xmm));
}

void ResetCpu(struct Machine *m) {
  m->faultaddr = 0;
  m->opcache->stashsize = 0;
  m->stashaddr = 0;
  m->writeaddr = 0;
  m->readaddr = 0;
  m->writesize = 0;
  m->readsize = 0;
  m->flags = 0;
  m->flags = SetFlag(m->flags, FLAGS_IF, 1);
  m->flags = SetFlag(m->flags, FLAGS_VF, 1);
  m->flags = SetFlag(m->flags, FLAGS_IOPL, 3);
  memset(m->beg, 0, sizeof(m->beg));
  memset(m->bofram, 0, sizeof(m->bofram));
  memset(&m->freelist, 0, sizeof(m->freelist));
  ResetSse(m);
  ResetFpu(m);
}

void ResetTlb(struct Machine *m) {
  m->tlbindex = 0;
  memset(m->tlb, 0, sizeof(m->tlb));
  m->opcache->codevirt = 0;
  m->opcache->codehost = 0;
}

void ResetInstructionCache(struct Machine *m) {
  memset(m->opcache->icache, 0, sizeof(m->opcache->icache));
  m->opcache->codevirt = 0;
  m->opcache->codehost = 0;
}
