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
#include "blink/macros.h"
#include "blink/pml4t.h"
#include "blink/util.h"

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
  if (pp->lines++) AppendStr(&pp->b, "\n");
  AppendFmt(&pp->b, "%012" PRIx64 "-", start);
}

static void FormatEndPage(struct Machine *m, struct Pml4tFormater *pp,
                          i64 end) {
  char size[16];
  pp->t = false;
  FormatSize(size, end - pp->start, 1024);
  AppendFmt(&pp->b, "%012" PRIx64 " %5s ", end - 1, size);
  if (m->nolinear) {
    AppendFmt(&pp->b, "%3d%% ",
              (int)ceil((double)pp->committed / pp->count * 100));
  }
  if (pp->flags & PAGE_U) AppendFmt(&pp->b, "r");
  if (pp->flags & PAGE_RW) AppendFmt(&pp->b, "w");
  if (~pp->flags & PAGE_XD) AppendFmt(&pp->b, "x");
}

static u8 *GetPt(struct Machine *m, u64 entry) {
  return GetPageAddress(m->system, entry);
}

char *FormatPml4t(struct Machine *m) {
  u8 *pd[4];
  u64 entry;
  u16 i, a[4];
  struct Pml4tFormater pp = {0};
  u16 range[][2] = {{256, 512}, {0, 256}};
  if (m->mode != XED_MODE_LONG) return strdup("");
  unassert(m->system->cr3);
  pd[0] = GetPt(m, m->system->cr3);
  for (i = 0; i < ARRAYLEN(range); ++i) {
    a[0] = range[i][0];
    do {
      a[1] = a[2] = a[3] = 0;
      if (~*(pd[0] + a[0] * 8) & PAGE_V) {
        if (pp.t) FormatEndPage(m, &pp, MakeAddress(a));
      } else {
        pd[1] = GetPt(m, Read64(pd[0] + a[0] * 8));
        do {
          a[2] = a[3] = 0;
          if (~*(pd[1] + a[1] * 8) & PAGE_V) {
            if (pp.t) FormatEndPage(m, &pp, MakeAddress(a));
          } else {
            pd[2] = GetPt(m, Read64(pd[1] + a[1] * 8));
            do {
              a[3] = 0;
              if (~*(pd[2] + a[2] * 8) & PAGE_V) {
                if (pp.t) FormatEndPage(m, &pp, MakeAddress(a));
              } else {
                pd[3] = GetPt(m, Read64(pd[2] + a[2] * 8));
                do {
                  entry = Read64(pd[3] + a[3] * 8);
                  if (~entry & PAGE_V) {
                    if (pp.t) FormatEndPage(m, &pp, MakeAddress(a));
                  } else {
                    if (pp.t && (pp.flags != (entry & INTERESTING_FLAGS))) {
                      FormatEndPage(m, &pp, MakeAddress(a));
                    }
                    if (!pp.t) {
                      FormatStartPage(&pp, MakeAddress(a));
                      pp.flags = entry & INTERESTING_FLAGS;
                    }
                    ++pp.count;
                    if (~entry & PAGE_RSRV) {
                      ++pp.committed;
                    }
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
