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
#include <fcntl.h>
#include <limits.h>
#include <stdatomic.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/debug.h"
#include "blink/dll.h"
#include "blink/errno.h"
#include "blink/fds.h"
#include "blink/log.h"
#include "blink/machine.h"

static int CloseFd(struct System *s, struct Fd *fd) {
  int sf, rc;
  unassert(fd->cb);
  if ((sf = atomic_load_explicit(&fd->systemfd, memory_order_acquire)) >= 0) {
    if (fd->dirstream) {
      rc = closedir(fd->dirstream);
    } else {
      rc = IB(fd->cb->close)(sf);
    }
    FreeFd(&s->fds, fd);
  } else {
    rc = ebadf();
  }
  return rc;
}

int SysClose(struct System *s, i32 fildes) {
  int rc;
  struct Fd *fd;
  LockFds(&s->fds);
  if ((fd = GetFd(&s->fds, fildes))) {
    rc = CloseFd(s, fd);
  } else {
    rc = -1;
  }
  UnlockFds(&s->fds);
  return rc;
}

int SysCloseRange(struct System *s, u32 first, u32 last, u32 flags) {
  int rc;
  struct Fd *fd;
  struct Dll *e, *e2;
  if (flags || first > last) {
    return einval();
  }
  LockFds(&s->fds);
  for (rc = 0, e = dll_first(s->fds.list); e; e = e2) {
    fd = FD_CONTAINER(e);
    e2 = dll_next(s->fds.list, e);
    if (first <= fd->fildes && fd->fildes <= last) {
      rc |= CloseFd(s, fd);
    }
  }
  UnlockFds(&s->fds);
  return rc;
}

void SysCloseExec(struct System *s) {
  struct Fd *fd;
  struct Dll *e, *e2;
  LockFds(&s->fds);
  for (e = dll_first(s->fds.list); e; e = e2) {
    fd = FD_CONTAINER(e);
    e2 = dll_next(s->fds.list, e);
    if (fd->cloexec) {
      CloseFd(s, fd);
    }
  }
  UnlockFds(&s->fds);
}
