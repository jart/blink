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
 *     struct Jit *jit;
 *     InitJit(&jit);
 *
 *     // workflow for composing two function calls
 *     long Add(long x, long y) { return x + y; }
 *     _Atomic(intptr_t) hook;
 *     struct JitBlock *jb;
 *     jb = StartJit(jit);
 *     AppendJitSetReg(jit, kJitArg0, 1);
 *     AppendJitSetReg(jit, kJitArg1, 2);
 *     AppendJitCall(jit, (void *)Add);
 *     AppendJitMovReg(jit, kJitRes0, kJitArg0);
 *     AppendJitSetReg(jit, kJitArg1, 3);
 *     AppendJitCall(jit, (void *)Add);
 *     FinishJit(jit, jb, &hook, 0);
 *     FlushJit(jit);
 *     printf("1+2+3=%ld\n", ((long (*)(void))hook)());
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

const u8 kJitRes[2] = {kJitRes0, kJitRes1};
const u8 kJitSav[5] = {kJitSav0, kJitSav1, kJitSav2, kJitSav3, kJitSav4};
const u8 kJitArg[6] = {kJitArg0, kJitArg1, kJitArg2,
                       kJitArg3, kJitArg4, kJitArg5};

#ifdef HAVE_JIT

// the maximum conceivable size of our blink program image
#define kJitLeeway 1048576

// how closely adjacent jit code needs to be, to our image
#ifdef __x86_64__
#define kJitProximity 0x7fffffff
#else
#define kJitProximity (kArmDispMax * 4)
#endif

static struct JitGlobals {
  pthread_mutex_t lock;
  _Atomic(int) prot;
  _Atomic(u8 *) brk;
  struct Dll *freeblocks GUARDED_BY(lock);
} g_jit = {
    PTHREAD_MUTEX_INITIALIZER,
    PROT_READ | PROT_WRITE | PROT_EXEC,
};

#if defined(__x86_64__)
static const u8 kJitPrologue[] = {
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
static const u8 kJitEpilogue[] = {
    0x48, 0x8b, 0x5d, 0x80,  // mov -0x80(%rbp),%rbx
    0x4c, 0x8b, 0x65, 0x88,  // mov -0x78(%rbp),%r12
    0x4c, 0x8b, 0x6d, 0x90,  // mov -0x70(%rbp),%r13
    0x4c, 0x8b, 0x75, 0x98,  // mov -0x68(%rbp),%r14
    0x4c, 0x8b, 0x7d, 0xa0,  // mov -0x60(%rbp),%r15
    0xc9,                    // leave
    0xc3,                    // ret
};
#elif defined(__aarch64__)
static const u32 kJitPrologue[] = {
    0xa9bc7bfd,  // stp x29, x30, [sp, #-64]!
    0x910003fd,  // mov x29, sp
    0xa90153f3,  // stp x19, x20, [sp, #16]
    0xa9025bf5,  // stp x21, x22, [sp, #32]
    0xa90363f7,  // stp x23, x24, [sp, #48]
    0xaa0003f3,  // mov x19, x0
};
static const u32 kJitEpilogue[] = {
    0xa94153f3,  // ldp x19, x20, [sp, #16]
    0xa9425bf5,  // ldp x21, x22, [sp, #32]
    0xa94363f7,  // ldp x23, x24, [sp, #48]
    0xa8c47bfd,  // ldp x29, x30, [sp], #64
    0xd65f03c0,  // ret
};
#endif

bool CanJitForImmediateEffect(void) {
  return atomic_load_explicit(&g_jit.prot, memory_order_relaxed) & PROT_EXEC;
}

static void RelinquishJitBlock(struct JitBlock *jb) {
  struct Dll *e;
  while ((e = dll_first(jb->staged))) {
    dll_remove(&jb->staged, e);
    free(JITSTAGE_CONTAINER(e));
  }
  jb->start = 0;
  jb->index = 0;
  jb->committed = 0;
  LOCK(&g_jit.lock);
  dll_make_first(&g_jit.freeblocks, &jb->elem);
  UNLOCK(&g_jit.lock);
}

static struct JitBlock *ReclaimJitBlock(void) {
  struct Dll *e;
  struct JitBlock *jb;
  LOCK(&g_jit.lock);
  if ((e = dll_first(g_jit.freeblocks))) {
    dll_remove(&g_jit.freeblocks, e);
    jb = JITBLOCK_CONTAINER(e);
  } else {
    jb = 0;
  }
  UNLOCK(&g_jit.lock);
  if (jb && !CanJitForImmediateEffect()) {
    unassert(
        !mprotect(jb->addr, jb->blocksize,
                  atomic_load_explicit(&g_jit.prot, memory_order_relaxed)));
  }
  return jb;
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
  int blocksize;
  memset(jit, 0, sizeof(*jit));
  jit->pagesize = GetSystemPageSize();
  jit->blocksize = blocksize = ROUNDUP(kJitMinBlockSize, jit->pagesize);
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
        (u8 *)ROUNDUP((intptr_t)IMAGE_END, blocksize) + kJitLeeway,
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
  while ((e = dll_first(jit->blocks))) {
    dll_remove(&jit->blocks, e);
    RelinquishJitBlock(JITBLOCK_CONTAINER(e));
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
bool IsJitDisabled(const struct Jit *jit) {
  return atomic_load_explicit(&jit->disabled, memory_order_acquire);
}

static struct JitBlock *AcquireJit(struct Jit *jit) {
  u8 *brk;
  int prot;
  struct Dll *e;
  intptr_t distance;
  struct JitBlock *jb;
  if (!IsJitDisabled(jit)) {
    LOCK(&jit->lock);
    e = dll_first(jit->blocks);
    if (e && (jb = JITBLOCK_CONTAINER(e)) &&
        jb->index + kJitFit <= jb->blocksize) {
      dll_remove(&jit->blocks, &jb->elem);
    } else if (!(jb = ReclaimJitBlock()) &&
               (jb = (struct JitBlock *)calloc(1, sizeof(struct JitBlock)))) {
      for (;;) {
        jb->blocksize =
            atomic_load_explicit(&jit->blocksize, memory_order_relaxed);
        brk = atomic_fetch_add_explicit(&g_jit.brk, jb->blocksize,
                                        memory_order_relaxed);
        jb->addr = (u8 *)Mmap(
            brk, jb->blocksize,
            (prot = atomic_load_explicit(&g_jit.prot, memory_order_relaxed)),
            MAP_JIT | MAP_PRIVATE | MAP_ANONYMOUS | MAP_DEMAND, -1, 0, "jit");
        if (jb->addr != MAP_FAILED) {
          distance = ABS(jb->addr - IMAGE_END);
          if (distance <= kJitProximity - kJitLeeway) {
            if (jb->addr != brk) {
              atomic_store_explicit(&g_jit.brk, jb->addr + jb->blocksize,
                                    memory_order_relaxed);
            }
            dll_init(&jb->elem);
            break;
          } else {
            // we currently only support jitting when we're able to have
            // the jit code adjacent to the blink image because our impl
            // currently makes assumptions such as a call operation will
            // have a fixed length (so it can be easily hopped over). if
            // we're unable to acquire jit memory within proximity or if
            // we run out of jit memory in proximity, then new jit paths
            // won't be created anymore; and the program will still run.
            LOGF("jit isn't supported in this environment because mmap()"
                 " yielded an address %p that's too far away (%" PRIdPTR
                 " bytes) from our program image (which ends near %p)",
                 jb, distance, IMAGE_END);
            unassert(!munmap(jb->addr, jb->blocksize));
            DisableJit(jit);
            free(jb);
            jb = 0;
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
          MEM_LOGF("operating system doesn't permit rwx memory; your"
                   " jit will have less predictable behavior");
          atomic_store_explicit(&g_jit.prot, prot & ~PROT_EXEC,
                                memory_order_relaxed);
          continue;
        } else {
          LOGF("mmap() error at %p is %s", brk, strerror(errno));
          DisableJit(jit);
          free(jb);
          jb = 0;
          break;
        }
      }
    }
    UNLOCK(&jit->lock);
  } else {
    jb = 0;
  }
  if (jb) {
    unassert(!(jb->start & (kJitAlign - 1)));
    unassert(jb->start == jb->index);
    if (pthread_jit_write_protect_supported_np()) {
      pthread_jit_write_protect_np(false);
    }
  }
  return jb;
}

/**
 * Returns number of bytes of space remaining in JIT memory block.
 *
 * @return number of bytes of space that can be appended into, or -1 if
 *     if an append operation previously failed due to lack of space
 */
long GetJitRemaining(const struct JitBlock *jb) {
  return jb->blocksize - jb->index;
}

/**
 * Returns current program counter or instruction pointer of JIT block.
 *
 * @return absolute instruction pointer memory address in bytes
 */
intptr_t GetJitPc(const struct JitBlock *jb) {
  return (intptr_t)jb->addr + jb->index;
}

/**
 * Returns byte length of JIT function prologue.
 */
size_t GetSizeOfJitPrologue(void) {
  return sizeof(kJitPrologue);
}

/**
 * Appends bytes to JIT block.
 *
 * Errors here safely propagate to FinishJit().
 *
 * @return true if room was available, otherwise false
 */
bool AppendJit(struct JitBlock *jb, const void *data, long size) {
  unassert(size > 0);
  if (size <= GetJitRemaining(jb)) {
    memcpy(jb->addr + jb->index, data, size);
    jb->index += size;
    return true;
  } else {
    jb->index = jb->blocksize + 1;
    return false;
  }
}

int CommitJit_(struct Jit *jit, struct JitBlock *jb) {
  int count = 0;
  long blockoff;
  struct Dll *e;
  struct JitStage *js;
  unassert(jb->start == jb->index);
  unassert(!(jb->committed & (jit->pagesize - 1)));
  if (!CanJitForImmediateEffect() &&
      (blockoff = ROUNDDOWN(jb->start, jit->pagesize)) > jb->committed) {
    unassert(!mprotect(jb->addr + jb->committed, blockoff - jb->committed,
                       PROT_READ | PROT_EXEC));
    MEM_LOGF("jit activated [%p,%p) w/ %zu kb", jb->addr + jb->committed,
             jb->addr + jb->committed + (blockoff - jb->committed),
             (blockoff - jb->committed) / 1024);
    unassert(jb->start == jb->index);
    while ((e = dll_first(jb->staged))) {
      js = JITSTAGE_CONTAINER(e);
      if (js->index <= blockoff) {
        atomic_store_explicit(js->hook, (jb->addr - IMAGE_END) + js->start,
                              memory_order_release);
        dll_remove(&jb->staged, e);
        free(js);
        ++count;
      } else {
        break;
      }
    }
    jb->committed = blockoff;
  }
  return count;
}

void ReinsertJitBlock_(struct Jit *jit, struct JitBlock *jb) {
  unassert(jb->start == jb->index);
  if (jb->index < jb->blocksize) {
    dll_make_first(&jit->blocks, &jb->elem);
  } else {
    dll_make_last(&jit->blocks, &jb->elem);
  }
}

static bool ReleaseJit(struct Jit *jit, struct JitBlock *jb, hook_t *hook,
                       intptr_t staging) {
  bool ok;
  u8 *addr;
  int blocksize;
  struct JitStage *js;
  unassert(hook);
  unassert(jb->index > jb->start);
  unassert(jb->start >= jb->committed);
  while (jb->index <= jb->blocksize && (jb->index & (kJitAlign - 1))) {
    unassert(AppendJitTrap(jb));
    unassert(jb->index <= jb->blocksize);
  }
  if (pthread_jit_write_protect_supported_np()) {
    pthread_jit_write_protect_np(true);
  }
  if (jb->index <= jb->blocksize) {
    addr = jb->addr + jb->start;
    if (CanJitForImmediateEffect()) {
      sys_icache_invalidate(addr, jb->index - jb->start);
      atomic_store_explicit(hook, addr - IMAGE_END, memory_order_release);
    } else {
      atomic_store_explicit(hook, (u8 *)staging - IMAGE_END,
                            memory_order_relaxed);
      if ((js = (struct JitStage *)calloc(1, sizeof(struct JitStage)))) {
        dll_init(&js->elem);
        js->hook = hook;
        js->start = jb->start;
        js->index = jb->index;
        dll_make_last(&jb->staged, &js->elem);
      }
    }
    if (jb->index + kJitFit > jb->blocksize) {
      jb->index = jb->blocksize;
    }
    ok = true;
  } else {
    // we ran out of room for the function, in which case we'll close
    // out this block and hope for better luck on the next pass.
    if (jb->index - jb->start > jb->blocksize / 2) {
      // if the function we failed to create is potentially bigger than
      // the block size, then we need to increase the block size, since
      // we'd otherwise risk endlessly burning memory.
      blocksize = atomic_load_explicit(&jit->blocksize, memory_order_relaxed);
      atomic_compare_exchange_strong_explicit(
          &jit->blocksize, &blocksize,
          ROUNDUP(blocksize + blocksize / 2, jit->pagesize),
          memory_order_relaxed, memory_order_relaxed);
    }
    ok = false;
  }
  jb->start = jb->index;
  unassert(jb->start == jb->index);
  CommitJit_(jit, jb);
  LOCK(&jit->lock);
  ReinsertJitBlock_(jit, jb);
  UNLOCK(&jit->lock);
  return ok;
}

/**
 * Begins writing function definition to JIT memory.
 *
 * This will acquire a block of JIT memory and insert a function
 * prologue. Code may be added to the function using methods like
 * AppendJitCall(). When a function is completed, FinishJit() should be
 * called. The calling thread is granted exclusive ownership of the
 * returned block of JIT memory, until it's relinquished by FinishJit().
 *
 * @return function builder object
 */
struct JitBlock *StartJit(struct Jit *jit) {
  struct JitBlock *jb;
  if ((jb = AcquireJit(jit))) {
    AppendJit(jb, kJitPrologue, sizeof(kJitPrologue));
  }
  return jb;
}

/**
 * Finishes writing function definition to JIT memory.
 *
 * Errors that happened earlier in AppendJit*() methods will safely
 * propagate to this function.
 *
 * @param jb is function builder object that was returned by StartJit();
 *     this function always relinquishes the calling thread's ownership
 *     of this object, even if this function returns an error
 * @param hook points to a function pointer where the address of the JIT
 *     code chunk will be stored, once it becomes active; hook is always
 *     updated before this function returns true, even if it has to be
 *     with the staging pointer; and hook isn't modified on error
 * @param staging should point to a statically compiled function which
 *     does the same thing as the generated function, so that the JITed
 *     function may go live at an arbitrary point in the future; it can
 *     also be zero in cases where FlushJit() is being used instead
 * @return true if the function was generated, in which case `hook` is
 *     updated with either (1) a function to the generated function, or
 *     (2) the value `staging` in which case `hook` shall be updated
 *     with the generated function later on, whenever `jb` is flushed;
 *     otherwise false if we ran out of room in the block while building
 *     the function, in which case the caller should simply try again
 */
bool FinishJit(struct Jit *jit, struct JitBlock *jb, hook_t *hook,
               intptr_t staging) {
  AppendJit(jb, kJitEpilogue, sizeof(kJitEpilogue));
  return ReleaseJit(jit, jb, hook, staging);
}

/**
 * Finishes function by having it tail call a previously created one.
 *
 * @param jb becomes owned by `jit` again after this call
 * @see FinishJit() for further documentation
 */
bool SpliceJit(struct Jit *jit, struct JitBlock *jb, hook_t *hook,
               intptr_t staging, intptr_t chunk) {
  unassert(!chunk || !memcmp((u8 *)chunk, kJitPrologue, sizeof(kJitPrologue)));
  if (chunk) {
    AppendJitJmp(jb, (u8 *)chunk + sizeof(kJitPrologue));
    return ReleaseJit(jit, jb, hook, staging);
  } else {
    return FinishJit(jit, jb, hook, staging);
  }
}

/**
 * Abandons writing function definition to JIT memory.
 *
 * @param jb becomes owned by `jit` again after this call
 */
int AbandonJit(struct Jit *jit, struct JitBlock *jb) {
  jb->index = jb->start;
  LOCK(&jit->lock);
  ReinsertJitBlock_(jit, jb);
  UNLOCK(&jit->lock);
  if (pthread_jit_write_protect_supported_np()) {
    pthread_jit_write_protect_np(true);
  }
  return 0;
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
  if (dst == src) return true;
#if defined(__x86_64__)
  unassert(!(dst & ~15));
  unassert(!(src & ~15));
  u8 buf[3];
  buf[0] = kAmdRexw | (src & 8 ? kAmdRexr : 0) | (dst & 8 ? kAmdRexb : 0);
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
  return AppendJit(jb, buf, sizeof(buf));
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
  intptr_t disp, addr;
  addr = (intptr_t)func;
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
  disp = (addr - GetJitPc(jb)) >> 2;
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
bool AppendJitJmp(struct JitBlock *jb, void *code) {
  int n;
  intptr_t disp, addr;
  addr = (intptr_t)code;
#if defined(__x86_64__)
  u8 buf[5];
  disp = addr - (GetJitPc(jb) + 5);
  if (kAmdDispMin <= disp && disp <= kAmdDispMax) {
    buf[0] = kAmdJmp;
    Write32(buf + 1, disp & kAmdDispMask);
    n = 5;
  } else {
    AppendJitSetReg(jb, kAmdAx, addr);
    buf[0] = kAmdJmpAx[0];
    buf[1] = kAmdJmpAx[1];
    n = 2;
  }
#elif defined(__aarch64__)
  uint32_t buf[1];
  disp = (addr - GetJitPc(jb)) >> 2;
  unassert(kArmDispMin <= disp && disp <= kArmDispMax);
  buf[0] = kArmJmp | (disp & kArmDispMask);
  n = 4;
#endif
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
  } else if ((i64)value < 0 && (i64)value >= INT32_MIN) {
    buf[n++] = rex | kAmdRexw;
    buf[n++] = 0xC7;
    buf[n++] = 0300 | (reg & 7);
    Write32(buf + n, value);
    n += 4;
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
  return AppendJit(jb, buf, n);
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

#else
// clang-format off
#define STUB(RETURN, NAME, PARAMS, RESULT) \
  RETURN NAME PARAMS {                     \
    return RESULT;                         \
  }
STUB(int, InitJit, (struct Jit *jit), 0)
STUB(int, DestroyJit, (struct Jit *jit), 0)
STUB(int, DisableJit, (struct Jit *jit), 0)
STUB(bool, IsJitDisabled, (const struct Jit *jit), 1)
STUB(intptr_t, GetJitPc, (const struct JitBlock *jb), 0)
STUB(long, GetJitRemaining, (const struct JitBlock *jb), 0)
STUB(bool, AppendJit, (struct JitBlock *jb, const void *data, long size), 0)
STUB(bool, AppendJitMovReg, (struct JitBlock *jb, int dst, int src), 0)
STUB(int, AbandonJit, (struct Jit *jit, struct JitBlock *jb), 0)
STUB(int, FlushJit, (struct Jit *jit), 0);
STUB(struct JitBlock *, StartJit, (struct Jit *jit), 0)
STUB(bool, FinishJit, (struct Jit *jit, struct JitBlock *jb, hook_t *hook, intptr_t staging), 0)
STUB(bool, AppendJitJmp, (struct JitBlock *jb, void *code), 0)
STUB(bool, AppendJitCall, (struct JitBlock *jb, void *func), 0)
STUB(bool, AppendJitSetReg, (struct JitBlock *jb, int reg, u64 value), 0)
STUB(bool, SpliceJit, (struct Jit *jit, struct JitBlock *jb, hook_t *hook, intptr_t staging, intptr_t chunk), 0)
STUB(bool, AppendJitNop, (struct JitBlock *jb), 0);
STUB(bool, AppendJitTrap, (struct JitBlock *jb), 0)
STUB(bool, CanJitForImmediateEffect, (void), 0)
STUB(size_t, GetSizeOfJitPrologue, (void), 0)
#endif
