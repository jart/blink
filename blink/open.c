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
#include <stdatomic.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "blink/errno.h"
#include "blink/fds.h"
#include "blink/log.h"
#include "blink/memory.h"
#include "blink/syscall.h"
#include "blink/xlat.h"

int SysOpenat(struct Machine *m, i32 dirfildes, i64 pathaddr, i32 oflags,
              i32 mode) {
  const char *path;
  struct Fd *fd, *dirfd;
  int rc, sf, fildes, sysdirfd;
  if (!(path = LoadStr(m, pathaddr))) return efault();
  if ((oflags = XlatOpenFlags(oflags)) == -1) return -1;
  LockFds(&m->system->fds);
  if ((rc = GetAfd(m, dirfildes, &dirfd)) != -1) {
    if (dirfd) LockFd(dirfd);
    fd = AllocateFd(&m->system->fds, 0, oflags);
  } else {
    dirfd = 0;
    fd = 0;
  }
  UnlockFds(&m->system->fds);
  if (fd && rc != -1) {
    if (dirfd) {
      sysdirfd = atomic_load_explicit(&dirfd->systemfd, memory_order_relaxed);
    } else {
      sysdirfd = AT_FDCWD;
    }
    if ((sf = openat(sysdirfd, path, oflags, mode)) != -1) {
      atomic_store_explicit(&fd->systemfd, sf, memory_order_release);
      fildes = fd->fildes;
    } else {
      LOGF("%s failed: %s", "openat", strerror(errno));
      fildes = -1;
    }
  } else {
    fildes = -1;
  }
  if (dirfd) UnlockFd(dirfd);
  if (fildes == -1 && fd) DropFd(m, fd);
  return fildes;
}
