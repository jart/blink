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
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "blink/assert.h"
#include "blink/biosrom.h"
#include "blink/bus.h"
#include "blink/checked.h"
#include "blink/debug.h"
#include "blink/endian.h"
#include "blink/errno.h"
#include "blink/linux.h"
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

u8 *GetPageAddress(struct System *s, u64 entry, bool is_cr3) {
  unassert(is_cr3 || (entry & PAGE_V));
  unassert(~entry & PAGE_RSRV);
  if (entry & PAGE_HOST) {
    return (u8 *)(uintptr_t)(entry & PAGE_TA);
  } else {
    unassert(s->real);
    if ((entry & PAGE_TA) + 4096 <= kRealSize) {
      return s->real + (entry & PAGE_TA);
    } else {
      return 0;
    }
  }
}

u64 HandlePageFault(struct Machine *m, u8 *pslot, u64 entry) {
  u64 x, page;
  unassert(entry & PAGE_RSRV);
  unassert(!HasLinearMapping());
  if (m->nofault) {
    m->segvcode = SEGV_MAPERR_LINUX;
    errno = ENOBUFS;
    return 0;
  }
  do {
    if (entry & (PAGE_HOST | PAGE_MAP | PAGE_MUG)) {
      // a file-mapped page is being accessed for the first time
      unassert((entry & (PAGE_HOST | PAGE_MAP)) == (PAGE_HOST | PAGE_MAP));
      x = entry & ~PAGE_RSRV;
      if (CasPte(pslot, entry, x)) {
        m->system->memstat.committed += 1;
        m->system->memstat.reserved -= 1;
        m->system->rss += 1;
        entry = x;
      } else {
        entry = LoadPte(pslot);
      }
    } else {
      // an anonymous page is being accessed for the first time
      if ((page = AllocateAnonymousPage(m->system)) == -1) {
        m->segvcode = SEGV_MAPERR_LINUX;
        entry = 0;
        break;
      }
      x = (page & (PAGE_TA | PAGE_HOST)) | (entry & ~(PAGE_TA | PAGE_RSRV));
      if (CasPte(pslot, entry, x)) {
        m->system->memstat.committed += 1;
        m->system->memstat.reserved -= 1;
        entry = x;
      } else {
        FreeAnonymousPage(m->system, (u8 *)(uintptr_t)(page & PAGE_TA));
        entry = LoadPte(pslot);
        m->system->rss -= 1;
      }
    }
  } while (entry & PAGE_RSRV);
  return entry;
}

bool HasPageLock(const struct Machine *m, i64 page) {
  int i;
  unassert(!(page & 4095));
  for (i = m->pagelocks.i; i--;) {
    if (m->pagelocks.p[i].page == page) {
      return true;
    }
  }
  return false;
}

static bool RecordPageLock(struct Machine *m, i64 page, u8 *pslot) {
  unassert(m->sysdepth > 0);
  unassert(!m->pagelocks.i ||
           m->pagelocks.p[m->pagelocks.i - 1].sysdepth <= m->sysdepth);
  if (m->pagelocks.i == m->pagelocks.n) {
    int n2;
    struct PageLock *p2;
    p2 = m->pagelocks.p;
    n2 = m->pagelocks.n;
    n2 += 3;
    n2 += n2 >> 1;
    if ((p2 = (struct PageLock *)realloc(p2, n2 * sizeof(*p2)))) {
      m->pagelocks.p = p2;
      m->pagelocks.n = n2;
    } else {
      return false;
    }
  }
  m->pagelocks.p[m->pagelocks.i].page = page;
  m->pagelocks.p[m->pagelocks.i].pslot = pslot;
  m->pagelocks.p[m->pagelocks.i].sysdepth = m->sysdepth;
  ++m->pagelocks.i;
  STATISTIC(++page_locks);
  return true;
}

static void ReleasePageLock(u8 *pslot) {
  u64 entry;
  do {
    entry = LoadPte(pslot);
    unassert(entry & PAGE_V);
    unassert(entry & PAGE_LOCKS);
  } while (!CasPte(pslot, entry, entry - PAGE_LOCK));
}

static bool HasOutdatedPageLocks(struct Machine *m) {
  return m->pagelocks.i &&
         m->pagelocks.p[m->pagelocks.i - 1].sysdepth > m->sysdepth;
}

void CollectPageLocks(struct Machine *m) {
  if (HasOutdatedPageLocks(m)) {
    LOCK(&m->system->pagelocks_lock);
    do ReleasePageLock(m->pagelocks.p[--m->pagelocks.i].pslot);
    while (HasOutdatedPageLocks(m));
    unassert(!pthread_cond_broadcast(&m->system->pagelocks_cond));
    UNLOCK(&m->system->pagelocks_lock);
  }
}

// returns page directory entry associated with virtual address
// @return raw page directory entry contents, or zero w/ errno
// @raise EFAULT if a valid 4096 page didn't exist at address
// @raise ENOMEM if memory couldn't be allocated internally
// @raise EAGAIN if too many locks are held on a page
u64 FindPageTableEntry(struct Machine *m, u64 page) {
  u8 *pslot;
  i64 table;
  u64 entry;
  long tlbkey;
  unsigned level, index;
  if (UNLIKELY(atomic_load_explicit(&m->invalidated, memory_order_acquire))) {
    ResetTlb(m);
    atomic_store_explicit(&m->invalidated, false, memory_order_relaxed);
  }
  tlbkey = (page >> 12) & (ARRAYLEN(m->tlb) - 1);
  if (LIKELY(m->tlb[tlbkey].page == page &&
             ((entry = m->tlb[tlbkey].entry) & PAGE_V))) {
    STATISTIC(++tlb_hits);
    return entry;
  }
  STATISTIC(++tlb_misses);
  unassert(!(page & 4095));
  if (!(-0x800000000000 <= (i64)page && (i64)page < 0x800000000000)) {
    m->segvcode = SEGV_MAPERR_LINUX;
    return (u64)(uintptr_t)efault0();
  }
TryAgain:
  unassert((entry = m->system->cr3));
  level = 39;
  do {
    table = entry;
    index = (page >> level) & 511;
    pslot = GetPageAddress(m->system, table, level == 39) + index * 8;
    if (!pslot) goto MapError;
    entry = LoadPte(pslot);
    if (!(entry & PAGE_V)) goto MapError;
    if (m->metal) {
      entry &= ~(u64)(PAGE_RSRV | PAGE_HOST | PAGE_MAP | PAGE_GROW | PAGE_MUG |
                      PAGE_FILE);
    }
    if ((entry & PAGE_PS) && level > 12) {
      // huge (1 GiB or 2 MiB) page; "rewrite" the TLB copy of the page table
      // entry, to point to the 4 KiB subpage being accessed
      // TODO: if partial TLB flushes are implemented in the future, we will
      // also need to somehow record the original huge page size in the TLB,
      // so we can correctly invalidate all TLB entries for the huge page
      u64 submask = ((u64)1 << level) - 4096;
      entry &= ~submask;
      entry |= page & submask;
      break;
    }
  } while ((level -= 9) >= 12);
  if ((entry & PAGE_RSRV) && !(entry = HandlePageFault(m, pslot, entry))) {
    return 0;
  }
  // system calls lock the pages they access
  // this prevents race conditions w/ munmap
  if (m->insyscall && !m->nofault && !HasPageLock(m, page)) {
    if ((entry & PAGE_LOCKS) < PAGE_LOCKS) {
      if (CasPte(pslot, entry, entry + PAGE_LOCK)) {
        unassert(LoadPte(pslot) & PAGE_LOCKS);
        if (RecordPageLock(m, page, pslot)) {
          entry += PAGE_LOCK;
        } else {
          ReleasePageLock(pslot);
          m->segvcode = SEGV_MAPERR_LINUX;
          return 0;
        }
      } else {
        goto TryAgain;
      }
    } else {
      LOGF("too many threads locked page %#" PRIx64, page);
      m->segvcode = SEGV_MAPERR_LINUX;
      errno = EAGAIN;
      return 0;
    }
  }
  m->tlb[tlbkey].page = page;
  m->tlb[tlbkey].entry = entry;
  return entry;
MapError:
  m->segvcode = SEGV_MAPERR_LINUX;
  return (uintptr_t)efault0();
}

u8 *LookupAddress2(struct Machine *m, i64 virt, u64 mask, u64 need) {
  u8 *host;
  u64 entry;
  if (!m->metal || m->mode.omode == XED_MODE_LONG ||
      (m->mode.genmode != XED_GEN_MODE_REAL && (m->system->cr0 & CR0_PG))) {
    if (!(entry = FindPageTableEntry(m, virt & -4096))) {
      return 0;
    }
  } else if (virt >= 0 && virt <= 0xffffffff &&
             (virt & 0xffffffff) + 4095 < kRealSize) {
    unassert(m->system->real);
    return m->system->real + virt;
  } else {
    m->segvcode = SEGV_MAPERR_LINUX;
    return (u8 *)efault0();
  }
  if ((entry & mask) != need) {
    m->segvcode = SEGV_ACCERR_LINUX;
    return (u8 *)efault0();
  }
#ifndef DISABLE_JIT
  if ((need & PAGE_RW) &&
      (entry & (PAGE_U | PAGE_RW | PAGE_XD)) == (PAGE_U | PAGE_RW) &&
      !IsJitDisabled(&m->system->jit) && !IsPageInSmcQueue(m, virt)) {
    AddPageToSmcQueue(m, virt);
  }
#endif
  if ((host = GetPageAddress(m->system, entry, false))) {
    return host + (virt & 4095);
  } else {
    m->segvcode = SEGV_MAPERR_LINUX;
    return (u8 *)efault0();
  }
}

u8 *LookupAddress(struct Machine *m, i64 virt) {
  u64 need = 0;
  if (Cpl(m) == 3) need = PAGE_U;
  return LookupAddress2(m, virt, need, need);
}

flattencalls u8 *GetAddress(struct Machine *m, i64 v) {
  if (HasLinearMapping()) return ToHost(v);
  return LookupAddress(m, v);
}

/**
 * Translates virtual address into pointer.
 *
 * This function bypasses memory protection, since it's used to display
 * memory in the debugger tui. That's useful, for example, if debugging
 * programs that specify an eXecute-only program header.
 *
 * It's recommended that the caller use:
 *
 *     BEGIN_NO_PAGE_FAULTS;
 *     i64 address = ...;
 *     u8 *pointer = SpyAddress(m, address);
 *     END_NO_PAGE_FAULTS;
 *
 * When calling this function.
 */
u8 *SpyAddress(struct Machine *m, i64 virt) {
  return LookupAddress2(m, virt, 0, 0);
}

u8 *ResolveAddress(struct Machine *m, i64 v) {
  u8 *r;
  if ((r = GetAddress(m, v))) return r;
  ThrowSegmentationFault(m, v);
}

bool IsValidMemory(struct Machine *m, i64 virt, i64 size, int prot) {
  i64 p, pe;
  u64 pte, mask, need;
  size += virt & 4095;
  virt &= -4096;
  unassert(m->mode.omode == XED_MODE_LONG);
  unassert(prot && !(prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC)));
  need = mask = 0;
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
  if (ckd_add(&pe, virt, size)) {
    eoverflow();
    return false;
  }
  for (p = virt; p < pe; p += 4096) {
    if (!(pte = FindPageTableEntry(m, p))) {
      return false;
    }
    if ((pte & mask) != need) {
      errno = EFAULT;
      return false;
    }
  }
  return true;
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
    } else if (!IsRomAddress(m, p)) {
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
  long k;
  u64 mask, need;
  u8 *res, *p1, *p2;
  if (writable) {
    SetWriteAddr(m, v, n);
  } else {
    SetReadAddr(m, v, n);
  }
  if (HasLinearMapping()) {
    return ToHost(v);
  }
  m->reserving = true;
  if (Cpl(m) == 3) {
    if (!writable) {
      mask = PAGE_U;
      need = PAGE_U;
    } else {
      mask = PAGE_U | PAGE_RW;
      need = PAGE_U | PAGE_RW;
    }
  } else {
    mask = 0;
    need = 0;
  }
  if ((v & 4095) + n <= 4096) {
    if ((res = LookupAddress2(m, v, mask, need))) {
      if (!IsRomAddress(m, res)) return res;
      p1 = res;
      m->stashaddr = v;
      m->opcache->stashsize = n;
      m->opcache->writable = writable;
      res = m->opcache->stash;
      IGNORE_RACES_START();
      memcpy(res, p1, n);
      IGNORE_RACES_END();
      return res;
    } else {
      ThrowSegmentationFault(m, v);
    }
  }
  STATISTIC(++page_overlaps);
  unassert(n <= 4096);
  m->stashaddr = v;
  m->opcache->stashsize = n;
  m->opcache->writable = writable;
  res = m->opcache->stash;
  k = 4096 - (v & 4095);
  if ((p1 = LookupAddress2(m, v, mask, need))) {
    if ((p2 = LookupAddress2(m, v + k, mask, need))) {
      IGNORE_RACES_START();
      memcpy(res, p1, k);
      memcpy(res + k, p2, n - k);
      IGNORE_RACES_END();
      return res;
    } else {
      ThrowSegmentationFault(m, v + k);
    }
  } else {
    ThrowSegmentationFault(m, v);
  }
}

static u8 *AccessRam2(struct Machine *m, i64 v, size_t n, void *p[2], u8 *tmp,
                      bool copy, bool protect_rom) {
  u8 *a, *b;
  unsigned k;
  unassert(n <= 4096);
  if ((v & 4095) + n <= 4096) {
    a = ResolveAddress(m, v);
    if (!protect_rom || !IsRomAddress(m, a)) return a;
    if (copy) memcpy(tmp, a, n);
    return tmp;
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
  if (protect_rom) {
    if (IsRomAddress(m, a)) a = NULL;
    if (IsRomAddress(m, b)) b = NULL;
  }
  p[0] = a;
  p[1] = b;
  return tmp;
}

u8 *AccessRam(struct Machine *m, i64 v, size_t n, void *p[2], u8 *tmp, bool d) {
  return AccessRam2(m, v, n, p, tmp, d, !d);
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

#if 0
u8 *BeginLoadStore(struct Machine *m, i64 v, size_t n, void *p[2], u8 *b) {
  SetWriteAddr(m, v, n);
  return AccessRam2(m, v, n, p, b, true, true);
}
#endif

void EndStore(struct Machine *m, i64 v, size_t n, void *p[2], u8 *b) {
  unsigned k;
  unassert(n <= 4096);
  if ((v & 4095) + n <= 4096) return;
  k = 4096;
  k -= v & 4095;
  unassert(n > k);
#ifdef DISABLE_ROM
  unassert(p[0]);
  unassert(p[1]);
  memcpy(p[0], b, k);
  memcpy(p[1], b + k, n - k);
#else
  if (p[0]) memcpy(p[0], b, k);
  if (p[1]) memcpy(p[1], b + k, n - k);
#endif
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
    STATISTIC(++freelisted);
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

// Returns pointer to string in guest memory. If the string overlaps a
// page boundary, then it's copied, and the temporary memory is pushed
// to the free list. Returns NULL w/ EFAULT or ENOMEM on error.
char *LoadStr(struct Machine *m, i64 addr) {
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
