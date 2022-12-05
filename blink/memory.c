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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "blink/assert.h"
#include "blink/debug.h"
#include "blink/endian.h"
#include "blink/likely.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/memory.h"
#include "blink/mop.h"
#include "blink/pml4t.h"
#include "blink/real.h"
#include "blink/stats.h"

void SetReadAddr(struct Machine *m, i64 addr, u32 size) {
  if (size) {
    m->readaddr = addr;
    m->readsize = size;
  }
}

void SetWriteAddr(struct Machine *m, i64 addr, u32 size) {
  if (size) {
    m->writeaddr = addr;
    m->writesize = size;
  }
}

u64 HandlePageFault(struct Machine *m, u64 entry, u64 table, unsigned index) {
  long page;
  u64 x;
  if ((page = AllocateLinearPage(m->system)) != -1) {
    --m->system->memstat.reserved;
    x = page | (entry & ~(PAGE_TA | PAGE_IGN1));
    Store64(m->system->real.p + table + index * 8, x);
    return x;
  } else {
    return 0;
  }
}

static u64 FindPage(struct Machine *m, u64 page) {
  long i;
  i64 table;
  u64 entry, res;
  unsigned level, index;
  struct MachineTlb bubble;
  for (i = 1; i < ARRAYLEN(m->tlb); ++i) {
    if (m->tlb[i].page == page && ((res = m->tlb[i].entry) & PAGE_V)) {
      STATISTIC(++tlb_hits_2);
      bubble = m->tlb[i - 1];
      m->tlb[i - 1] = m->tlb[i];
      m->tlb[i] = bubble;
      return res;
    }
  }
  STATISTIC(++tlb_misses);
  level = 39;
  entry = m->system->cr3;
  do {
    table = entry & PAGE_TA;
    unassert(table < GetRealMemorySize(m->system));
    index = (page >> level) & 511;
    entry = Load64(m->system->real.p + table + index * 8);
    if (!(entry & PAGE_V)) return 0;
  } while ((level -= 9) >= 12);
  if ((entry & PAGE_RSRV) &&
      !(entry = HandlePageFault(m, entry, table, index))) {
    return 0;
  }
  m->tlb[ARRAYLEN(m->tlb) - 1].page = page;
  m->tlb[ARRAYLEN(m->tlb) - 1].entry = entry;
  return entry;
}

static u8 *ConvertReal(struct Machine *m, u64 entry, int virt) {
  if (entry & PAGE_ID) {
    return ToHost(entry & PAGE_TA) + (virt & 4095);
  } else {
    return m->system->real.p + (entry & PAGE_TA) + (virt & 4095);
  }
}

u8 *FindReal(struct Machine *m, i64 virt) {
  u64 entry, page;
  if (m->mode != XED_MODE_REAL) {
    if (atomic_load_explicit(&m->tlb_invalidated, memory_order_relaxed)) {
      ResetTlb(m);
      atomic_store_explicit(&m->tlb_invalidated, false, memory_order_relaxed);
    }
    if ((page = virt & -4096) == m->tlb[0].page &&
        ((entry = m->tlb[0].entry) & PAGE_V)) {
      STATISTIC(++tlb_hits_1);
      return ConvertReal(m, entry, virt);
    }
    if (-0x800000000000 <= virt && virt < 0x800000000000) {
      if (!(entry = FindPage(m, page))) return 0;
      return ConvertReal(m, entry, virt);
    } else {
      return 0;
    }
  } else if (virt >= 0 && virt <= 0xffffffff &&
             (virt & 0xffffffff) + 4095 < GetRealMemorySize(m->system)) {
    return m->system->real.p + virt;
  } else {
    return 0;
  }
}

u8 *ResolveAddress(struct Machine *m, i64 v) {
  u8 *r;
  if (IsLinear(m)) return ToHost(v);
  if ((r = FindReal(m, v))) return r;
  ThrowSegmentationFault(m, v);
}

static void VirtualCopy(struct Machine *m, i64 v, char *r, u64 n, bool d) {
  u8 *p;
  u64 k;
  k = 4096 - (v & 4095);
  while (n) {
    k = MIN(k, n);
    p = ResolveAddress(m, v);
    if (d) {
      memcpy(r, p, k);
    } else {
      memcpy(p, r, k);
    }
    n -= k;
    r += k;
    v += k;
    k = 4096;
  }
}

u8 *CopyFromUser(struct Machine *m, void *dst, i64 src, u64 n) {
  VirtualCopy(m, src, (char *)dst, n, true);
  return (u8 *)dst;
}

void CopyFromUserRead(struct Machine *m, void *dst, i64 addr, u64 n) {
  CopyFromUser(m, dst, addr, n);
  SetReadAddr(m, addr, n);
}

void CopyToUser(struct Machine *m, i64 dst, void *src, u64 n) {
  VirtualCopy(m, dst, (char *)src, n, false);
}

void CopyToUserWrite(struct Machine *m, i64 addr, void *src, u64 n) {
  CopyToUser(m, addr, src, n);
  SetWriteAddr(m, addr, n);
}

void CommitStash(struct Machine *m) {
  unassert(m->stashaddr);
  if (m->opcache->writable) {
    CopyToUser(m, m->stashaddr, m->opcache->stash, m->opcache->stashsize);
  }
  m->stashaddr = 0;
}

u8 *ReserveAddress(struct Machine *m, i64 v, size_t n, bool writable) {
  m->reserving = true;
  if ((v & 4095) + n <= 4096) {
    return ResolveAddress(m, v);
  }
  STATISTIC(++page_overlaps);
  m->stashaddr = v;
  m->opcache->stashsize = n;
  m->opcache->writable = writable;
  u8 *r = m->opcache->stash;
  CopyFromUser(m, r, v, n);
  return r;
}

u8 *AccessRam(struct Machine *m, i64 v, size_t n, void *p[2], u8 *tmp,
              bool copy) {
  u8 *a, *b;
  unsigned k;
  unassert(n <= 4096);
  if ((v & 4095) + n <= 4096) {
    return ResolveAddress(m, v);
  }
  STATISTIC(++page_overlaps);
  k = 4096;
  k -= v & 4095;
  unassert(k <= 4096);
  a = ResolveAddress(m, v);
  b = ResolveAddress(m, v + k);
  if (copy) {
    memcpy(tmp, a, k);
    memcpy(tmp + k, b, n - k);
  }
  p[0] = a;
  p[1] = b;
  return tmp;
}

u8 *Load(struct Machine *m, i64 v, size_t n, u8 *b) {
  void *p[2];
  SetReadAddr(m, v, n);
  return AccessRam(m, v, n, p, b, true);
}

u8 *BeginStore(struct Machine *m, i64 v, size_t n, void *p[2], u8 *b) {
  SetWriteAddr(m, v, n);
  return AccessRam(m, v, n, p, b, false);
}

u8 *BeginStoreNp(struct Machine *m, i64 v, size_t n, void *p[2], u8 *b) {
  if (!v) return NULL;
  return BeginStore(m, v, n, p, b);
}

u8 *BeginLoadStore(struct Machine *m, i64 v, size_t n, void *p[2], u8 *b) {
  SetWriteAddr(m, v, n);
  return AccessRam(m, v, n, p, b, true);
}

void EndStore(struct Machine *m, i64 v, size_t n, void *p[2], u8 *b) {
  unsigned k;
  unassert(n <= 4096);
  if ((v & 4095) + n <= 4096) return;
  k = 4096;
  k -= v & 4095;
  unassert(k > n);
  unassert(p[0]);
  unassert(p[1]);
  memcpy(p[0], b, k);
  memcpy(p[1], b + k, n - k);
}

void EndStoreNp(struct Machine *m, i64 v, size_t n, void *p[2], u8 *b) {
  if (v) EndStore(m, v, n, p, b);
}

char *LoadStr(struct Machine *m, i64 addr) {
  size_t have;
  char *copy, *page, *p;
  have = 4096 - (addr & 4095);
  if (!addr) return 0;
  if (!(page = (char *)FindReal(m, addr))) return 0;
  if ((p = (char *)memchr(page, '\0', have))) {
    SetReadAddr(m, addr, p - page + 1);
    return page;
  }
  if (!(copy = (char *)malloc(have + 4096))) return 0;
  memcpy(copy, page, have);
  for (;;) {
    if (!(page = (char *)FindReal(m, addr + have))) break;
    if ((p = (char *)memccpy(copy + have, page, '\0', 4096))) {
      SetReadAddr(m, addr, have + (p - (copy + have)) + 1);
      m->freelist.p = (void **)realloc(
          m->freelist.p, ++m->freelist.n * sizeof(*m->freelist.p));
      return (char *)(m->freelist.p[m->freelist.n - 1] = copy);
    }
    have += 4096;
    if (!(p = (char *)realloc(copy, have + 4096))) break;
    copy = p;
  }
  free(copy);
  return 0;
}

char **LoadStrList(struct Machine *m, i64 addr) {
  int n;
  u8 b[8];
  char **list;
  for (list = 0, n = 0;;) {
    list = (char **)realloc(list, ++n * sizeof(*list));
    CopyFromUserRead(m, b, addr + n * 8 - 8, 8);
    if (Read64(b)) {
      list[n - 1] = (char *)LoadStr(m, Read64(b));
    } else {
      list[n - 1] = 0;
      break;
    }
  }
  return list;
}
