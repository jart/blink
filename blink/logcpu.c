/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
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
#include <inttypes.h>
#include <stdio.h>

#include "blink/debug.h"
#include "blink/endian.h"
#include "blink/machine.h"

// use cosmopolitan/tool/build/fastdiff.c
void LogCpu(struct Machine *m) {
  static FILE *f;
  if (!f) f = fopen("/tmp/cpu.log", "w");
  fprintf(f,
          "\n"
          "IP %" PRIx64 "\n"
          "AX %#" PRIx64 "\n"
          "CX %#" PRIx64 "\n"
          "DX %#" PRIx64 "\n"
          "BX %#" PRIx64 "\n"
          "SP %#" PRIx64 "\n"
          "BP %#" PRIx64 "\n"
          "SI %#" PRIx64 "\n"
          "DI %#" PRIx64 "\n"
          "R8 %#" PRIx64 "\n"
          "R9 %#" PRIx64 "\n"
          "R10 %#" PRIx64 "\n"
          "R11 %#" PRIx64 "\n"
          "R12 %#" PRIx64 "\n"
          "R13 %#" PRIx64 "\n"
          "R14 %#" PRIx64 "\n"
          "R15 %#" PRIx64 "\n"
          "FLAGS %s\n"
          "%s\n",
          m->ip, Read64(m->ax), Read64(m->cx), Read64(m->dx), Read64(m->bx),
          Read64(m->sp), Read64(m->bp), Read64(m->si), Read64(m->di),
          Read64(m->r8), Read64(m->r9), Read64(m->r10), Read64(m->r11),
          Read64(m->r12), Read64(m->r13), Read64(m->r14), Read64(m->r15),
          DescribeCpuFlags(m->flags), DescribeOp(m, GetPc(m)));
}
