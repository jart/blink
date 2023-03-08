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
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/atomic.h"
#include "blink/bitscan.h"
#include "blink/builtin.h"
#include "blink/bus.h"
#include "blink/debug.h"
#include "blink/errno.h"
#include "blink/fds.h"
#include "blink/jit.h"
#include "blink/linux.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/map.h"
#include "blink/pml4t.h"
#include "blink/random.h"
#include "blink/thread.h"
#include "blink/timespec.h"
#include "blink/types.h"
#include "blink/util.h"
#include "blink/x86.h"

struct Allocator {
  pthread_mutex_t_ lock;
  struct HostPage *pages GUARDED_BY(lock);
} g_allocator = {
    PTHREAD_MUTEX_INITIALIZER_,
};

struct Machine g_bssmachine;

static void FillPage(void *p, int c) {
  memset(p, c, 4096);
}

static void ClearPage(void *p) {
  FillPage(p, 0);
}

static struct HostPage *NewHostPage(void) {
  return (struct HostPage *)malloc(sizeof(struct HostPage));
}

static void FreeHostPage(struct HostPage *hp) {
  free(hp);
}

void FreeAnonymousPage(struct System *s, u8 *page) {
  struct HostPage *h;
  unassert((h = NewHostPage()));
  LOCK(&g_allocator.lock);
  h->page = page;
  h->next = g_allocator.pages;
  g_allocator.pages = h;
  UNLOCK(&g_allocator.lock);
}

static size_t GetBigSize(size_t n) {
  unassert(n);
  long z = FLAG_pagesize;
  return ROUNDUP(n, z);
}

static void FreeFileMap(struct FileMap *fm) {
  if (fm) {
    free(fm->present);
    free(fm->path);
    free(fm);
  }
}

void FreeBig(void *p, size_t n) {
  if (!p) return;
  unassert(!Munmap(p, n));
}

void *AllocateBig(size_t n, int prot, int flags, int fd, off_t off) {
  void *p = Mmap(0, n, prot, flags, fd, off, "big");
  return p != MAP_FAILED ? p : 0;
}

static void FreePageTable(struct System *s, u8 *page) {
  FreeAnonymousPage(s, page);
  s->memstat.tables -= 1;
  s->rss -= 1;
}

static bool FreeEmptyPageTables(struct System *s, u64 pt, long level) {
  u8 *mi;
  long i;
  bool isempty = true;
  mi = GetPageAddress(s, pt, level == 1);
  for (i = 0; i < 512; ++i) {
    if (level == 4) {
      if (LoadPte(mi + i * 8)) {
        isempty = false;
      }
    } else {
      pt = LoadPte(mi + i * 8);
      if (pt & PAGE_V) {
        if (FreeEmptyPageTables(s, pt, level + 1)) {
          StorePte(mi + i * 8, 0);
        } else {
          isempty = false;
        }
      } else {
        unassert(!pt);
      }
    }
  }
  if (isempty) {
    FreePageTable(s, mi);
  }
  return isempty;
}

static void FreeHostPages(struct System *s) {
  if (!s->real && s->cr3) {
    unassert(!FreeVirtual(s, -0x800000000000, 0x1000000000000));
    unassert(FreeEmptyPageTables(s, s->cr3, 1));
#if 0
    unassert(!s->memstat.committed);
    unassert(!s->memstat.reserved);
    unassert(!s->memstat.tables);
    unassert(!s->vss);
    unassert(!s->rss);
#endif
    s->cr3 = 0;
  }
  free(s->real);
  s->real = 0;
}

static void FreeFileMaps(struct System *s) {
  struct Dll *e;
  while ((e = dll_first(s->filemaps))) {
    dll_remove(&s->filemaps, e);
    FreeFileMap(FILEMAP_CONTAINER(e));
  }
}

void CleanseMemory(struct System *s, size_t size) {
  i64 oldrss;
  if (s->memchurn >= (s->rss >> 1)) {
    (void)(oldrss = s->rss);
    FreeEmptyPageTables(s, s->cr3, 1);
    MEM_LOGF("freed %" PRId64 " page tables", oldrss - s->rss);
    s->memchurn = 0;
  }
}

u64 GetFileDescriptorLimit(struct System *s) {
  u64 lim;
  LOCK(&s->mmap_lock);
  lim = Read64(s->rlim[RLIMIT_NOFILE_LINUX].cur);
  UNLOCK(&s->mmap_lock);
  return lim;
}

long GetMaxVss(struct System *s) {
  return MIN(kMaxVirtual, Read64(s->rlim[RLIMIT_AS_LINUX].cur)) / 4096;
}

long GetMaxRss(struct System *s) {
  return MIN(kMaxResident, Read64(s->rlim[RLIMIT_AS_LINUX].cur)) / 4096;
}

struct System *NewSystem(int mode) {
  long i;
  struct System *s;
  unassert(mode == XED_MODE_REAL ||    //
           mode == XED_MODE_LEGACY ||  //
           mode == XED_MODE_LONG);
  if (posix_memalign((void **)&s, _Alignof(struct System), sizeof(*s))) {
    enomem();
    return 0;
  }
  memset(s, 0, sizeof(*s));
  s->mode = mode;
  if (s->mode == XED_MODE_REAL) {
    if (posix_memalign((void **)&s->real, 4096, kRealSize)) {
      free(s);
      enomem();
      return 0;
    }
  }
#ifdef HAVE_JIT
  InitJit(&s->jit, (uintptr_t)JitlessDispatch);
#endif
  InitFds(&s->fds);
  unassert(!pthread_mutex_init(&s->sig_lock, 0));
  unassert(!pthread_mutex_init(&s->mmap_lock, 0));
  unassert(!pthread_mutex_init(&s->exec_lock, 0));
  unassert(!pthread_cond_init(&s->machines_cond, 0));
  unassert(!pthread_mutex_init(&s->machines_lock, 0));
  unassert(!pthread_cond_init(&s->pagelocks_cond, 0));
  unassert(!pthread_mutex_init(&s->pagelocks_lock, 0));
  s->blinksigs = 1ull << (SIGSYS_LINUX - 1) |   //
                 1ull << (SIGILL_LINUX - 1) |   //
                 1ull << (SIGFPE_LINUX - 1) |   //
                 1ull << (SIGSEGV_LINUX - 1) |  //
                 1ull << (SIGBUS_LINUX - 1) |   //
                 1ull << (SIGPIPE_LINUX - 1) |  //
                 1ull << (SIGTRAP_LINUX - 1);
  for (i = 0; i < RLIM_NLIMITS_LINUX; ++i) {
    Write64(s->rlim[i].cur, RLIM_INFINITY_LINUX);
    Write64(s->rlim[i].max, RLIM_INFINITY_LINUX);
  }
  s->pid = getpid();
  return s;
}

static void FreeMachineUnlocked(struct Machine *m) {
  THR_LOGF("pid=%d tid=%d FreeMachine", m->system->pid, m->tid);
  UnlockRobustFutexes(m);
  if (IsMakingPath(m)) {
    AbandonJit(&m->system->jit, m->path.jb);
  }
  m->sysdepth = 0;
  CollectPageLocks(m);
  CollectGarbage(m, 0);
  free(m->pagelocks.p);
  free(m->freelist.p);
  free(m);
  if (g_machine == m) {
    g_machine = 0;
  }
}

bool IsOrphan(struct Machine *m) {
  bool res;
  LOCK(&m->system->machines_lock);
  if (m->system->machines == m->system->machines->next &&
      m->system->machines == m->system->machines->prev) {
    unassert(m == MACHINE_CONTAINER(m->system->machines));
    res = true;
  } else {
    res = false;
  }
  UNLOCK(&m->system->machines_lock);
  return res;
}

void KillOtherThreads(struct System *s) {
#ifdef HAVE_THREADS
  int r, t;
  struct Dll *e;
  struct Machine *m;
  struct timespec deadline;
  if (atomic_exchange(&s->killer, true)) {
    FreeMachine(g_machine);
    pthread_exit(0);
  }
StartOver:
  unassert(s == g_machine->system);
  unassert(!dll_is_empty(s->machines));
  for (t = 0; !IsOrphan(g_machine); ++t) {
    LOCK(&s->machines_lock);
    for (e = dll_first(s->machines); e; e = dll_next(s->machines, e)) {
      if ((m = MACHINE_CONTAINER(e)) != g_machine) {
        THR_LOGF("pid=%d tid=%d is killing tid %d", s->pid, g_machine->tid,
                 m->tid);
        atomic_store_explicit(&m->killed, true, memory_order_release);
        atomic_store_explicit(&m->attention, true, memory_order_release);
        if (t < 10) {
          pthread_kill(m->thread, SIGSYS);
        } else {
          LOGF("kill9'd thread after 10 tries");
          pthread_kill(m->thread, SIGKILL);
          dll_remove(&s->machines, e);
          UNLOCK(&s->machines_lock);
          goto StartOver;
        }
      }
    }
    deadline = AddTime(GetTime(), FromMilliseconds(kPollingMs));
    r = pthread_cond_timedwait(&s->machines_cond, &s->machines_lock, &deadline);
    unassert(r == 0 || r == ETIMEDOUT);
    UNLOCK(&s->machines_lock);
  }
#endif
}

void RemoveOtherThreads(struct System *s) {
#ifdef HAVE_THREADS
  struct Dll *e, *g;
  struct Machine *m;
  LOCK(&s->machines_lock);
  for (e = dll_first(s->machines); e; e = g) {
    g = dll_next(s->machines, e);
    m = MACHINE_CONTAINER(e);
    if (m != g_machine) {
      dll_remove(&s->machines, e);
      FreeMachineUnlocked(m);
    }
  }
  UNLOCK(&s->machines_lock);
#endif
}

void FreeSystem(struct System *s) {
  THR_LOGF("pid=%d FreeSystem", s->pid);
  unassert(dll_is_empty(s->machines));  // Use KillOtherThreads & FreeMachine
  FreeHostPages(s);
  unassert(!pthread_mutex_destroy(&s->machines_lock));
  unassert(!pthread_cond_destroy(&s->machines_cond));
  unassert(!pthread_mutex_destroy(&s->pagelocks_lock));
  unassert(!pthread_cond_destroy(&s->pagelocks_cond));
  unassert(!pthread_mutex_destroy(&s->exec_lock));
  unassert(!pthread_mutex_destroy(&s->mmap_lock));
  unassert(!pthread_mutex_destroy(&s->sig_lock));
  free(s->elf.interpreter);
  DestroyFds(&s->fds);
  free(s->elf.execfn);
  free(s->elf.prog);
  FreeFileMaps(s);
#ifdef HAVE_JIT
  DestroyJit(&s->jit);
#endif
  free(s);
}

struct Machine *NewMachine(struct System *system, struct Machine *parent) {
  _Static_assert(IS2POW(kMaxThreadIds), "");
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
    memset(&m->pagelocks, 0, sizeof(m->pagelocks));
    ResetInstructionCache(m);
    m->insyscall = false;
    m->nofault = false;
    m->sysdepth = 0;
    m->sigdepth = 0;
    m->signals = 0;
  } else {
    memset(m, 0, sizeof(*m));
    ResetCpu(m);
  }
  m->ctid = 0;
  m->oplen = 0;
  m->system = system;
  m->mode = system->mode;
  m->thread = pthread_self();
  Write32(m->sigaltstack.flags, SS_DISABLE_LINUX);
  if (parent) {
    m->tid = (system->next_tid++ & (kMaxThreadIds - 1)) + kMinThreadId;
  } else {
    // TODO(jart): We shouldn't be doing system calls in an allocator.
    m->tid = m->system->pid;
  }
  dll_init(&m->elem);
  // TODO(jart): Child thread should add itself to system.
  dll_make_first(&system->machines, &m->elem);
  UNLOCK(&system->machines_lock);
  THR_LOGF("new machine thread pid=%d tid=%d", m->system->pid, m->tid);
  return m;
}

void CollectGarbage(struct Machine *m, size_t mark) {
  long i;
  for (i = mark; i < m->freelist.n; ++i) {
    free(m->freelist.p[i]);
  }
  m->freelist.n = mark;
}

void FreeMachine(struct Machine *m) {
  bool orphan;
  struct System *s;
  if (m) {
    unassert((s = m->system));
    m->sysdepth = 0;
    CollectPageLocks(m);
    LOCK(&s->machines_lock);
    dll_remove(&s->machines, &m->elem);
    if (!(orphan = dll_is_empty(s->machines))) {
      unassert(!pthread_cond_signal(&s->machines_cond));
    }
    UNLOCK(&s->machines_lock);
    FreeMachineUnlocked(m);
    if (orphan) {
      FreeSystem(s);
    } else {
      THR_LOGF("more threads remain in operation");
    }
  }
}

u64 AllocateAnonymousPage(struct System *s) {
  u8 *page;
  size_t i, n;
  uintptr_t real;
  struct HostPage *h;
  LOCK(&g_allocator.lock);
  if ((h = g_allocator.pages)) {
    g_allocator.pages = h->next;
    UNLOCK(&g_allocator.lock);
    page = h->page;
    FreeHostPage(h);
    goto Finished;
  } else {
    UNLOCK(&g_allocator.lock);
  }
  n = 64;
  page = (u8 *)AllocateBig(n * 4096, PROT_READ | PROT_WRITE,
                           MAP_ANONYMOUS_ | MAP_PRIVATE, -1, 0);
  if (!page) return -1;
  LOCK(&g_allocator.lock);
  for (i = n; i-- > 1;) {
    unassert((h = NewHostPage()));
    h->page = page + i * 4096;
    h->next = g_allocator.pages;
    g_allocator.pages = h;
  }
  UNLOCK(&g_allocator.lock);
Finished:
  s->rss += 1;
  real = (uintptr_t)page;
  unassert(!(real & ~PAGE_TA));
  return real | PAGE_HOST | PAGE_U | PAGE_RW | PAGE_V;
}

u64 AllocatePageTable(struct System *s) {
  u64 res;
  if ((res = AllocateAnonymousPage(s)) != -1) {
    s->memstat.tables += 1;
    res &= ~PAGE_U;
  }
  return res;
}

bool IsValidAddrSize(i64 virt, i64 size) {
  virt = (i64)((u64)virt << 16) >> 16;
  return size > 0 &&                 //
         !(virt & 4095) &&           //
         virt >= -0x800000000000 &&  //
         virt < 0x800000000000 &&    //
         size <= 0x1000000000000 &&  //
         virt + size <= 0x800000000000;
}

void InvalidateSystem(struct System *s, bool tlb, bool icache) {
#ifdef HAVE_THREADS
  struct Dll *e;
  struct Machine *m;
  LOCK(&s->machines_lock);
  for (e = dll_first(s->machines); e; e = dll_next(s->machines, e)) {
    m = MACHINE_CONTAINER(e);
    if (tlb) {
      atomic_store_explicit(&m->invalidated, true, memory_order_release);
    }
    if (icache) {
      atomic_store_explicit(&m->opcache->invalidated, true,
                            memory_order_release);
    }
  }
  UNLOCK(&s->machines_lock);
#endif
}

struct FileMap *AddFileMap(struct System *s, i64 virt, i64 size,
                           const char *path, u64 offset) {
  struct FileMap *fm;
  size_t pages, words;
  if (!path) return 0;
  if ((fm = (struct FileMap *)calloc(1, sizeof(struct FileMap)))) {
    fm->virt = virt;
    fm->size = size;
    fm->path = strdup(path);
    fm->offset = offset;
    pages = ROUNDUP(size, 4096) / 4096;
    words = ROUNDUP(pages, 64) / 64;
    if (fm->path && (fm->present = (u64 *)malloc(words * sizeof(u64)))) {
      memset(fm->present, -1, pages / 64 * sizeof(u64));
      if (pages % 64) fm->present[pages / 64] = (1ull << (pages % 64)) - 1;
      fm->pages = pages;
      dll_init(&fm->elem);
      dll_make_first(&s->filemaps, &fm->elem);
      ELF_LOGF("AddFileMap(%#" PRIx64 ", %#" PRIx64 ", %s, %#" PRIx64 ")", virt,
               size, path, offset);
      return fm;
    }
  }
  FreeFileMap(fm);
  return 0;
}

static void AddFileMapViaMap(struct System *s, i64 virt, i64 size, int fildes,
                             u64 offset) {
  char *path;
  struct Fd *fd;
  struct FileMap *fm;
  LOCK(&s->fds.lock);
  path = (fd = GetFd(&s->fds, fildes)) ? strdup(fd->path) : 0;
  UNLOCK(&s->fds.lock);
  fm = AddFileMap(s, virt, size, path, offset);
  free(path);
  if (fm && s->dis && s->onfilemap) {
    s->onfilemap(s, fm);
  }
}

struct FileMap *GetFileMap(struct System *s, i64 virt) {
  u64 i;
  struct Dll *e;
  struct FileMap *fm;
  for (e = dll_first(s->filemaps); e; e = dll_next(s->filemaps, e)) {
    fm = FILEMAP_CONTAINER(e);
    if (virt >= fm->virt && virt < fm->virt + fm->size) {
      i = virt - fm->virt;
      i /= 4096;
      if (fm->present[i / 64] & (1ull << (i % 64))) {
        return fm;
      }
    }
  }
  return 0;
}

static void UnmarkFilePage(struct System *s, i64 virt) {
  u64 i;
  bool lru;
  struct Dll *e;
  struct FileMap *fm;
  lru = false;
  for (e = dll_first(s->filemaps); e; e = dll_next(s->filemaps, e)) {
    fm = FILEMAP_CONTAINER(e);
    if (virt >= fm->virt && virt < fm->virt + fm->size) {
      unassert(fm->pages);
      i = virt - fm->virt;
      i /= 4096;
      if (fm->present[i / 64] & (1ull << (i % 64))) {
        fm->present[i / 64] &= ~(1ull << (i % 64));
        if (!--fm->pages) {
          dll_remove(&s->filemaps, e);
          FreeFileMap(fm);
        } else if (lru) {
          dll_remove(&s->filemaps, e);
          dll_make_first(&s->filemaps, e);
        }
        break;
      }
    }
    lru = true;
  }
}

static void WaitForPageToNotBeLocked(struct System *s, i64 virt, u8 *pte) {
  unassert(g_machine);
#ifdef DEBUG
  unassert(!IsOrphan(g_machine));
  unassert(!HasPageLock(g_machine, virt & -4096));
#endif
  LOCK(&s->pagelocks_lock);
  while (LoadPte(pte) & PAGE_LOCKS) {
    unassert(!pthread_cond_wait(&s->pagelocks_cond, &s->pagelocks_lock));
  }
  UNLOCK(&s->pagelocks_lock);
}

static bool FreePage(struct System *s, i64 virt, u64 entry, u64 size,
                     long *rss_delta) {
  u8 *page;
  long pagesize;
  uintptr_t real, mug;
  unassert(entry & PAGE_V);
  if (entry & PAGE_FILE) UnmarkFilePage(s, virt);
  if ((entry & (PAGE_HOST | PAGE_MAP | PAGE_MUG)) == PAGE_HOST) {
    unassert(~entry & PAGE_RSRV);
    s->memstat.committed -= 1;
    ClearPage((page = (u8 *)(uintptr_t)(entry & PAGE_TA)));
    FreeAnonymousPage(s, page);
    --*rss_delta;
    return false;
  } else if ((entry & (PAGE_HOST | PAGE_MAP | PAGE_MUG)) ==
             (PAGE_HOST | PAGE_MAP | PAGE_MUG)) {
    pagesize = FLAG_pagesize;
    real = entry & PAGE_TA;
    mug = ROUNDDOWN(real, pagesize);
    unassert(!Munmap((void *)mug, real - mug + size));
    if (entry & PAGE_RSRV) {
      s->memstat.reserved -= 1;
    } else {
      s->memstat.committed -= 1;
      --*rss_delta;
    }
    return false;
  } else if ((entry & (PAGE_HOST | PAGE_MAP | PAGE_MUG)) ==
             (PAGE_HOST | PAGE_MAP)) {
    unassert(!(entry & PAGE_RSRV));
    s->memstat.committed -= 1;
    --*rss_delta;
    return true;  // call is responsible for freeing
  } else if (entry & PAGE_RSRV) {
    s->memstat.reserved -= 1;
    return false;
  } else {
    unassert(!"impossible memory");
    return false;
  }
}

static void AddPageToRanges(struct ContiguousMemoryRanges *ranges, i64 virt,
                            i64 end) {
  if (!(ranges->i && ranges->p[ranges->i - 1].b == virt)) {
    if (ranges->i == ranges->n) {
      if (ranges->n) {
        ranges->n += ranges->n >> 1;
      } else {
        ranges->n = 8;
      }
      unassert(ranges->p = (struct ContiguousMemoryRange *)realloc(
                   ranges->p, ranges->n * sizeof(*ranges->p)));
    }
    ranges->p[ranges->i++].a = virt;
  }
  ranges->p[ranges->i - 1].b = virt + MIN(4096, end - virt);
}

// removes page table entries. anonymous pages will be added to the
// system's free list. mug pages will be freed one by one. linear pages
// won't be freed, and will instead have their intervals pooled in the
// ranges data structure; the caller is responsible for freeing those.
static void RemoveVirtual(struct System *s, i64 virt, i64 size,
                          struct ContiguousMemoryRanges *ranges,
                          bool *address_space_was_mutated,  //
                          long *vss_delta, long *rss_delta) {
  i64 end;
  u64 i, pt;
  u8 *pp, *pde;
  unsigned pi, p1;
  unassert(!(virt & 4095));
  MEM_LOGF("RemoveVirtual(%#" PRIx64 ", %#" PRIx64 ")", virt, size);
  for (pde = 0, end = virt + size; virt < end; virt += 1ull << i) {
    for (pt = s->cr3, i = 39;; i -= 9) {
      pi = p1 = (virt >> i) & 511;
      pp = GetPageAddress(s, pt, i == 39) + pi * 8;
      if (i == 12 + 9) pde = pp;
      pt = LoadPte(pp);
      if (i > 12 && !(pt & PAGE_V)) break;
      if (i > 12) continue;
    LastLevel:
      if (pt & PAGE_V) {
        for (;;) {
          if (pt & PAGE_LOCKS) {
            WaitForPageToNotBeLocked(s, virt, pp);
          } else if (CasPte(pp, pt, 0)) {
            break;
          }
          pt = LoadPte(pp);
          unassert(pt & PAGE_V);
        }
        if (FreePage(s, virt, pt, MIN(4096, end - virt), rss_delta) &&
            HasLinearMapping()) {
          AddPageToRanges(ranges, virt, end);
        }
        *address_space_was_mutated = true;
        --*vss_delta;
      }
      if (virt + 4096 < end && pi < 511) {
        pi += 1;
        pp += 8;
        pt = LoadPte(pp);
        virt += 4096;
        goto LastLevel;
      } else if (!p1 && pi == 511) {
        // when more than 512*4096 bytes are being unmapped, we
        // opportunistically unmap page directories too, if the
        // requested interval overlaps an entire page table. we
        // guarantee safety because mmap_lock is held, which is
        // required to create/remove (but not edit) the entries
        // therefore if we observed all entries are zero we can
        // say for certain it's safe to free. the only question
        // becomes readers like FindPageTableEntry() that still
        // crawl an old pointer to a free page table. free page
        // tables may be crawled because they always get zero'd
        // before being put into a freelist fifo that cools off
        FreePageTable(s, GetPageAddress(s, LoadPte(pde), i == 39));
        StorePte(pde, 0);
      }
      break;
    }
  }
}

_Noreturn static void PanicDueToMmap(void) {
#ifndef NDEBUG
  WriteErrorString(
      "unrecoverable mmap() crisis: see log for further details\n");
#else
  WriteErrorString(
      "unrecoverable mmap() crisis: Blink was built with NDEBUG\n");
#endif
  exit(250);
}

static int FailDueToHostAlignment(i64 virt, long pagesize, const char *kind) {
  LOGF("app chose mmap %s (%#" PRIx64 ") that's not aligned "
       "to the platform page size (%#lx) while using linear mode "
       "(try using `blink -m`)",
       kind, virt, pagesize);
  return einval();
}

static int DetermineHostProtection(int prot) {
  int sysprot;
  // blink never executes guest memory on metal
  sysprot = prot & ~PROT_EXEC;
  // when memory is being fully virtualized,
  // blink will check permissions on its own
  // note we can't force write permission in
  // some cases such as shared file mappings
  if (!HasLinearMapping()) {
    sysprot |= PROT_READ;
  }
  return sysprot;
}

i64 ReserveVirtual(struct System *s, i64 virt, i64 size, u64 flags, int fd,
                   i64 offset, bool shared, bool fixedmap) {
  u8 *mi;
  int demand;
  int method;
  i64 result;
  bool mutated;
  void *got, *want;
  long i, pagesize;
  int prot, sysprot;
  long vss_delta, rss_delta;
  i64 ti, pt, end, pages, level, entry;
  struct ContiguousMemoryRanges ranges;

  // we determine these
  unassert(!(flags & PAGE_TA));
  unassert(!(flags & PAGE_MAP));
  unassert(!(flags & PAGE_HOST));
  unassert(!(flags & PAGE_RSRV));
  unassert(s->mode == XED_MODE_LONG);

  // determine memory protection
  prot = GetProtection(flags);
  sysprot = DetermineHostProtection(prot);

  MEM_LOGF("ReserveVirtual(%#" PRIx64 ", %#" PRIx64 ", %s)", virt, size,
           DescribeProt(prot));

  if (!IsValidAddrSize(virt, size)) {
    LOGF("mmap(addr=%#" PRIx64 ", size=%#" PRIx64 ") is not a legal mapping",
         virt, size);
    return einval();
  }

  if (fd != -1 && (offset & 4095)) {
    LOGF("mmap(offset=%#" PRIx64 ") isn't 4096-byte page aligned", offset);
    return einval();
  }

  pagesize = FLAG_pagesize;

  if (HasLinearMapping()) {
    if (virt & (pagesize - 1)) {
      return FailDueToHostAlignment(virt, pagesize, "address");
    }
    if (offset & (pagesize - 1)) {
      return FailDueToHostAlignment(offset, pagesize, "file offset");
    }
  }

  // remove existing mapping
  vss_delta = 0;
  rss_delta = 0;
  mutated = false;
  pages = ROUNDUP(size, 4096) / 4096;
  if (HasLinearMapping() && FLAG_vabits <= 47) {
    if (fixedmap) {
      method = MAP_FIXED;
    } else if (virt) {
      method = MAP_DEMAND;
    } else {
      method = 0;
    }
  } else {
    if (FLAG_vabits <= 47) {
      demand = MAP_DEMAND;
    } else {
      demand = MAP_FIXED;
    }
    memset(&ranges, 0, sizeof(ranges));
    RemoveVirtual(s, virt, size, &ranges, &mutated, &vss_delta, &rss_delta);
    if (ranges.i) {
      // linear mappings exist within the requested interval
      if (ranges.i == 1 &&          //
          ranges.p[0].a == virt &&  //
          ranges.p[0].b == virt + size) {
        // it should be 100% safe to let the kernel blow it away
        method = MAP_FIXED;
      } else {
        // holes exist; try to create a greenfield
        for (i = 0; i < ranges.i; ++i) {
          Munmap(ToHost(ranges.p[i].a), ranges.p[i].b - ranges.p[i].a);
          mutated = true;
        }
        // errors in Munmap() should propagate to Mmap() below
        method = demand;
      }
      free(ranges.p);
    } else {
      // requested interval should be a greenfield
      method = demand;
    }
  }

  if (HasLinearMapping()) {
    // create a linear mapping. doing this runs the risk of destroying
    // things the kernel put into our address space that blink doesn't
    // know about. systems like linux and freebsd have a feature which
    // lets us report a friendly error to the user, when that happens.
    // the solution is most likely to rebuild with -Wl,-Ttext-segment=
    // please note we need to take off the seatbelt after an execve().
    errno = 0;
    want = virt ? ToHost(virt) : 0;
    if ((got = Mmap(want, size, sysprot,                    //
                    (method |                               //
                     (fd == -1 ? MAP_ANONYMOUS_ : 0) |      //
                     (shared ? MAP_SHARED : MAP_PRIVATE)),  //
                    fd, offset, "linear")) != want) {
      if (got == MAP_FAILED && errno == ENOMEM && !mutated) {
        LOGF("host system returned ENOMEM");
        return -1;
      } else if (got != MAP_FAILED && !want) {
        virt = ToGuest(got);
        unassert(IsValidAddrSize(virt, size));
      } else {
        ERRF("mmap(%#" PRIx64 "[%p], %#" PRIx64 ")"
             " -> %#" PRIx64 "[%p] crisis: %s",
             virt, want, size, ToGuest(got), got,
             (method == MAP_DEMAND && errno == MAP_DENIED)
                 ? "requested memory overlapped blink image or system memory. "
                   "try using `blink -m` to disable memory optimizations, or "
                   "try compiling blink using -Wl,--image-base=0x23000000 or "
                   "possibly -Wl,-Ttext-segment=0x23000000 in LDFLAGS"
                 : DescribeHostErrno(errno));
        PanicDueToMmap();
      }
    }
    s->memstat.committed += pages;
    flags |= PAGE_HOST | PAGE_MAP;
    vss_delta += pages;
    rss_delta += pages;
  } else if (fd != -1 || shared) {
    vss_delta += pages;
    s->memstat.reserved += pages;
    flags |= PAGE_HOST | PAGE_MAP | PAGE_MUG | PAGE_RSRV;
  } else {
    flags |= PAGE_RSRV;
    vss_delta += pages;
    s->memstat.reserved += pages;
  }

  MEM_LOGF("reserving virtual [%#" PRIx64 ",%#" PRIx64 ") w/ %" PRId64 " kb",
           virt, virt + size, size / 1024);

  // create a filemap object
  if (fd != -1 && !(flags & PAGE_FILE)) {
    flags |= PAGE_FILE;
    AddFileMapViaMap(s, virt, size, fd, offset);
  }

  // add pml4t entries ensuring intermediary tables exist
  for (result = virt, end = virt + size;;) {
    for (pt = s->cr3, level = 39; level >= 12; level -= 9) {
      ti = (virt >> level) & 511;
      mi = GetPageAddress(s, pt, level == 39) + ti * 8;
      if (level > 12) {
        pt = LoadPte(mi);
        if (!(pt & PAGE_V)) {
          if ((pt = AllocatePageTable(s)) == -1) {
            WriteErrorString("mmap() crisis: ran out of page table memory\n");
            exit(250);
          }
          StorePte(mi, pt);
        }
        continue;
      }
      for (;;) {
        uintptr_t real;
        if (flags & PAGE_MAP) {
          if (flags & PAGE_MUG) {
            void *mug;
            off_t mugoff;
            int mugflags;
            long mugsize;
            long mugskew;
            mugsize = MIN(4096, end - virt);
            if (fd != -1) {
              mugskew = offset - ROUNDDOWN(offset, pagesize);
              mugoff = ROUNDDOWN(offset, pagesize);
              mugsize += mugskew;
            } else {
              mugoff = 0;
              mugskew = 0;
            }
            mugflags = (shared ? MAP_SHARED : MAP_PRIVATE) |
                       (fd == -1 ? MAP_ANONYMOUS_ : 0);
            mug = AllocateBig(mugsize, sysprot, mugflags, fd, mugoff);
            if (!mug) {
              ERRF("mmap(virt=%" PRIx64
                   ", size=%ld, flags=%#x, fd=%d, offset=%#" PRIx64
                   ") crisis: %s",
                   virt, mugsize, mugflags, fd, (u64)mugoff,
                   DescribeHostErrno(errno));
              PanicDueToMmap();
            }
            real = (uintptr_t)mug + mugskew;
            offset += 4096;
          } else {
            real = (uintptr_t)ToHost(virt);
          }
          unassert(!(real & ~PAGE_TA));
          entry = real | flags | PAGE_V;
        } else {
          entry = flags | PAGE_V;
        }
        for (;;) {
          pt = LoadPte(mi);
          if (pt & PAGE_LOCKS) {
            unassert(pt & PAGE_V);
            WaitForPageToNotBeLocked(s, virt, mi);
          } else if (CasPte(mi, pt, entry)) {
            break;
          }
        }
        if ((virt += 4096) >= end) {
          s->rss += rss_delta;
          s->vss += vss_delta;
          // TODO(jart): We should call InvalidateSystem appropriately.
#ifndef DISABLE_JIT
          if (HasLinearMapping() && !IsJitDisabled(&s->jit)) {
            result = ProtectRwxMemory(s, result, result, size, pagesize, prot);
          }
#endif
          return result;
        }
        if (++ti == 512) break;
        mi += 8;
      }
    }
  }
}

i64 FindVirtual(struct System *s, i64 virt, i64 size) {
  u64 i, pt, got, orig_virt = virt;
  (void)orig_virt;
StartOver:
  if (!IsValidAddrSize(virt, size)) {
    LOGF("FindVirtual [%#" PRIx64 ",%#" PRIx64 ") -> "
         "[%#" PRIx64 ",%#" PRIx64 ") not possible",
         orig_virt, orig_virt + size, virt, virt + size);
    return enomem();
  }
  got = 0;
  do {
    for (i = 39, pt = s->cr3;; i -= 9) {
      pt = LoadPte(GetPageAddress(s, pt, i == 39) +
                   (((virt + got) >> i) & 511) * 8);
      if (i == 12 || !(pt & PAGE_V)) break;
    }
    got += 1ull << i;
    if ((pt & PAGE_V)) {
      virt += got;
      goto StartOver;
    }
  } while (got < size);
  return virt;
}

int FreeVirtual(struct System *s, i64 virt, i64 size) {
  int rc;
  long i;
  bool mutated;
  long vss_delta, rss_delta;
  struct ContiguousMemoryRanges ranges;
  MEM_LOGF("freeing virtual [%#" PRIx64 ",%#" PRIx64 ") w/ %" PRId64 " kb",
           virt, virt + size, size / 1024);
  if (!IsValidAddrSize(virt, size)) {
    LOGF("invalid addr size");
    return einval();
  }
  vss_delta = 0;
  rss_delta = 0;
  memset(&ranges, 0, sizeof(ranges));
  RemoveVirtual(s, virt, size, &ranges, &mutated, &vss_delta, &rss_delta);
  for (rc = i = 0; i < ranges.i; ++i) {
    if (Munmap(ToHost(ranges.p[i].a), ranges.p[i].b - ranges.p[i].a)) {
      LOGF("failed to %s subrange"
           " [%" PRIx64 ",%" PRIx64 ") within requested range"
           " [%" PRIx64 ",%" PRIx64 "): %s",
           "munmap", ranges.p[i].a, ranges.p[i].b, virt, virt + size,
           DescribeHostErrno(errno));
      rc = einval();
    }
  }
  free(ranges.p);
  s->vss += vss_delta;
  s->rss += rss_delta;
  s->memchurn -= vss_delta;
  InvalidateSystem(s, true, false);
  return rc;
}

int GetProtection(u64 key) {
  int prot = 0;
  if (key & PAGE_U) prot |= PROT_READ;
  if (key & PAGE_RW) prot |= PROT_WRITE;
  if (!(key & PAGE_XD)) prot |= PROT_EXEC;
  return prot;
}

u64 SetProtection(int prot) {
  u64 key = 0;
  if (prot & PROT_READ) key |= PAGE_U;
  if (prot & PROT_WRITE) key |= PAGE_RW;
  if (~prot & PROT_EXEC) key |= PAGE_XD;
  return key;
}

bool IsFullyMapped(struct System *s, i64 virt, i64 size) {
  u8 *mi;
  u64 pt;
  i64 ti, end, level;
  for (end = virt + size;;) {
    for (pt = s->cr3, level = 39; level >= 12; level -= 9) {
      ti = (virt >> level) & 511;
      mi = GetPageAddress(s, pt, level == 39) + ti * 8;
      pt = LoadPte(mi);
      if (level > 12) {
        if (!(pt & PAGE_V)) {
          return false;
        }
        continue;
      }
      for (;;) {
        if (!(pt & PAGE_V)) {
          return false;
        }
        if ((virt += 4096) >= end) {
          return true;
        }
        if (++ti == 512) break;
        pt = LoadPte((mi += 8));
      }
    }
  }
}

bool IsFullyUnmapped(struct System *s, i64 virt, i64 size) {
  u8 *mi;
  i64 end;
  u64 i, pt;
  for (end = virt + size; virt < end; virt += 1ull << i) {
    for (pt = s->cr3, i = 39;; i -= 9) {
      mi = GetPageAddress(s, pt, i == 39) + ((virt >> i) & 511) * 8;
      pt = LoadPte(mi);
      if (!(pt & PAGE_V)) {
        break;
      } else if (i == 12) {
        return false;
      }
    }
  }
  return true;
}

int ProtectVirtual(struct System *s, i64 virt, i64 size, int prot,
                   bool hostonly) {
  int rc;
  int sysprot;
  u8 *mi, *real;
  u64 pt, pt2, key;
  long i, pagesize;
  i64 a, b, ti, end, level, orig_virt;
  bool executable_code_was_made_non_executable;
  struct ContiguousMemoryRanges ranges;
  MEM_LOGF("protecting virtual [%#" PRIx64 ",%#" PRIx64 ") w/ %s", virt,
           virt + size, DescribeProt(prot));
  orig_virt = virt;
  pagesize = FLAG_pagesize;
  if (!IsValidAddrSize(virt, size)) {
    return einval();
  }
  if (!IsFullyMapped(s, virt, size)) {
    LOGF("mprotect(%#" PRIx64 ", %#" PRIx64 ") interval has unmapped pages",
         virt, size);
    return enomem();
  }
  key = SetProtection(prot);
  sysprot = DetermineHostProtection(prot);
  // in linear mode, the guest might try to do something like
  // set a 4096 byte guard page to PROT_NONE at the bottom of
  // its 64kb stack. if the host operating system has a 64 kb
  // page size, then that would be bad. we can't satisfy prot
  // unless the guest takes the page size into consideration.
  if (HasLinearMapping() &&
      (virt - ROUNDDOWN(virt, pagesize) >= 4096 ||
       ROUNDUP(virt + size, pagesize) - (virt + size) >= 4096)) {
    unassert(!hostonly);  // caller should know better
    sysprot = PROT_READ | PROT_WRITE;
  }
  memset(&ranges, 0, sizeof(ranges));
  executable_code_was_made_non_executable = false;
  for (rc = 0, end = virt + size;;) {
    for (pt = s->cr3, level = 39; level >= 12; level -= 9) {
      ti = (virt >> level) & 511;
      mi = GetPageAddress(s, pt, level == 39) + ti * 8;
      pt = LoadPte(mi);
      if (level > 12) {
        if (!(pt & PAGE_V)) {
          goto MemoryDisappeared;
        }
        continue;
      }
      for (;;) {
        if (!(pt & PAGE_V)) {
          goto MemoryDisappeared;
        }
        if (HasLinearMapping() && (pt & (PAGE_HOST | PAGE_MAP | PAGE_MUG)) ==
                                      (PAGE_HOST | PAGE_MAP)) {
          AddPageToRanges(&ranges, virt, end);
        } else if ((pt & (PAGE_HOST | PAGE_MAP | PAGE_MUG)) ==
                   (PAGE_HOST | PAGE_MAP | PAGE_MUG)) {
          real = (u8 *)ROUNDDOWN((uintptr_t)(pt & PAGE_TA), pagesize);
          if (Mprotect(real, pagesize, sysprot, "mug")) {
            LOGF("mprotect(pt=%#" PRIx64
                 ", real=%p, size=%#lx, prot=%d) failed: %s",
                 pt, real, pagesize, prot, DescribeHostErrno(errno));
            rc = -1;
          }
        }
        if (!hostonly) {
          for (;;) {
            pt2 = (pt & ~(PAGE_U | PAGE_RW | PAGE_XD)) | key;
            if (CasPte(mi, pt, pt2)) break;
            pt = LoadPte(mi);
            if (!(pt & PAGE_V)) {
              goto MemoryDisappeared;
            }
          }
#ifdef HAVE_JIT
          if (!(pt & PAGE_XD) && (pt2 & PAGE_XD)) {
            // exec -> non-exec
            // avoid clearing instruction cache which is costly
            executable_code_was_made_non_executable = true;
          } else if ((pt & PAGE_XD) && !(pt2 & PAGE_XD) &&
                     !IsJitDisabled(&s->jit)) {
            // non-exec -> exec
            // delete jit paths associated with this page
            ResetJitPage(&s->jit, virt);
          }
#endif
        }
        if ((virt += 4096) >= end) {
          goto FinishedCrawling;
        }
        if (++ti == 512) break;
        pt = LoadPte((mi += 8));
      }
    }
  }
FinishedCrawling:
  if (HasLinearMapping()) {
    for (i = 0; i < ranges.i; ++i) {
      a = ROUNDDOWN(ranges.p[i].a, pagesize);
      b = ranges.p[i].b;
      if (Mprotect(ToHost(a), b - a, sysprot, "linear")) {
        LOGF("failed to %s subrange"
             " [%" PRIx64 ",%" PRIx64 ") within requested range"
             " [%" PRIx64 ",%" PRIx64 "): %s",
             "mprotect", a, b, orig_virt, orig_virt + size,
             DescribeHostErrno(errno));
        rc = -1;
      }
    }
    free(ranges.p);
  }
  if (!hostonly) {
#ifndef DISABLE_JIT
    if (HasLinearMapping() && !IsJitDisabled(&s->jit)) {
      ProtectRwxMemory(s, rc, orig_virt, size, pagesize, prot);
    }
#endif
    InvalidateSystem(s, true, executable_code_was_made_non_executable);
  }
  return rc;
MemoryDisappeared:
  // mprotect() doesn't lock pages, so a race condition can
  // occur if the guest fiendishly munmaps this memory from
  // a different thread. in that case, the main thing we do
  // care about is not crashing. therefore it should not be
  // too concerning that this failure could occur after the
  // address space has been mutated.
  if (HasLinearMapping()) {
    free(ranges.p);
  }
  return enomem();
}

int SyncVirtual(struct System *s, i64 virt, i64 size, int sysflags) {
  int rc;
  u8 *mi;
  u64 pt;
  i64 orig_virt;
  i64 ti, end, level;
  long i, skew, pagesize;
  struct ContiguousMemoryRanges ranges;
  if (!IsValidAddrSize(virt, size)) {
    return einval();
  }
  orig_virt = virt;
  (void)orig_virt;
  pagesize = FLAG_pagesize;
  if (HasLinearMapping() && (skew = virt & (pagesize - 1))) {
    size += skew;
    virt -= skew;
  }
  if (!IsFullyMapped(s, virt, size)) {
    LOGF("mprotect(%#" PRIx64 ", %#" PRIx64 ") interval has unmapped pages",
         virt, size);
    return enomem();
  }
  memset(&ranges, 0, sizeof(ranges));
  for (rc = 0, end = virt + size;;) {
    for (pt = s->cr3, level = 39; level >= 12; level -= 9) {
      ti = (virt >> level) & 511;
      mi = GetPageAddress(s, pt, level == 39) + ti * 8;
      pt = LoadPte(mi);
      if (level > 12) {
        if (!(pt & PAGE_V)) {
          goto MemoryDisappeared;
        }
        continue;
      }
      for (;;) {
        if (!(pt & PAGE_V)) {
          goto MemoryDisappeared;
        }
        if (HasLinearMapping() && (pt & (PAGE_HOST | PAGE_MAP | PAGE_MUG)) ==
                                      (PAGE_HOST | PAGE_MAP)) {
          AddPageToRanges(&ranges, virt, end);
        } else if ((pt & (PAGE_HOST | PAGE_MAP | PAGE_MUG)) ==
                   (PAGE_HOST | PAGE_MAP | PAGE_MUG)) {
          uintptr_t real = pt & PAGE_TA;
          uintptr_t page = ROUNDDOWN(real, pagesize);
          long lilsize = (real - page) + MIN(4096, end - virt);
          if (Msync((void *)page, lilsize, sysflags, "mug")) {
            LOGF("msync(%p [pt=%#" PRIx64
                 "], size=%#lx, flags=%d) failed: %s\n%s",
                 (void *)page, pt, pagesize, sysflags, DescribeHostErrno(errno),
                 FormatPml4t(g_machine));
            rc = -1;
          }
        }
        if ((virt += 4096) >= end) {
          goto FinishedCrawling;
        }
        if (++ti == 512) break;
        pt = LoadPte((mi += 8));
      }
    }
  }
FinishedCrawling:
  if (HasLinearMapping()) {
    for (i = 0; i < ranges.i; ++i) {
      if (Msync(ToHost(ranges.p[i].a), ranges.p[i].b - ranges.p[i].a, sysflags,
                "linear")) {
        LOGF("failed to %s subrange"
             " [%" PRIx64 ",%" PRIx64 ") within requested range"
             " [%" PRIx64 ",%" PRIx64 "): %s",
             "msync", ranges.p[i].a, ranges.p[i].b, orig_virt, orig_virt + size,
             DescribeHostErrno(errno));
        rc = -1;
      }
    }
    free(ranges.p);
  }
  return rc;
MemoryDisappeared:
  if (HasLinearMapping()) {
    free(ranges.p);
  }
  return enomem();
}

// @asyncsignalsafe
static i64 FindGuestAddr(struct System *s, uintptr_t hp, u64 pt, long lvl,
                         u64 *out_pte) {
  u8 *mi;
  i64 res;
  u64 pte, i;
  if ((mi = GetPageAddress(s, pt, lvl == 1))) {
    for (i = 0; i < 512; ++i) {
      if ((pte = LoadPte(mi + i * 8)) & PAGE_V) {
        if (lvl == 4) {
          if ((pte & PAGE_HOST) && (pte & PAGE_TA) == hp) {
            if (out_pte) {
              *out_pte = pte;
            }
            return i << 39;
          }
        } else if ((res = FindGuestAddr(s, hp, pte, lvl + 1, out_pte)) != -1) {
          return i << 39 | res >> 9;
        }
      }
    }
  }
  return -1;
}

// Reverse maps real host address to virtual guest address if exists.
// On failure the host address is returned and zero is stored in pte.
// @asyncsignalsafe
i64 ConvertHostToGuestAddress(struct System *s, void *ha, u64 *out_pte) {
  i64 g48;
  uintptr_t base;
  if (out_pte) *out_pte = 0;
  if ((uintptr_t)ha < kNullSize) return (uintptr_t)ha;
  if (HasLinearMapping() && !out_pte) return ToGuest(ha);
  base = (uintptr_t)ha & -4096;
  if ((g48 = FindGuestAddr(s, base, s->cr3, 1, out_pte)) != -1) {
    return ((i64)((u64)g48 << 16) >> 16) | ((uintptr_t)ha & 4095);
  } else {
    return (uintptr_t)ha;
  }
}
