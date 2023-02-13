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
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/atomic.h"
#include "blink/builtin.h"
#include "blink/errno.h"
#include "blink/fds.h"
#include "blink/log.h"
#include "blink/macros.h"
#include "blink/thread.h"

void InitFds(struct Fds *fds) {
  fds->list = 0;
  unassert(!pthread_mutex_init(&fds->lock, 0));
}

struct Fd *AddFd(struct Fds *fds, int fildes, int oflags) {
  struct Fd *fd;
  if (fildes >= 0) {
    if ((fd = (struct Fd *)calloc(1, sizeof(*fd)))) {
      dll_init(&fd->elem);
      fd->cb = &kFdCbHost;
      fd->fildes = fildes;
      fd->oflags = oflags;
      unassert(!pthread_mutex_init(&fd->lock, 0));
      dll_make_first(&fds->list, &fd->elem);
    }
    return fd;
  } else {
    einval();
    return 0;
  }
}

struct Fd *ForkFd(struct Fds *fds, struct Fd *fd, int fildes, int oflags) {
  struct Fd *fd2;
  if ((fd2 = AddFd(fds, fildes, oflags))) {
    if (fd) {
      fd2->socktype = fd->socktype;
      fd2->norestart = fd->norestart;
      memcpy(&fd2->saddr, &fd->saddr, sizeof(fd->saddr));
    }
  }
  return fd2;
}

struct Fd *GetFd(struct Fds *fds, int fildes) {
  bool lru;
  struct Dll *e;
  if (fildes >= 0) {
    lru = false;
    for (e = dll_first(fds->list); e; e = dll_next(fds->list, e)) {
      if (FD_CONTAINER(e)->fildes == fildes) {
        if (lru) {
          dll_remove(&fds->list, e);
          dll_make_first(&fds->list, e);
        }
        return FD_CONTAINER(e);
      }
      lru = true;
    }
  }
  ebadf();
  return 0;
}

void LockFd(struct Fd *fd) {
  LOCK(&fd->lock);
}

void UnlockFd(struct Fd *fd) {
  UNLOCK(&fd->lock);
}

int CountFds(struct Fds *fds) {
  int n = 0;
  struct Dll *e;
  for (e = dll_first(fds->list); e; e = dll_next(fds->list, e)) {
    ++n;
  }
  return n;
}

void FreeFd(struct Fd *fd) {
  if (fd) {
    unassert(!pthread_mutex_destroy(&fd->lock));
    free(fd);
  }
}

void DestroyFds(struct Fds *fds) {
  struct Dll *e, *e2;
  for (e = dll_first(fds->list); e; e = e2) {
    e2 = dll_next(fds->list, e);
    dll_remove(&fds->list, e);
    FreeFd(FD_CONTAINER(e));
  }
  unassert(!fds->list);
  unassert(!pthread_mutex_destroy(&fds->lock));
}

static int GetFdSocketType(int fildes, int *type) {
  socklen_t len = sizeof(*type);
  return getsockopt(fildes, SOL_SOCKET, SO_TYPE, type, &len);
}

static bool IsNoRestartSocket(int fildes) {
  struct timeval tv = {0};
  socklen_t len = sizeof(tv);
  getsockopt(fildes, SOL_SOCKET, SO_RCVTIMEO, &tv, &len);
  return tv.tv_sec || tv.tv_usec;
}

void InheritFd(struct Fd *fd) {
#ifndef DISABLE_SOCKETS
  socklen_t addrlen;
  if (!fd) return;
  if (!GetFdSocketType(fd->fildes, &fd->socktype)) {
    fd->norestart = IsNoRestartSocket(fd->fildes);
    addrlen = sizeof(fd->saddr);
    getsockname(fd->fildes, (struct sockaddr *)&fd->saddr, &addrlen);
  }
#endif
}

void AddStdFd(struct Fds *fds, int fildes) {
  int flags;
  if ((flags = fcntl(fildes, F_GETFL, 0)) >= 0) {
    InheritFd(AddFd(fds, fildes, flags));
  }
}
