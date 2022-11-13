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
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/builtin.h"
#include "blink/dll.h"
#include "blink/endian.h"
#include "blink/errno.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/memory.h"
#include "blink/pml4t.h"
#include "blink/real.h"

#define MAX_THREAD_IDS    32768
#define MINIMUM_THREAD_ID 262144
#define GRANULARITY       131072
#define MAX_MEMORY        268435456

#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif

static void FillPage(u8 *p, int c) {
  IGNORE_RACES_START();
  memset(p, c, 4096);
  IGNORE_RACES_END();
}

static void ClearPage(u8 *p) {
  FillPage(p, 0);
}

static void PoisonPage(u8 *p) {
#ifndef NDEBUG
  FillPage(p, 0x55);
#endif
}

static bool IsPageStillPoisoned(u8 *p) {
#ifndef NDEBUG
  long i;
  IGNORE_RACES_START();
  for (i = 0; i < 4096; ++i) {
    if (p[i] != 0x55) {
      return false;
    }
  }
  IGNORE_RACES_END();
#endif
  return true;
}

struct System *NewSystem(void) {
  void *p;
  struct System *s;
  if ((p = mmap(0, MAX_MEMORY, PROT_NONE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0)) ==
      MAP_FAILED) {
    LOGF("could not register %zu bytes of memory: %s", MAX_MEMORY,
         strerror(errno));
    return 0;
  }
  if ((s = (struct System *)calloc(1, sizeof(*s)))) {
    s->real.p = (u8 *)p;
    InitFds(&s->fds);
    unassert(!pthread_mutex_init(&s->sig_lock, 0));
    unassert(!pthread_mutex_init(&s->real_lock, 0));
    unassert(!pthread_mutex_init(&s->machines_lock, 0));
    unassert(!pthread_mutex_init(&s->realfree_lock, 0));
  }
  return s;
}

static void FreeSystemRealFree(struct System *s) {
  struct SystemRealFree *rf;
  unassert(!pthread_mutex_lock(&s->realfree_lock));
  while ((rf = s->realfree)) {
    s->realfree = rf->next;
    free(rf);
  }
  unassert(!pthread_mutex_unlock(&s->realfree_lock));
}

static void AssignTid(struct Machine *m) {
  _Static_assert(IS2POW(MAX_THREAD_IDS), "");
  unassert(!pthread_mutex_lock(&m->system->machines_lock));
  if (dll_is_empty(m->system->machines)) {
    m->tid = getpid();
  } else {
    m->isthread = true;
    m->tid = (m->system->next_tid++ & (MAX_THREAD_IDS - 1)) + MINIMUM_THREAD_ID;
  }
  m->system->machines = dll_make_first(m->system->machines, &m->list);
  unassert(!pthread_mutex_unlock(&m->system->machines_lock));
}

struct Machine *NewMachine(struct System *s, struct Machine *p) {
  struct Machine *m;
  unassert(s);
  unassert(!p || s == p->system);
  if (!(m = (struct Machine *)malloc(sizeof(*m)))) {
    free(m);
    return 0;
  }
  if (p) {
    // TODO(jart): Why does TSAN whine here?
    IGNORE_RACES_START();
    memcpy(m, p, sizeof(*m));
    IGNORE_RACES_END();
    memset(&m->freelist, 0, sizeof(m->freelist));
  } else {
    memset(m, 0, sizeof(*m));
    ResetCpu(m);
  }
  dll_init(&m->list);
  m->system = s;
  AssignTid(m);
  return m;
}

void FreeSystem(struct System *s) {
  FreeSystemRealFree(s);
  unassert(!pthread_mutex_lock(&s->real_lock));
  unassert(!munmap(s->real.p, MAX_MEMORY));
  unassert(!pthread_mutex_unlock(&s->real_lock));
  unassert(!pthread_mutex_destroy(&s->realfree_lock));
  unassert(!pthread_mutex_destroy(&s->machines_lock));
  unassert(!pthread_mutex_destroy(&s->real_lock));
  unassert(!pthread_mutex_destroy(&s->sig_lock));
  DestroyFds(&s->fds);
  free(s);
}

void CollectGarbage(struct Machine *m) {
  long i;
  for (i = 0; i < m->freelist.n; ++i) {
    free(m->freelist.p[i]);
  }
  m->freelist.n = 0;
}

void FreeMachine(struct Machine *m) {
  if (m) {
    pthread_mutex_lock(&m->system->machines_lock);
    m->system->machines = dll_remove(m->system->machines, &m->list);
    pthread_mutex_unlock(&m->system->machines_lock);
    CollectGarbage(m);
    free(m->freelist.p);
    free(m);
  }
}

void ResetMem(struct Machine *m) {
  FreeSystemRealFree(m->system);
  ResetTlb(m);
  memset(&m->system->memstat, 0, sizeof(m->system->memstat));
  m->system->real.i = 0;
  m->system->cr3 = 0;
}

long AllocateLinearPage(struct System *s) {
  long page;
  if ((page = AllocateLinearPageRaw(s)) != -1) {
    unassert(IsPageStillPoisoned(s->real.p + page));
    ClearPage(s->real.p + page);
  }
  return page;
}

int ReserveReal(struct System *s, long n)
    EXCLUSIVE_LOCKS_REQUIRED(s->real_lock) {
  long i;
  unassert(!(n & 4095));
  if (s->real.n < n) {
    if (n > MAX_MEMORY) {
      return enomem();
    }
    if (mmap(s->real.p + s->real.n, n - s->real.n, PROT_READ | PROT_WRITE,
             MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) != MAP_FAILED) {
      for (i = 0; i < n - s->real.n; i += 4096) {
        PoisonPage(s->real.p + s->real.n + i);
      }
      s->real.n = n;
      ++s->memstat.resizes;
    } else {
      LOGF("could not grow memory from %zu to %zu bytes: %s", s->real.n, n,
           strerror(errno));
      return -1;
    }
  }
  return 0;
}

long AllocateLinearPageRaw(struct System *s) {
  size_t i, n;
  struct SystemRealFree *rf;
  pthread_mutex_lock(&s->realfree_lock);
  if ((rf = s->realfree)) {
    unassert(rf->n);
    unassert(!(rf->i & 4095));
    unassert(!(rf->n & 4095));
    unassert(rf->i + rf->n <= s->real.i);
    i = rf->i;
    rf->i += 4096;
    if (!(rf->n -= 4096)) {
      s->realfree = rf->next;
      pthread_mutex_unlock(&s->realfree_lock);
      free(rf);
    } else {
      pthread_mutex_unlock(&s->realfree_lock);
    }
    --s->memstat.freed;
    ++s->memstat.reclaimed;
  } else {
    pthread_mutex_unlock(&s->realfree_lock);
    pthread_mutex_lock(&s->real_lock);
    i = s->real.i;
    n = s->real.n;
    if (i == n) {
      n += GRANULARITY;
      n = ROUNDUP(n, 4096);
      if (ReserveReal(s, n) == -1) {
        pthread_mutex_unlock(&s->real_lock);
        return -1;
      }
    }
    unassert(!(i & 4095));
    unassert(!(n & 4095));
    unassert(i + 4096 <= n);
    s->real.i += 4096;
    pthread_mutex_unlock(&s->real_lock);
    ++s->memstat.allocated;
  }
  ++s->memstat.committed;
  return i;
}

static u64 SystemRead64(struct System *s, u64 i) {
  unassert(i + 8 <= GetRealMemorySize(s));
  return Read64(s->real.p + i);
}

static void SystemWrite64(struct System *s, u64 i, u64 x) {
  unassert(i + 8 <= GetRealMemorySize(s));
  Write64(s->real.p + i, x);
}

int ReserveVirtual(struct System *s, i64 virt, size_t size, u64 key) {
  i64 ti, mi, pt, end, level;
  for (end = virt + size;;) {
    for (pt = s->cr3, level = 39; level >= 12; level -= 9) {
      pt = pt & PAGE_TA;
      ti = (virt >> level) & 511;
      mi = (pt & PAGE_TA) + ti * 8;
      pt = SystemRead64(s, mi);
      if (level > 12) {
        if (!(pt & 1)) {
          if ((pt = AllocateLinearPage(s)) == -1) return -1;
          SystemWrite64(s, mi, pt | 7);
          ++s->memstat.pagetables;
        }
        continue;
      }
      for (;;) {
        if (!(pt & 1)) {
          SystemWrite64(s, mi, key);
          ++s->memstat.reserved;
        }
        if ((virt += 4096) >= end) return 0;
        if (++ti == 512) break;
        pt = SystemRead64(s, (mi += 8));
      }
    }
  }
}

i64 FindVirtual(struct System *s, i64 virt, size_t size) {
  u64 i, pt, got;
  got = 0;
  do {
    if (virt >= 0x800000000000) {
      return enomem();
    }
    for (pt = s->cr3, i = 39; i >= 12; i -= 9) {
      pt = SystemRead64(s, (pt & PAGE_TA) + ((virt >> i) & 511) * 8);
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

static void AppendRealFree(struct System *s, u64 real) {
  struct SystemRealFree *rf;
  pthread_mutex_lock(&s->realfree_lock);
  PoisonPage(s->real.p + real);
  if (s->realfree && real == s->realfree->i + s->realfree->n) {
    s->realfree->n += 4096;
  } else if ((rf = (struct SystemRealFree *)malloc(sizeof(*rf)))) {
    rf->i = real;
    rf->n = 4096;
    rf->next = s->realfree;
    s->realfree = rf;
  }
  pthread_mutex_unlock(&s->realfree_lock);
}

int FreeVirtual(struct System *s, i64 base, size_t size) {
  u64 i, mi, pt, end, virt;
  for (virt = base, end = virt + size; virt < end; virt += 1ull << i) {
    for (pt = s->cr3, i = 39;; i -= 9) {
      mi = (pt & PAGE_TA) + ((virt >> i) & 511) * 8;
      pt = SystemRead64(s, mi);
      if (!(pt & 1)) {
        break;
      } else if (i == 12) {
        ++s->memstat.freed;
        if (pt & PAGE_RSRV) {
          --s->memstat.reserved;
        } else {
          --s->memstat.committed;
          AppendRealFree(s, pt & PAGE_TA);
        }
        SystemWrite64(s, mi, 0);
        break;
      }
    }
  }
  return 0;
}
