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
#include <unistd.h>

#include "blink/dll.h"
#include "blink/jit.h"
#include "blink/lock.h"
#include "blink/macros.h"

/**
 * Forces activation of committed JIT chunks.
 *
 * Normally JIT chunks become active and have their function pointer
 * hook updated automatically once the system block fills up with jit
 * code. In some cases, such as unit tests, it's necessary to ensure
 * that JIT code goes live sooner. The tradeoff of flushing is it'll
 * lead to wasted memory and less performance, due to the additional
 * mprotect() system call overhead.
 */
int FlushJit(struct Jit *jit) {
  int count = 0;
  long pagesize;
  struct Dll *e;
  struct JitBlock *jb;
  struct JitStage *js;
  if (!CanJitForImmediateEffect()) {
    pagesize = sysconf(_SC_PAGESIZE);
    LOCK(&jit->lock);
  StartOver:
    for (e = dll_first(jit->blocks); e; e = dll_next(jit->blocks, e)) {
      jb = JITBLOCK_CONTAINER(e);
      if (jb->start >= jit->blocksize) break;
      if (!dll_is_empty(jb->staged)) {
        jit->blocks = dll_remove(jit->blocks, e);
        UNLOCK(&jit->lock);
        js = JITSTAGE_CONTAINER(dll_last(jb->staged));
        jb->start = ROUNDUP(js->index, pagesize);
        jb->index = jb->start;
        count += CommitJit_(jit, jb);
        LOCK(&jit->lock);
        ReinsertJitBlock_(jit, jb);
        goto StartOver;
      }
    }
    UNLOCK(&jit->lock);
  }
  return count;
}
