/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
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
#include <string.h>

#include "blink/endian.h"
#include "blink/types.h"
#include "blink/xlat.h"

static unsigned long XlatStatvfsFlags(unsigned long flags) {
  unsigned long res = 0;
#ifdef ST_RDONLY
  if ((flags & ST_RDONLY) == ST_RDONLY) res |= ST_RDONLY_LINUX;
#elif defined(MNT_RDONLY)
  if ((flags & MNT_RDONLY) == MNT_RDONLY) res |= MNT_RDONLY_LINUX;
#endif
#ifdef ST_NOSUID
  if ((flags & ST_NOSUID) == ST_NOSUID) res |= ST_NOSUID_LINUX;
#elif defined(MNT_NOSUID)
  if ((flags & MNT_NOSUID) == MNT_NOSUID) res |= MNT_NOSUID_LINUX;
#endif
#ifdef ST_NODEV
  if ((flags & ST_NODEV) == ST_NODEV) res |= ST_NODEV_LINUX;
#elif defined(MNT_NODEV)
  if ((flags & MNT_NODEV) == MNT_NODEV) res |= MNT_NODEV_LINUX;
#endif
#ifdef ST_NOEXEC
  if ((flags & ST_NOEXEC) == ST_NOEXEC) res |= ST_NOEXEC_LINUX;
#elif defined(MNT_NOEXEC)
  if ((flags & MNT_NOEXEC) == MNT_NOEXEC) res |= ST_NOEXEC_LINUX;
#endif
#ifdef ST_SYNCHRONOUS
  if ((flags & ST_SYNCHRONOUS) == ST_SYNCHRONOUS) res |= ST_SYNCHRONOUS_LINUX;
#elif defined(MNT_SYNCHRONOUS)
  if ((flags & MNT_SYNCHRONOUS) == MNT_SYNCHRONOUS) res |= ST_SYNCHRONOUS_LINUX;
#endif
#ifdef ST_NOATIME
  if ((flags & ST_NOATIME) == ST_NOATIME) res |= ST_NOATIME_LINUX;
#elif defined(MNT_NOATIME)
  if ((flags & MNT_NOATIME) == MNT_NOATIME) res |= ST_NOATIME_LINUX;
#endif
#ifdef ST_RELATIME
  if ((flags & ST_RELATIME) == ST_RELATIME) res |= ST_RELATIME_LINUX;
#elif defined(MNT_RELATIME)
  if ((flags & MNT_RELATIME) == MNT_RELATIME) res |= MNT_RELATIME_LINUX;
#endif
#ifdef ST_APPEND
  if ((flags & ST_APPEND) == ST_APPEND) res |= ST_APPEND_LINUX;
#elif defined(MNT_APPEND)
  if ((flags & MNT_APPEND) == MNT_APPEND) res |= MNT_APPEND_LINUX;
#endif
#ifdef ST_IMMUTABLE
  if ((flags & ST_IMMUTABLE) == ST_IMMUTABLE) res |= ST_IMMUTABLE_LINUX;
#elif defined(MNT_IMMUTABLE)
  if ((flags & MNT_IMMUTABLE) == MNT_IMMUTABLE) res |= MNT_IMMUTABLE_LINUX;
#endif
#ifdef ST_MANDLOCK
  if ((flags & ST_MANDLOCK) == ST_MANDLOCK) res |= ST_MANDLOCK_LINUX;
#elif defined(MNT_MANDLOCK)
  if ((flags & MNT_MANDLOCK) == MNT_MANDLOCK) res |= MNT_MANDLOCK_LINUX;
#endif
#ifdef ST_NODIRATIME
  if ((flags & ST_NODIRATIME) == ST_NODIRATIME) res |= ST_NODIRATIME_LINUX;
#elif defined(MNT_NODIRATIME)
  if ((flags & MNT_NODIRATIME) == MNT_NODIRATIME) res |= MNT_NODIRATIME_LINUX;
#endif
#ifdef ST_WRITE
  if ((flags & ST_WRITE) == ST_WRITE) res |= ST_WRITE_LINUX;
#elif defined(MNT_WRITE)
  if ((flags & MNT_WRITE) == MNT_WRITE) res |= MNT_WRITE_LINUX;
#endif
  return res;
}

void XlatStatvfsToLinux(struct statfs_linux *sf, const struct statvfs *vfs) {
  memset(sf, 0, sizeof(*sf));
  Write64(sf->bsize, vfs->f_bsize);
  Write64(sf->frsize, vfs->f_frsize);
  Write64(sf->blocks, vfs->f_blocks);
  Write64(sf->bfree, vfs->f_bfree);
  Write64(sf->bavail, vfs->f_bavail);
  Write64(sf->files, vfs->f_files);
  Write64(sf->ffree, vfs->f_ffree);
  Write64(sf->ffree, vfs->f_favail);
  Write32(sf->fsid[0], vfs->f_fsid);
  Write32(sf->fsid[1], (u64)vfs->f_fsid >> 32);
  Write64(sf->flags, XlatStatvfsFlags(vfs->f_flag));
  Write64(sf->namelen, vfs->f_namemax);
}
