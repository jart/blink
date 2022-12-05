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
#include <signal.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/builtin.h"
#include "blink/debug.h"
#include "blink/dll.h"
#include "blink/end.h"
#include "blink/endian.h"
#include "blink/errno.h"
#include "blink/jit.h"
#include "blink/lock.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/map.h"
#include "blink/memory.h"
#include "blink/pml4t.h"
#include "blink/real.h"
#include "blink/util.h"

#define MAX_THREAD_IDS    32768
#define MINIMUM_THREAD_ID 262144     // our fake tids start here
#define GRANULARITY       131072     // how often we check in with the os
#define MAX_MEMORY        268435456  // 256mb ought to be enough for anyone
#define JIT_RESERVE       134217728  // 128mb is max branch displacement on arm

static struct RealGlobals {
  _Atomic(u8 *) brk;  //
} g_real;

static void FillPage(u8 *p, int c) {
  memset(p, c, 4096);
  atomic_thread_fence(memory_order_release);
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

static void FreeSystemRealFree(struct System *s) {
  struct SystemRealFree *rf;
  LOCK(&s->realfree_lock);
  while ((rf = s->realfree)) {
    s->realfree = rf->next;
    free(rf);
  }
  UNLOCK(&s->realfree_lock);
}

struct System *NewSystem(void) {
  u8 *brk;
  void *p;
  struct System *s;
  // TODO: system->real.p needn't be contiguous past 1mb
  //       so we don't need this big limited reservation
  //       all we need to do is ensure it's not >47 bits
  if (!(brk = atomic_load_explicit(&g_real.brk, memory_order_relaxed))) {
    atomic_compare_exchange_strong_explicit(&g_real.brk, &brk, (u8 *)kRealStart,
                                            memory_order_relaxed,
                                            memory_order_relaxed);
  }
  for (;;) {
    brk = atomic_fetch_add_explicit(&g_real.brk, MAX_MEMORY,
                                    memory_order_relaxed);
    if ((p = Mmap(brk, MAX_MEMORY, PROT_NONE,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_DEMAND, -1,
                  0, "system")) != MAP_FAILED) {
      break;
    } else if (errno != MAP_DENIED) {
      LOGF("could not register %zu bytes of memory at %p: %s", MAX_MEMORY, brk,
           strerror(errno));
      return 0;
    }
  }
  if ((s = (struct System *)calloc(1, sizeof(*s)))) {
    s->real.p = (u8 *)p;
    InitJit(&s->jit);
    InitFds(&s->fds);
    pthread_mutex_init(&s->sig_lock, 0);
    pthread_mutex_init(&s->real_lock, 0);
    pthread_mutex_init(&s->mmap_lock, 0);
    pthread_mutex_init(&s->machines_lock, 0);
    pthread_mutex_init(&s->realfree_lock, 0);
  }
  return s;
}

void FreeMachineUnlocked(struct Machine *m) {
  if (g_machine == m) {
    g_machine = 0;
  }
  if (m->path.jp) {
    AbandonJit(&m->system->jit, m->path.jp);
  }
  CollectGarbage(m);
  free(m->freelist.p);
  m->cookie = 0;
  free(m);
}

void KillOtherThreads(struct System *s) {
  struct Machine *m;
  struct Dll *e, *g;
StartOver:
  LOCK(&s->machines_lock);
  for (e = dll_first(s->machines); e; e = g) {
    g = dll_next(s->machines, e);
    m = MACHINE_CONTAINER(e);
    if (m != g_machine) {
      s->machines = dll_remove(s->machines, &m->elem);
      unassert(!pthread_kill(m->thread, SIGKILL));
      UNLOCK(&s->machines_lock);
      FreeMachineUnlocked(m);
      goto StartOver;
    }
  }
  UNLOCK(&s->machines_lock);
}

void FreeSystem(struct System *s) {
  unassert(dll_is_empty(s->machines));  // Use KillOtherThreads & FreeMachine
  FreeSystemRealFree(s);
  LOCK(&s->real_lock);
  unassert(!munmap(s->real.p, MAX_MEMORY));
  UNLOCK(&s->real_lock);
  unassert(!pthread_mutex_destroy(&s->realfree_lock));
  unassert(!pthread_mutex_destroy(&s->machines_lock));
  unassert(!pthread_mutex_destroy(&s->mmap_lock));
  unassert(!pthread_mutex_destroy(&s->real_lock));
  unassert(!pthread_mutex_destroy(&s->sig_lock));
  DestroyFds(&s->fds);
  DestroyJit(&s->jit);
  free(s->fun);
  free(s);
}

struct Machine *NewMachine(struct System *system, struct Machine *parent) {
  _Static_assert(IS2POW(MAX_THREAD_IDS), "");
  struct Machine *m;
  unassert(system);
  unassert(!parent || system == parent->system);
  if (posix_memalign((void **)&m, _Alignof(struct Machine), sizeof(*m))) {
    enomem();
    return 0;
  }
  // TODO(jart): We shouldn't be doing expensive ops in an allocator.
  LOCK(&system->machines_lock);
  if (parent) {
    memcpy(m, parent, sizeof(*m));
    memset(&m->path, 0, sizeof(m->path));
    memset(&m->freelist, 0, sizeof(m->freelist));
  } else {
    memset(m, 0, sizeof(*m));
    ResetCpu(m);
  }
  m->cookie = kCookie;
  m->system = system;
  m->oldip = -1;
  if (parent) {
    m->tid = (system->next_tid++ & (MAX_THREAD_IDS - 1)) + MINIMUM_THREAD_ID;
  } else {
    // TODO(jart): We shouldn't be doing system calls in an allocator.
    m->tid = getpid();
  }
  dll_init(&m->elem);
  // TODO(jart): Child thread should add itself to system.
  system->machines = dll_make_first(system->machines, &m->elem);
  UNLOCK(&system->machines_lock);
  return m;
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
    unassert(m->system);
    LOCK(&m->system->machines_lock);
    m->system->machines = dll_remove(m->system->machines, &m->elem);
    UNLOCK(&m->system->machines_lock);
    FreeMachineUnlocked(m);
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
#ifndef NDEBUG
    if (!IsPageStillPoisoned(s->real.p + page)) {
      DumpHex(s->real.p + page, 4096);
      unassert(!"page should still be poisoned");
    }
#endif
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
    if (Mmap(s->real.p + s->real.n, n - s->real.n, PROT_READ | PROT_WRITE,
             MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0,
             "real") != MAP_FAILED) {
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
  LOCK(&s->realfree_lock);
  if ((rf = s->realfree)) {
    unassert(rf->n);
    unassert(!(rf->i & 4095));
    unassert(!(rf->n & 4095));
    unassert(rf->i + rf->n <= s->real.i);
    i = rf->i;
    rf->i += 4096;
    if (!(rf->n -= 4096)) {
      s->realfree = rf->next;
      UNLOCK(&s->realfree_lock);
      free(rf);
    } else {
      UNLOCK(&s->realfree_lock);
    }
    --s->memstat.freed;
    ++s->memstat.reclaimed;
  } else {
    UNLOCK(&s->realfree_lock);
    LOCK(&s->real_lock);
    i = s->real.i;
    n = s->real.n;
    if (i == n) {
      n += GRANULARITY;
      n = ROUNDUP(n, 4096);
      if (ReserveReal(s, n) == -1) {
        UNLOCK(&s->real_lock);
        return -1;
      }
    }
    unassert(!(i & 4095));
    unassert(!(n & 4095));
    unassert(i + 4096 <= n);
    s->real.i += 4096;
    UNLOCK(&s->real_lock);
    ++s->memstat.allocated;
  }
  ++s->memstat.committed;
  return i;
}

static u64 RealRead64(struct System *s, size_t i) {
  unassert(i + 8 <= GetRealMemorySize(s));
  return Get64(s->real.p + i);
}

static void RealWrite64(struct System *s, size_t i, u64 x) {
  unassert(i + 8 <= GetRealMemorySize(s));
  Put64(s->real.p + i, x);
}

bool IsValidAddrSize(i64 virt, i64 size) {
  return size &&                        //
         !(virt & 4095) &&              //
         virt >= -0x800000000000 &&     //
         virt < 0x800000000000 &&       //
         size <= 0x1000000000000ull &&  //
         virt + size <= 0x800000000000;
}

static void InvalidateSystemTlbs(struct System *s) {
  struct Dll *e;
  LOCK(&s->machines_lock);
  for (e = dll_first(s->machines); e; e = dll_next(s->machines, e)) {
    atomic_store_explicit(&MACHINE_CONTAINER(e)->tlb_invalidated, true,
                          memory_order_relaxed);
  }
  UNLOCK(&s->machines_lock);
}

// so far tsan is the only thing we've seen that dominates the virtual
// memory space heavily enough that cosmo can't have identity mappings
// TODO: We'd likely need some kind of GIL to make this actually work!
static void VirtualizeSystem(struct System *s) {
  struct Dll *e;
  for (long i = 0; i < s->codesize; ++i) {
    atomic_store_explicit(s->fun + i, JitlessDispatch, memory_order_relaxed);
  }
  LOCK(&s->real_lock);
  LOCK(&s->machines_lock);
  s->virtualized = true;
  for (e = dll_first(s->machines); e; e = dll_next(s->machines, e)) {
    atomic_store_explicit(&MACHINE_CONTAINER(e)->virtualized, true,
                          memory_order_relaxed);
  }
  UNLOCK(&s->machines_lock);
  UNLOCK(&s->real_lock);
  for (long i = 0; i < s->codesize; ++i) {
    atomic_store_explicit(s->fun + i, GeneralDispatch, memory_order_relaxed);
  }
}

static void AppendRealFree(struct System *s, u64 real) {
  struct SystemRealFree *rf;
  LOCK(&s->realfree_lock);
  PoisonPage(s->real.p + real);
  ++s->memstat.freed;
  if (s->realfree && real == s->realfree->i + s->realfree->n) {
    s->realfree->n += 4096;
  } else if ((rf = (struct SystemRealFree *)malloc(sizeof(*rf)))) {
    rf->i = real;
    rf->n = 4096;
    rf->next = s->realfree;
    s->realfree = rf;
  }
  UNLOCK(&s->realfree_lock);
}

static void FreePageTableEntry(struct System *s, u64 entry) {
  unassert(entry & PAGE_V);
  if (entry & PAGE_RSRV) {
    --s->memstat.reserved;
  } else {
    --s->memstat.committed;
    if (entry & PAGE_ID) {
      // unassert(!munmap(ToHost(entry & PAGE_TA), 4096));
    } else {
      AppendRealFree(s, entry & PAGE_TA);
    }
  }
}

int ReserveVirtual(struct System *s, i64 virt, i64 size, u64 flags) {
  i64 ti, mi, pt, key, end, level;

  // we determine these
  unassert(!(flags & PAGE_TA));
  unassert(!(flags & PAGE_ID));
  unassert(!(flags & PAGE_RSRV));

  // ensure requested interval is in 48-bit space
  if (!IsValidAddrSize(virt, size)) return einval();

  MEM_LOGF("reserving virtual [%#" PRIx64 ",%#" PRIx64 ") w/ %" PRId64 " kb",
           virt, virt + size, size / 1024);

  // if all guest memory is identity-mapped to host memory,
  // then we'll try to maintain the perfect correspondence,
  // with care taken to not clobber any unrelated mappings.
  if (!s->virtualized) {
    if (virt > 0 && s->mode == XED_MODE_LONG) {
      void *idmap =
          Mmap(ToHost(virt), size, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0, "identity");
      if (idmap != MAP_FAILED) {
        if (idmap == ToHost(virt)) {
          flags |= PAGE_ID;
        } else {
          unassert(!munmap(idmap, size));
        }
      }
      if (!(flags & PAGE_ID)) {
        LOGF("mapping crisis: wanted %p but got %p (%s) %s", ToHost(virt),
             idmap, strerror(errno),
             "will attempt to fall back to virtual memory model");
      }
    } else {
      LOGF("negative or null mapping requested at %#" PRIx64
           " with size %#" PRIx64 " %s",
           virt, size, "will attempt to fall back to virtual memory model");
    }
    if (!(flags & PAGE_ID)) {
      VirtualizeSystem(s);
    }
  }

  // update four-level page table ensuring all entries exist
  for (end = virt + size;;) {
    for (pt = s->cr3, level = 39; level >= 12; level -= 9) {
      pt = pt & PAGE_TA;
      ti = (virt >> level) & 511;
      mi = (pt & PAGE_TA) + ti * 8;
      pt = RealRead64(s, mi);
      if (level > 12) {
        // we're traversing an intermediary page table
        if (!(pt & PAGE_V)) {
          // if a branch doesn't exist we must create it
          if ((pt = AllocateLinearPage(s)) == -1) return -1;
          RealWrite64(s, mi, pt | PAGE_U | PAGE_RW | PAGE_V);
          ++s->memstat.pagetables;
        }
        continue;
      }
      // iterate innermost table overwriting page entries
      for (;;) {
        if (pt & PAGE_V) {
          FreePageTableEntry(s, pt);
        }
        if (flags & PAGE_ID) {
          ++s->memstat.committed;
          key = virt | flags | PAGE_V;
        } else {
          ++s->memstat.reserved;
          key = flags | PAGE_RSRV | PAGE_V;
        }
        RealWrite64(s, mi, key);
        if ((virt += 4096) >= end) {
          return 0;
        }
        if (++ti == 512) {
          break;
        }
        pt = RealRead64(s, (mi += 8));
      }
    }
  }
}

i64 FindVirtual(struct System *s, i64 virt, i64 size) {
  u64 i, pt, got = 0;
  if (!IsValidAddrSize(virt, size)) return einval();
  do {
    if (virt >= 0x800000000000) return enomem();
    for (pt = s->cr3, i = 39; i >= 12; i -= 9) {
      pt = RealRead64(s, (pt & PAGE_TA) + ((virt >> i) & 511) * 8);
      if (!(pt & PAGE_V)) break;
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

int FreeVirtual(struct System *s, i64 virt, i64 size) {
  u64 i, mi, pt, end;
  MEM_LOGF("freeing virtual [%#" PRIx64 ",%#" PRIx64 ") w/ %" PRId64 " kb",
           virt, virt + size, size / 1024);
  if (!IsValidAddrSize(virt, size)) return einval();
  for (end = virt + size; virt < end; virt += 1ull << i) {
    for (pt = s->cr3, i = 39;; i -= 9) {
      mi = (pt & PAGE_TA) + ((virt >> i) & 511) * 8;
      pt = RealRead64(s, mi);
      if (!(pt & PAGE_V)) {
        break;
      } else if (i == 12) {
        FreePageTableEntry(s, pt);
        RealWrite64(s, mi, 0);
        break;
      }
    }
  }
  InvalidateSystemTlbs(s);
  return 0;
}
