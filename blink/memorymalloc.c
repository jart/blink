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
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/errno.h"
#include "blink/linux.h"
#include "blink/lock.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/map.h"
#include "blink/mop.h"
#include "blink/types.h"
#include "blink/util.h"

struct Allocator {
  pthread_mutex_t lock;
  _Atomic(u8 *) brk;
  struct HostPage *pages GUARDED_BY(lock);
} g_allocator = {
    PTHREAD_MUTEX_INITIALIZER,
};

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

static size_t GetBigSize(size_t n) {
  unassert(n);
  long z = GetSystemPageSize();
  return ROUNDUP(n, z);
}

void FreeBig(void *p, size_t n) {
  if (!p) return;
#if defined(__CYGWIN__) || defined(__EMSCRIPTEN__)
  unassert(!munmap(p, n));
#else
  unassert(!munmap(p, GetBigSize(n)));
#endif
}

void *AllocateBig(size_t n) {
  void *p;
#if defined(__CYGWIN__) || defined(__EMSCRIPTEN__)
  p = Mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0,
           "big");
  return p != MAP_FAILED ? p : 0;
#else
  u8 *brk;
  if (!(brk = atomic_load_explicit(&g_allocator.brk, memory_order_relaxed))) {
    // we're going to politely ask the kernel for addresses starting
    // arbitrary megabytes past the end of our own executable's .bss
    // section. we'll cross our fingers, and hope that gives us room
    // away from a brk()-based libc malloc() function which may have
    // already allocated memory in this space. the reason it matters
    // is because the x86 and arm isas impose limits on displacement
    atomic_compare_exchange_strong_explicit(
        &g_allocator.brk, &brk, (u8 *)kPreciousStart, memory_order_relaxed,
        memory_order_relaxed);
  }
  n = GetBigSize(n);
  do {
    brk = atomic_fetch_add_explicit(&g_allocator.brk, n, memory_order_relaxed);
    if (brk + n > (u8 *)kPreciousEnd) {
      enomem();
      return 0;
    }
    p = Mmap(brk, n, PROT_READ | PROT_WRITE,
             MAP_DEMAND | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0, "big");
  } while (p == MAP_FAILED && errno == MAP_DENIED);
  return p != MAP_FAILED ? p : 0;
#endif
}

static void FreeHostPages(struct System *s) {
  // TODO(jart): Iterate the PML4T to find host pages.
}

struct System *NewSystem(void) {
  struct System *s;
  if ((s = (struct System *)AllocateBig(sizeof(struct System)))) {
    InitJit(&s->jit);
    InitFds(&s->fds);
    pthread_mutex_init(&s->sig_lock, 0);
    pthread_mutex_init(&s->mmap_lock, 0);
    pthread_mutex_init(&s->futex_lock, 0);
    pthread_cond_init(&s->machines_cond, 0);
    pthread_mutex_init(&s->machines_lock, 0);
    s->blinksigs = 1ull << (SIGSYS_LINUX - 1) |   //
                   1ull << (SIGILL_LINUX - 1) |   //
                   1ull << (SIGFPE_LINUX - 1) |   //
                   1ull << (SIGSEGV_LINUX - 1) |  //
                   1ull << (SIGTRAP_LINUX - 1);
    s->automap = kAutomapStart;
    s->brand = "GenuineCosmo";
    s->pid = getpid();
  }
  return s;
}

static void FreeMachineUnlocked(struct Machine *m) {
  THR_LOGF("pid=%d tid=%d FreeMachine", m->system->pid, m->tid);
  if (g_machine == m) {
    g_machine = 0;
  }
  if (IsMakingPath(m)) {
    AbandonJit(&m->system->jit, m->path.jb);
  }
  CollectGarbage(m);
  free(m->freelist.p);
  free(m);
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
  struct Dll *e;
  struct Machine *m;
  unassert(s == g_machine->system);
  unassert(!dll_is_empty(s->machines));
  while (!IsOrphan(g_machine)) {
    LOCK(&s->machines_lock);
    for (e = dll_first(s->machines); e; e = dll_next(s->machines, e)) {
      if ((m = MACHINE_CONTAINER(e)) != g_machine) {
        THR_LOGF("pid=%d tid=%d is killing tid %d", s->pid, g_machine->tid,
                 m->tid);
        atomic_store_explicit(&m->killed, true, memory_order_release);
      }
    }
    unassert(!pthread_cond_wait(&s->machines_cond, &s->machines_lock));
    UNLOCK(&s->machines_lock);
  }
}

void RemoveOtherThreads(struct System *s) {
  struct Dll *e, *g;
  struct Machine *m;
  LOCK(&s->machines_lock);
  for (e = dll_first(s->machines); e; e = g) {
    g = dll_next(s->machines, e);
    m = MACHINE_CONTAINER(e);
    if (m != g_machine) {
      FreeMachineUnlocked(m);
    }
  }
  UNLOCK(&s->machines_lock);
}

void FreeSystem(struct System *s) {
  THR_LOGF("pid=%d FreeSystem", s->pid);
  unassert(dll_is_empty(s->machines));  // Use KillOtherThreads & FreeMachine
  FreeHostPages(s);
  unassert(!pthread_mutex_destroy(&s->machines_lock));
  unassert(!pthread_cond_destroy(&s->machines_cond));
  unassert(!pthread_mutex_destroy(&s->futex_lock));
  unassert(!pthread_mutex_destroy(&s->mmap_lock));
  unassert(!pthread_mutex_destroy(&s->sig_lock));
  DestroyFds(&s->fds);
  DestroyJit(&s->jit);
  FreeBig(s->fun, s->codesize * sizeof(atomic_int));
  FreeBig(s, sizeof(struct System));
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
    ResetInstructionCache(m);
  } else {
    memset(m, 0, sizeof(*m));
    ResetCpu(m);
  }
  m->ctid = 0;
  m->oplen = 0;
  m->system = system;
  m->mode = system->mode;
  m->thread = pthread_self();
  m->nolinear = system->nolinear;
  m->codesize = system->codesize;
  m->codestart = system->codestart;
  Write32(m->sigaltstack.ss_flags, SS_DISABLE_LINUX);
  m->fun = system->fun ? system->fun - system->codestart : 0;
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

void CollectGarbage(struct Machine *m) {
  long i;
  for (i = 0; i < m->freelist.n; ++i) {
    free(m->freelist.p[i]);
  }
  m->freelist.n = 0;
}

void FreeMachine(struct Machine *m) {
  bool orphan;
  struct System *s;
  if (m) {
    unassert((s = m->system));
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

u64 AllocatePage(struct System *s) {
  u8 *page;
  size_t i, n;
  struct HostPage *h;
  LOCK(&g_allocator.lock);
  if ((h = g_allocator.pages)) {
    g_allocator.pages = h->next;
    UNLOCK(&g_allocator.lock);
    page = h->page;
    FreeHostPage(h);
    --s->memstat.freed;
    ++s->memstat.committed;
    ++s->memstat.reclaimed;
    goto Finished;
  } else {
    UNLOCK(&g_allocator.lock);
  }
  n = 64;
  if (!(page = (u8 *)AllocateBig(n * 4096))) return -1;
  s->memstat.allocated += n;
  s->memstat.committed += 1;
  s->memstat.freed += n - 1;
  LOCK(&g_allocator.lock);
  for (i = 0; i + 1 < n; ++i, page += 4096) {
    unassert((h = NewHostPage()));
    h->page = page;
    h->next = g_allocator.pages;
    g_allocator.pages = h;
  }
  UNLOCK(&g_allocator.lock);
Finished:
  return ToGuest(page) | PAGE_HOST | PAGE_U | PAGE_RW | PAGE_V;
}

u64 AllocatePageTable(struct System *s) {
  u64 res;
  if ((res = AllocatePage(s)) != -1) {
    res &= ~PAGE_U;
    ++s->memstat.pagetables;
  }
  return res;
}

bool OverlapsPrecious(i64 virt, i64 size) {
  uint64_t BegA, EndA, BegB, EndB;
  if (size <= 0) return false;
  BegA = virt + kSkew;
  EndA = virt + kSkew + (size - 1);
  BegB = kPreciousStart;
  EndB = kPreciousEnd - 1;
  return MAX(BegA, BegB) < MIN(EndA, EndB);
}

bool IsValidAddrSize(i64 virt, i64 size) {
  return size &&                        //
         !(virt & 4095) &&              //
         virt >= -0x800000000000 &&     //
         virt < 0x800000000000 &&       //
         size <= 0x1000000000000ull &&  //
         virt + size <= 0x800000000000;
}

void InvalidateSystem(struct System *s, bool tlb, bool icache) {
  struct Dll *e;
  struct Machine *m;
  LOCK(&s->machines_lock);
  for (e = dll_first(s->machines); e; e = dll_next(s->machines, e)) {
    m = MACHINE_CONTAINER(e);
    if (tlb) {
      atomic_store_explicit(&m->invalidated, true, memory_order_relaxed);
    }
    if (icache) {
      atomic_store_explicit(&m->opcache->invalidated, true,
                            memory_order_relaxed);
    }
  }
  UNLOCK(&s->machines_lock);
}

static bool FreePage(struct System *s, u64 entry) {
  u8 *page;
  struct HostPage *h;
  unassert(entry & PAGE_V);
  if (entry & PAGE_RSRV) {
    --s->memstat.reserved;
    return false;
  } else if ((entry & (PAGE_HOST | PAGE_MAP)) == PAGE_HOST) {
    ++s->memstat.freed;
    --s->memstat.committed;
    ClearPage((page = ToHost(entry & PAGE_TA)));
    unassert((h = NewHostPage()));
    LOCK(&g_allocator.lock);
    h->page = page;
    h->next = g_allocator.pages;
    g_allocator.pages = h;
    UNLOCK(&g_allocator.lock);
    return false;
  } else if ((entry & (PAGE_HOST | PAGE_MAP)) == (PAGE_HOST | PAGE_MAP)) {
    --s->memstat.allocated;
    --s->memstat.committed;
    return true;
  } else {
    unassert((entry & PAGE_TA) < kRealSize);
    return false;
  }
}

static bool RemoveVirtual(struct System *s, i64 virt, i64 size) {
  u8 *mi;
  i64 end;
  u64 i, pt;
  bool has_maps = false;
  for (end = virt + size; virt < end; virt += 1ull << i) {
    for (pt = s->cr3, i = 39;; i -= 9) {
      mi = GetPageAddress(s, pt) + ((virt >> i) & 511) * 8;
      pt = Load64(mi);
      if (!(pt & PAGE_V)) {
        break;
      } else if (i == 12) {
        has_maps |= FreePage(s, pt);
        Store64(mi, 0);
        break;
      }
    }
  }
  return has_maps;
}

_Noreturn static void PanicDueToMmap(void) {
#ifndef NDEBUG
  WriteErrorString(
      "Unrecoverable mmap() crisis: see log for further details\n");
#else
  WriteErrorString(
      "Unrecoverable mmap() crisis: Blink was built with NDEBUG\n");
#endif
  exit(250);
}

int ReserveVirtual(struct System *s, i64 virt, i64 size, u64 flags, int fd,
                   i64 offset, bool shared) {
  u8 *mi;
  long pagesize;
  i64 ti, pt, end, level, entry;

  // we determine these
  unassert(!(flags & PAGE_TA));
  unassert(!(flags & PAGE_MAP));
  unassert(!(flags & PAGE_HOST));
  unassert(!(flags & PAGE_RSRV));
  unassert(s->mode == XED_MODE_LONG);

  if (!IsValidAddrSize(virt, size)) {
    LOGF("app attempted to map memory outside 48-bit pml4t space"
         " virt=%#" PRIx64 " size=%#" PRIx64,
         virt, size);
    return einval();
  }

  if (OverlapsPrecious(virt, size)) {
    LOGF("app attempted to map memory that blink reserves for itself");
    return enomem();
  }

  if (HasLinearMapping(s)) {
    if (virt <= 0) {
      LOGF("app attempted to map null or negative in linear mode");
      return enotsup();
    }
    pagesize = GetSystemPageSize();
    if (virt & (pagesize - 1)) {
      LOGF("app chose mmap addr (%#" PRIx64 ") that's not aligned "
           "to the platform page size (%#lx) while using linear mode",
           virt, pagesize);
      return einval();
    }
  }

  MEM_LOGF("reserving virtual [%#" PRIx64 ",%#" PRIx64 ") w/ %" PRId64 " kb",
           virt, virt + size, size / 1024);

  RemoveVirtual(s, virt, size);

  if (HasLinearMapping(s)) {
    if (Mmap(ToHost(virt), size,                                  //
             ((flags & PAGE_U ? PROT_READ : 0) |                  //
              ((flags & PAGE_RW) || fd == -1 ? PROT_WRITE : 0)),  //
             (MAP_FIXED |                                         //
              (fd == -1 ? MAP_ANONYMOUS : 0) |                    //
              (shared ? MAP_SHARED : MAP_PRIVATE)),               //
             fd, offset, "linear") == MAP_FAILED) {
      LOGF("mmap(%#" PRIx64 "[%p], %#" PRIx64 ") crisis: %s", virt,
           ToHost(virt), size, strerror(errno));
      PanicDueToMmap();
    }
    s->memstat.allocated += size / 4096;
    s->memstat.committed += size / 4096;
    flags |= PAGE_HOST | PAGE_MAP;
  } else {
    s->memstat.reserved += size / 4096;
  }

  // add pml4t entries ensuring intermediary tables exist
  for (end = virt + size;;) {
    for (pt = s->cr3, level = 39; level >= 12; level -= 9) {
      ti = (virt >> level) & 511;
      mi = GetPageAddress(s, pt) + ti * 8;
      pt = Load64(mi);
      if (level > 12) {
        if (!(pt & PAGE_V)) {
          if ((pt = AllocatePageTable(s)) == -1) {
            WriteErrorString("mmap() crisis: ran out of page table memory\n");
            exit(250);
          }
          Store64(mi, pt);
        }
        continue;
      }
      for (;;) {
        unassert(~pt & PAGE_V);
        if (flags & PAGE_MAP) {
          entry = virt | flags | PAGE_V;
        } else {
          entry = flags | PAGE_RSRV | PAGE_V;
        }
        if (fd != -1 && virt + 4096 >= end) {
          entry |= PAGE_EOF;
        }
        Store64(mi, entry);
        if ((virt += 4096) >= end) {
          return 0;
        }
        if (++ti == 512) break;
        pt = Load64((mi += 8));
      }
    }
  }
}

i64 FindVirtual(struct System *s, i64 virt, i64 size) {
  u64 i, pt, got = 0;
  if (!IsValidAddrSize(virt, size)) return einval();
  if (OverlapsPrecious(virt, size)) virt = kPreciousEnd + kSkew;
  do {
    if (virt >= 0x800000000000) return enomem();
    for (pt = s->cr3, i = 39; i >= 12; i -= 9) {
      pt = Load64(GetPageAddress(s, pt) + ((virt >> i) & 511) * 8);
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
  long pagesize;
  if (!IsValidAddrSize(virt, size)) {
    return einval();
  }
  if (HasLinearMapping(s)) {
    pagesize = GetSystemPageSize();
    if (virt & (pagesize - 1)) {
      LOGF("app chose munmap addr (%#" PRIx64 ") that's not aligned "
           "to the platform page size (%#lx) while using linear mode",
           virt, pagesize);
      return einval();
    }
  }
  MEM_LOGF("freeing virtual [%#" PRIx64 ",%#" PRIx64 ") w/ %" PRId64 " kb",
           virt, virt + size, size / 1024);
  // TODO(jart): We should probably validate a PAGE_EOF exists at the
  //             end when size isn't a multiple of platform page size
  if (RemoveVirtual(s, virt, size)) {
    unassert(!munmap(ToHost(virt & PAGE_TA), size));
  }
  InvalidateSystem(s, true, false);
  return 0;
}

int GetProtection(u64 key) {
  int prot = 0;
  if (key & PAGE_U) prot |= PROT_READ;
  if (key & PAGE_RW) prot |= PROT_WRITE;
  if (~key & PAGE_XD) prot |= PROT_EXEC;
  return prot;
}

u64 SetProtection(int prot) {
  u64 key = 0;
  if (prot & PROT_READ) key |= PAGE_U;
  if (prot & PROT_WRITE) key |= PAGE_RW;
  if (~prot & PROT_EXEC) key |= PAGE_XD;
  return key;
}

int CheckVirtual(struct System *s, i64 virt, i64 size) {
  u8 *mi;
  u64 pt;
  i64 ti, end, level;
  for (end = virt + size;;) {
    for (pt = s->cr3, level = 39; level >= 12; level -= 9) {
      ti = (virt >> level) & 511;
      mi = GetPageAddress(s, pt) + ti * 8;
      pt = Get64(mi);
      if (level > 12) {
        if (!(pt & PAGE_V)) {
          return enomem();
        }
        continue;
      }
      for (;;) {
        if (!(pt & PAGE_V)) {
          return enomem();
        }
        if ((virt += 4096) >= end) {
          return 0;
        }
        if (++ti == 512) break;
        pt = Get64((mi += 8));
      }
    }
  }
}

int ProtectVirtual(struct System *s, i64 virt, i64 size, int prot) {
  u8 *mi;
  u64 pt, key;
  long pagesize;
  i64 ti, end, level, mpstart, last = -1;
  pagesize = GetSystemPageSize();
  if (!IsValidAddrSize(virt, size)) {
    return einval();
  }
  if (HasLinearMapping(s) && (virt & (pagesize - 1))) {
    LOGF("mprotect(%#" PRIx64 ", %#" PRIx64 ", %d)"
         " not aligned to host page size while using linear mode",
         virt, size, prot);
    return einval();
  }
  if (CheckVirtual(s, virt, size) == -1) {
    LOGF("mprotect(%#" PRIx64 ", %#" PRIx64 ") interval has unmapped pages",
         virt, size);
    return -1;
  }
  key = SetProtection(prot);
  for (end = virt + size;;) {
    for (pt = s->cr3, level = 39; level >= 12; level -= 9) {
      ti = (virt >> level) & 511;
      mi = GetPageAddress(s, pt) + ti * 8;
      pt = Get64(mi);
      if (level > 12) {
        unassert(pt & PAGE_V);
        continue;
      }
      for (;;) {
        unassert(pt & PAGE_V);
        // TODO(jart): have fewer system calls
        if (HasLinearMapping(s) && (pt & PAGE_HOST) &&
            (mpstart = ROUNDDOWN(virt, pagesize)) != last) {
          last = mpstart;
          if (mprotect(ToHost(mpstart), pagesize, prot & ~PROT_EXEC)) {
            LOGF("mprotect(%#" PRIx64 " [%p], %#" PRIx64 ", %d) failed",
                 mpstart, ToHost(mpstart), pagesize, prot);
            Abort();
          }
        }
        pt &= ~(PAGE_U | PAGE_RW | PAGE_XD);
        pt |= key;
        Put64(mi, pt);
        if ((virt += 4096) >= end) return 0;
        if (++ti == 512) break;
        pt = Get64((mi += 8));
      }
    }
  }
  InvalidateSystem(s, true, false);
}

void SyncVirtual(struct Machine *m, i64 virt, i64 size, int fd, i64 offset) {
  // TODO(jart): Raise SIGBUS on i/o error.
  // TODO(jart): Support lazy file mappings.
  size_t i;
  void *tmp;
  ssize_t rc;
  sigset_t ss, oldss;
  unassert(!sigfillset(&ss));
  unassert(!pthread_sigmask(SIG_SETMASK, &ss, &oldss));
  unassert((tmp = calloc(1, size)));
  for (i = 0; i < size; i += rc) {
    rc = pread(fd, (char *)tmp + i, size - i, offset + i);
    if (!rc) break;
    if (rc == -1) {
      LOGF("failed to read %zu bytes at offset %" PRId64
           " from fd %d into memory map: %s",
           size, offset, fd, strerror(errno));
      Abort();
    }
  }
  CopyToUserWrite(m, virt, tmp, size);
  free(tmp);
  unassert(!pthread_sigmask(SIG_SETMASK, &oldss, 0));
}
