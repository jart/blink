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
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "blink/endian.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/memory.h"
#include "blink/pml4t.h"
#include "blink/throw.h"

void SetReadAddr(struct Machine *m, int64_t addr, uint32_t size) {
  if (size) {
    m->readaddr = addr;
    m->readsize = size;
  }
}

void SetWriteAddr(struct Machine *m, int64_t addr, uint32_t size) {
  if (size) {
    m->writeaddr = addr;
    m->writesize = size;
  }
}

uint64_t HandlePageFault(struct Machine *m, uint64_t entry, uint64_t table,
                         unsigned index) {
  long page;
  uint64_t x;
  if ((page = AllocateLinearPage(m)) != -1) {
    --m->system->memstat.reserved;
    x = page | (entry & ~(PAGE_TA | PAGE_IGN1));
    Write64(m->system->real.p + table + index * 8, x);
    return x;
  } else {
    return 0;
  }
}

uint64_t FindPage(struct Machine *m, int64_t virt) {
  uint64_t table, entry;
  unsigned level, index, i;
  virt &= -4096;
  for (i = 0; i < ARRAYLEN(m->tlb); ++i) {
    if (m->tlb[i].virt == virt && (m->tlb[i].entry & 1)) {
      return m->tlb[i].entry;
    }
  }
  level = 39;
  entry = m->system->cr3;
  do {
    table = entry & PAGE_TA;
    assert(table < m->system->real.n);
    index = (virt >> level) & 511;
    entry = Read64(m->system->real.p + table + index * 8);
    if (!(entry & 1)) return 0;
  } while ((level -= 9) >= 12);
  if ((entry & PAGE_RSRV) &&
      (entry = HandlePageFault(m, entry, table, index)) == -1) {
    return 0;
  }
  m->tlbindex = (m->tlbindex + 1) & (ARRAYLEN(m->tlb) - 1);
  m->tlb[m->tlbindex] = m->tlb[0];
  m->tlb[0].virt = virt;
  m->tlb[0].entry = entry;
  return entry;
}

void *FindReal(struct Machine *m, int64_t virt) {
  uint64_t entry;
  if ((m->mode & 3) != XED_MODE_REAL) {
    if (-0x800000000000 <= virt && virt < 0x800000000000) {
      if (!(entry = FindPage(m, virt))) return NULL;
      return m->system->real.p + (entry & PAGE_TA) + (virt & 4095);
    } else {
      return NULL;
    }
  } else if (0 <= virt && virt + 4095 < m->system->real.n) {
    return m->system->real.p + virt;
  } else {
    return NULL;
  }
}

void *ResolveAddress(struct Machine *m, int64_t v) {
  void *r;
  if ((r = FindReal(m, v))) return r;
  ThrowSegmentationFault(m, v);
}

void VirtualSet(struct Machine *m, int64_t v, char c, uint64_t n) {
  char *p;
  uint64_t k;
  k = 4096 - (v & 4095);
  while (n) {
    k = MIN(k, n);
    p = ResolveAddress(m, v);
    memset(p, c, k);
    n -= k;
    v += k;
    k = 4096;
  }
}

void VirtualCopy(struct Machine *m, int64_t v, char *r, uint64_t n, bool d) {
  char *p;
  uint64_t k;
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

void *VirtualSend(struct Machine *m, void *dst, int64_t src, uint64_t n) {
  VirtualCopy(m, src, dst, n, true);
  return dst;
}

void VirtualSendRead(struct Machine *m, void *dst, int64_t addr, uint64_t n) {
  VirtualSend(m, dst, addr, n);
  SetReadAddr(m, addr, n);
}

void VirtualRecv(struct Machine *m, int64_t dst, void *src, uint64_t n) {
  VirtualCopy(m, dst, src, n, false);
}

void VirtualRecvWrite(struct Machine *m, int64_t addr, void *src, uint64_t n) {
  VirtualRecv(m, addr, src, n);
  SetWriteAddr(m, addr, n);
}

void *ReserveAddress(struct Machine *m, int64_t v, size_t n) {
  void *r;
  assert(n <= sizeof(m->opcache->stash));
  if ((v & 4095) + n <= 4096) return ResolveAddress(m, v);
  m->opcache->stashaddr = v;
  m->opcache->stashsize = n;
  r = m->opcache->stash;
  VirtualSend(m, r, v, n);
  return r;
}

void *AccessRam(struct Machine *m, int64_t v, size_t n, void *p[2],
                uint8_t *tmp, bool copy) {
  unsigned k;
  uint8_t *a, *b;
  assert(n <= 4096);
  if ((v & 4095) + n <= 4096) return ResolveAddress(m, v);
  k = 4096;
  k -= v & 4095;
  assert(k <= 4096);
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

void *Load(struct Machine *m, int64_t v, size_t n, uint8_t *b) {
  void *p[2];
  SetReadAddr(m, v, n);
  return AccessRam(m, v, n, p, b, true);
}

void *BeginStore(struct Machine *m, int64_t v, size_t n, void *p[2],
                 uint8_t *b) {
  SetWriteAddr(m, v, n);
  return AccessRam(m, v, n, p, b, false);
}

void *BeginStoreNp(struct Machine *m, int64_t v, size_t n, void *p[2],
                   uint8_t *b) {
  if (!v) return NULL;
  return BeginStore(m, v, n, p, b);
}

void *BeginLoadStore(struct Machine *m, int64_t v, size_t n, void *p[2],
                     uint8_t *b) {
  SetWriteAddr(m, v, n);
  return AccessRam(m, v, n, p, b, true);
}

void EndStore(struct Machine *m, int64_t v, size_t n, void *p[2], uint8_t *b) {
  unsigned k;
  assert(n <= 4096);
  if ((v & 4095) + n <= 4096) return;
  k = 4096;
  k -= v & 4095;
  assert(k > n);
  assert(p[0]);
  assert(p[1]);
  memcpy(p[0], b, k);
  memcpy(p[1], b + k, n - k);
}

void EndStoreNp(struct Machine *m, int64_t v, size_t n, void *p[2],
                uint8_t *b) {
  if (v) EndStore(m, v, n, p, b);
}

void *LoadBuf(struct Machine *m, int64_t addr, size_t size) {
  size_t have, need;
  char *buf, *copy, *page;
  have = 4096 - (addr & 4095);
  if (!addr) return NULL;
  if (!(buf = FindReal(m, addr))) return NULL;
  if (size > have) {
    if (!(copy = malloc(size))) return NULL;
    buf = memcpy(copy, buf, have);
    do {
      need = MIN(4096, size - have);
      if ((page = FindReal(m, addr + have))) {
        memcpy(copy + have, page, need);
        have += need;
      } else {
        free(copy);
        return NULL;
      }
    } while (have < size);
  }
  SetReadAddr(m, addr, size);
  return buf;
}

void *LoadStr(struct Machine *m, int64_t addr) {
  size_t have;
  char *copy, *page, *p;
  have = 4096 - (addr & 4095);
  if (!addr) return NULL;
  if (!(page = FindReal(m, addr))) return NULL;
  if ((p = memchr(page, '\0', have))) {
    SetReadAddr(m, addr, p - page + 1);
    return page;
  }
  if (!(copy = malloc(have + 4096))) return NULL;
  memcpy(copy, page, have);
  for (;;) {
    if (!(page = FindReal(m, addr + have))) break;
    if ((p = memccpy(copy + have, page, '\0', 4096))) {
      SetReadAddr(m, addr, have + (p - (copy + have)) + 1);
      m->freelist.p =
          realloc(m->freelist.p, ++m->freelist.n * sizeof(*m->freelist.p));
      return (m->freelist.p[m->freelist.n - 1] = copy);
    }
    have += 4096;
    if (!(p = realloc(copy, have + 4096))) break;
    copy = p;
  }
  free(copy);
  return NULL;
}

char **LoadStrList(struct Machine *m, int64_t addr) {
  int n;
  char **list;
  uint8_t b[8];
  for (list = 0, n = 0;;) {
    list = realloc(list, ++n * sizeof(*list));
    VirtualSendRead(m, b, addr + n * 8 - 8, 8);
    if (Read64(b)) {
      list[n - 1] = LoadStr(m, Read64(b));
    } else {
      list[n - 1] = 0;
      break;
    }
  }
  return list;
}
