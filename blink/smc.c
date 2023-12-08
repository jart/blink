/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2023 Justine Alexandra Roberts Tunney                              │
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
#include "blink/assert.h"
#include "blink/builtin.h"
#include "blink/flag.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/map.h"
#include "blink/stats.h"
#include "blink/tunables.h"
#include "blink/util.h"
#include "blink/xlat.h"

#ifndef DISABLE_JIT

// @asyncsignalsafe
static int ProtectHostPages(struct System *s, i64 vaddr, i64 size, int prot) {
  i64 a, b;
  unassert(HasLinearMapping());
  a = ROUNDDOWN(vaddr, FLAG_pagesize);
  b = ROUNDUP(vaddr + size, FLAG_pagesize);
  return mprotect(ToHost(a), b - a, prot);
}

static int ProtectSelfModifyingCode(struct System *s, i64 vaddr, i64 size) {
  MEM_LOGF("ProtectSelfModifyingCode(%#" PRIx64 ", %#" PRIx64 ")", vaddr, size);
  return ProtectHostPages(s, vaddr, size, PROT_READ);
}

// @asyncsignalsafe
static int UnprotectSelfModifyingCode(struct System *s, i64 vaddr, i64 size) {
  MEM_LOGF("UnprotectSelfModifyingCode(%#" PRIx64 ", %#" PRIx64 ")", vaddr,
           size);
  return ProtectHostPages(s, vaddr, size, PROT_READ | PROT_WRITE);
}

// @asyncsignalsafe
bool IsPageInSmcQueue(struct Machine *m, i64 page) {
  int i;
  i64 tmp;
  page &= -4096;
  STATISTIC(++smc_checks);
  for (i = 0; i < kSmcQueueSize; ++i) {
    if ((tmp = m->smcqueue.p[i]) == page) {
      if (i) {
        m->smcqueue.p[i - 0] = m->smcqueue.p[i - 1];
        m->smcqueue.p[i - 1] = tmp;
      }
      return true;
    }
  }
  return false;
}

// @asyncsignalsafe
void AddPageToSmcQueue(struct Machine *m, i64 page) {
  int i;
  page &= -4096;
  for (i = 0; i < kSmcQueueSize; ++i) {
    if (!m->smcqueue.p[i]) {
      STATISTIC(++smc_enqueued);
      m->smcqueue.p[i] = page;
      m->selfmodifying = true;
      atomic_store_explicit(&m->attention, true, memory_order_release);
      return;
    }
  }
  ERRF("self-modifying code page queue exhausted");
  Abort();
}

void FlushSmcQueue(struct Machine *m) {
  int i;
  i64 page;
  unassert(m->selfmodifying);
  STATISTIC(++smc_flushes);
  for (i = 0; i < kSmcQueueSize; ++i) {
    if ((page = m->smcqueue.p[i])) {
      m->smcqueue.p[i] = 0;
      if (!IsJitDisabled(&m->system->jit)) {
        if (HasLinearMapping()) {
          unassert(!ProtectSelfModifyingCode(m->system, page, 1));
        }
        ResetJitPage(&m->system->jit, page);
      }
      if (IsMakingPath(m) && (m->path.start & -4096) == (page & -4096)) {
        AbandonPath(m);
      }
    }
  }
}

i64 ProtectRwxMemory(struct System *s, i64 rc, i64 virt, i64 size,
                     long pagesize, int prot) {
  i64 a, b;
  unassert(HasLinearMapping());
  unassert(!IsJitDisabled(&s->jit));
  if (rc == -1) return -1;
  if (prot == (PROT_READ | PROT_WRITE | PROT_EXEC)) {
    a = ROUNDDOWN(virt, 4096);
    b = ROUNDUP(virt + size, 4096);
    // we can only do proper smc invalidation on the host page aligned
    // subset of the mmap()'d or mprotect()'d executable memory region
    a = ROUNDUP(a, pagesize);
    b = ROUNDDOWN(b, pagesize);
    if (b > a) {
      unassert(!ProtectSelfModifyingCode(s, a, b - a));
    }
  }
  return rc;
}

// @asyncsignalsafe
bool IsSelfModifyingCodeSegfault(struct Machine *m, const siginfo_t *si) {
  u64 pte;
  i64 vaddr;
  SIG_LOGF("IsSelfModifyingCodeSegfault()");
  unassert(m->system->loaded);
  if (si->si_signo != SIGSEGV) return false;
  if (si->si_code != SEGV_ACCERR) return false;
  vaddr = ConvertHostToGuestAddress(m->system, si->si_addr, &pte);
  SIG_LOGF("ConvertHostToGuestAddress(%p) -> %#" PRIx64, si->si_addr, vaddr);
  if ((pte & (PAGE_V | PAGE_U | PAGE_RW | PAGE_XD)) !=
      (PAGE_V | PAGE_U | PAGE_RW)) {
    return false;
  }
  STATISTIC(++smc_segfaults);
  if (UnprotectSelfModifyingCode(m->system, vaddr, 1)) {
    ERRF("failed to unprotect self modifying code");
    return false;
  }
  if (!IsPageInSmcQueue(m, vaddr)) {
    AddPageToSmcQueue(m, vaddr);
  }
  return true;
}

#endif /* DISABLE_JIT */
