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
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/atomic.h"
#include "blink/debug.h"
#include "blink/errno.h"
#include "blink/fds.h"
#include "blink/fspath.h"
#include "blink/log.h"
#include "blink/overlays.h"
#include "blink/random.h"
#include "blink/syscall.h"
#include "blink/thread.h"
#include "blink/vfs.h"
#include "blink/xlat.h"

static int SysTmpfile(struct Machine *m, i32 dirfildes, i64 pathaddr,
                      i32 oflags, i32 mode) {
  long i;
  int lim;
  u64 rng;
  int tmpdir;
  int fildes;
  int sysflags;
  char name[13];
  int supported;
  int unsupported;
  sigset_t ss, oldss;
  sysflags = O_CREAT | O_EXCL | O_CLOEXEC;
  switch (oflags & O_ACCMODE_LINUX) {
    case O_RDWR_LINUX:
      sysflags |= O_RDWR;
      break;
    case O_WRONLY_LINUX:
      sysflags |= O_WRONLY;
      break;
    default:
      LOGF("O_TMPFILE must O_WRONLY or O_RDWR");
      return einval();
  }
  supported =
      O_ACCMODE_LINUX | O_CLOEXEC_LINUX | O_EXCL_LINUX | O_LARGEFILE_LINUX;
  if ((unsupported = oflags & ~supported)) {
    LOGF("O_TMPFILE unsupported flags %#x", unsupported);
    return einval();
  }
  if (!(lim = GetFileDescriptorLimit(m->system))) return emfile();
  unassert(!sigfillset(&ss));
  unassert(!pthread_sigmask(SIG_BLOCK, &ss, &oldss));
  if ((tmpdir = VfsOpen(GetDirFildes(dirfildes), LoadStr(m, pathaddr),
                        O_RDONLY | O_DIRECTORY | O_CLOEXEC, 0)) != -1) {
    if (tmpdir >= lim) {
      VfsClose(tmpdir);
      fildes = emfile();
    } else {
      unassert(GetRandom(&rng, 8, 0) == 8);
      for (i = 0; i < 12; ++i) {
        name[i] = "0123456789abcdefghijklmnopqrstuvwxyz"[rng % 36];
        rng /= 36;
      }
      name[i] = 0;
      if ((fildes = VfsOpen(tmpdir, name, sysflags, mode)) != -1) {
        unassert(!VfsUnlink(tmpdir, name, 0));
        unassert(VfsDup2(fildes, tmpdir) == tmpdir);
        fildes = tmpdir;
        if (oflags & O_CLOEXEC_LINUX) {
          unassert(!VfsFcntl(fildes, F_SETFD, FD_CLOEXEC));
        }
        LOCK(&m->system->fds.lock);
        unassert(AddFd(&m->system->fds, fildes, oflags));
        UNLOCK(&m->system->fds.lock);
      } else {
        unassert(!VfsClose(tmpdir));
      }
    }
  } else {
    fildes = -1;
  }
  unassert(!pthread_sigmask(SIG_SETMASK, &oldss, 0));
  return fildes;
}

int SysOpenat(struct Machine *m, i32 dirfildes, i64 pathaddr, i32 oflags,
              i32 mode) {
  int lim;
  int fildes;
  int sysflags;
  struct Fd *fd;
  const char *path;
#ifndef O_TMPFILE
#ifndef DISABLE_NONPOSIX
  if ((oflags & O_TMPFILE_LINUX) == O_TMPFILE_LINUX) {
    return SysTmpfile(m, dirfildes, pathaddr, oflags & ~O_TMPFILE_LINUX, mode);
  }
#endif
#endif
  if ((sysflags = XlatOpenFlags(oflags)) == -1) return -1;
  if (!(lim = GetFileDescriptorLimit(m->system))) return emfile();
  if (!(path = LoadStr(m, pathaddr))) return -1;
  RESTARTABLE(fildes = VfsOpen(GetDirFildes(dirfildes), path, sysflags, mode));
  if (fildes != -1) {
    if (fildes >= lim) {
      close(fildes);
      fildes = emfile();
    } else {
      LOCK(&m->system->fds.lock);
      unassert(fd = AddFd(&m->system->fds, fildes, sysflags));
      fd->path = JoinPath(GetDirFildesPath(m->system, dirfildes), path);
      UNLOCK(&m->system->fds.lock);
    }
  } else {
#ifdef __FreeBSD__
    // Address FreeBSD divergence from IEEE Std 1003.1-2008 (POSIX.1)
    // in the case when O_NOFOLLOW is used, but fails due to symlink.
    if (errno == EMLINK) {
      errno = ELOOP;
    }
#endif
#ifdef __NetBSD__
    // Address NetBSD divergence from IEEE Std 1003.1-2008 (POSIX.1)
    // in the case when O_NOFOLLOW is used but fails due to symlink.
    if (errno == EFTYPE) {
      errno = ELOOP;
    }
#endif
  }
  return fildes;
}
