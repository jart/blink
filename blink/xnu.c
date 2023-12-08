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
#include "blink/machine.h"

// @asyncsignalsafe
int FixXnuSignal(struct Machine *m, int sig, siginfo_t *si) {
  u64 pte;
  // The fruit platform will raise SIGBUS on writes to anon mappings
  // which don't have the PROT_WRITE protection, even when aligned!!
  // We only create this kind of memory for getting notifications if
  // RWX memory is modified. The XNU codebase implements a hack that
  // prevents this from happening for stack memory, but for anything
  // else we're on our own.
  if (HasLinearMapping() && sig == SIGBUS && si->si_code == BUS_ADRALN) {
    ConvertHostToGuestAddress(m->system, si->si_addr, &pte);
    // check that fault address mapped to an actual guest address, and
    // that either (1) the page doesn't have read+write permission, or
    // (2) it's an RWX page and therefore likely to be sneak protected
    if ((pte & PAGE_V) &&
        ((pte & (PAGE_U | PAGE_RW)) != (PAGE_U | PAGE_RW) ||
         (pte & (PAGE_U | PAGE_RW | PAGE_XD)) == (PAGE_U | PAGE_RW))) {
      // we're now quite certain this is actually a segmentation fault
      sig = SIGSEGV;
      si->si_signo = SIGSEGV;
      si->si_code = SEGV_ACCERR;
    }
  }
  return sig;
}
