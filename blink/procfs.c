/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2023 Trung Nguyen                                                  │
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
#include "blink/errno.h"
#include "blink/hostfs.h"
#include "blink/log.h"
#include "blink/procfs.h"

#ifndef DISABLE_VFS

static int ProcfsInit(const char *source, u64 flags, const void *data,
                      struct VfsDevice **device, struct VfsMount **mount) {
  if (source == NULL) {
    return efault();
  }
  if (*source == '\0') {
    source = "/proc";
  }
  VFS_LOGF("real procfs not implemented, delegating to hostfs");
  return HostfsInit(source, flags, data, device, mount);
}

struct VfsSystem g_procfs = {.name = "procfs",
                             .ops = {
                                 .init = ProcfsInit,
                                 .freeinfo = HostfsFreeInfo,
                                 .freedevice = HostfsFreeDevice,
                                 .finddir = HostfsFinddir,
                                 .readlink = HostfsReadlink,
                                 .mkdir = HostfsMkdir,
                                 .open = HostfsOpen,
                                 .access = HostfsAccess,
                                 .stat = HostfsStat,
                                 .fstat = HostfsFstat,
                                 .chmod = HostfsChmod,
                                 .fchmod = HostfsFchmod,
                                 .chown = HostfsChown,
                                 .fchown = HostfsFchown,
                                 .ftruncate = HostfsFtruncate,
                                 .link = HostfsLink,
                                 .unlink = HostfsUnlink,
                                 .read = HostfsRead,
                                 .write = HostfsWrite,
                                 .pread = HostfsPread,
                                 .pwrite = HostfsPwrite,
                                 .readv = HostfsReadv,
                                 .writev = HostfsWritev,
                                 .preadv = HostfsPreadv,
                                 .pwritev = HostfsPwritev,
                                 .seek = HostfsSeek,
                                 .fsync = HostfsFsync,
                                 .fdatasync = HostfsFdatasync,
                                 .flock = HostfsFlock,
                                 .fcntl = HostfsFcntl,
                                 .ioctl = HostfsIoctl,
                                 .dup = HostfsDup,
#ifdef HAVE_DUP3
                                 .dup3 = HostfsDup3,
#endif
                                 .poll = HostfsPoll,
                                 .opendir = HostfsOpendir,
#ifdef HAVE_SEEKDIR
                                 .seekdir = HostfsSeekdir,
                                 .telldir = HostfsTelldir,
#endif
                                 .readdir = HostfsReaddir,
                                 .rewinddir = HostfsRewinddir,
                                 .closedir = HostfsClosedir,
                                 .bind = HostfsBind,
                                 .connect = HostfsConnect,
                                 .connectunix = HostfsConnectUnix,
                                 .accept = HostfsAccept,
                                 .listen = HostfsListen,
                                 .shutdown = HostfsShutdown,
                                 .recvmsg = HostfsRecvmsg,
                                 .sendmsg = HostfsSendmsg,
                                 .recvmsgunix = HostfsRecvmsgUnix,
                                 .sendmsgunix = HostfsSendmsgUnix,
                                 .getsockopt = HostfsGetsockopt,
                                 .setsockopt = HostfsSetsockopt,
                                 .getsockname = HostfsGetsockname,
                                 .getpeername = HostfsGetpeername,
                                 .rename = HostfsRename,
                                 .utime = HostfsUtime,
                                 .futime = HostfsFutime,
                                 .symlink = HostfsSymlink,
                                 .mmap = HostfsMmap,
                                 .munmap = HostfsMunmap,
                                 .mprotect = HostfsMprotect,
                                 .msync = HostfsMsync,
                                 .pipe = NULL,
#ifdef HAVE_PIPE2
                                 .pipe2 = NULL,
#endif
                                 .socket = NULL,
                                 .socketpair = NULL,
                                 .tcgetattr = NULL,
                                 .tcsetattr = NULL,
                                 .tcflush = NULL,
                                 .tcdrain = NULL,
                                 .tcsendbreak = NULL,
                                 .tcflow = NULL,
                                 .tcgetsid = NULL,
                                 .tcgetpgrp = NULL,
                                 .tcsetpgrp = NULL,
                                 .sockatmark = NULL,
                                 .fexecve = NULL,
                             }};

#endif /* DISABLE_VFS */
