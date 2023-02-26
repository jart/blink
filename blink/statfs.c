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
#include <sys/types.h>
// ordering matters on openbsd
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/statvfs.h>

#include "blink/endian.h"
#include "blink/errno.h"
#include "blink/linux.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/syscall.h"
#include "blink/types.h"

#ifdef __linux
#include <sys/vfs.h>
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <sys/disklabel.h>
#endif

struct FsId {
  u32 val[2];
};

static unsigned long XlatStatvfsFlags(unsigned long flags) {
  unsigned long res = 0;
#ifdef MNT_RDONLY
  if ((flags & MNT_RDONLY) == MNT_RDONLY) res |= ST_RDONLY_LINUX;
#elif defined(ST_RDONLY)
  if ((flags & ST_RDONLY) == ST_RDONLY) res |= ST_RDONLY_LINUX;
#endif
#ifdef MNT_NOSUID
  if ((flags & MNT_NOSUID) == MNT_NOSUID) res |= ST_NOSUID_LINUX;
#elif defined(ST_NOSUID)
  if ((flags & ST_NOSUID) == ST_NOSUID) res |= ST_NOSUID_LINUX;
#endif
#ifdef MNT_NODEV
  if ((flags & MNT_NODEV) == MNT_NODEV) res |= ST_NODEV_LINUX;
#elif defined(ST_NODEV)
  if ((flags & ST_NODEV) == ST_NODEV) res |= ST_NODEV_LINUX;
#endif
#ifdef MNT_NOEXEC
  if ((flags & MNT_NOEXEC) == MNT_NOEXEC) res |= ST_NOEXEC_LINUX;
#elif defined(ST_NOEXEC)
  if ((flags & ST_NOEXEC) == ST_NOEXEC) res |= ST_NOEXEC_LINUX;
#endif
#ifdef MNT_SYNCHRONOUS
  if ((flags & MNT_SYNCHRONOUS) == MNT_SYNCHRONOUS) res |= ST_SYNCHRONOUS_LINUX;
#elif defined(ST_SYNCHRONOUS)
  if ((flags & ST_SYNCHRONOUS) == ST_SYNCHRONOUS) res |= ST_SYNCHRONOUS_LINUX;
#endif
#ifdef MNT_NOATIME
  if ((flags & MNT_NOATIME) == MNT_NOATIME) res |= ST_NOATIME_LINUX;
#elif defined(ST_NOATIME)
  if ((flags & ST_NOATIME) == ST_NOATIME) res |= ST_NOATIME_LINUX;
#endif
#ifdef MNT_RELATIME
  if ((flags & MNT_RELATIME) == MNT_RELATIME) res |= ST_RELATIME_LINUX;
#elif defined(ST_RELATIME)
  if ((flags & ST_RELATIME) == ST_RELATIME) res |= ST_RELATIME_LINUX;
#endif
#ifdef MNT_APPEND
  if ((flags & MNT_APPEND) == MNT_APPEND) res |= ST_APPEND_LINUX;
#elif defined(ST_APPEND)
  if ((flags & ST_APPEND) == ST_APPEND) res |= ST_APPEND_LINUX;
#endif
#ifdef MNT_IMMUTABLE
  if ((flags & MNT_IMMUTABLE) == MNT_IMMUTABLE) res |= ST_IMMUTABLE_LINUX;
#elif defined(ST_IMMUTABLE)
  if ((flags & ST_IMMUTABLE) == ST_IMMUTABLE) res |= ST_IMMUTABLE_LINUX;
#endif
#ifdef MNT_MANDLOCK
  if ((flags & MNT_MANDLOCK) == MNT_MANDLOCK) res |= ST_MANDLOCK_LINUX;
#elif defined(ST_MANDLOCK)
  if ((flags & ST_MANDLOCK) == ST_MANDLOCK) res |= ST_MANDLOCK_LINUX;
#endif
#ifdef MNT_NODIRATIME
  if ((flags & MNT_NODIRATIME) == MNT_NODIRATIME) res |= ST_NODIRATIME_LINUX;
#elif defined(ST_NODIRATIME)
  if ((flags & ST_NODIRATIME) == ST_NODIRATIME) res |= ST_NODIRATIME_LINUX;
#endif
#ifdef MNT_WRITE
  if ((flags & MNT_WRITE) == MNT_WRITE) res |= ST_WRITE_LINUX;
#elif defined(ST_WRITE)
  if ((flags & ST_WRITE) == ST_WRITE) res |= ST_WRITE_LINUX;
#endif
  return res;
}

static int ConvertBsdFsTypeNameToLinux(const char *fstypename) {
  size_t i;
  static const struct FsTypeName {
    char name[8];
    u32 lunix;
  } kFsTypeName[] = {
      {"apfs", 0x4253584e},                                    //
      {"zfs", 0x2fc12fc1},                                     //
      {"ufs", 0x00011954},                                     //
      {"iso9660", 0x9660},                                     //
      {"ext", 0x4244},                                         //
      {"ext2fs", 0x4244},                                      //
      {"hfs", 0x4244},                                         //
      {"msdos", 0x4d44},                                       //
      {"ntfs", 0x5346544e},                                    //
      {"efs", 0x00414a53},                                     //
      {"jfs2", 0x3153464a},                                    //
      {"udf", 0x15013346},                                     //
      {"hpfs", 0xf995e849},                                    //
      {"sysv", 0x012ff7b5},                                    //
      {"devfs", 0x1373},                                       //
      {"procfs", 0x002f},                                      //
      {{'l', 'i', 'n', 's', 'y', 's', 'f', 's'}, 0x62656572},  // linsysfs
  };
  for (i = 0; i < ARRAYLEN(kFsTypeName); ++i) {
    if (Read64((const u8 *)fstypename) ==
        Read64((const u8 *)kFsTypeName[i].name)) {
      return kFsTypeName[i].lunix;
    }
  }
  return 0;
}

static void XlatStatvfsToLinux(struct statfs_linux *sf, const void *arg) {
#ifdef __linux
  struct FsId fsid;
  const struct statfs *vfs = (const struct statfs *)arg;
  memset(sf, 0, sizeof(*sf));
  memcpy(&fsid, &vfs->f_fsid, sizeof(fsid));
  Write64(sf->type, vfs->f_type);
  Write64(sf->bsize, vfs->f_bsize);
  Write64(sf->frsize, vfs->f_frsize);
  Write64(sf->blocks, vfs->f_blocks);
  Write64(sf->bfree, vfs->f_bfree);
  Write64(sf->bavail, vfs->f_bavail);
  Write64(sf->files, vfs->f_files);
  Write64(sf->ffree, vfs->f_ffree);
  Write32(sf->fsid[0], fsid.val[0]);
  Write32(sf->fsid[1], fsid.val[1]);
  Write64(sf->flags, XlatStatvfsFlags(vfs->f_flags));
  Write64(sf->namelen, vfs->f_namelen);
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__NetBSD__)
  struct FsId fsid;
  const struct statfs *vfs = (const struct statfs *)arg;
  memset(sf, 0, sizeof(*sf));
  memcpy(&fsid, &vfs->f_fsid, sizeof(fsid));
  Write64(sf->type, ConvertBsdFsTypeNameToLinux(vfs->f_fstypename));
  Write64(sf->bsize, vfs->f_iosize);
  Write64(sf->frsize, vfs->f_bsize);
  Write64(sf->blocks, vfs->f_blocks);
  Write64(sf->bfree, vfs->f_bfree);
  Write64(sf->bavail, vfs->f_bavail);
  Write64(sf->files, vfs->f_files);
  Write64(sf->ffree, vfs->f_ffree);
  Write32(sf->fsid[0], fsid.val[0]);
  Write32(sf->fsid[1], fsid.val[1]);
  Write64(sf->flags, XlatStatvfsFlags(vfs->f_flags));
#if defined(__APPLE__) || defined(__NetBSD__)
  Write64(sf->namelen, NAME_MAX);
#else
  Write64(sf->namelen, vfs->f_namemax);
#endif
#else
  const struct statvfs *vfs = (const struct statvfs *)arg;
  memset(sf, 0, sizeof(*sf));
  Write64(sf->bsize, vfs->f_bsize);
  Write64(sf->frsize, vfs->f_frsize);
  Write64(sf->blocks, vfs->f_blocks);
  Write64(sf->bfree, vfs->f_bfree);
  Write64(sf->bavail, vfs->f_bavail);
  Write64(sf->files, vfs->f_files);
  Write64(sf->ffree, vfs->f_ffree);
  Write32(sf->fsid[0], vfs->f_fsid);
  Write32(sf->fsid[1], (u64)vfs->f_fsid >> 32);
  Write64(sf->flags, XlatStatvfsFlags(vfs->f_flag));
  Write64(sf->namelen, vfs->f_namemax);
#endif
}

#if defined(__linux) || defined(__APPLE__) || defined(__FreeBSD__) || \
    defined(__OpenBSD__) || defined(__NetBSD__)
static int Statfs(uintptr_t arg, struct statfs *buf) {
  return statfs((const char *)arg, buf);
}
static int Fstatfs(uintptr_t arg, struct statfs *buf) {
  return fstatfs((int)arg, buf);
}
static int SysStatfsImpl(struct Machine *m, uintptr_t arg, i64 addr,
                         int thunk(uintptr_t, struct statfs *)) {
  int rc;
  struct statfs vfs;
  struct statfs_linux sf;
  if (!IsValidMemory(m, addr, sizeof(sf), PROT_WRITE)) return efault();
  RESTARTABLE(rc = thunk(arg, &vfs));
  if (rc != -1) {
    XlatStatvfsToLinux(&sf, &vfs);
    CopyToUserWrite(m, addr, &sf, sizeof(sf));
  }
  return rc;
}
#else
static int Statfs(uintptr_t arg, struct statvfs *buf) {
  return statvfs((const char *)arg, buf);
}
static int Fstatfs(uintptr_t arg, struct statvfs *buf) {
  return fstatvfs((int)arg, buf);
}
static int SysStatfsImpl(struct Machine *m, uintptr_t arg, i64 addr,
                         int thunk(uintptr_t, struct statvfs *)) {
  int rc;
  struct statvfs vfs;
  struct statfs_linux sf;
  if (!IsValidMemory(m, addr, sizeof(sf), PROT_WRITE)) return efault();
  RESTARTABLE(rc = thunk(arg, &vfs));
  if (rc != -1) {
    XlatStatvfsToLinux(&sf, &vfs);
    CopyToUserWrite(m, addr, &sf, sizeof(sf));
  }
  return rc;
}
#endif

int SysStatfs(struct Machine *m, i64 path, i64 addr) {
  return SysStatfsImpl(m, (uintptr_t)LoadStr(m, path), addr, Statfs);
}

int SysFstatfs(struct Machine *m, i32 fildes, i64 addr) {
  return SysStatfsImpl(m, (uintptr_t)(u32)fildes, addr, Fstatfs);
}
