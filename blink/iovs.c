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
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "blink/endian.h"
#include "blink/errno.h"
#include "blink/iovs.h"
#include "blink/limits.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/stats.h"
#include "blink/types.h"
#include "blink/util.h"

void InitIovs(struct Iovs *ib) {
  ib->p = ib->init;
  ib->i = 0;
  ib->n = ARRAYLEN(ib->init);
}

void FreeIovs(struct Iovs *ib) {
  if (ib->p != ib->init) {
    free(ib->p);
  }
}

/**
 * Appends memory region to i/o vector builder.
 */
static int AppendIovs(struct Iovs *ib, void *base, size_t len) {
  unsigned i, n;
  struct iovec *p;
  if (len) {
    p = ib->p;
    i = ib->i;
    n = ib->n;
    if (i &&
        (uintptr_t)base == (uintptr_t)p[i - 1].iov_base + p[i - 1].iov_len) {
      STATISTIC(++iov_stretches);
      if (p[i - 1].iov_len + len > NUMERIC_MAX(ssize_t)) return einval();
      p[i - 1].iov_len += len;
    } else {
      if (i < n) {
        if (!i) {
          STATISTIC(++iov_created);
        } else {
          STATISTIC(++iov_fragments);
        }
      } else {
        STATISTIC(++iov_reallocs);
        n += n >> 1;
        if (p == ib->init) {
          if (!(p = (struct iovec *)malloc(sizeof(*p) * n))) return -1;
          memcpy(p, ib->init, sizeof(ib->init));
        } else {
          if (!(p = (struct iovec *)realloc(p, sizeof(*p) * n))) return -1;
        }
        ib->p = p;
        ib->n = n;
      }
      p[i].iov_base = base;
      p[i].iov_len = len;
      ++ib->i;
    }
  }
  return 0;
}

int AppendIovsReal(struct Machine *m, struct Iovs *ib, i64 addr, u64 size,
                   int prot) {
  void *real;
  unsigned got;
  u64 have, mask, need;
  if (size > NUMERIC_MAX(ssize_t)) return einval();
  need = 0;
  mask = 0;
  if (prot & PROT_READ) {
    mask |= PAGE_U;
    need |= PAGE_U;
  }
  if (prot & PROT_WRITE) {
    mask |= PAGE_RW;
    need |= PAGE_RW;
  }
  while (size && ib->i < GetIovMax()) {
    if (!(real = LookupAddress2(m, addr, mask, need))) return efault();
    have = 4096 - (addr & 4095);
    got = MIN(size, have);
    if (AppendIovs(ib, real, got) == -1) return -1;
    addr += got;
    size -= got;
  }
  return 0;
}

int AppendIovsGuest(struct Machine *m, struct Iovs *iv, i64 iovaddr, int iovlen,
                    int prot) {
  int rc;
  size_t i, iovsize;
  const struct iovec_linux *guestiovs;
  if (iovlen > IOV_MAX_LINUX) return einval();
  iovsize = iovlen * sizeof(struct iovec_linux);
  if ((guestiovs = (const struct iovec_linux *)SchlepR(m, iovaddr, iovsize))) {
    for (rc = i = 0; i < iovlen && iv->i < GetIovMax(); ++i) {
      if (AppendIovsReal(m, iv, Read64(guestiovs[i].base),
                         Read64(guestiovs[i].len), prot) == -1) {
        rc = -1;
        break;
      }
    }
    return rc;
  } else {
    return -1;
  }
}
