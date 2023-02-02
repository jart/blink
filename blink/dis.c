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
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "blink/assert.h"
#include "blink/debug.h"
#include "blink/dis.h"
#include "blink/endian.h"
#include "blink/high.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"

#include "blink/types.h"
#include "blink/util.h"

#define ADDRLEN 8
#define BYTELEN 11
#define PFIXLEN 4
#define NAMELEN 8
#define CODELEN 40
#define CODELIM 15
#define DATALIM 8
#define PIVOTOP pos_opcode

char *DisColumn(char *p2, char *p1, long need) {
  char *p;
  long have;
  unassert(p2 >= p1);
  have = p2 - p1;
  p = p2;
  do {
    *p++ = ' ';
  } while (++have < need);
  *p = '\0';
  return p;
}

static char *DisOctets(char *p, const u8 *d, size_t n) {
  size_t i;
  for (i = 0; i < n; ++i) {
    if (i) *p++ = ',';
    *p++ = '0';
    *p++ = 'x';
    *p++ = "0123456789abcdef"[(d[i] & 0xf0) >> 4];
    *p++ = "0123456789abcdef"[(d[i] & 0x0f) >> 0];
  }
  *p = '\0';
  return p;
}

static char *DisByte(char *p, const u8 *d, size_t n) {
  p = HighStart(p, g_high.keyword);
  p = DisColumn(stpcpy(p, ".byte"), p, NAMELEN);
  p = HighEnd(p);
  p = DisOctets(p, d, n);
  return p;
}

static char *DisError(struct Dis *d, char *p, int err) {
  p = DisColumn(DisByte(p, d->xedd->bytes, MIN(15, d->xedd->length)), p,
                CODELEN);
  p = HighStart(p, g_high.comment);
  *p++ = '#';
  *p++ = ' ';
  p = stpcpy(p, "error");
  p = HighEnd(p);
  *p = '\0';
  return p;
}

static size_t uint64toarray_fixed16(u64 x, char b[17], u8 k) {
  char *p;
  unassert(k <= 64 && !(k & 3));
  for (p = b; k > 0;) *p++ = "0123456789abcdef"[(x >> (k -= 4)) & 15];
  *p = '\0';
  return p - b;
}

static char *DisAddr(struct Dis *d, char *p) {
  i64 x = d->addr;
  if (0 <= x && x < 0x10fff0) {
    return p + uint64toarray_fixed16(x, p, 24);
  } else if (INT_MIN <= x && x <= INT_MAX) {
    return p + uint64toarray_fixed16(x, p, 32);
  } else {
    return p + uint64toarray_fixed16(x, p, 48);
  }
}

static char *DisRaw(struct Dis *d, char *p) {
  long i;
  int plen;
  if (0 <= d->addr && d->addr < 0x10fff0) {
    plen = 2;
  } else {
    plen = PFIXLEN;
  }
  for (i = 0; i < plen - MIN(plen, d->xedd->op.PIVOTOP); ++i) {
    *p++ = ' ';
    *p++ = ' ';
  }
  for (i = 0; i < MIN(15, d->xedd->length); ++i) {
    if (i == d->xedd->op.PIVOTOP) *p++ = ' ';
    *p++ = "0123456789abcdef"[(d->xedd->bytes[i] & 0xf0) >> 4];
    *p++ = "0123456789abcdef"[(d->xedd->bytes[i] & 0x0f) >> 0];
  }
  *p = '\0';
  return p;
}

static char *DisCode(struct Dis *d, char *p, int err) {
  char optspecbuf[128];
  if (!err) {
    return DisInst(d, p, DisSpec(d->xedd, optspecbuf));
  } else {
    return DisError(d, p, err);
  }
}

static char *DisLineCode(struct Dis *d, char *p, int err) {
  intptr_t hook;
  int blen, plen;
  if (0 <= d->addr && d->addr < 0x10fff0) {
    plen = 2;
    blen = 6;
  } else {
    blen = BYTELEN;
    plen = PFIXLEN;
  }
  p = DisColumn(DisAddr(d, p), p, ADDRLEN);
  if (d->m && !IsJitDisabled(&d->m->system->jit)) {
    if ((hook = GetJitHook(&d->m->system->jit, d->addr, 0))) {
      if (hook == (intptr_t)GeneralDispatch) {
        *p++ = 'G';  // general explicit
      } else if (hook == (intptr_t)JitlessDispatch) {
        *p++ = 'S';  // staging hook
      } else {
        *p++ = '*';  // committed jit hook
      }
    } else {
      *p++ = ' ';  // no hook
    }
  }
  if (!d->noraw) {
    p = DisColumn(DisRaw(d, p), p, plen * 2 + 1 + blen * 2);
  } else {
    *p++ = ' ';
    *p++ = ' ';
  }
  p = DisCode(d, p, err);
  return p;
}

static char *DisLabel(struct Dis *d, char *p, const char *name) {
  p = DisColumn(DisAddr(d, p), p, ADDRLEN);
  p = HighStart(p, g_high.label);
  p = Demangle(p, name, DIS_MAX_SYMBOL_LENGTH);
  p = HighEnd(p);
  *p++ = ':';
  *p = '\0';
  return p;
}

long DisFind(struct Dis *d, i64 addr) {
  int l, r, m;
  l = 0;
  r = d->ops.i - 1;
  while (l <= r) {
    m = (l + r) >> 1;
    if (d->ops.p[m].addr < addr) {
      l = m + 1;
    } else if (d->ops.p[m].addr > addr) {
      r = m - 1;
    } else {
      return m;
    }
  }
  return -1;
}

static long DisAppendOpLines(struct Dis *d, struct Machine *m, i64 addr) {
  u8 *r;
  i64 ip;
  unsigned k;
  u8 *p, b[15];
  long n, symbol;
  struct DisOp op;
  n = 15;
  ip = addr - m->cs.base;
  if ((symbol = DisFindSym(d, ip)) != -1) {
    if (d->syms.p[symbol].addr <= ip &&
        ip < d->syms.p[symbol].addr + d->syms.p[symbol].size) {
      n = d->syms.p[symbol].size - (ip - d->syms.p[symbol].addr);
    }
    if (ip == d->syms.p[symbol].addr && d->syms.p[symbol].name) {
      op.addr = addr;
      op.size = 0;
      op.active = true;
      d->addr = addr;
      DisLabel(d, d->buf, d->syms.stab + d->syms.p[symbol].name);
      if (!(op.s = strdup(d->buf))) return -1;
      if (d->ops.i++ == d->ops.n) {
        d->ops.n = d->ops.i + (d->ops.i >> 1);
        d->ops.p =
            (struct DisOp *)realloc(d->ops.p, d->ops.n * sizeof(*d->ops.p));
      }
      d->ops.p[d->ops.i - 1] = op;
    }
  }
  n = MAX(1, MIN(15, n));
  if (!(r = LookupAddress(m, addr))) return -1;
  k = 0x1000 - (addr & 0xfff);
  if (n <= k) {
    p = (u8 *)r;
  } else {
    p = b;
    memcpy(b, r, k);
    if ((r = LookupAddress(m, addr + k))) {
      memcpy(b + k, r, n - k);
    } else {
      n = k;
    }
  }
  DecodeInstruction(d->xedd, p, n, m->mode);
  n = d->xedd->length;
  op.addr = addr;
  op.size = n;
  op.active = true;
  op.s = NULL;
  if (d->ops.i++ == d->ops.n) {
    d->ops.n = d->ops.i + (d->ops.i >> 1);
    d->ops.p = (struct DisOp *)realloc(d->ops.p, d->ops.n * sizeof(*d->ops.p));
  }
  d->ops.p[d->ops.i - 1] = op;
  return n;
}

long Dis(struct Dis *d, struct Machine *m, i64 addr, i64 ip, int lines) {
  i64 i, j, symbol;
  DisFreeOps(&d->ops);
  if ((symbol = DisFindSym(d, addr)) != -1 &&
      (d->syms.p[symbol].addr < addr &&
       addr < d->syms.p[symbol].addr + d->syms.p[symbol].size)) {
    for (i = d->syms.p[symbol].addr; i < addr; i += j) {
      if ((j = DisAppendOpLines(d, m, i)) == -1) return -1;
    }
  }
  for (i = 0; i < lines; ++i, addr += j) {
    if ((j = DisAppendOpLines(d, m, addr)) == -1) return -1;
  }
  return 0;
}

const char *DisGetLine(struct Dis *d, struct Machine *m, int i) {
  int err;
  if (i >= d->ops.i) return "";
  if (d->ops.p[i].s) return d->ops.p[i].s;
  unassert(d->ops.p[i].size <= 15);
  err = GetInstruction(m, d->ops.p[i].addr, d->xedd);
  d->m = m;
  d->addr = d->ops.p[i].addr;
  if (DisLineCode(d, d->buf, err) - d->buf >= (int)sizeof(d->buf)) Abort();
  return d->buf;
}

void DisFreeOp(struct DisOp *o) {
  free(o->s);
}

void DisFreeOps(struct DisOps *ops) {
  long i;
  for (i = 0; i < ops->i; ++i) {
    DisFreeOp(&ops->p[i]);
  }
  free(ops->p);
  memset(ops, 0, sizeof(*ops));
}

void DisFree(struct Dis *d) {
  DisFreeOps(&d->ops);
  free(d->edges.p);
  free(d->loads.p);
  free(d->syms.p);
  memset(d, 0, sizeof(*d));
}
