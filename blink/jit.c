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
#include "blink/end.h"
#include "blink/endian.h"
#include "blink/jit.h"
#include "blink/lock.h"
#include "blink/log.h"
#include "blink/macros.h"
#include "blink/map.h"
#include "blink/tsan.h"
#include "blink/util.h"

#if (defined(__x86_64__) || defined(__aarch64__)) && \
    !__has_feature(memory_sanitizer)

/**
 * @fileoverview Just-In-Time Function Threader
 *
 * This file implements an abstraction for assembling executable code at
 * runtime. This is intended to be used in cases where it's desirable to
 * have fast "threaded" pathways, between existing functions, which were
 * compiled statically. We need this because virtual machine dispatching
 * isn't very fast, when it's implemented by loops or indirect branches.
 * Modern CPUs go much faster if glue code without branches is outputted
 * to memory at runtime, i.e. a small function that calls the functions.
 */

// the maximum conceivable size of our blink program image
#define kJitLeeway 1048576

// how closely adjacent jit code needs to be, to our image
#ifdef __x86_64__
#define kJitProximity 0x7fffffff
#else
#define kJitProximity (kArmDispMax * 4)
#endif

#define JITSTAGE_CONTAINER(e) DLL_CONTAINER(struct JitStage, elem, e)

struct JitStage {
  int start;
  int index;
  hook_t *hook;
  struct Dll elem;
};

#if defined(__x86_64__)
static const u8 kPrologue[] = {
    0x55,                                      // push %rbp
    0x48, 0x89, 0xe5,                          // mov  %rsp,%rbp
    0x48, 0x81, 0xec, 0x80, 0x00, 0x00, 0x00,  // sub  $0x80,%rsp
    0x48, 0x89, 0x5d, 0x80,                    // mov  %rbx,-0x80(%rbp)
    0x4c, 0x89, 0x65, 0x88,                    // mov  %r12,-0x78(%rbp)
    0x4c, 0x89, 0x6d, 0x90,                    // mov  %r13,-0x70(%rbp)
    0x4c, 0x89, 0x75, 0x98,                    // mov  %r14,-0x68(%rbp)
    0x4c, 0x89, 0x7d, 0xa0,                    // mov  %r15,-0x60(%rbp)
    0x48, 0x89, 0xfb,                          // mov  %rdi,%rbx
};
static const u8 kEpilogue[] = {
    0x48, 0x8b, 0x5d, 0x80,  // mov -0x80(%rbp),%rbx
    0x4c, 0x8b, 0x65, 0x88,  // mov -0x78(%rbp),%r12
    0x4c, 0x8b, 0x6d, 0x90,  // mov -0x70(%rbp),%r13
    0x4c, 0x8b, 0x75, 0x98,  // mov -0x68(%rbp),%r14
    0x4c, 0x8b, 0x7d, 0xa0,  // mov -0x60(%rbp),%r15
    0xc9,                    // leave
    0xc3,                    // ret
};
#elif defined(__aarch64__)
static const u32 kPrologue[] = {
    0xa9be7bfd,  // stp x29, x30, [sp, #-32]!
    0x910003fd,  // mov x29, sp
    0xf9000bf3,  // str x19, [sp, #16]
    0xaa0003f3,  // mov x19, x0
};
static const u32 kEpilogue[] = {
    0xf9400bf3,  // ldr x19, [sp, #16]
    0xa8c27bfd,  // ldp x29, x30, [sp], #32
    0xd65f03c0,  // ret
};
#endif

static struct JitGlobals {
  pthread_mutex_t lock;
  _Atomic(int) prot;
  _Atomic(u8 *) brk;
  struct Dll *freepages GUARDED_BY(lock);
} g_jit = {
    PTHREAD_MUTEX_INITIALIZER,
#if (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 403 || \
    __has_builtin(__builtin___clear_cache)
    PROT_READ | PROT_WRITE | PROT_EXEC,
#else
#define __builtin___clear_cache(x, y) (void)0
    PROT_READ | PROT_WRITE,
#endif
};

static long GetSystemPageSize(void) {
  long pagesize;
  unassert((pagesize = sysconf(_SC_PAGESIZE)) > 0);
  unassert(IS2POW(pagesize));
  unassert(pagesize <= kJitPageSize);
  return pagesize;
}

static bool CanJitForImmediateEffect(void) {
  return atomic_load_explicit(&g_jit.prot, memory_order_relaxed) & PROT_EXEC;
}

static void RelinquishJitPage(struct JitPage *jp) {
  struct Dll *e;
  while ((e = dll_first(jp->staged))) {
    jp->staged = dll_remove(jp->staged, e);
    free(JITSTAGE_CONTAINER(e));
  }
  jp->start = 0;
  jp->index = 0;
  jp->committed = 0;
  unassert(mmap(jp->addr, kJitPageSize, PROT_NONE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_NORESERVE, -1,
                0) == (void *)jp->addr);
  LOCK(&g_jit.lock);
  g_jit.freepages = dll_make_first(g_jit.freepages, &jp->elem);
  UNLOCK(&g_jit.lock);
}

static struct JitPage *ReclaimJitPage(void) {
  struct Dll *e;
  struct JitPage *jp;
  LOCK(&g_jit.lock);
  if ((e = dll_first(g_jit.freepages))) {
    g_jit.freepages = dll_remove(g_jit.freepages, e);
    jp = JITPAGE_CONTAINER(e);
  } else {
    jp = 0;
  }
  UNLOCK(&g_jit.lock);
  if (jp) {
    unassert(mmap(jp->addr, kJitPageSize,
                  atomic_load_explicit(&g_jit.prot, memory_order_relaxed),
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1,
                  0) == (void *)jp->addr);
  }
  return jp;
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
int InitJit(struct Jit *jit) {
  u8 *brk;
  memset(jit, 0, sizeof(*jit));
  pthread_mutex_init(&jit->lock, 0);
  if (!(brk = atomic_load_explicit(&g_jit.brk, memory_order_relaxed))) {
    // we're going to politely ask the kernel for addresses starting
    // arbitrary megabytes past the end of our own executable's .bss
    // section. we'll cross our fingers, and hope that gives us room
    // away from a brk()-based libc malloc() function which may have
    // already allocated memory in this space. the reason it matters
    // is because the x86 and arm isas impose limits on displacement
    atomic_compare_exchange_strong_explicit(
        &g_jit.brk, &brk,
        (u8 *)ROUNDUP((intptr_t)IMAGE_END, kJitPageSize) + kJitLeeway,
        memory_order_relaxed, memory_order_relaxed);
  }
  return 0;
}

/**
 * Destroyes initialized JIT object.
 *
 * Passing a value not previously initialized by InitJit() is undefined.
 *
 * @return 0 on success
 */
int DestroyJit(struct Jit *jit) {
  struct Dll *e;
  LOCK(&jit->lock);
  while ((e = dll_first(jit->pages))) {
    jit->pages = dll_remove(jit->pages, e);
    RelinquishJitPage(JITPAGE_CONTAINER(e));
  }
  UNLOCK(&jit->lock);
  unassert(!pthread_mutex_destroy(&jit->lock));
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
 * Returns true if DisableJit() was called or AcquireJit() had failed.
 */
bool IsJitDisabled(struct Jit *jit) {
  return atomic_load_explicit(&jit->disabled, memory_order_acquire);
}

/**
 * Starts writing chunk of code to JIT memory.
 *
 * The returned object becomes owned by the calling thread until it is
 * relinquished by passing the result to ReleaseJit(). Any given chunk
 * can't exceed the JIT page size in length. Many chunks may be placed
 * in the same page by multiple threads.
 *
 * @param reserve is the anticipated number of bytes needed; passing
 *     a negative or unreasonably large number is undefined behavior
 * @return object representing a page of JIT memory, having at least
 *     `reserve` bytes of memory, or NULL if out of memory, in which
 *     case `jit` will enter the disabled state, after which this'll
 *     always return NULL
 */
struct JitPage *AcquireJit(struct Jit *jit, long reserve) {
  u8 *brk;
  int prot;
  struct Dll *e;
  intptr_t distance;
  struct JitPage *jp;
  unassert(reserve > 0);
  unassert(reserve <= kJitPageSize - sizeof(struct JitPage));
  if (!IsJitDisabled(jit)) {
    LOCK(&jit->lock);
    e = dll_first(jit->pages);
    if (e && (jp = JITPAGE_CONTAINER(e))->index + reserve <= kJitPageSize) {
      jit->pages = dll_remove(jit->pages, &jp->elem);
    } else if (!(jp = ReclaimJitPage()) &&
               (jp = (struct JitPage *)calloc(1, sizeof(struct JitPage)))) {
      for (;;) {
        brk = atomic_fetch_add_explicit(&g_jit.brk, kJitPageSize,
                                        memory_order_relaxed);
        jp->addr = (u8 *)Mmap(
            brk, kJitPageSize,
            (prot = atomic_load_explicit(&g_jit.prot, memory_order_relaxed)),
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_DEMAND, -1, 0, "jit");
        if (jp->addr != MAP_FAILED) {
          distance = ABS(jp->addr - IMAGE_END);
          if (distance <= kJitProximity - kJitLeeway) {
            if (jp->addr != brk) {
              atomic_store_explicit(&g_jit.brk, jp->addr + kJitPageSize,
                                    memory_order_relaxed);
            }
            dll_init(&jp->elem);
            break;
          } else {
            // we currently only support jitting when we're able to have
            // the jit code adjacent to the blink image because our impl
            // currently makes assumptions such as a call operation will
            // have a fixed length (so it can be easily hopped over). if
            // we're unable to acquire jit memory within proximity or if
            // we run out of jit memory in proximity, then new jit paths
            // won't be created anymore; and the program will still run.
            LOGF("mmap() returned address %p that's too far away (%" PRIdPTR
                 " bytes) from our program image (which ends near %p)",
                 jp, distance, IMAGE_END);
            unassert(!munmap(jp->addr, kJitPageSize));
            DisableJit(jit);
            free(jp);
            jp = 0;
            break;
          }
        } else if (errno == MAP_DENIED) {
          // our fixed noreplace mmap request probably overlapped some
          // dso library, so let's try again with a different address.
          continue;
        } else if (prot & PROT_EXEC) {
          // OpenBSD imposes a R^X invariant and raises ENOTSUP if RWX
          // memory is requested. Since other OSes might exist, having
          // this same requirement, and possible a different errno, we
          // shall just clear the exec flag and try again.
          atomic_store_explicit(&g_jit.prot, prot & ~PROT_EXEC,
                                memory_order_relaxed);
          continue;
        } else {
          LOGF("mmap() error at %p is %s", brk, strerror(errno));
          DisableJit(jit);
          free(jp);
          jp = 0;
          break;
        }
      }
    }
    UNLOCK(&jit->lock);
  } else {
    jp = 0;
  }
  if (jp) {
    unassert(!(jp->start & (kJitAlign - 1)));
    unassert(jp->start == jp->index);
  }
  return jp;
}

/**
 * Returns number of bytes of space remaining in JIT memory page.
 *
 * @return number of bytes of space that can be appended into, or -1 if
 *     if an append operation previously failed due to lack of space
 */
long GetJitRemaining(struct JitPage *jp) {
  return kJitPageSize - jp->index;
}

/**
 * Returns current program counter or instruction pointer of JIT page.
 *
 * @return absolute instruction pointer memory address in bytes
 */
intptr_t GetJitPc(struct JitPage *jp) {
  return (intptr_t)jp->addr + jp->index;
}

/**
 * Appends bytes to JIT page.
 *
 * Errors here safely propagate to ReleaseJit().
 *
 * @return true if room was available, otherwise false
 */
bool AppendJit(struct JitPage *jp, const void *data, long size) {
  unassert(size > 0);
  if (size <= GetJitRemaining(jp)) {
    memcpy(jp->addr + jp->index, data, size);
    jp->index += size;
    return true;
  } else {
    jp->index = kJitPageSize + 1;
    return false;
  }
}

static int CommitJit(struct JitPage *jp, long pagesize) {
  long pageoff;
  int count = 0;
  struct Dll *e;
  struct JitStage *js;
  unassert(jp->start == jp->index);
  unassert(!(jp->committed & (pagesize - 1)));
  pageoff = ROUNDDOWN(jp->start, pagesize);
  if (pageoff > jp->committed) {
    unassert(jp->start == jp->index);
    unassert(!mprotect(jp->addr + jp->committed, pageoff - jp->committed,
                       PROT_READ | PROT_EXEC));
    MEM_LOGF("activated [%p,%p) w/ %zu kb", jp->addr + jp->committed,
             jp->addr + jp->committed + (pageoff - jp->committed),
             (pageoff - jp->committed) / 1024);
    unassert(jp->start == jp->index);
    while ((e = dll_first(jp->staged))) {
      js = JITSTAGE_CONTAINER(e);
      if (js->index <= pageoff) {
        atomic_store_explicit(js->hook, (intptr_t)jp->addr + js->start,
                              memory_order_relaxed);
        jp->staged = dll_remove(jp->staged, e);
        free(js);
        ++count;
      } else {
        break;
      }
    }
    jp->committed = pageoff;
  }
  return count;
}

static void ReinsertPage(struct Jit *jit, struct JitPage *jp) {
  unassert(jp->start == jp->index);
  if (jp->index < kJitPageSize) {
    jit->pages = dll_make_first(jit->pages, &jp->elem);
  } else {
    jit->pages = dll_make_last(jit->pages, &jp->elem);
  }
}

/**
 * Forces activation of committed JIT chunks.
 *
 * Normally JIT chunks become active and have their function pointer
 * hook updated automatically once the system page fills up with jit
 * code. In some cases, such as unit tests, it's necessary to ensure
 * that JIT code goes live sooner. The tradeoff of flushing is it'll
 * lead to wasted memory and less performance, due to the additional
 * mprotect() system call overhead.
 */
int FlushJit(struct Jit *jit) {
  int count = 0;
  long pagesize;
  struct Dll *e;
  struct JitPage *jp;
  struct JitStage *js;
  pagesize = GetSystemPageSize();
  LOCK(&jit->lock);
StartOver:
  for (e = dll_first(jit->pages); e; e = dll_next(jit->pages, e)) {
    jp = JITPAGE_CONTAINER(e);
    if (jp->start >= kJitPageSize) break;
    if (!dll_is_empty(jp->staged)) {
      jit->pages = dll_remove(jit->pages, e);
      UNLOCK(&jit->lock);
      js = JITSTAGE_CONTAINER(dll_last(jp->staged));
      jp->start = ROUNDUP(js->index, pagesize);
      jp->index = jp->start;
      count += CommitJit(jp, pagesize);
      LOCK(&jit->lock);
      ReinsertPage(jit, jp);
      goto StartOver;
    }
  }
  UNLOCK(&jit->lock);
  return count;
}

/**
 * Finishes writing chunk of code to JIT page.
 *
 * @param jp is function builder object that was returned by StartJit();
 *     this function always relinquishes the calling thread's ownership
 *     of this object, even if this function returns an error
 * @param hook points to a function pointer where the address of the JIT
 *     code chunk will be stored, once it becomes active
 * @return pointer to start of chunk, or zero if an append operation
 *     had previously failed due to lack of space
 */
intptr_t ReleaseJit(struct Jit *jit, struct JitPage *jp, hook_t *hook,
                    intptr_t staging) {
  u8 *addr;
  struct JitStage *js;
  unassert(jp->index >= jp->start);
  unassert(jp->start >= jp->committed);
  if (jp->index > jp->start) {
    if (jp->index <= kJitPageSize) {
      addr = jp->addr + jp->start;
      jp->index = ROUNDUP(jp->index, kJitAlign);
      if (hook) {
        if (CanJitForImmediateEffect()) {
          __builtin___clear_cache((char *)addr,
                                  (char *)addr + (jp->index - jp->start));
          atomic_store_explicit(hook, (intptr_t)addr, memory_order_release);
        } else {
          atomic_store_explicit(hook, staging, memory_order_release);
          if ((js = (struct JitStage *)calloc(1, sizeof(struct JitStage)))) {
            dll_init(&js->elem);
            js->hook = hook;
            js->start = jp->start;
            js->index = jp->index;
            jp->staged = dll_make_last(jp->staged, &js->elem);
          }
        }
      }
      if (jp->index + kJitPageFit > kJitPageSize) {
        jp->index = kJitPageSize;
      }
    } else if (jp->start) {
      addr = 0;  // fail and let it try again
    } else {
      LOG_ONCE(LOGF("kJitPageSize needs to be increased"));
      if (hook) {
        atomic_store_explicit(hook, staging, memory_order_release);
      }
      addr = 0;
    }
    jp->start = jp->index;
    unassert(jp->start == jp->index);
    CommitJit(jp, GetSystemPageSize());
  } else {
    addr = 0;
  }
  LOCK(&jit->lock);
  ReinsertPage(jit, jp);
  UNLOCK(&jit->lock);
  return (intptr_t)addr;
}

/**
 * Begins writing function definition to JIT memory.
 *
 * This will acquire a page of JIT memory and insert a function
 * prologue. Code may be added to the function using methods like
 * AppendJitCall(). When a function is completed, FinishJit() should be
 * called. The calling thread is granted exclusive ownership of the
 * returned page of JIT memory, until it's relinquished by FinishJit().
 *
 * @return function builder object
 */
struct JitPage *StartJit(struct Jit *jit) {
  struct JitPage *jp;
  if ((jp = AcquireJit(jit, kJitPageFit))) {
    AppendJit(jp, kPrologue, sizeof(kPrologue));
  }
  return jp;
}

/**
 * Finishes writing function definition to JIT memory.
 *
 * Errors that happened earlier in AppendJit*() methods will safely
 * propagate to this function resulting in an error.
 *
 * @param jp is function builder object that was returned by StartJit();
 *     this function always relinquishes the calling thread's ownership
 *     of this object, even if this function returns an error
 * @param hook points to a function pointer where the address of the JIT
 *     code chunk will be stored, once it becomes active
 * @return address of generated function, or zero if an error occurred
 *     at some point in the function writing process
 */
intptr_t FinishJit(struct Jit *jit, struct JitPage *jp, hook_t *hook,
                   intptr_t staging) {
  AppendJit(jp, kEpilogue, sizeof(kEpilogue));
  return ReleaseJit(jit, jp, hook, staging);
}

/**
 * Abandons writing function definition to JIT memory.
 *
 * @param jp becomes owned by `jit` again after this call
 */
int AbandonJit(struct Jit *jit, struct JitPage *jp) {
  jp->index = jp->start;
  LOCK(&jit->lock);
  ReinsertPage(jit, jp);
  UNLOCK(&jit->lock);
  return 0;
}

/**
 * Finishes function by having it tail call a previously created one.
 *
 * Splicing a `chunk` that wasn't created by StartJit() is undefined.
 *
 * @param chunk is a jit function that was created earlier, or null in
 *     which case this method is identical to FinishJit()
 * @return address of generated function, or NULL if an error occurred
 *     at some point in the function writing process
 */
intptr_t SpliceJit(struct Jit *jit, struct JitPage *jp, hook_t *hook,
                   intptr_t staging, intptr_t chunk) {
  unassert(!chunk || !memcmp((u8 *)chunk, kPrologue, sizeof(kPrologue)));
  if (chunk) {
    AppendJitJmp(jp, (u8 *)chunk + sizeof(kPrologue));
    return ReleaseJit(jit, jp, hook, staging);
  } else {
    return FinishJit(jit, jp, hook, staging);
  }
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
bool AppendJitMovReg(struct JitPage *jp, int dst, int src) {
  if (dst == src) return true;
#if defined(__x86_64__)
  unassert(!(dst & ~15));
  unassert(!(src & ~15));
  u8 buf[3];
  buf[0] = kAmdRexw | (dst & 8 ? kAmdRexr : 0) | (dst & 8 ? kAmdRexb : 0);
  buf[1] = 0x89;
  buf[2] = 0300 | (src & 7) << 3 | (dst & 7);
#elif defined(__aarch64__)
  //               src            target
  //              ┌─┴─┐           ┌─┴─┐
  // 0b10101010000000000000001111110011 mov x19, x0
  // 0b10101010000000010000001111110100 mov x20, x1
  // 0b10101010000101000000001111100001 mov x1, x20
  // 0b10101010000100110000001111100000 mov x0, x19
  unassert(!(dst & ~31));
  unassert(!(src & ~31));
  u32 buf[1] = {0xaa0003e0 | src << 16 | dst};
#endif
  return AppendJit(jp, buf, sizeof(buf));
}

/**
 * Sets function argument.
 *
 * @param jp is function builder object, returned by StartJit
 * @param arg is your 0-indexed function argument (six total)
 * @param value is the constant value to use as the parameter
 * @return true if room was available, otherwise false
 */
bool AppendJitSetArg(struct JitPage *jp, int arg, u64 value) {
  int reg;
  unassert(0 <= arg && arg < 6);
#ifdef __x86_64__
  u8 arg2reg[6] = {kAmdDi, kAmdSi, kAmdDx, kAmdCx, 8, 9};
  reg = arg2reg[arg];
#else
  reg = arg;
#endif
  return AppendJitSetReg(jp, reg, value);
}

/**
 * Appends function call instruction to JIT memory.
 *
 * @param jp is function builder object returned by StartJit()
 * @param func points to another callee function in memory
 * @return true if room was available, otherwise false
 */
bool AppendJitCall(struct JitPage *jp, void *func) {
  int n;
  intptr_t disp, addr;
  addr = (intptr_t)func;
#if defined(__x86_64__)
  u8 buf[5];
  disp = addr - (GetJitPc(jp) + 5);
  if (kAmdDispMin <= disp && disp <= kAmdDispMax) {
    // AMD function calls are encoded using an 0xE8 byte, followed by a
    // 32-bit signed two's complement little-endian integer, containing
    // the relative location between the function being called, and the
    // instruction at the location that follows our 5 byte call opcode.
    buf[0] = kAmdCall;
    Write32(buf + 1, disp & kAmdDispMask);
    n = 5;
  } else {
    AppendJitSetReg(jp, kAmdAx, addr);
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
  disp = (addr - GetJitPc(jp)) >> 2;
  unassert(kArmDispMin <= disp && disp <= kArmDispMax);
  buf[0] = kArmCall | (disp & kArmDispMask);
  n = 4;
#endif
  return AppendJit(jp, buf, n);
}

/**
 * Appends unconditional branch instruction to JIT memory.
 *
 * @param jp is function builder object returned by StartJit()
 * @param code points to some other code address in memory
 * @return true if room was available, otherwise false
 */
bool AppendJitJmp(struct JitPage *jp, void *code) {
  int n;
  intptr_t disp, addr;
  addr = (intptr_t)code;
#if defined(__x86_64__)
  u8 buf[5];
  disp = addr - (GetJitPc(jp) + 5);
  if (kAmdDispMin <= disp && disp <= kAmdDispMax) {
    buf[0] = kAmdJmp;
    Write32(buf + 1, disp & kAmdDispMask);
    n = 5;
  } else {
    AppendJitSetReg(jp, kAmdAx, addr);
    buf[0] = kAmdJmpAx[0];
    buf[1] = kAmdJmpAx[1];
    n = 2;
  }
#elif defined(__aarch64__)
  uint32_t buf[1];
  disp = (addr - GetJitPc(jp)) >> 2;
  unassert(kArmDispMin <= disp && disp <= kArmDispMax);
  buf[0] = kArmJmp | (disp & kArmDispMask);
  n = 4;
#endif
  return AppendJit(jp, buf, n);
}

/**
 * Sets register to immediate value.
 *
 * @param jp is function builder object returned by StartJit()
 * @param param is the zero-based index into the register file
 * @param value is the constant value to use as the parameter
 * @return true if room was available, otherwise false
 */
bool AppendJitSetReg(struct JitPage *jp, int reg, u64 value) {
  int n = 0;
#if defined(__x86_64__)
  u8 buf[10];
  u8 rex = 0;
  if (reg & 8) rex |= kAmdRexb;
  if (!value) {
    if (reg & 8) rex |= kAmdRexr;
    if (rex) buf[n++] = rex;
    buf[n++] = kAmdXor;
    buf[n++] = 0300 | (reg & 7) << 3 | (reg & 7);
  } else {
    if (value > 0xffffffff) rex |= kAmdRexw;
    if (rex) buf[n++] = rex;
    buf[n++] = kAmdMovImm | (reg & 7);
    if ((rex & kAmdRexw) != kAmdRexw) {
      Write32(buf + n, value);
      n += 4;
    } else {
      Write64(buf + n, value);
      n += 8;
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
  int i;
  u32 op;
  u32 buf[4];
  unassert(!(reg & ~kArmRegMask));
  // TODO: This could be improved some more.
  if ((i64)value < 0 && (i64)value >= -0x8000) {
    buf[n++] = kArmMovSex | ~value << kArmImmOff | reg << kArmRegOff;
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
      buf[n++] = op;
      op = kArmMovNex;
    } while ((value >>= 16));
  }
  n *= 4;
#endif
  return AppendJit(jp, buf, n);
}

#else
// clang-format off
#define STUB(RETURN, NAME, PARAMS, RESULT) \
  RETURN NAME PARAMS {                     \
    return RESULT;                         \
  }
STUB(int, InitJit, (struct Jit *jit), 0)
STUB(int, DestroyJit, (struct Jit *jit), 0)
STUB(int, DisableJit, (struct Jit *jit), 0)
STUB(bool, IsJitDisabled, (struct Jit *jit), 1)
STUB(struct JitPage *, AcquireJit, (struct Jit *jit, long reserve), 0)
STUB(long, GetJitRemaining, (struct JitPage *jp), 0)
STUB(intptr_t, GetJitPc, (struct JitPage *jp), 0)
STUB(bool, AppendJit, (struct JitPage *jp, const void *data, long size), 0)
STUB(intptr_t, ReleaseJit, (struct Jit *jit, struct JitPage *jp, hook_t *hook, intptr_t staging), 0)
STUB(bool, AppendJitMovReg, (struct JitPage *jp, int dst, int src), 0)
STUB(int, AbandonJit, (struct Jit *jit, struct JitPage *jp), 0)
STUB(int, FlushJit, (struct Jit *jit), 0);
STUB(struct JitPage *, StartJit, (struct Jit *jit), 0)
STUB(intptr_t, FinishJit, (struct Jit *jit, struct JitPage *jp, hook_t *hook, intptr_t staging), 0)
STUB(bool, AppendJitJmp, (struct JitPage *jp, void *code), 0)
STUB(bool, AppendJitCall, (struct JitPage *jp, void *func), 0)
STUB(bool, AppendJitSetReg, (struct JitPage *jp, int reg, u64 value), 0)
STUB(bool, AppendJitSetArg, (struct JitPage *jp, int param, u64 value), 0)
STUB(intptr_t, SpliceJit, (struct Jit *jit, struct JitPage *jp, hook_t *hook, intptr_t staging, intptr_t chunk), 0);
#endif
