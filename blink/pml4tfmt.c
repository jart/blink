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
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "blink/assert.h"
#include "blink/buffer.h"
#include "blink/endian.h"
#include "blink/flag.h"
#include "blink/high.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/pml4t.h"
#include "blink/rde.h"
#include "blink/util.h"
#include "blink/x86.h"

#define INTERESTING_FLAGS (PAGE_U | PAGE_RW | PAGE_XD)

struct Pml4tFormater {
  bool t;
  i64 start;
  u64 flags;
  int count;
  int committed;
  struct Buffer b;
  long lines;
};

static i64 MakeAddress(u16 a[4]) {
  u64 x;
  x = 0;
  x |= a[0];
  x <<= 9;
  x |= a[1];
  x <<= 9;
  x |= a[2];
  x <<= 9;
  x |= a[3];
  x <<= 12;
  return x;
}

static void FormatStartPage(struct Pml4tFormater *pp, i64 start) {
  pp->t = true;
  pp->start = start;
  pp->count = 0;
  pp->committed = 0;
}

static void FormatEndPage(struct Machine *m, struct Pml4tFormater *pp,
                          i64 end) {
  int i;
  char size[16];
  struct FileMap *fm;
  bool isreading, iswriting, isexecuting;
  pp->t = false;
  if (pp->lines++) AppendStr(&pp->b, "\n");
  isexecuting =
      MAX(pp->start, m->ip) < MIN(m->ip + Oplength(m->xedd->op.rde), end);
  isreading = MAX(pp->start, m->readaddr) < MIN(m->readaddr + m->readsize, end);
  iswriting =
      MAX(pp->start, m->writeaddr) < MIN(m->writeaddr + m->writesize, end);
  if (g_high.enabled) {
    if (isexecuting) AppendStr(&pp->b, "\033[7m");
    if (isreading || iswriting) AppendStr(&pp->b, "\033[1m");
  }
  AppendFmt(&pp->b, "%012" PRIx64 "-%012" PRIx64, pp->start, end - 1);
  if (g_high.enabled && (isreading || iswriting || isexecuting)) {
    AppendStr(&pp->b, "\033[0m");
  }
  FormatSize(size, end - pp->start, 1024);
  AppendFmt(&pp->b, " %5s ", size);
  if (FLAG_nolinear) {
    AppendFmt(&pp->b, "%3d%% ",
              (int)ceil((double)pp->committed / pp->count * 100));
  }
  i = 0;
  if (pp->flags & PAGE_U) {
    AppendStr(&pp->b, "r");
    ++i;
  }
  if (pp->flags & PAGE_RW) {
    AppendStr(&pp->b, "w");
    ++i;
  }
  if (~pp->flags & PAGE_XD) {
    AppendStr(&pp->b, "x");
    ++i;
  }
  while (i++ < 4) {
    AppendFmt(&pp->b, " ");
  }
  if ((fm = GetFileMap(m->system, pp->start))) {
    AppendStr(&pp->b, fm->path);
  }
}

static void FormatPdeOrPte(struct Machine *m, struct Pml4tFormater *pp,
                           u64 entry, u16 a[4], int n) {
  if (pp->t && (pp->flags != (entry & INTERESTING_FLAGS))) {
    FormatEndPage(m, pp, MakeAddress(a));
  }
  if (!pp->t) {
    FormatStartPage(pp, MakeAddress(a));
    pp->flags = entry & INTERESTING_FLAGS;
  }
  pp->count += n;
  if (~entry & PAGE_RSRV) {
    pp->committed += n;
  }
}

static u8 *GetPt(struct Machine *m, u64 entry, bool is_cr3) {
  return GetPageAddress(m->system, entry, is_cr3);
}

char *FormatPml4t(struct Machine *m) {
  u8 *pd[4];
  u64 entry;
  u16 i, a[4];
  struct Pml4tFormater pp = {0};
  u16 range[][2] = {{256, 512}, {0, 256}};
  if (m->mode != XED_MODE_LONG) return strdup("");
  unassert(m->system->cr3);
  pd[0] = GetPt(m, m->system->cr3, true);
  for (i = 0; i < ARRAYLEN(range); ++i) {
    a[0] = range[i][0];
    do {
      a[1] = a[2] = a[3] = 0;
      entry = Read64(pd[0] + a[0] * 8);
      if (!(entry & PAGE_V)) {
        if (pp.t) FormatEndPage(m, &pp, MakeAddress(a));
      } else if (entry & PAGE_PS) {
        FormatPdeOrPte(m, &pp, entry, a, 1 << 27);
      } else {
        pd[1] = GetPt(m, entry, false);
        do {
          a[2] = a[3] = 0;
          entry = Read64(pd[1] + a[1] * 8);
          if (!(entry & PAGE_V)) {
            if (pp.t) FormatEndPage(m, &pp, MakeAddress(a));
          } else if (entry & PAGE_PS) {
            FormatPdeOrPte(m, &pp, entry, a, 1 << 18);
          } else {
            pd[2] = GetPt(m, entry, false);
            do {
              a[3] = 0;
              entry = Read64(pd[2] + a[2] * 8);
              if (!(entry & PAGE_V)) {
                if (pp.t) FormatEndPage(m, &pp, MakeAddress(a));
              } else if (entry & PAGE_PS) {
                FormatPdeOrPte(m, &pp, entry, a, 1 << 9);
              } else {
                pd[3] = GetPt(m, entry, false);
                do {
                  entry = Read64(pd[3] + a[3] * 8);
                  if (~entry & PAGE_V) {
                    if (pp.t) FormatEndPage(m, &pp, MakeAddress(a));
                  } else {
                    FormatPdeOrPte(m, &pp, entry, a, 1);
                  }
                } while (++a[3] != 512);
              }
            } while (++a[2] != 512);
          }
        } while (++a[1] != 512);
      }
    } while (++a[0] != range[i][1]);
  }
  if (pp.t) {
    FormatEndPage(m, &pp, 0x800000000000);
  }
  if (pp.b.p) {
    return (char *)realloc(pp.b.p, pp.b.i + 1);
  } else {
    return strdup("");
  }
}
