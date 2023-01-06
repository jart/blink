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
#include <stdatomic.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/debug.h"
#include "blink/errno.h"
#include "blink/fds.h"
#include "blink/linux.h"
#include "blink/log.h"
#include "blink/syscall.h"

int SysDup(struct Machine *m, i32 fildes, i32 newfildes, i32 flags, i32 minfd) {
  int systemfd;
  struct Fd *fd1, *fd2, *freeme;
  unassert(newfildes >= -1);
  if (minfd < 0) {
    LOGF("dup minfd is negative");
    return einval();
  }
  if (flags & ~O_CLOEXEC_LINUX) {
    LOGF("dup bad flags");
    return einval();
  }
  LockFds(&m->system->fds);
  if ((fd1 = GetFd(&m->system->fds, fildes))) {
    if (fildes == newfildes) {
      UnlockFds(&m->system->fds);
      return fildes;
    }
    LockFd(fd1);
    if (newfildes >= 0) {
      fd2 = GetFd(&m->system->fds, newfildes);
      if (!fd2) minfd = newfildes;
    } else {
      fd2 = 0;
    }
    if (!fd2) {
      fd2 = freeme = AllocateFd(&m->system->fds, minfd, 0);
    } else {
      freeme = 0;
    }
    if (fd2) {
      LockFd(fd2);
    }
  } else {
    freeme = 0;
    fd2 = 0;
  }
  if (fd1 && fd2) {
    if (fd1 != fd2) {
      systemfd = atomic_load_explicit(&fd1->systemfd, memory_order_relaxed);
      systemfd = fcntl(systemfd, flags ? F_DUPFD_CLOEXEC : F_DUPFD,
                       kMinBlinkFd + fd2->fildes);
      if (systemfd != -1) {
        fildes = fd2->fildes;
        fd2->cb = fd1->cb;
        fd2->cloexec = !!flags;
        fd2->systemfd = systemfd;
        fd2->oflags = fd1->oflags;
      } else {
        fildes = -1;
      }
    } else {
      fildes = fd2->fildes;
    }
  } else {
    fildes = -1;
  }
  UnlockFds(&m->system->fds);
  if (fd1) UnlockFd(fd1);
  if (fd2) UnlockFd(fd2);
  if (freeme && fildes == -1) DropFd(m, freeme);
  return fildes;
}
