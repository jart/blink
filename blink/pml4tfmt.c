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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blink/assert.h"
#include "blink/bus.h"
#include "blink/endian.h"
#include "blink/flag.h"
#include "blink/high.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/pml4t.h"
#include "blink/rde.h"
#include "blink/thread.h"
#include "blink/util.h"
#include "blink/x86.h"

#define INTERESTING_FLAGS (PAGE_U | PAGE_RW | PAGE_XD | PAGE_FILE)

#define BYTES       16384
#define APPEND(...) u->o += snprintf(u->b + u->o, BYTES - u->o, __VA_ARGS__)

struct MapMaker {
  bool t;
  int o;
  int count;
  int committed;
  long lines;
  char *b;
  i64 start;
  u64 flags;
  struct FileMap *fm;
};

static bool IsStackRead(long mop) {
  switch (mop) {
    case 0x007:  // OpPopSeg
    case 0x00F:  // OpPopSeg
    case 0x017:  // OpPopSeg
    case 0x01F:  // OpPopSeg
    case 0x058:  // OpPopZvq
    case 0x059:  // OpPopZvq
    case 0x05A:  // OpPopZvq
    case 0x05B:  // OpPopZvq
    case 0x05C:  // OpPopZvq
    case 0x05D:  // OpPopZvq
    case 0x05E:  // OpPopZvq
    case 0x05F:  // OpPopZvq
    case 0x061:  // OpPopa
    case 0x08F:  // OpPopEvq
    case 0x09D:  // OpPopf
    case 0x1A1:  // OpPopSeg
    case 0x1A9:  // OpPopSeg
    case 0x0C2:  // OpRetIw
    case 0x0C3:  // OpRet
    case 0x0CA:  // OpRetf
    case 0x0CB:  // OpRetf
    case 0x0C9:  // OpLeave
      return true;
    default:
      return false;
  }
}

static bool IsStackWrite(long mop) {
  switch (mop) {
    case 0x006:  // OpPushSeg
    case 0x00E:  // OpPushSeg
    case 0x016:  // OpPushSeg
    case 0x01E:  // OpPushSeg
    case 0x050:  // OpPushZvq
    case 0x051:  // OpPushZvq
    case 0x052:  // OpPushZvq
    case 0x053:  // OpPushZvq
    case 0x054:  // OpPushZvq
    case 0x055:  // OpPushZvq
    case 0x056:  // OpPushZvq
    case 0x057:  // OpPushZvq
    case 0x060:  // OpPusha
    case 0x068:  // OpPushImm
    case 0x06A:  // OpPushImm
    case 0x09C:  // OpPushf
    case 0x1A0:  // OpPushSeg
    case 0x1A8:  // OpPushSeg
    case 0x0C9:  // OpLeave
    case 0x09A:  // OpCallf
    case 0x0E8:  // OpCallJvds
      return true;
    default:
      return false;
  }
}

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

static void FormatStartPage(struct Machine *m, struct MapMaker *u, i64 start) {
  u->t = true;
  u->start = start;
  u->count = 0;
  u->committed = 0;
  u->fm = GetFileMap(m->system, start);
}

static void FormatEndPage(struct Machine *m, struct MapMaker *u, i64 end) {
  int i;
  char size[16];
  bool isreading, iswriting, isexecuting, isfault;
  u->t = false;
  if (u->lines++) APPEND("\n");
  isexecuting = m->xedd && MAX(u->start, m->ip) <
                               MIN(m->ip + Oplength(m->xedd->op.rde), end);
  isreading =
      MAX(u->start, m->readaddr) < MIN(m->readaddr + m->readsize, end) ||
      (m->xedd && IsStackRead(Mopcode(m->xedd->op.rde)) &&
       MAX(u->start, Read64(m->sp)) < MIN(Read64(m->sp) - 8, end));
  iswriting =
      MAX(u->start, m->writeaddr) < MIN(m->writeaddr + m->writesize, end) ||
      (m->xedd && IsStackWrite(Mopcode(m->xedd->op.rde)) &&
       MAX(u->start, Read64(m->sp)) < MIN(Read64(m->sp) + 8, end));
  isfault = MAX(u->start, m->faultaddr) < MIN(m->faultaddr + 1, end);
  if (g_high.enabled) {
    if (isexecuting) APPEND("\033[7m");
    if (isreading) APPEND("\033[1m");
    if (iswriting) APPEND("\033[31m");
  }
  APPEND("%012" PRIx64 "-%012" PRIx64, u->start, end - 1);
  if (g_high.enabled && (isreading || iswriting || isexecuting)) {
    APPEND("\033[0m");
  }
  FormatSize(size, end - u->start, 1024);
  APPEND("%c%5s ", isfault ? '*' : ' ', size);
  if (FLAG_nolinear) {
    APPEND("%3d%% ", (int)ceil((double)u->committed / u->count * 100));
  }
  i = 0;
  if (u->flags & PAGE_U) {
    APPEND("r");
    ++i;
  }
  if (u->flags & PAGE_RW) {
    APPEND("w");
    ++i;
  }
  if (~u->flags & PAGE_XD) {
    APPEND("x");
    ++i;
  }
  while (i++ < 4) {
    APPEND(" ");
  }
  if (u->fm) {
    APPEND("%s", u->fm->path);
  }
}

static void FormatPdeOrPte(struct Machine *m, struct MapMaker *u, u64 entry,
                           u16 a[4], int n) {
  i64 addr;
  addr = MakeAddress(a);
  if (u->t && u->flags != (entry & INTERESTING_FLAGS)) {
    FormatEndPage(m, u, addr);
  }
  if (!u->t) {
    FormatStartPage(m, u, addr);
    u->flags = entry & INTERESTING_FLAGS;
  }
  u->count += n;
  if (~entry & PAGE_RSRV) {
    u->committed += n;
  }
}

static u8 *GetPt(struct Machine *m, u64 entry, bool is_cr3) {
  return GetPageAddress(m->system, entry, is_cr3);
}

char *FormatPml4t(struct Machine *m) {
  _Thread_local static char b[BYTES];
  u8 *pd[4];
  u64 entry;
  u16 i, a[4];
  struct MapMaker u = {.b = b};
  u16 range[][2] = {{256, 512}, {0, 256}};
  b[0] = 0;
  if (m->mode.omode != XED_MODE_LONG) return b;
  unassert(m->system->cr3);
  pd[0] = GetPt(m, m->system->cr3, true);
  for (i = 0; i < ARRAYLEN(range); ++i) {
    a[0] = range[i][0];
    do {
      a[1] = a[2] = a[3] = 0;
      entry = LoadPte(pd[0] + a[0] * 8);
      if (!(entry & PAGE_V)) {
        if (u.t) FormatEndPage(m, &u, MakeAddress(a));
      } else if (entry & PAGE_PS) {
        FormatPdeOrPte(m, &u, entry, a, 1 << 27);
      } else {
        pd[1] = GetPt(m, entry, false);
        do {
          a[2] = a[3] = 0;
          entry = LoadPte(pd[1] + a[1] * 8);
          if (!(entry & PAGE_V)) {
            if (u.t) FormatEndPage(m, &u, MakeAddress(a));
          } else if (entry & PAGE_PS) {
            FormatPdeOrPte(m, &u, entry, a, 1 << 18);
          } else {
            pd[2] = GetPt(m, entry, false);
            do {
              a[3] = 0;
              entry = LoadPte(pd[2] + a[2] * 8);
              if (!(entry & PAGE_V)) {
                if (u.t) FormatEndPage(m, &u, MakeAddress(a));
              } else if (entry & PAGE_PS) {
                FormatPdeOrPte(m, &u, entry, a, 1 << 9);
              } else {
                pd[3] = GetPt(m, entry, false);
                do {
                  entry = LoadPte(pd[3] + a[3] * 8);
                  if (~entry & PAGE_V) {
                    if (u.t) FormatEndPage(m, &u, MakeAddress(a));
                  } else {
                    FormatPdeOrPte(m, &u, entry, a, 1);
                  }
                } while (++a[3] != 512);
              }
            } while (++a[2] != 512);
          }
        } while (++a[1] != 512);
      }
    } while (++a[0] != range[i][1]);
  }
  if (u.t) {
    FormatEndPage(m, &u, 0x800000000000);
  }
  return b;
}
