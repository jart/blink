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
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "blink/assert.h"
#include "blink/endian.h"
#include "blink/errno.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/memory.h"
#include "blink/pml4t.h"

#define GRANULARITY ((size_t)0x00020000)

#if LONG_BITS == 64
#define MAX_MEMORY ((size_t)0x7ffff000)
#else
#define MAX_MEMORY ((size_t)0x10000000)
#endif

#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif

struct Machine *NewMachine(void) {
  struct Machine *m;
  void *registration;
  pthread_mutexattr_t mattr;
  if ((registration = mmap(0, MAX_MEMORY, PROT_NONE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1,
                           0)) == MAP_FAILED) {
    LOGF("could not register %zu bytes of memory: %s", MAX_MEMORY,
         strerror(errno));
    return 0;
  }
  m = malloc(sizeof(struct Machine));
  memset(m, 0, sizeof(struct Machine));
  m->system = calloc(1, sizeof(struct System));
  m->opcache = calloc(1, sizeof(struct OpCache));
  m->system->real.p = registration;
  unassert(!pthread_mutexattr_init(&mattr));
  unassert(!pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_NORMAL));
  unassert(!pthread_mutex_init(&m->lock, &mattr));
  unassert(!pthread_mutex_init(&m->system->lock, &mattr));
  unassert(!pthread_mutexattr_destroy(&mattr));
  ResetCpu(m);
  ResetMem(m);
  return m;
}

static void FreeMachineRealFree(struct Machine *m) {
  struct MachineRealFree *rf;
  while ((rf = m->system->realfree)) {
    m->system->realfree = rf->next;
    free(rf);
  }
}

void FreeMachine(struct Machine *m) {
  size_t i;
  if (m) {
    for (i = 0; i < m->freelist.n; ++i) {
      free(m->freelist.p[i]);
    }
    if (m->system) {
      FreeMachineRealFree(m);
      unassert(!pthread_mutex_destroy(&m->system->lock));
      munmap(m->system->real.p, MAX_MEMORY);
      free(m->system);
    }
    unassert(!pthread_mutex_destroy(&m->lock));
    free(m->opcache);
    free(m);
  }
}

void ResetMem(struct Machine *m) {
  FreeMachineRealFree(m);
  ResetTlb(m);
  memset(&m->system->memstat, 0, sizeof(m->system->memstat));
  m->system->real.i = 0;
  m->system->cr3 = 0;
}

long AllocateLinearPage(struct Machine *m) {
  long page;
  if ((page = AllocateLinearPageRaw(m)) != -1) {
    memset(m->system->real.p + page, 0, 0x1000);
  }
  return page;
}

int ReserveReal(struct Machine *m, size_t n) {
  unassert(!(n & 0xfff));
  if (m->system->real.n < n) {
    if (n > MAX_MEMORY) {
      return enomem();
    }
    if (mmap(m->system->real.p + m->system->real.n, n - m->system->real.n,
             PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS,
             -1, 0) != MAP_FAILED) {
      m->system->real.n = n;
      ResetTlb(m);
      ++m->system->memstat.resizes;
    } else {
      LOGF("\ncould not grow memory from %zu to %zu bytes: %s",
           m->system->real.n, n, strerror(errno));
      return -1;
    }
  }
  return 0;
}

long AllocateLinearPageRaw(struct Machine *m) {
  size_t i, n;
  struct MachineRealFree *rf;
  pthread_mutex_lock(&m->system->lock);
  if ((rf = m->system->realfree)) {
    unassert(rf->n);
    unassert(!(rf->i & 0xfff));
    unassert(!(rf->n & 0xfff));
    unassert(rf->i + rf->n <= m->system->real.i);
    i = rf->i;
    rf->i += 0x1000;
    if (!(rf->n -= 0x1000)) {
      m->system->realfree = rf->next;
      free(rf);
    }
    --m->system->memstat.freed;
    ++m->system->memstat.reclaimed;
  } else {
    i = m->system->real.i;
    n = m->system->real.n;
    if (i == n) {
      n += GRANULARITY;
      n = ROUNDUP(n, 0x1000);
      if (ReserveReal(m, n) == -1) {
        pthread_mutex_unlock(&m->system->lock);
        return -1;
      }
    }
    unassert(!(i & 0xfff));
    unassert(!(n & 0xfff));
    unassert(i + 0x1000 <= n);
    m->system->real.i += 0x1000;
    ++m->system->memstat.allocated;
  }
  pthread_mutex_unlock(&m->system->lock);
  ++m->system->memstat.committed;
  return i;
}

static uint64_t MachineRead64(struct Machine *m, uint64_t i) {
  unassert(i + 8 <= m->system->real.n);
  return Read64(m->system->real.p + i);
}

static void MachineWrite64(struct Machine *m, uint64_t i, uint64_t x) {
  unassert(i + 8 <= m->system->real.n);
  Write64(m->system->real.p + i, x);
}

int ReserveVirtual(struct Machine *m, int64_t virt, size_t size, uint64_t key) {
  int64_t ti, mi, pt, end, level;
  for (end = virt + size;;) {
    for (pt = m->system->cr3, level = 39; level >= 12; level -= 9) {
      pt = pt & PAGE_TA;
      ti = (virt >> level) & 511;
      mi = (pt & PAGE_TA) + ti * 8;
      pt = MachineRead64(m, mi);
      if (level > 12) {
        if (!(pt & 1)) {
          if ((pt = AllocateLinearPage(m)) == -1) return -1;
          MachineWrite64(m, mi, pt | 7);
          ++m->system->memstat.pagetables;
        }
        continue;
      }
      for (;;) {
        if (!(pt & 1)) {
          MachineWrite64(m, mi, key);
          ++m->system->memstat.reserved;
        }
        if ((virt += 4096) >= end) return 0;
        if (++ti == 512) break;
        pt = MachineRead64(m, (mi += 8));
      }
    }
  }
}

int64_t FindVirtual(struct Machine *m, int64_t virt, size_t size) {
  uint64_t i, pt, got;
  got = 0;
  do {
    if (virt >= 0x800000000000) {
      return enomem();
    }
    for (pt = m->system->cr3, i = 39; i >= 12; i -= 9) {
      pt = MachineRead64(m, (pt & PAGE_TA) + ((virt >> i) & 511) * 8);
      if (!(pt & 1)) break;
    }
    if (i >= 12) {
      got += 1ull << i;
    } else {
      virt += 4096;
      got = 0;
    }
  } while (got < size);
  return virt;
}

static void AppendRealFree(struct Machine *m, uint64_t real) {
  struct MachineRealFree *rf;
  pthread_mutex_lock(&m->system->lock);
  if (m->system->realfree &&
      real == m->system->realfree->i + m->system->realfree->n) {
    m->system->realfree->n += 4096;
  } else if ((rf = malloc(sizeof(struct MachineRealFree)))) {
    rf->i = real;
    rf->n = 4096;
    rf->next = m->system->realfree;
    m->system->realfree = rf;
  }
  pthread_mutex_unlock(&m->system->lock);
}

int FreeVirtual(struct Machine *m, int64_t base, size_t size) {
  uint64_t i, mi, pt, end, virt;
  for (virt = base, end = virt + size; virt < end; virt += 1ull << i) {
    for (pt = m->system->cr3, i = 39;; i -= 9) {
      mi = (pt & PAGE_TA) + ((virt >> i) & 511) * 8;
      pt = MachineRead64(m, mi);
      if (!(pt & 1)) {
        break;
      } else if (i == 12) {
        ++m->system->memstat.freed;
        if (pt & PAGE_RSRV) {
          --m->system->memstat.reserved;
        } else {
          --m->system->memstat.committed;
          AppendRealFree(m, pt & PAGE_TA);
        }
        MachineWrite64(m, mi, 0);
        break;
      }
    }
  }
  ResetTlb(m);
  return 0;
}
