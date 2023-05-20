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
#include "blink/jit.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/atomic.h"
#include "blink/bitscan.h"
#include "blink/builtin.h"
#include "blink/checked.h"
#include "blink/debug.h"
#include "blink/dll.h"
#include "blink/end.h"
#include "blink/endian.h"
#include "blink/errno.h"
#include "blink/flag.h"
#include "blink/log.h"
#include "blink/macros.h"
#include "blink/map.h"
#include "blink/memcpy.h"
#include "blink/stats.h"
#include "blink/thread.h"
#include "blink/tsan.h"
#include "blink/util.h"

/**
 * @fileoverview Just-In-Time Function Builder
 *
 * This file implements a low-level systems abstraction that enables
 * native functions to be written to memory and then executed. These
 * interfaces are used by Jitter() which is a higher-level interface
 * that's intended for generating implementations of x86 operations.
 *
 * This JIT only supports x86-64 and aarch64. That makes life easier
 * since both architectures have many registers and a register-based
 * calling convention. This JIT works around the restrictions placed
 * upon self-modifying code imposed by both OSes and microprocessors
 * such as:
 *
 * 1. ARM CPUs require a non-trivial flushing of instruction caches.
 * 2. OpenBSD always imposes strict W^X memory protection invariant.
 * 3. Apple requires we use JIT memory and toggle thread JIT states.
 * 4. ISAs limit the distance between JIT memory and our executable.
 * 5. Some platforms have a weird page size, e.g. Apple M1 is 16384.
 *
 * Meeting the requirements of all these platforms, means we have to
 * follow a very narrow path of possibilities, in terms of practices
 * plus design. These APIs make conformance easier. The way it works
 * here is multiple pages of memory are allocated in chunks, that we
 * call "blocks". Each time a thread wishes to create a function, it
 * must lease one of these blocks using StartJit(), FinishJit(), and
 * SpliceJit(). During that lease, functions may be generated, which
 * call arbitrary functions built by the compiler. Any function made
 * here needs to have a non-JIT implementation that can be called to
 * allow time for the JIT block to be flushed, at which time the JIT
 * updates a corresponding function hook.
 *
 *     // setup a multi-threaded jit manager
 *     struct Jit jit;
 *     InitJit(&jit, 0);
 *
 *     // workflow for composing two function calls
 *     i64 key = 1234;
 *     long Adder(long x, long y) { return x + y; }
 *     struct JitBlock *jb;
 *     jb = StartJit(jit, key);
 *     AppendJit(jb, kPrologue, sizeof(kPrologue));
 *     AppendJitSetReg(jb, kJitArg0, 1);
 *     AppendJitSetReg(jb, kJitArg1, 2);
 *     AppendJitCall(jb, (void *)Adder);
 *     AppendJitMovReg(jb, kJitRes0, kJitArg0);
 *     AppendJitSetReg(jb, kJitArg1, 3);
 *     AppendJitCall(jb, (void *)Adder);
 *     AppendJit(jb, kEpilogue, sizeof(kEpilogue));
 *     FinishJit(jit, jb);
 *     FlushJit(jit);
 *     printf("1+2+3=%ld\n", ((long (*)(void))(GetJitHook(jit, key)))());
 *
 *     // destroy jit and all its functions
 *     DestroyJit(&jit);
 *
 * Note that FlushJit() should only be a last resort. Functions will
 * become live immediately on operating systems which authorize self
 * modifying-code using RWX memory and have the appropriate compiler
 * builtin for flushing the CPU instruction cach. If weird platforms
 * aren't a concern, then the hairiness of hooking and/or FlushJit()
 * shouldn't be of any concern. Try using CanJitForImmediateEffect()
 * after having called StartJit() to check.
 */

#define kMaximumAnticipatedPageSize 65536

const u8 kJitRes[2] = {kJitRes0, kJitRes1};
const u8 kJitArg[4] = {kJitArg0, kJitArg1, kJitArg2, kJitArg3};
const u8 kJitSav[5] = {kJitSav0, kJitSav1, kJitSav2, kJitSav3, kJitSav4};

#ifdef HAVE_JIT

#define ACTION_MOVE    0x010000
#define ACTION(a)      ((0xff0000 & a))
#define MOVE(dst, src) ((dst) | (src) << 8 | ACTION_MOVE)
#define MOVE_DST(a)    ((0x0000ff & (a)) >> 0)
#define MOVE_SRC(a)    ((0x00ff00 & (a)) >> 8)
#define HASH(virt)     (virt)

static u8 g_code[kJitMemorySize];

static struct JitGlobals {
  pthread_mutex_t_ lock;
  _Atomic(long) prot;
  int freecount;
  struct Dll *freeblocks;
} g_jit = {
    PTHREAD_MUTEX_INITIALIZER_,
    PROT_READ | PROT_WRITE | PROT_EXEC,
};

static inline u64 RoundupTwoPow(u64 x) {
  return x > 1 ? (u64)2 << bsr(x - 1) : x ? 1 : 0;
}

// begins write operation to memory that may be read locklessly
// - generation is monotonic
// - even numbers mean memory is ready
// - odd numbers mean memory is actively being changed
static inline unsigned BeginUpdate(_Atomic(unsigned) *genptr) {
  unsigned gen = atomic_load_explicit(genptr, memory_order_relaxed);
  unassert(~gen & 1);  // prevents re-entering transaction
  atomic_store_explicit(genptr, gen + 1, memory_order_release);
  return gen;
}

// finishes write operation to memory that may be read locklessly
static inline void EndUpdate(_Atomic(unsigned) *genptr, unsigned gen) {
  unassert(~gen & 1);
  atomic_store_explicit(genptr, gen + 2, memory_order_release);
}

// determines if lockless operation should be retried or abandoned
static inline unsigned ShallNotPass(unsigned gen1, _Atomic(unsigned) *genptr) {
  unsigned gen2 = atomic_load_explicit(genptr, memory_order_acquire);
  unsigned is_being_actively_changed = gen1 & 1;
  unsigned we_lost_race_with_writers = gen1 ^ gen2;
  return is_being_actively_changed | we_lost_race_with_writers;
}

// apple forbids rwx memory on their new m1 macbooks and requires that
// we use a non-posix api in order to have jit. the problem is the api
// frequently flakes with "Trace/BPT trap: 5" errors. this fixes that.
static void pthread_jit_write_protect_np_workaround(int enabled) {
#if defined(__APPLE__) && defined(__aarch64__)
  int count_start = 8192;
  volatile int count = count_start;
  uint64_t *addr, *other, val, val2, reread = -1;
  addr = (uint64_t *)(!enabled ? _COMM_PAGE_APRR_WRITE_ENABLE
                               : _COMM_PAGE_APRR_WRITE_DISABLE);
  other = (uint64_t *)(enabled ? _COMM_PAGE_APRR_WRITE_ENABLE
                               : _COMM_PAGE_APRR_WRITE_DISABLE);
  switch (*(volatile uint8_t *)_COMM_PAGE_APRR_SUPPORT) {
    case 1:
      do {
        val = *addr;
        reread = -1;
        asm volatile("msr S3_4_c15_c2_7, %0\n"
                     "isb sy\n"
                     : /* no outputs */
                     : "r"(val)
                     : "memory");
        val2 = *addr;
        asm volatile("mrs %0, S3_4_c15_c2_7\n"
                     : "=r"(reread)
                     : /* no inputs */
                     : "memory");
        if (val2 == reread) {
          return;
        }
        usleep(10);
      } while (count-- > 0);
      break;
    case 3:
      do {
        val = *addr;
        reread = -1;
        asm volatile("msr S3_6_c15_c1_5, %0\n"
                     "isb sy\n"
                     : /* no outputs */
                     : "r"(val)
                     : "memory");
        val2 = *addr;
        asm volatile("mrs %0, S3_6_c15_c1_5\n"
                     : "=r"(reread)
                     : /* no inputs */
                     : "memory");
        if (val2 == reread) {
          return;
        }
        usleep(10);
      } while (count-- > 0);
      break;
    default:
      pthread_jit_write_protect_np(enabled);
      return;
  }
  ERRF("failed to set jit write protection");
  Abort();
#else
  pthread_jit_write_protect_np(enabled);
#endif
}

static void *Calloc(size_t nmemb, size_t size) {
  STATISTIC(++jit_callocs);
  return calloc(nmemb, size);
#define calloc please_use_Calloc
}

static void *Realloc(void *p, size_t n) {
  STATISTIC(++jit_reallocs);
  return realloc(p, n);
#define realloc please_use_Realloc
}

static void Free(void *ptr) {
  if (!ptr) return;
  STATISTIC(++jit_frees);
  free(ptr);
#define free please_use_Free
}

static struct JitJump *NewJitJump(struct Dll **freejumps) {
  struct Dll *e;
  struct JitJump *jj;
  if ((e = dll_first(*freejumps))) {
    STATISTIC(++jit_jump_alloc_freelist);
    dll_remove(freejumps, e);
    jj = JITJUMP_CONTAINER(e);
  } else if ((jj = (struct JitJump *)Calloc(1, sizeof(struct JitJump)))) {
    STATISTIC(++jit_jump_alloc_system);
    dll_init(&jj->elem);
  }
  return jj;
}

static struct JitPage *NewJitPage(void) {
  struct JitPage *jj;
  if ((jj = (struct JitPage *)Calloc(1, sizeof(struct JitPage)))) {
    dll_init(&jj->elem);
  }
  return jj;
}

static struct JitBlock *NewJitBlock(void) {
  struct JitBlock *jb;
  if ((jb = (struct JitBlock *)Calloc(1, sizeof(struct JitBlock)))) {
    dll_init(&jb->elem);
    dll_init(&jb->aged);
    JIT_LOGF("new jit block %p", jb);
  }
  return jb;
}

static struct JitStage *NewJitStage(void) {
  struct JitStage *js;
  if ((js = (struct JitStage *)Calloc(1, sizeof(struct JitStage)))) {
    dll_init(&js->elem);
  }
  return js;
}

static struct JitFreed *NewJitFreed(void) {
  struct JitFreed *jf;
  if ((jf = (struct JitFreed *)Calloc(1, sizeof(struct JitFreed)))) {
    dll_init(&jf->elem);
  }
  return jf;
}

static struct JitIntsSlab *NewJitIntsSlab(void) {
  struct JitIntsSlab *jis;
  if ((jis = (struct JitIntsSlab *)Calloc(1, sizeof(struct JitIntsSlab)))) {
    dll_init(&jis->elem);
  }
  return jis;
}

static void FreeJitJump(struct JitJump *jj) {
  Free(jj);
}

static void FreeJitPage(struct JitPage *jp) {
  Free(jp);
}

static void FreeJitFreed(struct JitFreed *jf) {
  Free(jf->data);
  Free(jf);
}

static void DestroyInts(struct JitInts *ji) {
  if (ji->p != ji->m) {
    Free(ji->p);
  }
}

static void FreeJitBlock(struct JitBlock *jb) {
  struct Dll *e;
  JIT_LOGF("freed jit block %p", jb);
  while ((e = dll_first(jb->freejumps))) {
    dll_remove(&jb->freejumps, e);
    FreeJitJump(JITJUMP_CONTAINER(e));
  }
  Free(jb);
}

static void FreeJitStage(struct JitStage *js) {
  Free(js);
}

static void DestroyIntsAllocator(struct JitIntsAllocator *jia) {
  int i;
  struct Dll *e, *e2;
  struct JitIntsSlab *jis;
  Free(jia->p);
  for (e = dll_first(jia->slabs); e; e = e2) {
    e2 = dll_next(jia->slabs, e);
    jis = JIASLAB_CONTAINER(e);
    for (i = 0; i < ARRAYLEN(jis->p); ++i) {
      DestroyInts(jis->p + i);
    }
    Free(jis);
  }
}

static dontinline bool GrowIntsAllocator(struct JitIntsAllocator *jia) {
  int n2;
  struct JitInts **p2;
  p2 = jia->p;
  n2 = jia->n;
  if (n2 >= 2) {
    n2 += n2 >> 1;
  } else {
    n2 = 8;
  }
  if ((p2 = (struct JitInts **)Realloc(p2, n2 * sizeof(*p2)))) {
    jia->p = p2;
    jia->n = n2;
    return true;
  } else {
    return false;
  }
}

static struct JitInts *NewInts(struct JitIntsAllocator *jia) {
  struct Dll *e;
  struct JitInts *ji;
  struct JitIntsSlab *slab;
  if (jia->i) {
    STATISTIC(++jit_ints_alloc_freelist);
    return jia->p[--jia->i];
  }
  if ((e = dll_first(jia->slabs))) {
    STATISTIC(++jit_ints_alloc_slab);
    slab = JIASLAB_CONTAINER(e);
    if (slab->i < ARRAYLEN(slab->p)) {
      ji = slab->p + slab->i++;
      ji->p = ji->m;
      ji->n = ARRAYLEN(ji->m);
      return ji;
    }
  }
  if ((slab = NewJitIntsSlab())) {
    STATISTIC(++jit_ints_alloc_system);
    dll_make_first(&jia->slabs, &slab->elem);
    return slab->p + slab->i++;
  }
  return 0;
}

static void FreeInts(struct JitIntsAllocator *jia, struct JitInts *ji) {
  if (ji) {
    ji->i = 0;
    if (jia->i == jia->n && !GrowIntsAllocator(jia)) return;
    jia->p[jia->i++] = ji;
  }
}

static bool GrowInts(struct JitInts *ji) {
  i64 *p2;
  size_t i, n2;
  p2 = ji->p;
  if (!ji->p) {
    unassert(!ji->n);
    ji->p = ji->m;
    ji->n = ARRAYLEN(ji->m);
    return true;
  } else if (ji->p == ji->m) {
    unassert(ji->n == ARRAYLEN(ji->m));
    n2 = ARRAYLEN(ji->m) * 2;
    if ((p2 = Calloc(n2, sizeof(*p2)))) {
      for (i = 0; i < ARRAYLEN(ji->m); ++i) {
        p2[i] = ji->m[i];
      }
      ji->p = p2;
      ji->n = n2;
      return true;
    } else {
      return false;
    }
  } else {
    n2 = ji->n;
    n2 += n2 >> 1;
    if ((p2 = (i64 *)Realloc(p2, n2 * sizeof(*p2)))) {
      ji->p = p2;
      ji->n = n2;
      return true;
    } else {
      return false;
    }
  }
}

static bool AddInt(struct JitInts *ji, i64 x) {
  if (ji->i == ji->n && !GrowInts(ji)) return false;
  ji->p[ji->i++] = x;
  return true;
}

static bool RemoveInt(struct JitInts *ji, i64 x) {
  int i, j;
  for (i = j = 0; i < ji->i; ++i) {
    if (ji->p[i] != x) {
      ji->p[j++] = ji->p[i];
    }
  }
  ji->i -= i - j;
  return i > j;
}

static void InitEdges(struct JitEdges *e) {
  memset(e, 0, sizeof(*e));
  e->n = RoundupTwoPow(kJitInitialEdges);
  unassert(e->src = (i64 *)Calloc(e->n, sizeof(*e->src)));
  unassert(e->dst = (struct JitInts **)Calloc(e->n, sizeof(*e->dst)));
}

static void DestroyEdges(struct JitEdges *edges) {
  DestroyIntsAllocator(&edges->jia);
  Free(edges->dst);
  Free(edges->src);
}

static nosideeffect int GetEdge(const struct JitEdges *edges, i64 src) {
  unsigned hash, spot, step;
  hash = HASH(src);
  for (spot = step = 0;; ++step) {
    spot = (hash + step * ((step + 1) >> 1)) & (edges->n - 1);
    if (!edges->src[spot] || edges->src[spot] == src) {
      return spot;
    }
  }
}

static unsigned GrowEdges(struct JitEdges *edges) {
  i64 *src, *src2;
  struct JitInts **dst, **dst2;
  unsigned i, i1, i2, n1, n2, used, hash, spot, step;
  i1 = edges->i;
  n1 = edges->n;
  src = edges->src;
  dst = edges->dst;
  unassert(n1 > 1 && IS2POW(n1));
  for (used = i = 0; i < n1; ++i) used += !!dst[i];
  n2 = n1 << (used > (n1 >> 2));
  if (!(src2 = (i64 *)Calloc(n2, sizeof(*src2))) ||
      !(dst2 = (struct JitInts **)Calloc(n2, sizeof(*dst2)))) {
    Free(src2);
    return 0;
  }
  for (i2 = i = 0; i < n1; ++i) {
    if (!src[i]) {
      unassert(!dst[i]);
      continue;
    }
    --i1;
    if (!dst[i]) {
      continue;
    }
    ++i2;
    spot = 0;
    step = 0;
    hash = HASH(src[i]);
    do {
      spot = (hash + step * ((step + 1) >> 1)) & (n2 - 1);
      unassert(src2[spot] != src[i]);
      ++step;
    } while (src2[spot]);
    src2[spot] = src[i];
    dst2[spot] = dst[i];
  }
  unassert(!i1);
  edges->i = i2;
  edges->n = n2;
  edges->src = src2;
  edges->dst = dst2;
  Free(src);
  Free(dst);
  return n2;
}

static bool AddEdge(struct JitEdges *edges, i64 src, i64 dst) {
  int s;
  if (edges->i == (edges->n >> 1)) {
    if (!GrowEdges(edges)) return false;
  }
  if (!edges->src[(s = GetEdge(edges, src))]) {
    edges->src[s] = src;
    ++edges->i;
  }
  if (!edges->dst[s]) {
    if (!(edges->dst[s] = NewInts(&edges->jia))) return false;
  }
  return AddInt(edges->dst[s], dst);
}

static void RemoveEdgesByIndex(struct JitEdges *edges, int s) {
  unassert(s >= 0 && s < edges->n);
  FreeInts(&edges->jia, edges->dst[s]);
  edges->dst[s] = 0;
}

static bool RemoveEdges(struct JitEdges *edges, i64 src) {
  int s;
  if (!edges->dst[(s = GetEdge(edges, src))]) return false;
  RemoveEdgesByIndex(edges, s);
  return true;
}

static bool RemoveEdge(struct JitEdges *edges, i64 src, i64 dst) {
  int s;
  if (!edges->dst[(s = GetEdge(edges, src))]) return false;
  if (!RemoveInt(edges->dst[s], dst)) return false;
  if (!edges->dst[s]->i) RemoveEdgesByIndex(edges, s);
  return true;
}

static void ClearEdges(struct JitEdges *edges) {
  int i;
  for (i = 0; i < edges->n; ++i) {
    edges->src[i] = 0;
    RemoveEdgesByIndex(edges, i);
  }
  edges->i = 0;
}

static bool IsCyclic(struct JitEdges *edges, i64 V[kJitDepth], int d, i64 dst) {
  int i, s;
  if (d == kJitDepth) {
    return true;
  }
  for (i = 0; i < d; ++i) {
    if (dst == V[i]) {
      return true;
    }
  }
  V[d++] = dst;
  if (edges->dst[(s = GetEdge(edges, dst))]) {
    for (i = 0; i < edges->dst[s]->i; ++i) {
      if (IsCyclic(edges, V, d, edges->dst[s]->p[i])) {
        return true;
      }
    }
  }
  return false;
}

static inline uintptr_t DecodeJitFunc(int func) {
  unassert(func);
  return (intptr_t)IMAGE_END + func;
}

static nosideeffect int EncodeJitFunc(intptr_t addr) {
  int func;
  intptr_t base, disp;
  if (addr) {
    base = (intptr_t)IMAGE_END;
    disp = addr - base;
    func = disp;
    unassert(func && func == disp);
  } else {
    func = 0;
  }
  return func;
}

bool CanJitForImmediateEffect(void) {
  return atomic_load_explicit(&g_jit.prot, memory_order_relaxed) & PROT_EXEC;
}

static u8 *AllocateJitMemory(long *state) {
  long i, brk;
  uintptr_t p;
  brk = *state;
  p = (uintptr_t)g_code;
  i = ROUNDUP(p + brk, FLAG_pagesize) - p;
  if (i + kJitBlockSize > kJitMemorySize) {
    return 0;
  }
  *state = i + kJitBlockSize;
  return g_code + i;
}

static int MakeJitJump(u8 buf[5], uintptr_t pc, uintptr_t addr) {
  int n;
  intptr_t disp;
#if defined(__x86_64__)
  disp = addr - (pc + 5);
  unassert(kAmdDispMin <= disp && disp <= kAmdDispMax);
  buf[0] = kAmdJmp;
  Write32(buf + 1, disp & kAmdDispMask);
  n = 5;
#elif defined(__aarch64__)
  disp = addr - pc;
  disp >>= 2;
  unassert(kArmDispMin <= disp && disp <= kArmDispMax);
  Write32(buf, kArmJmp | (disp & kArmDispMask));
  n = 4;
#endif
  return n;
}

// Obtains JitBlock from global pool or creates one if none exist.
static struct JitBlock *AcquireJitBlock(struct Jit *jit) {
  struct Dll *e;
  struct JitBlock *jb;
  LOCK(&g_jit.lock);
  if ((e = dll_first(g_jit.freeblocks))) {
    dll_remove(&g_jit.freeblocks, e);
    jb = JITBLOCK_CONTAINER(e);
    unassert(g_jit.freecount > 0);
    --g_jit.freecount;
  } else {
    jb = 0;
  }
  UNLOCK(&g_jit.lock);
  if (jb) dll_make_last(&jit->agedblocks, &jb->aged);
  JIT_LOGF("acquired jit block %p (freecount=%d)", jb, g_jit.freecount);
  return jb;
}

// Frees JitBlock. The JIT block is added to a global free list, so it
// can be reclaimed if a new Jit system is created. This is intended for
// once all threads have shut down, due to exit_group() or execve().
// @assume jit->lock
static void ReleaseJitBlock(struct JitBlock *jb) {
  struct Dll *e;
  JIT_LOGF("released jit block %p", jb);
  while ((e = dll_first(jb->staged))) {
    dll_remove(&jb->staged, e);
    FreeJitStage(JITSTAGE_CONTAINER(e));
  }
  dll_make_first(&jb->freejumps, jb->jumps);
  jb->jumps = 0;
  jb->start = 0;
  jb->index = 0;
  jb->committed = 0;
  jb->wasretired = false;
  jb->isprotected = false;
  dll_init(&jb->aged);
  LOCK(&g_jit.lock);
  dll_make_first(&g_jit.freeblocks, &jb->elem);
  ++g_jit.freecount;
  UNLOCK(&g_jit.lock);
}

// Frees JitBlock and adds it to the global free list in such a way that
// it'll take a long time before it's reused. This is intended for a JIT
// under active use that's trying to reclaim jit memory.
// @assume jit->lock
static void RetireJitBlock(struct Jit *jit, struct JitBlock *jb) {
  JIT_LOGF("retiring jit block %p", jb);
  unassert(!jb->isprotected);
  unassert(dll_is_empty(jb->jumps));
  unassert(dll_is_empty(jb->staged));
  STATISTIC(++jit_blocks_retired);
  dll_remove(&jit->blocks, &jb->elem);
  dll_remove(&jit->agedblocks, &jb->aged);
  jb->start = 0;
  jb->index = 0;
  jb->committed = 0;
  jb->wasretired = true;
  LOCK(&g_jit.lock);
  dll_make_last(&g_jit.freeblocks, &jb->elem);
  ++g_jit.freecount;
  UNLOCK(&g_jit.lock);
}

// creates new jit block and sets up its jit memory
static struct JitBlock *InitJitBlock(struct Jit *jit, long *state) {
  struct JitBlock *jb;
  if ((jb = NewJitBlock())) {
    if (!(jb->addr = AllocateJitMemory(state))) {
      FreeJitBlock(jb);
      jb = 0;
    }
  }
  return jb;
}

static void LockJit(struct Jit *jit) {
  if (jit->threaded) {
    LOCK(&jit->lock);
  }
}

static void UnlockJit(struct Jit *jit) {
  if (jit->threaded) {
    UNLOCK(&jit->lock);
  }
}

/**
 * Initializes memory object for Just-In-Time (JIT) threader.
 *
 * The `jit` struct itself is owned by the caller. Internal memory
 * associated with this object should be reclaimed later by calling
 * DestroyJit().
 *
 * @return 0 on success
 */
int InitJit(struct Jit *jit, uintptr_t opt_staging_function) {
  long brk;
  unsigned n;
  struct JitBlock *jb;
  _Atomic(int) *funcs;
  _Atomic(uintptr_t) *virts;
  _Static_assert(kJitAlign >= 1, "");
  _Static_assert(kJitBlockSize >= 4096, "");
  _Static_assert(kJitInitialHooks >= 2, "");
  unassert(FLAG_pagesize >= 4096);
  unassert(kJitBlockSize >= FLAG_pagesize);
  unassert(!(kJitBlockSize % FLAG_pagesize));
  memset(jit, 0, sizeof(*jit));
  InitEdges(&jit->edges);
  InitEdges(&jit->redges);
  jit->staging = EncodeJitFunc(opt_staging_function);
  unassert(!pthread_mutex_init(&jit->lock, 0));
  jit->hooks.n = n = RoundupTwoPow(kJitInitialHooks);
  unassert(virts = (_Atomic(uintptr_t) *)Calloc(n, sizeof(*virts)));
  unassert(funcs = (_Atomic(int) *)Calloc(n, sizeof(*funcs)));
  atomic_store_explicit(&jit->hooks.virts, virts, memory_order_relaxed);
  atomic_store_explicit(&jit->hooks.funcs, funcs, memory_order_relaxed);
  for (brk = 0; (jb = InitJitBlock(jit, &brk));) {
    dll_make_last(&g_jit.freeblocks, &jb->elem);
    ++g_jit.freecount;
  }
  JIT_LOGF("initialized jit %p", jit);
  return 0;
}

/**
 * Destroys initialized JIT object.
 *
 * Passing a value not previously initialized by InitJit() is undefined.
 *
 * @return 0 on success
 */
int DestroyJit(struct Jit *jit) {
  struct Dll *e, *e2;
  LockJit(jit);
  JIT_LOGF("destroying jit %p", jit);
  for (e = dll_first(jit->freeds.p); e; e = e2) {
    e2 = dll_next(jit->freeds.p, e);
    FreeJitFreed(JITFREED_CONTAINER(e));
  }
  while ((e = dll_first(jit->blocks))) {
    dll_remove(&jit->blocks, e);
    ReleaseJitBlock(JITBLOCK_CONTAINER(e));
  }
  dll_make_first(&jit->freejumps, jit->jumps);
  for (e = dll_first(jit->freejumps); e; e = e2) {
    e2 = dll_next(jit->freejumps, e);
    FreeJitJump(JITJUMP_CONTAINER(e));
  }
  for (e = dll_first(jit->pages); e; e = e2) {
    e2 = dll_next(jit->pages, e);
    FreeJitPage(JITPAGE_CONTAINER(e));
  }
  UnlockJit(jit);
  unassert(!pthread_mutex_destroy(&jit->lock));
  DestroyEdges(&jit->redges);
  DestroyEdges(&jit->edges);
  Free(jit->hooks.funcs);
  Free(jit->hooks.virts);
  return 0;
}

/**
 * Releases global JIT resources at shutdown.
 */
int ShutdownJit(void) {
  struct Dll *e;
  JIT_LOGF("shutting down jit");
  while ((e = dll_first(g_jit.freeblocks))) {
    dll_remove(&g_jit.freeblocks, e);
    FreeJitBlock(JITBLOCK_CONTAINER(e));
    --g_jit.freecount;
  }
  unassert(!g_jit.freecount);
  return 0;
}

/**
 * Disables Just-In-Time threader.
 */
int DisableJit(struct Jit *jit) {
  atomic_store_explicit(&jit->disabled, true, memory_order_release);
  return 0;
}

/**
 * Enables Just-In-Time threader.
 */
int EnableJit(struct Jit *jit) {
  atomic_store_explicit(&jit->disabled, false, memory_order_release);
  return 0;
}

/**
 * Fixes the memory protection for existing Just-In-Time code blocks.
 */
int FixJitProtection(struct Jit *jit) {
  int prot;
  struct Dll *e;
  LockJit(jit);
  prot = atomic_load_explicit(&g_jit.prot, memory_order_relaxed);
  for (e = dll_first(jit->blocks); e; e = dll_next(jit->blocks, e)) {
    unassert(
        !Mprotect(JITBLOCK_CONTAINER(e)->addr, kJitBlockSize, prot, "jit"));
  }
  UnlockJit(jit);
  return 0;
}

// @assume jit->lock
static struct JitPage *GetJitPage(struct Jit *jit, i64 addr) {
  i64 page;
  bool lru;
  struct Dll *e;
  struct JitPage *jp;
  lru = false;
  page = addr & -4096;
  for (e = dll_first(jit->pages); e; e = dll_next(jit->pages, e)) {
    jp = JITPAGE_CONTAINER(e);
    if (jp->page == page) {
      if (!lru) {
        STATISTIC(++jit_pages_hits_1);
      } else {
        STATISTIC(++jit_pages_hits_2);
        dll_remove(&jit->pages, e);
        dll_make_first(&jit->pages, e);
      }
      return jp;
    }
    lru = true;
  }
  return 0;
}

// @assume jit->lock
static struct JitPage *GetOrCreateJitPage(struct Jit *jit, i64 addr) {
  i64 page;
  struct JitPage *jp;
  page = addr & -4096;
  if (!(jp = GetJitPage(jit, page))) {
    if ((jp = NewJitPage())) {
      dll_make_first(&jit->pages, &jp->elem);
      jp->page = page;
    }
  }
  return jp;
}

// adds heap memory to freelist
// this is intended for synchronization cooloff
// @assume jit->lock
static void RetireJitHeap(struct Jit *jit, void *data, size_t size) {
  struct Dll *e;
  struct JitFreed *jf = 0;
  if (!data) return;
  if (!jit->threaded) {
    Free(data);
    return;
  }
  if ((e = dll_first(jit->freeds.f))) {
    dll_remove(&jit->freeds.f, e);
    jf = JITFREED_CONTAINER(e);
  } else if (!(jf = NewJitFreed())) {
    return;  // we can't free data so just leak it
  }
  jf->data = data;
  jf->size = size;
  dll_make_last(&jit->freeds.p, &jf->elem);
  ++jit->freeds.n;
}

// same as calloc, but pilfers old retired heap memory from freelist
// @assume jit->lock
static void *GetJitHeap(struct Jit *jit, size_t count, size_t elsize) {
  u64 size;
  void *res = 0;
  struct Dll *e;
  struct JitFreed *jf;
  if (CheckedMul(count, elsize, &size)) return 0;
  if (jit->freeds.n > kJitRetireQueue) {
    for (e = dll_first(jit->freeds.p); e; e = dll_next(jit->freeds.p, e)) {
      dll_remove(&jit->freeds.p, e);
      jf = JITFREED_CONTAINER(e);
      if (jf->size >= size) {
        res = jf->data;
        dll_make_first(&jit->freeds.f, e);
        break;
      }
    }
  }
  if (res) {
    memset(res, 0, size);
  } else {
    res = Calloc(1, size);
  }
  return res;
}

// @assume jit->lock
static unsigned RehashJitHooks(struct Jit *jit) {
  int func;
  uintptr_t key, virt;
  unsigned i, i2, n1, n2, used, hash, spot, step, kgen;
  _Atomic(int) *funcs, *funcs2;
  _Atomic(uintptr_t) *virts, *virts2;
  virts = atomic_load_explicit(&jit->hooks.virts, memory_order_relaxed);
  funcs = atomic_load_explicit(&jit->hooks.funcs, memory_order_relaxed);
  // grow allocation unless this rehash is due to many deleted values
  n1 = atomic_load_explicit(&jit->hooks.n, memory_order_relaxed);
  unassert(n1 > 1 && IS2POW(n1));
  for (used = i = 0; i < n1; ++i) {
    used += !!atomic_load_explicit(funcs + i, memory_order_relaxed);
  }
  n2 = n1 << (used > (n1 >> 2));
  JIT_LOGF("rehashing jit hooks %u -> %u", n1, n2);
  // allocate an entirely new hash table
  if (!(virts2 = (_Atomic(uintptr_t) *)GetJitHeap(jit, n2, sizeof(*virts2))) ||
      !(funcs2 = (_Atomic(int) *)GetJitHeap(jit, n2, sizeof(*funcs2)))) {
    RetireJitHeap(jit, virts2, n2 * sizeof(*virts2));
    return 0;
  }
  // copy entries over to new hash table, removing deleted entries
  for (i2 = i = 0; i < n1; ++i) {
    virt = atomic_load_explicit(virts + i, memory_order_relaxed);
    func = atomic_load_explicit(funcs + i, memory_order_relaxed);
    if (virt && func) {
      spot = 0;
      step = 0;
      hash = HASH(virt);
      do {
        spot = (hash + step * ((step + 1) >> 1)) & (n2 - 1);
        key = atomic_load_explicit(virts2 + spot, memory_order_relaxed);
        unassert(key != virt);
        ++step;
      } while (key);
      atomic_store_explicit(virts2 + spot, virt, memory_order_relaxed);
      atomic_store_explicit(funcs2 + spot, func, memory_order_relaxed);
      ++i2;
    }
  }
  // update the hash table pointers for the lockless reader
  kgen = BeginUpdate(&jit->keygen);
  atomic_store_explicit(&jit->hooks.virts, virts2, memory_order_release);
  atomic_store_explicit(&jit->hooks.funcs, funcs2, memory_order_relaxed);
  atomic_store_explicit(&jit->hooks.n, n2, memory_order_release);
  EndUpdate(&jit->keygen, kgen);
  // leak old table so failed reads won't segfault from free munmap
  RetireJitHeap(jit, virts, n1 * sizeof(*virts));
  RetireJitHeap(jit, funcs, n1 * sizeof(*funcs));
  jit->hooks.i = i2;
  return n2;
}

// @assume jit->lock
static bool SetJitHookUnlocked(struct Jit *jit, u64 virt, int cas,
                               intptr_t funcaddr) {
  uintptr_t key;
  int func, oldfunc;
  struct JitPage *jp;
  _Atomic(int) *funcs;
  _Atomic(uintptr_t) *virts;
  unsigned n, kgen, hash, spot, step;
  unassert(virt);
  // ensure there's room to add this hook
  unassert(jit->hooks.i <= jit->hooks.n / 2);
  n = atomic_load_explicit(&jit->hooks.n, memory_order_relaxed);
  if (jit->hooks.i == n / 2 && !(n = RehashJitHooks(jit))) {
    DisableJit(jit);
    return false;
  }
  // probe for spot in hash table. this is guaranteed to halt since we
  // never place more than jit->hooks.n/2 items within this hash table
  spot = 0;
  step = 0;
  hash = HASH(virt);
  virts = atomic_load_explicit(&jit->hooks.virts, memory_order_relaxed);
  funcs = atomic_load_explicit(&jit->hooks.funcs, memory_order_relaxed);
  do {
    spot = (hash + step * ((step + 1) >> 1)) & (n - 1);
    key = atomic_load_explicit(virts + spot, memory_order_relaxed);
    ++step;
  } while (key && key != virt);
  func = EncodeJitFunc(funcaddr);
  oldfunc = atomic_load_explicit(funcs + spot, memory_order_relaxed);
  if (jit->staging) {
    if (func == jit->staging) {
      STATISTIC(++jit_hooks_staged);
      if (key && oldfunc != jit->staging) {
        STATISTIC(++jit_hooks_deleted);
      }
    } else {
      if (key && cas && oldfunc != cas) {
        // if staging is enabled and the hook isn't the staging address,
        // then some other thread must have won the race to install this
        return false;
      }
      STATISTIC(--jit_hooks_staged);
      if (func) {
        STATISTIC(++jit_hooks_installed);
      }
    }
  } else {
    if (key && oldfunc) {
      STATISTIC(++jit_hooks_deleted);
    }
    if (func) {
      STATISTIC(++jit_hooks_installed);
    }
  }
  if (!key) {
    ++jit->hooks.i;
    STATISTIC(jit_hash_elements = MAX(jit_hash_elements, jit->hooks.i));
  }
  if (func && (jp = GetOrCreateJitPage(jit, virt))) {
    jp->bitset |= (u64)1 << ((virt & 4095) >> 6);
  }
  kgen = BeginUpdate(&jit->keygen);
  atomic_store_explicit(virts + spot, virt, memory_order_release);
  atomic_store_explicit(funcs + spot, func, memory_order_relaxed);
  EndUpdate(&jit->keygen, kgen);
  return true;
}

static bool SetJitHook(struct Jit *jit, u64 virt, int cas, intptr_t funcaddr) {
  bool res;
  LockJit(jit);
  res = SetJitHookUnlocked(jit, virt, cas, funcaddr);
  UnlockJit(jit);
  return res;
}

/**
 * Retrieves native function for executing virtual address.
 *
 * @param jit is the System's Jit object
 * @param virt is the hash table key, or virtual address of path start
 * @return native function address, or 0 if it doesn't exist
 */
uintptr_t GetJitHook(struct Jit *jit, u64 virt) {
  int off;
  uintptr_t key, res;
  _Atomic(int) *funcs;
  _Atomic(uintptr_t) *virts;
  unsigned n, kgen, hash, spot, step;
  COSTLY_STATISTIC(++jit_hash_lookups);
  hash = HASH(virt);
  do {
    kgen = atomic_load_explicit(&jit->keygen, memory_order_relaxed);
    n = atomic_load_explicit(&jit->hooks.n, memory_order_relaxed);
    virts = atomic_load_explicit(&jit->hooks.virts, memory_order_acquire);
    for (spot = step = 0;; ++step) {
      spot = (hash + step * ((step + 1) >> 1)) & (n - 1);
      key = atomic_load_explicit(virts + spot, memory_order_acquire);
      if (key == virt) {
        funcs = atomic_load_explicit(&jit->hooks.funcs, memory_order_relaxed);
        off = atomic_load_explicit(funcs + spot, memory_order_relaxed);
        res = off ? DecodeJitFunc(off) : 0;
        break;
      }
      if (!key) {
        return 0;
      }
      COSTLY_STATISTIC(++jit_hash_collisions);
    }
  } while (ShallNotPass(kgen, &jit->keygen));
  return res;
}

// removes hook and edges for jit path and all paths that depend on it
// @assume jit->lock
static void DeleteJitPath(struct Jit *jit, i64 virt) {
  i64 dep;
  uintptr_t key;
  int i, s, old;
  _Atomic(int) *funcs;
  _Atomic(uintptr_t) *virts;
  unsigned n, hash, spot, step;
  // delete hook for this path from hash table
  hash = HASH(virt);
  for (spot = step = 0;; ++step) {
    n = atomic_load_explicit(&jit->hooks.n, memory_order_relaxed);
    spot = (hash + step * ((step + 1) >> 1)) & (n - 1);
    virts = atomic_load_explicit(&jit->hooks.virts, memory_order_relaxed);
    key = atomic_load_explicit(virts + spot, memory_order_relaxed);
    if (!key) return;
    if ((i64)key == virt) {
      JIT_LOGF("deleting jit hook for path starting at %#" PRIx64, virt);
      funcs = atomic_load_explicit(&jit->hooks.funcs, memory_order_relaxed);
      old = atomic_load_explicit(funcs + spot, memory_order_relaxed);
      if (old) {
        atomic_store_explicit(funcs + spot, 0, memory_order_release);
        if (old == jit->staging) {
          STATISTIC(--jit_hooks_staged);
        } else {
          STATISTIC(--jit_hooks_installed);
          STATISTIC(++jit_hooks_deleted);
        }
      }
      break;
    }
  }
  // delete paths that point to this path
  while (jit->redges.dst[(s = GetEdge(&jit->redges, virt))] &&
         jit->redges.dst[s]->i) {
    dep = jit->redges.dst[s]->p[jit->redges.dst[s]->i - 1];
    JIT_LOGF("jit path %#" PRIx64 " depends on %#" PRIx64, dep, virt);
    DeleteJitPath(jit, dep);
  }
  // delete edges associated with this path from bimap
  if (jit->edges.dst[(s = GetEdge(&jit->edges, virt))]) {
    for (i = jit->edges.dst[s]->i; i--;) {
      RemoveEdge(&jit->redges, jit->edges.dst[s]->p[i], virt);
    }
    RemoveEdgesByIndex(&jit->edges, s);
  }
}

// @assume jit->lock
static void ResetJitPageHooks(struct Jit *jit, i64 page) {
  i64 virt;
  unsigned i, boff;
  struct JitPage *jp;
  if (!(jp = GetJitPage(jit, page))) return;
  STATISTIC(AVERAGE(jit_page_average_bits, popcount(jp->bitset)));
  while (jp->bitset) {
    boff = bsr(jp->bitset);
    virt = page + boff * (4096 / 64);
    jp->bitset &= ~((u64)1 << boff);
    for (i = 0; i < 64; ++i) {
      DeleteJitPath(jit, virt + i);
    }
  }
  dll_remove(&jit->pages, &jp->elem);
  FreeJitPage(jp);
}

// @assume jit->lock
static int ResetJitPageUnlocked(struct Jit *jit, i64 virt) {
  i64 page;
  unsigned gen;
  page = virt & -4096;
  STATISTIC(++jit_page_resets);
  JIT_LOGF("resetting jit page %#" PRIx64, page);
  gen = BeginUpdate(&jit->pagegen);
  ResetJitPageHooks(jit, page);
  dll_make_first(&jit->freejumps, jit->jumps);
  jit->jumps = 0;
  EndUpdate(&jit->pagegen, gen);
  return 0;
}

/**
 * Clears JIT paths installed to memory page.
 *
 * This is intended to be called when functions like mmap(), munmap(),
 * and mprotect() cause a memory page to either disappear or lose exec
 * permissions.
 *
 * @param virt is virtual address of 4096-byte page (needn't be aligned)
 * @return 0 on success, or -1 w/ errno
 */
int ResetJitPage(struct Jit *jit, i64 virt) {
  int res;
  if (IsJitDisabled(jit)) return einval();
  LockJit(jit);
  res = ResetJitPageUnlocked(jit, virt);
  UnlockJit(jit);
  return res;
}

// @assume jit->lock
static void ForceJitBlocksToRetire(struct Jit *jit) {
  int i;
  struct Dll *e, *e2;
  struct JitBlock *jb;
  unsigned n, pgen, kgen;
  JIT_LOGF("retiring jit blocks to avoid oom");
  dll_make_first(&jit->freejumps, jit->jumps);
  jit->jumps = 0;
  pgen = BeginUpdate(&jit->pagegen);
  for (e = dll_first(jit->agedblocks); e; e = e2) {
    e2 = dll_next(jit->agedblocks, e);
    jb = AGEDBLOCK_CONTAINER(e);
    if (!jb->isprotected) {
      JIT_LOGF("forcing jit block %p to retire", jb);
      RetireJitBlock(jit, jb);
    }
  }
  n = atomic_load_explicit(&jit->hooks.n, memory_order_relaxed);
  for (i = 0; i < n; ++i) {
    if (atomic_load_explicit(jit->hooks.virts + i, memory_order_relaxed)) {
      kgen = BeginUpdate(&jit->keygen);
      atomic_store_explicit(jit->hooks.virts + i, 0, memory_order_release);
      atomic_store_explicit(jit->hooks.funcs + i, 0, memory_order_relaxed);
      EndUpdate(&jit->keygen, kgen);
    }
  }
  jit->hooks.i = 0;
  ClearEdges(&jit->redges);
  ClearEdges(&jit->edges);
  EndUpdate(&jit->pagegen, pgen);
}

static bool CheckMmapResult(void *want, void *got) {
  if (got == MAP_FAILED) {
    LOGF("failed to mmap() jit block: %s", DescribeHostErrno(errno));
    return false;
  }
  if (got != want) {
    LOGF("jit block mmap(%p) returned unexpected address %p: %s", want, got,
         DescribeHostErrno(errno));
    return false;
  }
  return true;
}

static bool PrepareJitMemory(void *addr, size_t size) {
#ifdef MAP_JIT
  // Apple M1 only permits RWX memory if we use MAP_JIT, which Apple has
  // chosen to make incompatible with MAP_FIXED.
  if (Munmap(addr, size)) {
    LOGF("failed to munmap() jit block: %s", DescribeHostErrno(errno));
    return false;
  }
  return CheckMmapResult(
      addr, Mmap(addr, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                 MAP_JIT | MAP_PRIVATE | MAP_ANONYMOUS_, -1, 0, "jit"));
#else
  int prot;
  prot = atomic_load_explicit(&g_jit.prot, memory_order_relaxed);
  if (!Mprotect(addr, size, prot, "jit")) {
    return true;
  }
  if (~prot & PROT_EXEC) {
    LOGF("failed to mprotect() jit block: %s", DescribeHostErrno(errno));
    return false;
  }
  // OpenBSD imposes a R^X invariant and raises ENOTSUP if RWX
  // memory is requested. Since other OSes might exist, having
  // this same requirement, and possible a different errno, we
  // shall just clear the exec flag and try again.
  prot &= ~PROT_EXEC;
  atomic_store_explicit(&g_jit.prot, prot, memory_order_relaxed);
  return CheckMmapResult(
      addr, Mmap(addr, size, prot, MAP_PRIVATE | MAP_ANONYMOUS_ | MAP_FIXED, -1,
                 0, "jit"));
#endif
}

/**
 * Begins writing function definition to JIT memory.
 *
 * This will acquire a block of JIT memory. Code may be added to the
 * function using methods like AppendJitPrologue() and AppendJitCall().
 * When a chunk is completed, FinishJit() should be called. The calling
 * thread is granted exclusive ownership of the returned block of JIT
 * memory, until it's relinquished by FinishJit().
 *
 * @param opt_virt is hash key for finished function, or 0 for manual
 * @return function builder object
 */
struct JitBlock *StartJit(struct Jit *jit, i64 opt_virt) {
  struct Dll *e;
  struct JitBlock *jb;
  if (!IsJitDisabled(jit)) {
    LockJit(jit);
    if ((e = dll_first(jit->blocks)) &&  //
        (jb = JITBLOCK_CONTAINER(e)) &&  //
        jb->index + kJitFit <= kJitBlockSize) {
      // we found a block with adequate free space owned by jit
      dll_remove(&jit->blocks, &jb->elem);
    } else {
      if (g_jit.freecount <= kJitRetireQueue) {
        ForceJitBlocksToRetire(jit);
      }
      if (!(jb = AcquireJitBlock(jit))) {
        LOG_ONCE(LOGF("ran out of jit memory"));
      } else if (!PrepareJitMemory(jb->addr, kJitBlockSize)) {
        // this system isn't allowing us to use jit memory
        dll_remove(&jit->agedblocks, &jb->aged);
        ReleaseJitBlock(jb);
        DisableJit(jit);
        jb = 0;
      }
    }
    if (jb) {
      dll_make_first(&jb->freejumps, jit->freejumps);
      jit->freejumps = 0;
    }
    UnlockJit(jit);
  } else {
    jb = 0;
  }
  if (jb) {
    jb->virt = opt_virt;
    unassert(!(jb->start & (kJitAlign - 1)));
    unassert(jb->start == jb->index);
    jb->pagegen = atomic_load_explicit(&jit->pagegen, memory_order_acquire);
    if (jb->virt && jit->staging) {
      unassert(SetJitHook(jit, jb->virt, 0, DecodeJitFunc(jit->staging)));
    } else {
      JIT_LOGF("marking jit block %p as protected due to manual mode", jb);
      jb->isprotected = true;
    }
    if (pthread_jit_write_protect_supported_np()) {
      pthread_jit_write_protect_np_workaround(false);
    }
  }
  return jb;
}

static bool OomJit(struct JitBlock *jb) {
  jb->index = kJitBlockSize + 1;
  return false;
}

/**
 * Appends bytes to JIT block.
 *
 * Errors here safely propagate to FinishJit().
 *
 * @return true if room was available, otherwise false
 */
inline bool AppendJit(struct JitBlock *jb, const void *data, long size) {
  unassert(size > 0);
  jb->lastaction = 0;
  if (size <= GetJitRemaining(jb)) {
    memcpy(jb->addr + jb->index, data, size);
    jb->index += size;
    return true;
  } else {
    return OomJit(jb);
  }
}

static struct Dll *GetJitJumps(struct Jit *jit, struct JitBlock *jb, u64 virt) {
  struct JitJump *jj;
  struct Dll *res, *rem, *e, *e2;
  LockJit(jit);
  for (rem = res = 0, e = dll_first(jit->jumps); e; e = e2) {
    e2 = dll_next(jit->jumps, e);
    jj = JITJUMP_CONTAINER(e);
    if (jj->virt == virt) {
      dll_remove(&jit->jumps, e);
      dll_make_first(&res, e);
    } else if (++jj->tries == kJitJumpTries) {
      dll_remove(&jit->jumps, e);
      dll_make_first(&rem, e);
    }
  }
  UnlockJit(jit);
  dll_make_first(&jb->freejumps, rem);
  return res;
}

static void FixupJitJumps(struct JitBlock *jb, struct Dll *list,
                          uintptr_t addr) {
  int n;
  union {
    u32 i;
    u64 q;
    u8 b[8];
  } u;
  struct Dll *e;
  struct JitJump *jj;
  for (e = dll_first(list); e; e = dll_next(list, e)) {
    STATISTIC(++jumps_applied);
    STATISTIC(++path_connected_directly);
    jj = JITJUMP_CONTAINER(e);
    u.q = 0;
    n = MakeJitJump(u.b, (uintptr_t)jj->code, addr + jj->addend);
    unassert(!((uintptr_t)jj->code & 3));
#if defined(__aarch64__)
    atomic_store_explicit((_Atomic(u32) *)jj->code, u.i, memory_order_release);
#elif defined(__x86_64__)
    u64 old, neu;
    old = atomic_load_explicit((_Atomic(u64) *)jj->code, memory_order_relaxed);
    do {
      neu = (old & 0xffffff0000000000) | u.q;
    } while (!atomic_compare_exchange_weak_explicit(
        (_Atomic(u64) *)jj->code, &old, neu, memory_order_release,
        memory_order_relaxed));
#else
#error "not supported"
#endif
    sys_icache_invalidate(jj->code, n);
  }
  dll_make_first(&jb->freejumps, list);
}

static bool UpdateJitHook(struct Jit *jit, struct JitBlock *jb, u64 virt,
                          uintptr_t funcaddr) {
  struct Dll *jumps;
  unassert(funcaddr);
  jumps = GetJitJumps(jit, jb, virt);
  if (SetJitHook(jit, virt, jit->staging, funcaddr)) {
    FixupJitJumps(jb, jumps, funcaddr);
    return true;
  } else {
    dll_make_first(&jb->freejumps, jumps);
    return false;
  }
}

static void AbandonJitHook(struct Jit *jit, u64 virt) {
  if (virt && jit->staging) {
    SetJitHook(jit, virt, 0, 0);
  }
}

// mprotects jit memory if a system page worth of code was generated
// @assume jit->lock
int CommitJit_(struct Jit *jit, struct JitBlock *jb) {
  u8 *addr;
  size_t size;
  int count = 0;
  long blockoff;
  struct JitJump *jj;
  struct JitStage *js;
  struct Dll *e, *e2, *rem;
  unassert(jb->start == jb->index);
  unassert(!(jb->committed & (FLAG_pagesize - 1)));
  if (!CanJitForImmediateEffect() &&
      (blockoff = ROUNDDOWN(jb->start, FLAG_pagesize)) > jb->committed) {
    addr = jb->addr + jb->committed;
    size = blockoff - jb->committed;
    JIT_LOGF("jit activating [%p,%p) w/ %zu kb", addr, addr + size,
             size / 1024);
    // abandon fixups pointing into the block being protected
    LockJit(jit);
    for (rem = 0, e = dll_first(jit->jumps); e; e = e2) {
      e2 = dll_next(jit->jumps, e);
      jj = JITJUMP_CONTAINER(e);
      if (MAX(jj->code, addr) < MIN(jj->code + 5, addr + size)) {
        dll_remove(&jit->jumps, e);
        dll_make_first(&rem, e);
      }
    }
    UnlockJit(jit);
    for (e = dll_first(rem); e; e = e2) {
      e2 = dll_next(rem, e);
      FreeJitJump(JITJUMP_CONTAINER(e));
    }
    // ask system to change the page memory protections
    unassert(!Mprotect(addr, size, PROT_READ | PROT_EXEC, "jit"));
    // update interpreter hooks so our new jit code goes live
    while ((e = dll_first(jb->staged))) {
      js = JITSTAGE_CONTAINER(e);
      unassert(js->index >= jb->committed);
      if (js->index <= blockoff) {
        if (!ShallNotPass(js->pagegen, &jit->pagegen)) {
          UpdateJitHook(jit, jb, js->virt, (uintptr_t)jb->addr + js->start);
        } else {
          AbandonJitHook(jit, js->virt);
        }
        dll_remove(&jb->staged, e);
        FreeJitStage(js);
        ++count;
      } else {
        break;
      }
    }
    jb->committed = blockoff;
  }
  return count;
}

// puts user leased block back into jit->blocks pool for potential reuse
// @assume jit->lock
void ReinsertJitBlock_(struct Jit *jit, struct JitBlock *jb) {
  unassert(jb->start == jb->index);
  unassert(dll_is_empty(jb->jumps));
  if (jb->index < kJitBlockSize) {
    // there's still memory remaining; reinsert for immediate reuse.
    dll_make_first(&jit->blocks, &jb->elem);
  } else {
    // block has been filled; relegate it to the end of the list.
    // guarantee that staged hooks shall be committed on openbsd.
    _Static_assert(kJitBlockSize % kMaximumAnticipatedPageSize == 0,
                   "unassert(dll_is_empty(jb->staged)) can't pass "
                   "unless kJitBlockSize is divisible by page size");
    unassert(dll_is_empty(jb->staged));
    dll_make_last(&jit->blocks, &jb->elem);
  }
}

// append our list of code fixups to jit system which may apply it later
static void CommitJitJumps(struct Jit *jit, struct JitBlock *jb) {
  if (!dll_is_empty(jb->jumps)) {
    LockJit(jit);
    dll_make_first(&jit->jumps, jb->jumps);
    jb->jumps = 0;
    UnlockJit(jit);
  }
}

// discards fixups associated with function that couldn't be generated
static void AbandonJitJumps(struct JitBlock *jb) {
  struct Dll *e, *e2;
  for (e = dll_first(jb->jumps); e; e = e2) {
    e2 = dll_next(jb->jumps, e);
    FreeJitJump(JITJUMP_CONTAINER(e));
  }
  jb->jumps = 0;
}

/**
 * Records jump instruction fixup.
 *
 * When a JIT path ends because it's branching into an address for which
 * a JIT path doesn't exist yet, the caller may choose to generated code
 * that falls back into the main interpreter loop while recording a jump
 * fixup, so that if the destination virtual address is generated later,
 * the JIT will atomically self-modify this code to jump to the new one.
 *
 * @param virt is hash table key of destination (should be empty now)
 * @param addend is added to dest function pointer to compute jump
 */
bool RecordJitJump(struct JitBlock *jb, u64 virt, int addend) {
  struct JitJump *jj;
  if (jb->index > kJitBlockSize) return false;
#if defined(__x86_64__)
  unassert(!(GetJitPc(jb) & 7));
#endif
  if (!CanJitForImmediateEffect()) return false;
  if (!(jj = NewJitJump(&jb->freejumps))) return false;
  jj->tries = 0;
  jj->virt = virt;
  jj->code = (u8 *)GetJitPc(jb);
  jj->addend = addend;
  dll_make_first(&jb->jumps, &jj->elem);
  STATISTIC(++jumps_recorded);
  return true;
}

// @assume jit->lock
static bool RecordJitEdgeImpl(struct Jit *jit, i64 src, i64 dst) {
  i64 visits[kJitDepth];
  if (src == dst) return false;
  visits[0] = src;
  if (IsCyclic(&jit->edges, visits, 1, dst)) {
    STATISTIC(++jit_cycles_avoided);
    return false;
  }
  if (!AddEdge(&jit->edges, src, dst)) {
    return false;
  }
  if (!AddEdge(&jit->redges, dst, src)) {
    RemoveEdge(&jit->edges, src, dst);
    return false;
  }
  return true;
}

/**
 * Records JIT edge or returns false if it'd create a cycle.
 */
bool RecordJitEdge(struct Jit *jit, i64 src, i64 dst) {
  bool res;
  LockJit(jit);
  res = RecordJitEdgeImpl(jit, src, dst);
  UnlockJit(jit);
  return res;
}

static void DiscardGeneratedJitCode(struct JitBlock *jb) {
  jb->index = jb->start;
}

/**
 * Finishes writing function definition to JIT memory.
 *
 * Errors that happened earlier in AppendJit*() methods will safely
 * propagate to this function. This function may disable the JIT on
 * errors that aren't recoverable.
 *
 * @param jb is function builder object that was returned by StartJit();
 *     this function always relinquishes the calling thread's ownership
 *     of this object, even if this function returns an error
 * @return true if the function was generated, otherwise false if we ran
 *     out of room in the block while building the function, in which
 *     case the caller should simply try again
 */
bool FinishJit(struct Jit *jit, struct JitBlock *jb) {
  bool ok;
  u8 *addr;
  struct JitStage *js;
  unassert(jb->index > jb->start);
  unassert(jb->start >= jb->committed);
  // check if we lost race with page reset
  if (ShallNotPass(jb->pagegen, &jit->pagegen)) {
    return AbandonJit(jit, jb);
  }
  // align generated functions using breakpoint opcodes
  while (jb->index < kJitBlockSize && (jb->index & (kJitAlign - 1))) {
    unassert(AppendJitTrap(jb));
    unassert(jb->index <= kJitBlockSize);
  }
  if (jb->index <= kJitBlockSize) {
    // function code was generated successfully
    if (jb->virt) {
      // since we have a hash table key we must install the hook
      JIP_LOGF("finishing jit path in block %p at %#" PRIx64, jb, jb->virt);
      if (CanJitForImmediateEffect()) {
        // operating system permits us to use rwx memory
        addr = jb->addr + jb->start;
        sys_icache_invalidate(addr, jb->index - jb->start);
        if (!UpdateJitHook(jit, jb, jb->virt, (uintptr_t)addr)) {
          // we lost race with another thread creating path at same addr
          return AbandonJit(jit, jb);
        }
      } else if ((js = NewJitStage())) {
        // updating hook must be deferred until we've filled system page
        // with code and then change its permission: mprotect(rwx -> rx)
        js->virt = jb->virt;
        js->start = jb->start;
        js->index = jb->index;
        js->pagegen = jb->pagegen;
        dll_make_last(&jb->staged, &js->elem);
      }
    } else {
      JIT_LOGF("finishing manual mode jit path in block %p", jb);
    }
    CommitJitJumps(jit, jb);
    // mark the generated jit memory as having been used
    // if there's only a tiny bit left we advance to end
    if (jb->index + kJitFit > kJitBlockSize) {
      JIT_LOGF("ending jit block %p due to pretty good fit", jb);
      STATISTIC(AVERAGE(jit_average_block, jb->index));
      jb->index = kJitBlockSize;
    }
    jb->start = jb->index;
    ok = true;
  } else {
    // we ran out of jit memory in block while generating the function
    STATISTIC(++path_ooms);
    AbandonJitJumps(jb);
    if (jb->index - jb->start < (kJitBlockSize >> 1)) {
      // we ran out of block space when trying to create a path that's
      // shorter than half the maximum block size. in that case, abandon
      // the path. this will reset the hook on the initial path address.
      // hopefully the next time it's executed, there'll be enough room.
      JIT_LOGF("oom'd jit block %p at %#" PRIx64 " due to lack of room", jb,
               jb->virt);
      AbandonJitHook(jit, jb->virt);
    } else {
      // we ran out of block space trying to create a path that's very
      // long. it's possibly longer than the maximum block size. in that
      // case, just permanently leave the starting address in staging so
      // that we won't try to create this path again.
      JIT_LOGF("oom'd jit block %p at %#" PRIx64 " because long code is long",
               jb, jb->virt);
    }
    DiscardGeneratedJitCode(jb);
    ok = false;
  }
  unassert(jb->start == jb->index);
  CommitJit_(jit, jb);
  LockJit(jit);
  ReinsertJitBlock_(jit, jb);
  if (jb->index >= kJitBlockSize) {
    dll_make_first(&jit->freejumps, jb->freejumps);
    jb->freejumps = 0;
  }
  UnlockJit(jit);
  if (pthread_jit_write_protect_supported_np()) {
    pthread_jit_write_protect_np_workaround(true);
  }
  return ok;
}

/**
 * Abandons writing function definition to JIT memory.
 *
 * @param jb becomes owned by `jit` again after this call
 * @return always false
 */
bool AbandonJit(struct Jit *jit, struct JitBlock *jb) {
  JIT_LOGF("abandoning jit path in block %p at %#" PRIx64, jb, jb->virt);
  STATISTIC(++path_abandoned);
  AbandonJitJumps(jb);
  AbandonJitHook(jit, jb->virt);
  DiscardGeneratedJitCode(jb);
  LockJit(jit);
  ReinsertJitBlock_(jit, jb);
  UnlockJit(jit);
  if (pthread_jit_write_protect_supported_np()) {
    pthread_jit_write_protect_np_workaround(true);
  }
  return false;
}

/**
 * Appends NOPs until PC is aligned to `align`.
 *
 * @param align is byte alignment, which must be a two power
 */
bool AlignJit(struct JitBlock *jb, int align, int misalign) {
  unassert(align > 0 && IS2POW(align));
  unassert(misalign >= 0 && misalign < align);
  while ((jb->index & (align - 1)) != misalign) {
#ifdef __x86_64__
    // Intel's Official Fat NOP Instructions
    //
    //     90                 nop
    //     6690               xchg %ax,%ax
    //     0F1F00             nopl (%rax)
    //     0F1F4000           nopl 0x00(%rax)
    //     0f1f440000         nopl 0x00(%rax,%rax,1)
    //     660f1f440000       nopw 0x00(%rax,%rax,1)
    //     0F1F8000000000     nopl 0x00000000(%rax)
    //     0F1F840000000000   nopl 0x00000000(%rax,%rax,1)
    //     660F1F840000000000 nopw 0x00000000(%rax,%rax,1)
    //
    // See Intel's Six Thousand Page Manual, Volume 2, Table 4-12:
    // "Recommended Multi-Byte Sequence of NOP Instruction".
    static const u8 kNops[7][8] = {
        {1, 0x90},
        {2, 0x66, 0x90},
        {3, 0x0f, 0x1f, 0x00},
        {4, 0x0f, 0x1f, 0x40, 0x00},
        {5, 0x0f, 0x1f, 0x44, 0x00, 0x00},
        {6, 0x66, 0x0f, 0x1f, 0x44, 0x00, 0x00},
        {7, 0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00},
    };
    int skew, need;
    skew = jb->index & (align - 1);
    if (skew > misalign) {
      need = align - skew + misalign;
    } else {
      need = misalign - skew;
    }
    if (!AppendJit(jb, kNops[MIN(7, need) - 1] + 1,
                   kNops[MIN(7, need) - 1][0])) {
      return false;
    }
#else
    if (!AppendJitNop(jb)) {
      return false;
    }
#endif
  }
  unassert((jb->index & (align - 1)) == misalign);
  return true;
}

/**
 * Moves one register's value into another register.
 *
 * The `src` and `dst` register indices are architecture defined.
 * Predefined constants such as `kJitArg0` may be used to provide
 * register indices to this function in a portable way.
 *
 * @param dst is the index of the destination register
 * @param src is the index of the source register
 */
bool AppendJitMovReg(struct JitBlock *jb, int dst, int src) {
  long action;
  if (dst == src) return true;
  if (GetJitRemaining(jb) < 4) return OomJit(jb);
  if ((action = MOVE(dst, src)) == jb->lastaction) return true;
#if defined(__x86_64__)
  unassert(!(dst & ~15));
  unassert(!(src & ~15));
  Write32(jb->addr + jb->index,
          ((kAmdRexw | (src & 8 ? kAmdRexr : 0) | (dst & 8 ? kAmdRexb : 0)) |
           0x89 << 010 | (0300 | (src & 7) << 3 | (dst & 7)) << 020));
  jb->index += 3;
#elif defined(__aarch64__)
  //               src            target
  //              ┌─┴─┐           ┌─┴─┐
  // 0b10101010000000000000001111110011 mov x19, x0
  // 0b10101010000000010000001111110100 mov x20, x1
  // 0b10101010000101000000001111100001 mov x1, x20
  // 0b10101010000100110000001111100000 mov x0, x19
  unassert(!(dst & ~31));
  unassert(!(src & ~31));
  Put32(jb->addr + jb->index, 0xaa0003e0 | src << 16 | dst);
  jb->index += 4;
#endif
  jb->lastaction = action;
  return true;
}

/**
 * Appends function call instruction to JIT memory.
 *
 * @param jb is function builder object returned by StartJit()
 * @param func points to another callee function in memory
 * @return true if room was available, otherwise false
 */
bool AppendJitCall(struct JitBlock *jb, void *func) {
  int n;
  intptr_t disp;
  uintptr_t addr;
  addr = (uintptr_t)func;
#if defined(__x86_64__)
  u8 buf[5];
  disp = addr - (GetJitPc(jb) + 5);
  if (kAmdDispMin <= disp && disp <= kAmdDispMax) {
    // AMD function calls are encoded using an 0xE8 byte, followed by a
    // 32-bit signed two's complement little-endian integer, containing
    // the relative location between the function being called, and the
    // instruction at the location that follows our 5 byte call opcode.
    buf[0] = kAmdCall;
    Write32(buf + 1, disp & kAmdDispMask);
    n = 5;
  } else {
    AppendJitSetReg(jb, kAmdAx, addr);
    buf[0] = kAmdCallAx[0];
    buf[1] = kAmdCallAx[1];
    n = 2;
  }
#elif defined(__aarch64__)
  uint32_t buf[1];
  // ARM function calls are encoded as:
  //
  //       BL          displacement
  //     ┌─┴──┐┌────────────┴───────────┐
  //   0b100101sddddddddddddddddddddddddd
  //
  // Where:
  //
  //   - BL (0x94000000) is what ARM calls its CALL instruction
  //
  //   - sddddddddddddddddddddddddd is a 26-bit two's complement integer
  //     of how far away the function is that's being called. This is
  //     measured in terms of instructions rather than bytes. Unlike AMD
  //     the count here starts at the Program Counter (PC) address where
  //     the BL INSN is stored, rather than the one that follows.
  //
  // Displacement is computed as such:
  //
  //   INSN = BL | (((FUNC - PC) >> 2) & 0x03ffffffu)
  //
  // The inverse of the above operation is:
  //
  //   FUNC = PC + ((i32)((u32)(INSN & 0x03ffffffu) << 6) >> 4)
  //
  disp = addr - GetJitPc(jb);
  disp >>= 2;
  unassert(kArmDispMin <= disp && disp <= kArmDispMax);
  buf[0] = kArmCall | (disp & kArmDispMask);
  n = 4;
#endif
  return AppendJit(jb, buf, n);
}

/**
 * Appends unconditional branch instruction to JIT memory.
 *
 * @param jb is function builder object returned by StartJit()
 * @param code points to some other code address in memory
 * @return true if room was available, otherwise false
 */
bool AppendJitJump(struct JitBlock *jb, void *code) {
  u8 buf[5];
  int n = MakeJitJump(buf, GetJitPc(jb), (uintptr_t)code);
  return AppendJit(jb, buf, n);
}

/**
 * Sets register to immediate value.
 *
 * @param jb is function builder object returned by StartJit()
 * @param param is the zero-based index into the register file
 * @param value is the constant value to use as the parameter
 * @return true if room was available, otherwise false
 */
bool AppendJitSetReg(struct JitBlock *jb, int reg, u64 value) {
  long lastaction;
#if defined(__x86_64__)
  u8 rex = 0;
  if (GetJitRemaining(jb) < 10) return OomJit(jb);
  if (reg & 8) rex |= kAmdRexb;
  if (!value) {
    if (reg & 8) rex |= kAmdRexr;
    if (rex) jb->addr[jb->index++] = rex;
    jb->addr[jb->index++] = kAmdXor;
    jb->addr[jb->index++] = 0300 | (reg & 7) << 3 | (reg & 7);
  } else if ((i64)value < 0 && (i64)value >= INT32_MIN) {
    jb->addr[jb->index++] = rex | kAmdRexw;
    jb->addr[jb->index++] = 0xC7;
    jb->addr[jb->index++] = 0300 | (reg & 7);
    Write32(jb->addr + jb->index, value);
    jb->index += 4;
  } else {
    if (value > 0xffffffff) rex |= kAmdRexw;
    if (rex) jb->addr[jb->index++] = rex;
    jb->addr[jb->index++] = kAmdMovImm | (reg & 7);
    if ((rex & kAmdRexw) != kAmdRexw) {
      Write32(jb->addr + jb->index, value);
      jb->index += 4;
    } else {
      Write64(jb->addr + jb->index, value);
      jb->index += 8;
    }
  }
#elif defined(__aarch64__)
  // ARM immediate moves are encoded as:
  //
  //     ┌64-bit
  //     │
  //     │┌{sign,???,zero,non}-extending
  //     ││
  //     ││       ┌short[4] index
  //     ││       │
  //     ││  MOV  │    immediate   register
  //     │├┐┌─┴──┐├┐┌──────┴───────┐┌─┴─┐
  //   0bmxx100101iivvvvvvvvvvvvvvvvrrrrr
  //
  // Which allows 16 bits to be loaded into a register at a time, with
  // tricks for clearing other parts of the register. For example, the
  // sign-extending mode will set the higher order shorts to all ones,
  // and it expects the immediate to be encoded using ones' complement
  u32 op;
  u32 *p;
  int i, n = 0;
  unassert(!(reg & ~kArmRegMask));
  if (GetJitRemaining(jb) < 16) return OomJit(jb);
  p = (u32 *)(jb->addr + jb->index);
  // TODO: This could be improved some more.
  if ((i64)value < 0 && (i64)value >= -0x8000) {
    p[n++] = kArmMovSex | ~value << kArmImmOff | reg << kArmRegOff;
  } else {
    i = 0;
    op = kArmMovZex;
    while (value && !(value & 0xffff)) {
      value >>= 16;
      ++i;
    }
    do {
      op |= (value & 0xffff) << kArmImmOff;
      op |= reg << kArmRegOff;
      op |= i++ << kArmIdxOff;
      p[n++] = op;
      op = kArmMovNex;
    } while ((value >>= 16));
  }
  jb->index += n * 4;
#endif
  lastaction = jb->lastaction;
  if (ACTION(lastaction) == ACTION_MOVE &&  //
      MOVE_DST(lastaction) != reg &&        //
      MOVE_SRC(lastaction) != reg) {
    jb->lastaction = lastaction;
  }
  return true;
}

/**
 * Appends return instruction.
 */
bool AppendJitRet(struct JitBlock *jb) {
#if defined(__x86_64__)
  u8 buf[1] = {0xc3};
#elif defined(__aarch64__)
  u32 buf[1] = {0xd65f03c0};
#endif
  return AppendJit(jb, buf, sizeof(buf));
}

/**
 * Appends no-op instruction.
 */
bool AppendJitNop(struct JitBlock *jb) {
#if defined(__x86_64__)
  u8 buf[1] = {0x90};  // nop
#elif defined(__aarch64__)
  u32 buf[1] = {0xd503201f};  // nop
#endif
  return AppendJit(jb, buf, sizeof(buf));
}

/**
 * Appends pause instruction.
 */
bool AppendJitPause(struct JitBlock *jb) {
#if defined(__x86_64__)
  u8 buf[2] = {0xf3, 0x90};  // pause
#elif defined(__aarch64__)
  u32 buf[1] = {0xd503203f};  // yield
#endif
  return AppendJit(jb, buf, sizeof(buf));
}

/**
 * Appends debugger breakpoint.
 */
bool AppendJitTrap(struct JitBlock *jb) {
#if defined(__x86_64__)
  u8 buf[1] = {0xcc};  // int3
#elif defined(__aarch64__)
  u32 buf[1] = {0xd4207d00};  // brk #0x3e8
#endif
  return AppendJit(jb, buf, sizeof(buf));
}

#endif /* HAVE_JIT */
