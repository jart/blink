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
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/debug.h"
#include "blink/dll.h"
#include "blink/errno.h"
#include "blink/fds.h"
#include "blink/lock.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/syscall.h"

static int CloseFd(struct Fd *fd) {
  int rc;
  unassert(fd->cb);
  if (fd->dirstream) {
    rc = closedir(fd->dirstream);
  } else {
    rc = fd->cb->close(fd->fildes);
  }
  FreeFd(fd);
  return rc;
}

static int CloseFds(struct Dll *fds) {
  int rc = 0;
  struct Dll *e, *e2;
  for (e = dll_first(fds); e; e = e2) {
    e2 = dll_next(fds, e);
    dll_remove(&fds, e);
    rc |= CloseFd(FD_CONTAINER(e));
  }
  return rc;
}

static int FinishClose(struct Machine *m, int rc) {
  if (rc == -1 && errno == EINTR && !CheckInterrupt(m, true)) rc = 0;
  return rc;
}

int SysClose(struct Machine *m, i32 fildes) {
  struct Fd *fd;
  LOCK(&m->system->fds.lock);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    dll_remove(&m->system->fds.list, &fd->elem);
  }
  UNLOCK(&m->system->fds.lock);
  if (!fd) return -1;
  return FinishClose(m, CloseFd(fd));
}

static int SysCloseRangeCloexec(struct Machine *m, u32 first, u32 last) {
  struct Fd *fd;
  struct Dll *e;
  LOCK(&m->system->fds.lock);
  for (e = dll_first(m->system->fds.list); e;
       e = dll_next(m->system->fds.list, e)) {
    fd = FD_CONTAINER(e);
    if (first <= fd->fildes && fd->fildes <= last) {
      if (~fd->oflags & O_CLOEXEC) {
        fd->oflags |= O_CLOEXEC;
        fcntl(fd->fildes, F_SETFD, FD_CLOEXEC);
      }
    }
  }
  UNLOCK(&m->system->fds.lock);
  return 0;
}

int SysCloseRange(struct Machine *m, u32 first, u32 last, u32 flags) {
  int rc;
  struct Fd *fd;
  sigset_t block, oldmask;
  struct Dll *e, *e2, *fds;
  if ((flags & ~CLOSE_RANGE_CLOEXEC_LINUX) || first > last) {
    return einval();
  }
  if (flags & CLOSE_RANGE_CLOEXEC_LINUX) {
    return SysCloseRangeCloexec(m, first, last);
  }
  LOCK(&m->system->fds.lock);
  for (fds = 0, e = dll_first(m->system->fds.list); e; e = e2) {
    fd = FD_CONTAINER(e);
    e2 = dll_next(m->system->fds.list, e);
    if (first <= fd->fildes && fd->fildes <= last) {
      dll_remove(&m->system->fds.list, e);
      dll_make_last(&fds, e);
    }
  }
  UNLOCK(&m->system->fds.lock);
  unassert(!sigfillset(&block));
  unassert(!pthread_sigmask(SIG_BLOCK, &block, &oldmask));
  rc = CloseFds(fds);
  unassert(!pthread_sigmask(SIG_SETMASK, &oldmask, 0));
  unassert(!(rc == -1 && errno == EINTR));
  return rc;
}

void SysCloseExec(struct System *s) {
  struct Fd *fd;
  struct Dll *e, *e2, *fds;
  LOCK(&s->fds.lock);
  for (fds = 0, e = dll_first(s->fds.list); e; e = e2) {
    fd = FD_CONTAINER(e);
    e2 = dll_next(s->fds.list, e);
    if (fd->oflags & O_CLOEXEC) {
      dll_remove(&s->fds.list, e);
      dll_make_last(&fds, e);
    }
  }
  UNLOCK(&s->fds.lock);
  CloseFds(fds);
}
