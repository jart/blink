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
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "blink/assert.h"
#include "blink/errno.h"
#include "blink/fds.h"
#include "blink/lock.h"
#include "blink/log.h"
#include "blink/macros.h"

// TODO(jart): We should track the first hole.
// TODO(jart): We should track recently used fds.

void InitFds(struct Fds *fds) {
  fds->list = 0;
  pthread_mutex_init(&fds->lock, 0);
}

void LockFds(struct Fds *fds) {
  LOCK(&fds->lock);
}

struct Fd *AllocateFd(struct Fds *fds, int minfd, int oflags) {
  struct Fd *fd;
  struct Dll *e1, *e2;
  if (minfd < 0) {
    einval();
    return 0;
  }
  if ((fd = (struct Fd *)calloc(1, sizeof(*fd)))) {
    dll_init(&fd->elem);
    fd->cb = &kFdCbHost;
    fd->oflags = oflags & ~O_CLOEXEC;
    fd->cloexec = !!(oflags & O_CLOEXEC);
    pthread_mutex_init(&fd->lock, 0);
    atomic_store_explicit(&fd->systemfd, -1, memory_order_release);
    if (!(e1 = dll_first(fds->list)) || minfd < FD_CONTAINER(e1)->fildes) {
      fd->fildes = minfd;
      dll_make_first(&fds->list, &fd->elem);
    } else {
      for (;; e1 = e2) {
        if ((!(e2 = dll_next(fds->list, e1)) ||
             (FD_CONTAINER(e1)->fildes < minfd &&
              minfd < FD_CONTAINER(e2)->fildes))) {
          fd->fildes = MAX(FD_CONTAINER(e1)->fildes + 1, minfd);
          if (e2) {
            dll_splice_after(e1, &fd->elem);
          } else {
            dll_make_last(&fds->list, &fd->elem);
          }
          break;
        }
      }
    }
  }
  return fd;
}

struct Fd *GetFd(struct Fds *fds, int fildes) {
  struct Dll *e;
  if (fildes >= 0) {
    for (e = dll_first(fds->list); e; e = dll_next(fds->list, e)) {
      if (FD_CONTAINER(e)->fildes == fildes) {
        if (atomic_load_explicit(&FD_CONTAINER(e)->systemfd,
                                 memory_order_acquire) >= 0) {
          return FD_CONTAINER(e);
        } else {
          break;
        }
      }
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

void FreeFd(struct Fds *fds, struct Fd *fd) {
  if (fd) {
    unassert(!pthread_mutex_destroy(&fd->lock));
    dll_remove(&fds->list, &fd->elem);
    free(fd);
  }
}

void UnlockFds(struct Fds *fds) {
  UNLOCK(&fds->lock);
}

void DestroyFds(struct Fds *fds) {
  struct Dll *e, *e2;
  for (e = dll_first(fds->list); e; e = e2) {
    e2 = dll_next(fds->list, e);
    FreeFd(fds, FD_CONTAINER(e));
  }
  unassert(!fds->list);
  unassert(!pthread_mutex_destroy(&fds->lock));
}
