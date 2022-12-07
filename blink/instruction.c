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
#include "blink/assert.h"
#include "blink/bitscan.h"
#include "blink/endian.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/modrm.h"
#include "blink/stats.h"
#include "blink/x86.h"

static bool IsOpcodeEqual(struct XedDecodedInst *xedd, u8 *a) {
  int n;
  u64 w;
  if ((n = xedd->length)) {
    unassert(n <= 15);
    if (n <= 7) {
      w = Read64(a) ^ Read64(xedd->bytes);
      return !w || (bsf(w) >> 3) >= n;
    } else {
      return !memcmp(a, xedd->bytes, n);
    }
  } else {
    return false;
  }
}

static void ReadInstruction(struct Machine *m, u8 *p, unsigned n) {
  struct XedDecodedInst xedd[1];
  STATISTIC(++instructions_decoded);
  if (!DecodeInstruction(xedd, p, n, m->mode)) {
    memcpy(m->xedd, xedd, kInstructionBytes);
  } else {
    HaltMachine(m, kMachineDecodeError);
  }
}

static void LoadInstructionSlow(struct Machine *m, u64 ip) {
  u8 *addr;
  unsigned i;
  u8 copy[15], *toil;
  i = 4096 - (ip & 4095);
  STATISTIC(++page_overlaps);
  addr = ResolveAddress(m, ip);
  if ((toil = LookupAddress(m, ip + i))) {
    memcpy(copy, addr, i);
    memcpy(copy + i, toil, 15 - i);
    ReadInstruction(m, copy, 15);
  } else {
    ReadInstruction(m, addr, i);
  }
}

void LoadInstruction(struct Machine *m) {
  u64 pc;
  u8 *addr;
  unsigned key;
  if (atomic_load_explicit(&m->opcache->invalidated, memory_order_relaxed)) {
    ResetInstructionCache(m);
  }
  pc = GetPc(m);
  key = pc & (ARRAYLEN(m->opcache->icache) - 1);
  m->xedd = (struct XedDecodedInst *)m->opcache->icache[key];
  if ((pc & 4095) < 4096 - 15) {
    if (pc - (pc & 4095) == m->opcache->codevirt && m->opcache->codehost) {
      addr = m->opcache->codehost + (pc & 4095);
    } else {
      m->opcache->codevirt = pc - (pc & 4095);
      m->opcache->codehost = ResolveAddress(m, m->opcache->codevirt);
      addr = m->opcache->codehost + (pc & 4095);
    }
    if (IsOpcodeEqual(m->xedd, addr)) {
      STATISTIC(++instructions_cached);
    } else {
      ReadInstruction(m, addr, 15);
    }
  } else {
    LoadInstructionSlow(m, pc);
  }
}
