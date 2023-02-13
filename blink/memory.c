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
#include <sys/mman.h>

#include "blink/assert.h"
#include "blink/bus.h"
#include "blink/debug.h"
#include "blink/endian.h"
#include "blink/errno.h"
#include "blink/likely.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/pml4t.h"
#include "blink/stats.h"
#include "blink/thread.h"
#include "blink/util.h"
#include "blink/x86.h"

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

u8 *GetPageAddress(struct System *s, u64 entry) {
  unassert(entry & PAGE_V);
  unassert(~entry & PAGE_RSRV);
  if (entry & PAGE_HOST) {
    return (u8 *)(intptr_t)(entry & PAGE_TA);
  } else if ((entry & PAGE_TA) + 4096 <= kRealSize) {
    unassert(s->real);
    return s->real + (entry & PAGE_TA);
  } else {
    return 0;
  }
}

u64 HandlePageFault(struct Machine *m, u64 entry, u64 table, unsigned index) {
  u64 x, res, page;
  unassert(entry & PAGE_RSRV);
  LOCK(&m->system->mmap_lock);
  // page faults should only happen in non-linear mode
  if (!(entry & (PAGE_HOST | PAGE_MAP | PAGE_MUG))) {
    // an anonymous page is being accessed for the first time
    if ((page = AllocatePage(m->system)) != -1) {
      --m->system->memstat.reserved;
      ++m->system->memstat.committed;
      x = (page & (PAGE_TA | PAGE_HOST)) | (entry & ~(PAGE_TA | PAGE_RSRV));
      Store64(GetPageAddress(m->system, table) + index * 8, x);
      res = x;
    } else {
      res = 0;
    }
  } else {
    // a file-mapped page is being accessed for the first time
    unassert((entry & (PAGE_HOST | PAGE_MAP)) == (PAGE_HOST | PAGE_MAP));
    --m->system->memstat.reserved;
    ++m->system->memstat.committed;
    x = entry & ~PAGE_RSRV;
    Store64(GetPageAddress(m->system, table) + index * 8, x);
    res = x;
  }
  UNLOCK(&m->system->mmap_lock);
  return res;
}

static u64 FindPageTableEntry(struct Machine *m, u64 page) {
  long i;
  i64 table;
  u64 entry, res;
  unsigned level, index;
  struct MachineTlb bubble;
  for (i = 0; i < ARRAYLEN(m->tlb); ++i) {
    if (m->tlb[i].page == page && ((res = m->tlb[i].entry) & PAGE_V)) {
      if (i) {
        STATISTIC(++tlb_hits_2);
        bubble = m->tlb[i - 1];
        m->tlb[i - 1] = m->tlb[i];
        m->tlb[i] = bubble;
      }
      return res;
    }
  }
  STATISTIC(++tlb_misses);
  unassert((entry = m->system->cr3));
  level = 39;
  do {
    table = entry;
    index = (page >> level) & 511;
    entry = Load64(GetPageAddress(m->system, table) + index * 8);
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

u8 *LookupAddress2(struct Machine *m, i64 virt, u64 mask, u64 need) {
  u8 *host;
  u64 entry, page;
  if (m->mode == XED_MODE_LONG ||
      (m->mode != XED_MODE_REAL && (m->system->cr0 & CR0_PG))) {
    if (atomic_load_explicit(&m->invalidated, memory_order_relaxed)) {
      ResetTlb(m);
      atomic_store_explicit(&m->invalidated, false, memory_order_relaxed);
    }
    if ((page = virt & -4096) == m->tlb[0].page &&
        ((entry = m->tlb[0].entry) & PAGE_V)) {
      STATISTIC(++tlb_hits_1);
    } else if (-0x800000000000 <= virt && virt < 0x800000000000) {
      if (!(entry = FindPageTableEntry(m, page))) {
        return (u8 *)efault0();
      }
    } else {
      return (u8 *)efault0();
    }
  } else if (virt >= 0 && virt <= 0xffffffff &&
             (virt & 0xffffffff) + 4095 < kRealSize) {
    unassert(m->system->real);
    return m->system->real + virt;
  } else {
    return (u8 *)efault0();
  }
  if ((entry & mask) != need) {
    return (u8 *)efault0();
  }
  if ((host = GetPageAddress(m->system, entry))) {
    return host + (virt & 4095);
  } else {
    return (u8 *)efault0();
  }
}

u8 *LookupAddress(struct Machine *m, i64 virt) {
  return LookupAddress2(m, virt, PAGE_U, PAGE_U);
}

u8 *GetAddress(struct Machine *m, i64 v) {
  if (HasLinearMapping(m)) return ToHost(v);
  return LookupAddress(m, v);
}

u8 *ResolveAddress(struct Machine *m, i64 v) {
  u8 *r;
  if ((r = GetAddress(m, v))) return r;
  ThrowSegmentationFault(m, v);
}

bool IsValidMemory(struct Machine *m, i64 virt, i64 size, int prot) {
  i64 p, pe;
  u64 pte, mask, need;
  unassert(m->mode == XED_MODE_LONG);
  unassert(prot && !(prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC)));
  if ((-0x800000000000 <= virt && virt < 0x800000000000) &&
      size <= 0x800000000000 && virt + size <= 0x800000000000) {
    need = 0;
    mask = 0;
    if (prot & PROT_READ) {
      mask |= PAGE_U;
      need |= PAGE_U;
    }
    if (prot & PROT_WRITE) {
      mask |= PAGE_RW;
      need |= PAGE_RW;
    }
    if (prot & PROT_EXEC) {
      mask |= PAGE_XD;
    }
    for (p = virt, pe = virt + size; p < pe; p += 4096) {
      if (!(pte = FindPageTableEntry(m, p))) {
        return false;
      }
      if ((pte & mask) != need) {
        return false;
      }
    }
    return true;
  } else {
    return false;
  }
}

int VirtualCopy(struct Machine *m, i64 v, char *r, u64 n, bool d) {
  u8 *p;
  u64 k;
  k = 4096 - (v & 4095);
  while (n) {
    k = MIN(k, n);
    if (!(p = LookupAddress(m, v))) return -1;
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
  return 0;
}

int CopyFromUser(struct Machine *m, void *dst, i64 src, u64 n) {
  return VirtualCopy(m, src, (char *)dst, n, true);
}

int CopyFromUserRead(struct Machine *m, void *dst, i64 addr, u64 n) {
  if (CopyFromUser(m, dst, addr, n) == -1) return -1;
  SetReadAddr(m, addr, n);
  return 0;
}

int CopyToUser(struct Machine *m, i64 dst, void *src, u64 n) {
  return VirtualCopy(m, dst, (char *)src, n, false);
}

int CopyToUserWrite(struct Machine *m, i64 addr, void *src, u64 n) {
  if (CopyToUser(m, addr, src, n) == -1) return -1;
  SetWriteAddr(m, addr, n);
  return 0;
}

void CommitStash(struct Machine *m) {
  unassert(m->stashaddr);
  if (m->opcache->writable) {
    CopyToUser(m, m->stashaddr, m->opcache->stash, m->opcache->stashsize);
  }
  m->stashaddr = 0;
}

u8 *ReserveAddress(struct Machine *m, i64 v, size_t n, bool writable) {
  if (HasLinearMapping(m)) return ToHost(v);
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
  unassert(n > k);
  unassert(p[0]);
  unassert(p[1]);
  memcpy(p[0], b, k);
  memcpy(p[1], b + k, n - k);
}

void EndStoreNp(struct Machine *m, i64 v, size_t n, void *p[2], u8 *b) {
  if (v) EndStore(m, v, n, p, b);
}

void *AddToFreeList(struct Machine *m, void *mem) {
  int n;
  void *p;
  p = m->freelist.p;
  n = m->freelist.n + 1;
  if ((p = realloc(p, n * sizeof(*m->freelist.p)))) {
    m->freelist.p = (void **)p;
    m->freelist.n = n;
    m->freelist.p[n - 1] = mem;
    return mem;
  } else {
    free(mem);
    return 0;
  }
}

// Returns pointer to memory in guest memory. If the memory overlaps a
// page boundary, then it's copied, and the temporary memory is pushed
// to the free list. Returns NULL w/ EFAULT or ENOMEM on error.
void *Schlep(struct Machine *m, i64 addr, size_t size, u64 mask, u64 need) {
  char *copy;
  size_t have;
  void *res, *page;
  if (!size) return 0;
  if (!(page = LookupAddress2(m, addr, mask, need))) return 0;
  have = 4096 - (addr & 4095);
  if (size <= have) {
    res = page;
  } else {
    if (!(copy = (char *)malloc(size))) return 0;
    memcpy(copy, page, have);
    for (; have < size; have += 4096) {
      if (!(page = LookupAddress2(m, addr + have, mask, need))) {
        free(copy);
        return 0;
      }
      memcpy(copy + have, page, MIN(4096, size - have));
    }
    res = AddToFreeList(m, copy);
  }
  return res;
}

void *SchlepR(struct Machine *m, i64 addr, size_t size) {
  SetReadAddr(m, addr, size);
  return Schlep(m, addr, size, PAGE_U, PAGE_U);
}

void *SchlepW(struct Machine *m, i64 addr, size_t size) {
  SetWriteAddr(m, addr, size);
  return Schlep(m, addr, size, PAGE_RW, PAGE_RW);
}

void *SchlepRW(struct Machine *m, i64 addr, size_t size) {
  SetReadAddr(m, addr, size);
  SetWriteAddr(m, addr, size);
  return Schlep(m, addr, size, PAGE_U | PAGE_RW, PAGE_U | PAGE_RW);
}

static char *LoadStrImpl(struct Machine *m, i64 addr) {
  size_t have;
  char *copy, *page, *p;
  have = 4096 - (addr & 4095);
  if (!addr) return 0;
  if (!(page = (char *)LookupAddress2(m, addr, PAGE_U, PAGE_U))) return 0;
  if ((p = (char *)memchr(page, '\0', have))) {
    SetReadAddr(m, addr, p - page + 1);
    return page;
  }
  if (!(copy = (char *)malloc(have + 4096))) return 0;
  memcpy(copy, page, have);
  for (;;) {
    if (!(page = (char *)LookupAddress2(m, addr + have, PAGE_U, PAGE_U))) break;
    if ((p = (char *)memccpy(copy + have, page, '\0', 4096))) {
      SetReadAddr(m, addr, have + (p - (copy + have)) + 1);
      return (char *)AddToFreeList(m, copy);
    }
    have += 4096;
    if (!(p = (char *)realloc(copy, have + 4096))) break;
    copy = p;
  }
  free(copy);
  return 0;
}

// Returns pointer to string in guest memory. If the string overlaps a
// page boundary, then it's copied, and the temporary memory is pushed
// to the free list. Returns NULL w/ EFAULT or ENOMEM on error.
char *LoadStr(struct Machine *m, i64 addr) {
  char *res;
  if ((res = LoadStrImpl(m, addr))) {
    SYS_LOGF("LoadStr(%#" PRIx64 ") -> \"%s\"", addr, res);
  }
  return res;
}

// Copies string from guest memory. The returned memory is pushed to the
// machine free list. NULL w/ ENOMEM is returned if we're out of memory.
char *CopyStr(struct Machine *m, i64 addr) {
  char *s;
  if (!(s = LoadStr(m, addr))) return 0;
  return (char *)AddToFreeList(m, strdup(s));
}

// Returns fully copied NULL-terminated NUL-terminated string list. All
// memory allocated by this routine is pushed to the machine free list.
char **CopyStrList(struct Machine *m, i64 addr) {
  int n;
  u8 b[8];
  char *s;
  void *mem;
  char **list;
  for (list = 0, n = 0;;) {
    if ((mem = realloc(list, ++n * sizeof(*list)))) {
      list = (char **)mem;
    } else {
      free(list);
      return 0;
    }
    CopyFromUserRead(m, b, addr + n * 8 - 8, 8);
    if (Read64(b)) {
      if ((s = CopyStr(m, Read64(b)))) {
        list[n - 1] = s;
      } else {
        free(list);
        return 0;
      }
    } else {
      list[n - 1] = 0;
      return (char **)AddToFreeList(m, list);
    }
  }
}
