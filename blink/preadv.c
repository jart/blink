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
#include "blink/preadv.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blink/errno.h"
#include "blink/iovs.h"
#include "blink/limits.h"
#include "blink/macros.h"

// preadv() and pwritev() need MacOS 11+ c. 2020

ssize_t preadv_(int fd, const struct iovec *iov, int iovcnt, off_t offset) {
  int i;
  char *p;
  ssize_t rc;
  size_t j, n, m, t;
  if (offset < 0 || iovcnt <= 0 || iovcnt > GetIovMax()) {
    return einval();
  }
  for (n = 0, i = 0; i < iovcnt; ++i) {
    t = n + iov[i].iov_len;
    if (t < n || n > NUMERIC_MAX(ssize_t)) {
      return einval();
    }
    n = t;
  }
  if (!(p = (char *)malloc(n))) {
    return enomem();
  }
  if ((rc = pread(fd, p, n, offset)) != -1) {
    for (j = 0, i = 0; i < iovcnt && j < rc; ++i, j += m) {
      m = MIN(iov[i].iov_len, rc - j);
      memcpy(iov[i].iov_base, p + j, m);
    }
  }
  free(p);
  return rc;
}

ssize_t pwritev_(int fd, const struct iovec *iov, int iovcnt, off_t offset) {
  int i;
  char *p;
  ssize_t rc;
  size_t j, n, m, t;
  if (offset < 0 || iovcnt <= 0 || iovcnt > GetIovMax()) {
    return einval();
  }
  for (n = 0, i = 0; i < iovcnt; ++i) {
    t = n + iov[i].iov_len;
    if (t < n || n > NUMERIC_MAX(ssize_t)) {
      return einval();
    }
    n = t;
  }
  if (!(p = (char *)malloc(n))) {
    return enomem();
  }
  for (j = 0, i = 0; i < iovcnt && j < n; ++i, j += m) {
    m = MIN(iov[i].iov_len, n - j);
    memcpy(p + j, iov[i].iov_base, m);
  }
  rc = pwrite(fd, p, n, offset);
  free(p);
  return rc;
}
