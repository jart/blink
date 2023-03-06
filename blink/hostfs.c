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
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "blink/errno.h"
#include "blink/hostfs.h"
#include "blink/log.h"
#include "blink/macros.h"
#include "blink/vfs.h"

#ifndef DISABLE_VFS

struct HostfsDevice {
  const char *source;
};

static u64 HostfsHash(u64 parent, const char *data, size_t size) {
  u64 hash;
  if (data == NULL) {
    efault();
    return 0;
  }
  hash = parent;
  while (size--) {
    hash = *data++ + (hash << 6) + (hash << 16) - hash;
  }
  return hash;
}

int HostfsInit(const char *source, u64 flags, const void *data,
               struct VfsDevice **device, struct VfsMount **mount) {
  struct HostfsDevice *hostdevice;
  struct HostfsInfo *hostfsrootinfo;
  struct stat st;
  if (source == NULL) {
    return efault();
  }
  if (stat(source, &st) == -1) {
    return -1;
  }
  if (!S_ISDIR(st.st_mode)) {
    return enotdir();
  }
  hostdevice = NULL;
  hostfsrootinfo = NULL;
  *device = NULL;
  *mount = NULL;
  hostdevice = malloc(sizeof(struct HostfsDevice));
  if (hostdevice == NULL) {
    return enomem();
  }
  hostdevice->source = strdup(source);
  if (hostdevice->source == NULL) {
    goto cleananddie;
  }
  if (VfsCreateDevice(device) == -1) {
    goto cleananddie;
  }
  (*device)->data = hostdevice;
  (*device)->ops = &g_hostfs.ops;
  *mount = malloc(sizeof(struct VfsMount));
  if (*mount == NULL) {
    goto cleananddie;
  }
  if (VfsCreateInfo(&(*mount)->root) == -1) {
    goto cleananddie;
  }
  unassert(!VfsAcquireDevice(*device, &(*mount)->root->device));
  if (HostfsCreateInfo(&hostfsrootinfo) == -1) {
    goto cleananddie;
  }
  (*mount)->root->data = hostfsrootinfo;
  hostfsrootinfo->mode = st.st_mode;
  (*mount)->root->mode = st.st_mode;
  (*mount)->root->ino =
      HostfsHash(st.st_dev, (const char *)&st.st_ino, sizeof(st.st_ino));
  // Weak reference.
  (*device)->root = (*mount)->root;
  VFS_LOGF("Mounted a hostfs device for \"%s\"", source);
  return 0;
cleananddie:
  if (*device) {
    unassert(!VfsFreeDevice(*device));
  } else {
    if (hostdevice) {
      free((void *)hostdevice->source);
      free(hostdevice);
    }
  }
  if (*mount) {
    if ((*mount)->root) {
      unassert(!VfsFreeInfo((*mount)->root));
    } else {
      free(hostfsrootinfo);
    }
    free(*mount);
  }
  return -1;
}

int HostfsFreeInfo(void *info) {
  struct HostfsInfo *hostfsinfo = (struct HostfsInfo *)info;
  if (info == NULL) {
    return 0;
  }
  VFS_LOGF("HostfsFreeInfo(%p)", info);
  if (S_ISDIR(hostfsinfo->mode)) {
    if (hostfsinfo->dirstream) {
      unassert(!closedir(hostfsinfo->dirstream));
    } else if (hostfsinfo->filefd != -1) {
      unassert(!close(hostfsinfo->filefd));
    }
  } else if (S_ISSOCK(hostfsinfo->mode)) {
    if (hostfsinfo->filefd != -1) {
      unassert(!close(hostfsinfo->filefd));
    }
    free(hostfsinfo->socketaddr);
    free(hostfsinfo->socketpeeraddr);
  } else {
    if (hostfsinfo->filefd != -1) {
      unassert(!close(hostfsinfo->filefd));
    }
  }
  free(info);
  return 0;
}

int HostfsFreeDevice(void *device) {
  struct HostfsDevice *hostfsdevice = (struct HostfsDevice *)device;
  if (device == NULL) {
    return 0;
  }
  VFS_LOGF("HostfsFreeDevice(%p)", device);
  free((void *)hostfsdevice->source);
  free(hostfsdevice);
  return 0;
}

int HostfsCreateInfo(struct HostfsInfo **output) {
  *output = malloc(sizeof(struct HostfsInfo));
  if (*output == NULL) {
    return enomem();
  }
  (*output)->mode = 0;
  (*output)->socketfamily = 0;
  (*output)->filefd = -1;
  (*output)->dirstream = NULL;
  (*output)->socketaddr = NULL;
  (*output)->socketaddrlen = 0;
  (*output)->socketpeeraddr = NULL;
  (*output)->socketpeeraddrlen = 0;
  return 0;
}

int HostfsGetHostPath(struct VfsInfo *info, char **output) {
  struct HostfsDevice *hostfsdevice;
  char *path, *hostpath;
  size_t pathlen, sourcelen;
  if (info == NULL) {
    efault();
    return -1;
  }
  hostfsdevice = (struct HostfsDevice *)info->device->data;
  if (VfsPathBuild(info, info->device->root, &path) == -1) {
    return -1;
  }
  sourcelen = strlen(hostfsdevice->source);
  pathlen = strlen(path);
  pathlen += sourcelen;
  hostpath = malloc(pathlen + 1);
  if (hostpath == NULL) {
    free(path);
    return enomem();
  }
  memcpy(hostpath, hostfsdevice->source, sourcelen);
  if (hostpath[sourcelen - 1] == '/') {
    --sourcelen;
  }
  memcpy(hostpath + sourcelen, path, pathlen - sourcelen);
  hostpath[pathlen] = '\0';
  *output = hostpath;
  free(path);
  return 0;
}

int HostfsGetDirFd(struct VfsInfo *dir) {
  struct HostfsInfo *dirinfo;
  char *hostpath;
  int ret = -1;
  if (dir == NULL) {
    efault();
    return -1;
  }
  if (!S_ISDIR(dir->mode)) {
    enotdir();
    return -1;
  }
  dirinfo = (struct HostfsInfo *)dir->data;
  if (dirinfo->filefd != -1) {
    return dup(dirinfo->filefd);
  } else {
    if (HostfsGetHostPath(dir, &hostpath) == -1) {
      return -1;
    }
    ret = open(hostpath, O_RDONLY | O_DIRECTORY);
    free(hostpath);
    return ret;
  }
}

int HostfsFinddir(struct VfsInfo *parent, const char *name,
                  struct VfsInfo **output) {
  struct HostfsInfo *outputinfo;
  struct stat st;
  int parentfd;
  VFS_LOGF("HostfsFinddir(%p, \"%s\", %p)", parent, name, output);
  if (parent == NULL || name == NULL || output == NULL) {
    efault();
    return -1;
  }
  if (!S_ISDIR(parent->mode)) {
    enotdir();
    return -1;
  }
  *output = NULL;
  outputinfo = NULL;
  if ((parentfd = HostfsGetDirFd(parent)) == -1) {
    return -1;
  }
  if (fstatat(parentfd, name, &st, AT_SYMLINK_NOFOLLOW) == -1) {
    VFS_LOGF("HostfsFinddir: fstatat(%d, \"%s\", %p, AT_SYMLINK_NOFOLLOW) "
             "failed (%d)",
             parentfd, name, &st, errno);
    goto cleananddie;
  }
  if (HostfsCreateInfo(&outputinfo) == -1) {
    goto cleananddie;
  }
  outputinfo->mode = st.st_mode;
  outputinfo->filefd = -1;
  if (VfsCreateInfo(output) == -1) {
    goto cleananddie;
  }
  (*output)->name = strdup(name);
  if ((*output)->name == NULL) {
    enomem();
    goto cleananddie;
  }
  (*output)->data = outputinfo;
  unassert(!VfsAcquireDevice(parent->device, &(*output)->device));
  (*output)->dev = parent->dev;
  (*output)->ino =
      HostfsHash(st.st_dev, (const char *)&st.st_ino, sizeof(st.st_ino));
  (*output)->mode = st.st_mode;
  (*output)->refcount = 1;
  unassert(!VfsAcquireInfo(parent, &(*output)->parent));
  unassert(!close(parentfd));
  return 0;
cleananddie:
  unassert(!close(parentfd));
  if (*output) {
    unassert(!VfsFreeInfo(*output));
  } else {
    unassert(!HostfsFreeInfo(outputinfo));
  }
  return -1;
}

ssize_t HostfsReadlink(struct VfsInfo *info, char **output) {
  struct HostfsInfo *hostinfo;
  char *buf, *name;
  ssize_t len, reallen;
  int fd;
  VFS_LOGF("HostfsReadlink(%p, %p)", info, output);
  if (info == NULL || output == NULL) {
    efault();
    return -1;
  }
  if (!S_ISLNK(info->mode)) {
    einval();
    return -1;
  }
  hostinfo = (struct HostfsInfo *)info->data;
  if (hostinfo->filefd != -1) {
    fd = hostinfo->filefd;
    name = strdup("");
  } else {
    fd = -1;
    if (HostfsGetHostPath(info, &name) == -1) {
      return -1;
    }
  }
  len = PATH_MAX;
  buf = malloc(len);
  if (buf == NULL) {
    enomem();
    goto cleananddie;
  }
  while (true) {
    reallen = readlinkat(fd, name, buf, len);
    if (reallen == -1) {
      goto cleananddie;
    }
    if (reallen < len) {
      break;
    }
    len *= 2;
    buf = realloc(buf, len);
    if (buf == NULL) {
      enomem();
      goto cleananddie;
    }
  }
  buf[reallen] = '\0';
  *output = buf;
  if (fd != -1) {
    unassert(!close(fd));
  }
  free(name);
  return reallen;
cleananddie:
  if (fd != -1) {
    unassert(!close(fd));
  }
  free(name);
  free(buf);
  return -1;
}

int HostfsMkdir(struct VfsInfo *parent, const char *name, mode_t mode) {
  int parentfd, ret;
  VFS_LOGF("HostfsMkdir(%p, \"%s\", %d)", parent, name, mode);
  if ((parentfd = HostfsGetDirFd(parent)) == -1) {
    return -1;
  }
  ret = mkdirat(parentfd, name, mode);
  unassert(!close(parentfd));
  return ret;
}

int HostfsOpen(struct VfsInfo *parent, const char *name, int flags, int mode,
               struct VfsInfo **output) {
  struct HostfsInfo *outputinfo;
  struct stat st;
  int parentfd;
  VFS_LOGF("HostfsOpen(%p, \"%s\", %d, %d, %p)", parent, name, flags, mode,
           output);
  if (parent == NULL || name == NULL || output == NULL) {
    return efault();
  }
  if (!S_ISDIR(parent->mode)) {
    return enotdir();
  }
  *output = NULL;
  outputinfo = NULL;
  if ((parentfd = HostfsGetDirFd(parent)) == -1) {
    goto cleananddie;
  }
  if (HostfsCreateInfo(&outputinfo) == -1) {
    goto cleananddie;
  }
  outputinfo->filefd = openat(parentfd, name, flags, mode);
  VFS_LOGF("HostfsOpen: openat(%d, \"%s\", %d, %d) -> %d", parentfd, name,
           flags, mode, outputinfo->filefd);
  if (outputinfo->filefd == -1) {
    goto cleananddie;
  }
  unassert(!fstatat(parentfd, name, &st, 0));
  outputinfo->mode = st.st_mode;
  if (VfsCreateInfo(output) == -1) {
    goto cleananddie;
  }
  (*output)->data = outputinfo;
  (*output)->name = strdup(name);
  if ((*output)->name == NULL) {
    enomem();
    goto cleananddie;
  }
  unassert(!VfsAcquireDevice(parent->device, &(*output)->device));
  (*output)->dev = parent->dev;
  (*output)->ino =
      HostfsHash(st.st_dev, (const char *)&st.st_ino, sizeof(st.st_ino));
  (*output)->mode = outputinfo->mode;
  (*output)->refcount = 1;
  unassert(!VfsAcquireInfo(parent, &(*output)->parent));
  unassert(!close(parentfd));
  return 0;
cleananddie:
  if (parentfd != -1) {
    unassert(!close(parentfd));
  }
  if (*output) {
    unassert(!VfsFreeInfo(*output));
  } else {
    unassert(!HostfsFreeInfo(outputinfo));
  }
  return -1;
}

int HostfsAccess(struct VfsInfo *parent, const char *name, mode_t mode,
                 int flags) {
  int parentfd, ret;
  VFS_LOGF("HostfsAccess(%p, \"%s\", %d, %d)", parent, name, mode, flags);
  if ((parentfd = HostfsGetDirFd(parent)) == -1) {
    return -1;
  }
  ret = faccessat(parentfd, name, mode, flags);
  unassert(!close(parentfd));
  return ret;
}

int HostfsStat(struct VfsInfo *parent, const char *name, struct stat *st,
               int flags) {
  int parentfd, ret;
  VFS_LOGF("HostfsStat(%p, \"%s\", %p, %d)", parent, name, st, flags);
  if ((parentfd = HostfsGetDirFd(parent)) == -1) {
    return -1;
  }
  ret = fstatat(parentfd, name, st, flags);
  if (ret != -1) {
    st->st_ino =
        HostfsHash(st->st_dev, (const char *)&st->st_ino, sizeof(st->st_ino));
    st->st_dev = parent->dev;
  }
  unassert(!close(parentfd));
  return ret;
}

int HostfsFstat(struct VfsInfo *info, struct stat *st) {
  struct HostfsInfo *hostinfo;
  int ret;
  VFS_LOGF("HostfsFstat(%p, %p)", info, st);
  if (info == NULL || st == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  ret = fstat(hostinfo->filefd, st);
  if (ret != -1) {
    st->st_ino =
        HostfsHash(st->st_dev, (const char *)&st->st_ino, sizeof(st->st_ino));
    st->st_dev = info->dev;
  }
  return ret;
}

int HostfsChmod(struct VfsInfo *parent, const char *name, mode_t mode,
                int flags) {
  int parentfd, ret;
  VFS_LOGF("HostfsChmod(%p, \"%s\", %d)", parent, name, mode);
  if ((parentfd = HostfsGetDirFd(parent)) == -1) {
    return -1;
  }
  ret = fchmodat(parentfd, name, mode, flags);
  unassert(!close(parentfd));
  return ret;
}

int HostfsFchmod(struct VfsInfo *info, mode_t mode) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsFchmod(%p, %d)", info, mode);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return fchmod(hostinfo->filefd, mode);
}

int HostfsChown(struct VfsInfo *parent, const char *name, uid_t uid, gid_t gid,
                int flags) {
  int parentfd, ret;
  VFS_LOGF("HostfsChown(%p, \"%s\", %d, %d)", parent, name, uid, gid);
  if ((parentfd = HostfsGetDirFd(parent)) == -1) {
    return -1;
  }
  ret = fchownat(parentfd, name, uid, gid, flags);
  unassert(!close(parentfd));
  return ret;
}

int HostfsFchown(struct VfsInfo *info, uid_t uid, gid_t gid) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsFchown(%p, %d, %d)", info, uid, gid);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return fchown(hostinfo->filefd, uid, gid);
}

int HostfsFtruncate(struct VfsInfo *info, off_t length) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsFtruncate(%p, %ld)", info, length);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return ftruncate(hostinfo->filefd, length);
}

int HostfsLink(struct VfsInfo *oldparent, const char *oldname,
               struct VfsInfo *newparent, const char *newname, int flags) {
  int oldparentfd, newparentfd, ret;
  VFS_LOGF("HostfsLink(%p, \"%s\", %p, \"%s\")", oldparent, oldname, newparent,
           newname);
  if ((oldparentfd = HostfsGetDirFd(oldparent)) == -1) {
    return -1;
  }
  if ((newparentfd = HostfsGetDirFd(newparent)) == -1) {
    close(oldparentfd);
    return -1;
  }
  ret = linkat(oldparentfd, oldname, newparentfd, newname, flags);
  close(oldparentfd);
  close(newparentfd);
  return ret;
}

int HostfsUnlink(struct VfsInfo *parent, const char *name, int flags) {
  int parentfd, ret;
  VFS_LOGF("HostfsUnlink(%p, \"%s\")", parent, name);
  if ((parentfd = HostfsGetDirFd(parent)) == -1) {
    return -1;
  }
  ret = unlinkat(parentfd, name, flags);
  unassert(!close(parentfd));
  return ret;
}

ssize_t HostfsRead(struct VfsInfo *info, void *buf, size_t size) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsRead(%p, %p, %ld)", info, buf, size);
  if (info == NULL || buf == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return read(hostinfo->filefd, buf, size);
}

ssize_t HostfsWrite(struct VfsInfo *info, const void *buf, size_t size) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsWrite(%p, %p, %ld)", info, buf, size);
  if (info == NULL || buf == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return write(hostinfo->filefd, buf, size);
}

ssize_t HostfsPread(struct VfsInfo *info, void *buf, size_t size,
                    off_t offset) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsPread(%p, %p, %ld, %ld)", info, buf, size, offset);
  if (info == NULL || buf == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return pread(hostinfo->filefd, buf, size, offset);
}

ssize_t HostfsPwrite(struct VfsInfo *info, const void *buf, size_t size,
                     off_t offset) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsPwrite(%p, %p, %ld, %ld)", info, buf, size, offset);
  if (info == NULL || buf == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return pwrite(hostinfo->filefd, buf, size, offset);
}

ssize_t HostfsReadv(struct VfsInfo *info, const struct iovec *iov, int iovcnt) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsReadv(%p, %p, %d)", info, iov, iovcnt);
  if (info == NULL || iov == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return readv(hostinfo->filefd, iov, iovcnt);
}

ssize_t HostfsWritev(struct VfsInfo *info, const struct iovec *iov,
                     int iovcnt) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsWritev(%p, %p, %d)", info, iov, iovcnt);
  if (info == NULL || iov == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return writev(hostinfo->filefd, iov, iovcnt);
}

ssize_t HostfsPreadv(struct VfsInfo *info, const struct iovec *iov, int iovcnt,
                     off_t offset) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsPreadv(%p, %p, %d, %ld)", info, iov, iovcnt, offset);
  if (info == NULL || iov == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return preadv(hostinfo->filefd, iov, iovcnt, offset);
}

ssize_t HostfsPwritev(struct VfsInfo *info, const struct iovec *iov, int iovcnt,
                      off_t offset) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsPwritev(%p, %p, %d, %ld)", info, iov, iovcnt, offset);
  if (info == NULL || iov == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return pwritev(hostinfo->filefd, iov, iovcnt, offset);
}

off_t HostfsSeek(struct VfsInfo *info, off_t offset, int whence) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsLseek(%p, %ld, %d)", info, offset, whence);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return lseek(hostinfo->filefd, offset, whence);
}

int HostfsFsync(struct VfsInfo *info) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsFsync(%p)", info);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return fsync(hostinfo->filefd);
}

int HostfsFdatasync(struct VfsInfo *info) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsFdatasync(%p)", info);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return fdatasync(hostinfo->filefd);
}

int HostfsFlock(struct VfsInfo *info, int operation) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsFlock(%p, %d)", info, operation);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return flock(hostinfo->filefd, operation);
}

int HostfsFcntl(struct VfsInfo *info, int cmd, va_list args) {
  struct HostfsInfo *hostinfo;
  int rc;
  VFS_LOGF("HostfsFcntl(%p, %d, ...)", info, cmd);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  if (hostinfo == NULL) {
    return enoent();
  }
  switch (cmd) {
    case F_GETFD:
    case F_GETFL:
    case F_GETOWN:
#ifdef F_GETSIG
    case F_GETSIG:
#endif
#ifdef F_GETLEASE
    case F_GETLEASE:
#endif
#ifdef F_GETPIPE_SZ
    case F_GETPIPE_SZ:
#endif
#ifdef F_GET_SEALS
    case F_GET_SEALS:
#endif
      rc = fcntl(hostinfo->filefd, cmd);
      break;
    case F_SETFD:
    case F_SETFL:
    case F_SETOWN:
#ifdef F_SETSIG
    case F_SETSIG:
#endif
#ifdef F_SETLEASE
    case F_SETLEASE:
#endif
#ifdef F_NOTIFY
    case F_NOTIFY:
#endif
#ifdef F_SETPIPE_SZ
    case F_SETPIPE_SZ:
#endif
#ifdef F_ADD_SEALS
    case F_ADD_SEALS:
#endif
      rc = fcntl(hostinfo->filefd, cmd, va_arg(args, int));
      break;
    case F_SETLK:
    case F_SETLKW:
    case F_GETLK:
      rc = fcntl(hostinfo->filefd, cmd, va_arg(args, struct flock *));
      break;
#ifdef F_OFD_SETLK
    case F_OFD_SETLK:
    case F_OFD_SETLKW:
    case F_OFD_GETLK:
      rc = fcntl(hostinfo->filefd, cmd, va_arg(args, struct flock *));
      break;
#endif
#ifdef F_GETOWN_EX
    case F_GETOWN_EX:
    case F_SETOWN_EX:
      rc = fcntl(hostinfo->filefd, cmd, va_arg(args, struct f_owner_ex *));
      break;
#endif
#ifdef F_GET_RW_HINT
    case F_GET_RW_HINT:
    case F_SET_RW_HINT:
    case F_GET_FILE_RW_HINT:
    case F_SET_FILE_RW_HINT:
      rc = fcntl(hostinfo->filefd, cmd, va_arg(args, uint64_t *));
#endif
    default:
      rc = einval();
  }
  return rc;
}

int HostfsIoctl(struct VfsInfo *info, unsigned long request, const void *arg) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsIoctl(%p, %lu, %p)", info, request, arg);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return ioctl(hostinfo->filefd, request, arg);
}

int HostfsDup(struct VfsInfo *info, struct VfsInfo **newinfo) {
  struct HostfsInfo *hostinfo, *newhostinfo;
  VFS_LOGF("HostfsDup(%p, %p)", info, newinfo);
  if (info == NULL || newinfo == NULL) {
    return efault();
  }
  *newinfo = NULL;
  newhostinfo = NULL;
  hostinfo = (struct HostfsInfo *)info->data;
  if (hostinfo == NULL) {
    return enoent();
  }
  if (HostfsCreateInfo(&newhostinfo) == -1) {
    return -1;
  }
  newhostinfo->filefd = dup(hostinfo->filefd);
  if (newhostinfo->filefd == -1) {
    goto cleananddie;
  }
  newhostinfo->mode = hostinfo->mode;
  if (S_ISSOCK(hostinfo->mode) && hostinfo->socketaddr != NULL) {
    newhostinfo->socketaddr = malloc(hostinfo->socketaddrlen);
    if (newhostinfo->socketaddr != NULL) {
      memcpy(newhostinfo->socketaddr, hostinfo->socketaddr,
             hostinfo->socketaddrlen);
      newhostinfo->socketaddrlen = hostinfo->socketaddrlen;
    }
  } else if (S_ISDIR(hostinfo->mode)) {
    newhostinfo->dirstream = fdopendir(newhostinfo->filefd);
    if (newhostinfo->dirstream == NULL) {
      goto cleananddie;
    }
  }
  if (VfsCreateInfo(newinfo) == -1) {
    goto cleananddie;
  }
  if (info->name != NULL) {
    (*newinfo)->name = strdup(info->name);
    if ((*newinfo)->name == NULL) {
      goto cleananddie;
    }
  }
  (*newinfo)->data = newhostinfo;
  (*newinfo)->dev = info->dev;
  unassert(!VfsAcquireDevice(info->device, &(*newinfo)->device));
  (*newinfo)->ino = info->ino;
  (*newinfo)->mode = info->mode;
  unassert(!VfsAcquireInfo(info->parent, &(*newinfo)->parent));
  (*newinfo)->refcount = 1;
  return 0;
cleananddie:
  unassert(!HostfsFreeInfo(newhostinfo));
  unassert(!VfsFreeInfo(*newinfo));
  return -1;
}

#ifdef HAVE_DUP3
int HostfsDup3(struct VfsInfo *info, struct VfsInfo **newinfo, int flags) {
  struct HostfsInfo *hostinfo, *newhostinfo;
  VFS_LOGF("HostfsDup3(%p, %p, %i)", info, newinfo, flags);
  if (info == NULL || newinfo == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  if (hostinfo == NULL) {
    return enoent();
  }
  if (HostfsCreateInfo(&newhostinfo) == -1) {
    return -1;
  }
  newhostinfo->filefd =
      dup3(hostinfo->filefd, open("/dev/null", O_RDONLY), flags);
  if (newhostinfo->filefd == -1) {
    goto cleananddie;
  }
  (*newinfo)->name = strdup(info->name);
  if ((*newinfo)->name == NULL) {
    goto cleananddie;
  }
  if (VfsCreateInfo(newinfo) == -1) {
    goto cleananddie;
  }
  (*newinfo)->data = newhostinfo;
  (*newinfo)->dev = info->dev;
  unassert(!VfsAcquireDevice(info->device, &(*newinfo)->device));
  (*newinfo)->ino = info->ino;
  (*newinfo)->mode = info->mode;
  unassert(!VfsAcquireInfo(info->parent, &(*newinfo)->parent));
  (*newinfo)->refcount = 1;
  return 0;
cleananddie:
  unassert(!HostfsFreeInfo(newhostinfo));
  unassert(!VfsFreeInfo(*newinfo));
  return -1;
}
#endif

int HostfsPoll(struct VfsInfo **infos, struct pollfd *fds, nfds_t nfds,
               int timeout) {
  int rc;
  int oldfd;
  VFS_LOGF("HostfsPoll(%p, %lli, %i)", infos, (long long)nfds, timeout);
  if (infos == NULL) {
    return efault();
  }
  // Same reason as in VfsPoll above
  unassert(nfds == 1);
  oldfd = fds->fd;
  fds->fd = ((struct HostfsInfo *)infos[0]->data)->filefd;
  rc = poll(fds, nfds, timeout);
  fds->fd = oldfd;
  return rc;
}

int HostfsOpendir(struct VfsInfo *info, struct VfsInfo **output) {
  struct HostfsInfo *hostinfo;
  DIR *dirstream;
  VFS_LOGF("HostfsOpendir(%p)", info);
  if (info == NULL || output == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  unassert(hostinfo->filefd != -1);
  dirstream = fdopendir(hostinfo->filefd);
  if (dirstream == NULL) {
    return -1;
  }
  hostinfo->dirstream = dirstream;
  unassert(!VfsAcquireInfo(info, output));
  return 0;
}

#ifdef HAVE_SEEKDIR
void HostfsSeekdir(struct VfsInfo *info, long loc) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsSeekdir(%p, %ld)", info, loc);
  if (info == NULL) {
    efault();
    return;
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return seekdir(hostinfo->dirstream, loc);
}

long HostfsTelldir(struct VfsInfo *info) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsTelldir(%p)", info);
  if (info == NULL) {
    efault();
    return -1;
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return telldir(hostinfo->dirstream);
}
#endif

struct dirent *HostfsReaddir(struct VfsInfo *info) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsReaddir(%p)", info);
  if (info == NULL) {
    efault();
    return NULL;
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return readdir(hostinfo->dirstream);
}

void HostfsRewinddir(struct VfsInfo *info) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsRewinddir(%p)", info);
  if (info == NULL) {
    efault();
    return;
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return rewinddir(hostinfo->dirstream);
}

int HostfsClosedir(struct VfsInfo *info) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsClosedir(%p)", info);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  if (closedir(hostinfo->dirstream) == -1) {
    return -1;
  }
  hostinfo->dirstream = NULL;
  hostinfo->filefd = -1;
  unassert(!VfsFreeInfo(info));
  return 0;
}

int HostfsBind(struct VfsInfo *info, const struct sockaddr *addr,
               socklen_t addrlen) {
  struct HostfsInfo *hostinfo;
  struct sockaddr_un *hostun;
  struct stat st;
  char *hostpath;
  size_t len;
  int ret;
  VFS_LOGF("HostfsBind(%p, %p, %i)", info, addr, addrlen);
  if (info == NULL || addr == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  if (addr->sa_family != AF_UNIX) {
    ret = bind(hostinfo->filefd, addr, addrlen);
    if (ret == 0) {
      hostinfo->socketfamily = addr->sa_family;
    }
  } else {
    if (HostfsGetHostPath(info, &hostpath) == -1) {
      ret = -1;
    } else {
      len = strlen(hostpath) + 1 + offsetof(struct sockaddr_un, sun_path);
      hostun = malloc(len);
      if (hostun == NULL) {
        ret = -1;
      } else {
        hostun->sun_family = AF_UNIX;
        strcpy(hostun->sun_path, hostpath);
        ret = bind(hostinfo->filefd, (struct sockaddr *)hostun, len);
        if (ret == 0) {
          hostinfo->socketfamily = AF_UNIX;
          unassert(!fstat(hostinfo->filefd, &st));
          info->dev = info->parent->dev;
          info->ino = HostfsHash(st.st_dev, (const char *)&st.st_ino,
                                 sizeof(st.st_ino));
          info->mode = st.st_mode;
          hostinfo->socketaddr = malloc(addrlen);
          if (hostinfo->socketaddr != NULL) {
            memcpy(hostinfo->socketaddr, addr, addrlen);
            hostinfo->socketaddrlen = addrlen;
          }
        }
        free(hostun);
      }
      free(hostpath);
    }
  }
  return ret;
}

int HostfsConnect(struct VfsInfo *info, const struct sockaddr *addr,
                  socklen_t addrlen) {
  struct HostfsInfo *hostinfo;
  int ret;
  VFS_LOGF("HostfsConnect(%p, %p, %i)", info, addr, addrlen);
  if (info == NULL || addr == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  if (addr->sa_family == AF_UNIX) {
    ret = eopnotsupp();
  } else {
    ret = connect(hostinfo->filefd, addr, addrlen);
    if (ret == 0) {
      hostinfo->socketfamily = addr->sa_family;
    }
  }
  return ret;
}

int HostfsConnectUnix(struct VfsInfo *sock, struct VfsInfo *info,
                      const struct sockaddr_un *addr, socklen_t addrlen) {
  struct HostfsInfo *hostinfo;
  struct sockaddr_un *hostun;
  socklen_t hostlen;
  char *hostpath;
  size_t hostpathlen;
  int ret;
  VFS_LOGF("HostfsConnectUnix(%p, %p, %p, %i)", sock, info, addr, addrlen);
  if (sock == NULL || info == NULL || addr == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  if (HostfsGetHostPath(sock, &hostpath) == -1) {
    return -1;
  }
  hostpathlen = strlen(hostpath);
  hostlen = hostpathlen + 1 + offsetof(struct sockaddr_un, sun_path);
  hostun = malloc(hostlen);
  if (hostun == NULL) {
    free(hostpath);
    return enomem();
  }
  hostun->sun_family = AF_UNIX;
  memcpy(hostun->sun_path, hostpath, hostpathlen + 1);
  free(hostpath);
  ret = connect(hostinfo->filefd, (struct sockaddr *)hostun, hostlen);
  free(hostun);
  if (ret != -1) {
    hostinfo->socketpeeraddr = malloc(addrlen);
    if (hostinfo->socketpeeraddr != NULL) {
      memcpy(hostinfo->socketpeeraddr, addr, addrlen);
      hostinfo->socketpeeraddrlen = addrlen;
    }
  }
  return ret;
}

int HostfsAccept(struct VfsInfo *info, struct sockaddr *addr,
                 socklen_t *addrlen, struct VfsInfo **output) {
  struct HostfsInfo *hostinfo, *newhostinfo;
  int ret;
  VFS_LOGF("HostfsAccept(%p, %p, %p)", info, addr, addrlen);
  if (info == NULL || output == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  if (HostfsCreateInfo(&newhostinfo) == -1) {
    return -1;
  }
  if (VfsCreateInfo(output) == -1) {
    unassert(!HostfsFreeInfo(newhostinfo));
    unassert(!VfsFreeInfo(*output));
    return -1;
  }
  (*output)->data = newhostinfo;
  ret = accept(hostinfo->filefd, addr, addrlen);
  if (ret != -1) {
    newhostinfo->filefd = ret;
    newhostinfo->socketfamily = hostinfo->socketfamily;
    unassert(!VfsAcquireDevice(info->device, &(*output)->device));
    (*output)->ino = info->ino;
    (*output)->dev = info->dev;
    (*output)->mode = info->mode;
    (*output)->parent = info->parent;
    (*output)->refcount = 1;
    ret = 0;
  } else {
    unassert(!VfsFreeInfo(*output));
  }
  return ret;
}

int HostfsListen(struct VfsInfo *info, int backlog) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsListen(%p, %i)", info, backlog);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return listen(hostinfo->filefd, backlog);
}

int HostfsShutdown(struct VfsInfo *info, int how) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsShutdown(%p, %i)", info, how);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  if (hostinfo->socketfamily != AF_UNIX) {
    return shutdown(hostinfo->filefd, how);
  }
  return eopnotsupp();
}

ssize_t HostfsRecvmsg(struct VfsInfo *info, struct msghdr *msg, int flags) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsRecvmsg(%p, %p, %i)", info, msg, flags);
  if (info == NULL || msg == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return recvmsg(hostinfo->filefd, msg, flags);
}

ssize_t HostfsSendmsg(struct VfsInfo *info, const struct msghdr *msg,
                      int flags) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsSendmsg(%p, %p, %i)", info, msg, flags);
  if (info == NULL || msg == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return sendmsg(hostinfo->filefd, msg, flags);
}

ssize_t HostfsRecvmsgUnix(struct VfsInfo *sock, struct VfsInfo *info,
                          struct msghdr *msg, int flags) {
  struct HostfsInfo *hostinfo;
  struct sockaddr_un *hostun, *oldun;
  socklen_t hostlen, oldlen;
  char *hostpath;
  size_t hostpathlen;
  int ret;
  VFS_LOGF("HostfsRecvmsgUnix(%p, %p, %p, %i)", sock, info, msg, flags);
  if (sock == NULL || info == NULL || msg == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  if (HostfsGetHostPath(sock, &hostpath) == -1) {
    return -1;
  }
  hostpathlen = strlen(hostpath);
  hostlen = hostpathlen + 1 + offsetof(struct sockaddr_un, sun_path);
  hostun = malloc(hostlen);
  if (hostun == NULL) {
    free(hostpath);
    return enomem();
  }
  hostun->sun_family = AF_UNIX;
  memcpy(hostun->sun_path, hostpath, hostpathlen + 1);
  free(hostpath);
  oldun = msg->msg_name;
  oldlen = msg->msg_namelen;
  msg->msg_name = hostun;
  msg->msg_namelen = hostlen;
  ret = recvmsg(hostinfo->filefd, msg, flags);
  free(hostun);
  msg->msg_name = oldun;
  msg->msg_namelen = oldlen;
  return ret;
}

ssize_t HostfsSendmsgUnix(struct VfsInfo *sock, struct VfsInfo *info,
                          const struct msghdr *msg, int flags) {
  struct HostfsInfo *hostinfo;
  struct msghdr newmsg;
  struct sockaddr_un *hostun;
  socklen_t hostlen;
  char *hostpath;
  size_t hostpathlen;
  int ret;
  VFS_LOGF("HostfsSendmsgUnix(%p, %p, %p, %i)", sock, info, msg, flags);
  if (sock == NULL || info == NULL || msg == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  if (HostfsGetHostPath(sock, &hostpath) == -1) {
    return -1;
  }
  hostpathlen = strlen(hostpath);
  hostlen = hostpathlen + 1 + offsetof(struct sockaddr_un, sun_path);
  hostun = malloc(hostlen);
  if (hostun == NULL) {
    free(hostpath);
    return enomem();
  }
  hostun->sun_family = AF_UNIX;
  memcpy(hostun->sun_path, hostpath, hostpathlen + 1);
  free(hostpath);
  memcpy(&newmsg, msg, sizeof(struct msghdr));
  newmsg.msg_name = hostun;
  newmsg.msg_namelen = hostlen;
  ret = sendmsg(hostinfo->filefd, &newmsg, flags);
  free(hostun);
  return ret;
}

int HostfsGetsockopt(struct VfsInfo *info, int level, int optname, void *optval,
                     socklen_t *optlen) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsGetsockopt(%p, %i, %i, %p, %p)", info, level, optname, optval,
           optlen);
  if (info == NULL || optval == NULL || optlen == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return getsockopt(hostinfo->filefd, level, optname, optval, optlen);
}

int HostfsSetsockopt(struct VfsInfo *info, int level, int optname,
                     const void *optval, socklen_t optlen) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsSetsockopt(%p, %i, %i, %p, %i)", info, level, optname, optval,
           optlen);
  if (info == NULL || optval == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return setsockopt(hostinfo->filefd, level, optname, optval, optlen);
}

int HostfsGetsockname(struct VfsInfo *info, struct sockaddr *addr,
                      socklen_t *addrlen) {
  struct HostfsInfo *hostinfo;
  struct sockaddr_un *un;
  char *path;
  VFS_LOGF("HostfsGetsockname(%p, %p, %p)", info, addr, addrlen);
  if (info == NULL || addr == NULL || addrlen == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  if (hostinfo->socketfamily != AF_UNIX) {
    return getsockname(hostinfo->filefd, addr, addrlen);
  }
  if (hostinfo->socketaddr) {
    memcpy(addr, hostinfo->socketaddr, MIN(*addrlen, hostinfo->socketaddrlen));
    *addrlen = hostinfo->socketaddrlen;
  } else {
    if (info->parent == NULL) {
      addr->sa_family = AF_UNIX;
      *addrlen = offsetof(struct sockaddr_un, sun_path);
      return 0;
    }
    un = (struct sockaddr_un *)addr;
    if (VfsPathBuild(info, NULL, &path) == -1) {
      return -1;
    }
    un->sun_family = AF_UNIX;
    strncpy(un->sun_path, path,
            *addrlen - offsetof(struct sockaddr_un, sun_path));
    *addrlen = strlen(path) + offsetof(struct sockaddr_un, sun_path);
  }
  return 0;
}

int HostfsGetpeername(struct VfsInfo *info, struct sockaddr *addr,
                      socklen_t *addrlen) {
  struct HostfsInfo *hostinfo;
  struct sockaddr_un *hostun;
  socklen_t hostlen = sizeof(*hostun);
  char *s;
  VFS_LOGF("HostfsGetpeername(%p, %p, %p)", info, addr, addrlen);
  if (info == NULL || addr == NULL || addrlen == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  if (hostinfo->socketfamily != AF_UNIX) {
    return getpeername(hostinfo->filefd, addr, addrlen);
  } else {
    if (hostinfo->socketpeeraddr) {
      memcpy(addr, hostinfo->socketpeeraddr,
             MIN(*addrlen, hostinfo->socketpeeraddrlen));
      *addrlen = hostinfo->socketpeeraddrlen;
      return 0;
    }
    hostun = malloc(hostlen + 1);
    if (hostun == NULL) {
      return enomem();
    }
    memset(hostun, 0, hostlen + 1);
    if (getpeername(hostinfo->filefd, (struct sockaddr *)hostun, &hostlen) ==
        -1) {
      return -1;
    } else {
      // As we don't manage the socket connections, this is the best we can do.
      // For relative names we don't know where it's relative from, and for
      // absolute names we don't know which device it lives on.
      // VFS_SYSTEM_ROOT_MOUNT may have been unmounted by the user.
      if (hostun->sun_path[0] == '/') {
        s = malloc(sizeof(VFS_SYSTEM_ROOT_MOUNT) + hostlen -
                   offsetof(struct sockaddr_un, sun_path));
        if (s == NULL) {
          free(hostun);
          return enomem();
        }
        strcpy(s, VFS_SYSTEM_ROOT_MOUNT);
        strncat(s, hostun->sun_path,
                hostlen - offsetof(struct sockaddr_un, sun_path));
        addr->sa_family = AF_UNIX;
        strncpy(addr->sa_data, s,
                *addrlen - offsetof(struct sockaddr_un, sun_path));
        *addrlen = strlen(s) + offsetof(struct sockaddr_un, sun_path);
        free(s);
      } else {
        addr->sa_family = AF_UNIX;
        strncpy(addr->sa_data, hostun->sun_path,
                *addrlen - offsetof(struct sockaddr_un, sun_path));
        *addrlen =
            strlen(hostun->sun_path) + offsetof(struct sockaddr_un, sun_path);
      }
      free(hostun);
      return 0;
    }
  }
}

int HostfsRename(struct VfsInfo *oldinfo, const char *oldname,
                 struct VfsInfo *newinfo, const char *newname) {
  int oldparentfd, newparentfd, ret;
  VFS_LOGF("HostfsRename(%p, %s, %p, %s)", oldinfo, oldname, newinfo, newname);
  if ((oldparentfd = HostfsGetDirFd(oldinfo)) == -1) {
    return -1;
  }
  if ((newparentfd = HostfsGetDirFd(newinfo)) == -1) {
    return -1;
  }
  ret = renameat(oldparentfd, oldname, newparentfd, newname);
  unassert(!close(oldparentfd));
  unassert(!close(newparentfd));
  return ret;
}

int HostfsUtime(struct VfsInfo *info, const char *name,
                const struct timespec times[2], int flags) {
  int parentfd, ret;
  VFS_LOGF("HostfsUtime(%p, %s, %p, %d)", info, name, times, flags);
  if (info == NULL) {
    return efault();
  }
  if ((parentfd = HostfsGetDirFd(info)) == -1) {
    return -1;
  }
  ret = utimensat(parentfd, name, times, flags);
  unassert(!close(parentfd));
  return ret;
}

int HostfsFutime(struct VfsInfo *info, const struct timespec times[2]) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsFutime(%p, %p)", info, times);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  if (futimens(hostinfo->filefd, times) == -1) {
    return -1;
  }
  return 0;
}

int HostfsSymlink(const char *target, struct VfsInfo *info, const char *name) {
  int parentfd, ret;
  VFS_LOGF("HostfsSymlink(%s, %p, %s)", target, info, name);
  if ((parentfd = HostfsGetDirFd(info)) == -1) {
    return -1;
  }
  ret = symlinkat(target, parentfd, name);
  unassert(!close(parentfd));
  return ret;
}

void *HostfsMmap(struct VfsInfo *info, void *addr, size_t len, int prot,
                 int flags, off_t offset) {
  struct HostfsInfo *hostinfo;
  void *ret;
  VFS_LOGF("HostfsMmap(%p, %p, %zu, %d, %d, %zd)", info, addr, len, prot, flags,
           offset);
  if (info == NULL) {
    efault();
    return MAP_FAILED;
  }
  hostinfo = (struct HostfsInfo *)info->data;
  ret = mmap(addr, len, prot, flags, hostinfo->filefd, offset);
  VFS_LOGF("mmap(%p, %zu, %d, %d, %d, %zd) -> %p", addr, len, prot, flags,
           hostinfo->filefd, offset, ret);
  return ret;
}

int HostfsMunmap(struct VfsInfo *info, void *addr, size_t len) {
  VFS_LOGF("HostfsMunmap(%p, %p, %zu)", info, addr, len);
  // Do nothing. The host should handle all the cleanup.
  return 0;
}

int HostfsMprotect(struct VfsInfo *info, void *addr, size_t len, int prot) {
  VFS_LOGF("HostfsMprotect(%p, %p, %zu, %d)", info, addr, len, prot);
  // Do nothing, as the host should handle the protection details.
  return 0;
}

int HostfsMsync(struct VfsInfo *info, void *addr, size_t len, int flags) {
  VFS_LOGF("HostfsMsync(%p, %p, %zu, %d)", info, addr, len, flags);
  // Do nothing, as the host should handle the syncing.
  return 0;
}

int HostfsTcgetattr(struct VfsInfo *info, struct termios *termios) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsTcgetattr(%p, %p)", info, termios);
  if (info == NULL || termios == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return tcgetattr(hostinfo->filefd, termios);
}

int HostfsTcsetattr(struct VfsInfo *info, int optional_actions,
                    const struct termios *termios) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsTcsetattr(%p, %d, %p)", info, optional_actions, termios);
  if (info == NULL || termios == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return tcsetattr(hostinfo->filefd, optional_actions, termios);
}

int HostfsTcflush(struct VfsInfo *info, int queue_selector) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsTcflush(%p, %d)", info, queue_selector);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return tcflush(hostinfo->filefd, queue_selector);
}

int HostfsTcdrain(struct VfsInfo *info) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsTcdrain(%p)", info);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return tcdrain(hostinfo->filefd);
}

int HostfsTcsendbreak(struct VfsInfo *info, int duration) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsTcsendbreak(%p, %d)", info, duration);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return tcsendbreak(hostinfo->filefd, duration);
}

int HostfsTcflow(struct VfsInfo *info, int action) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsTcflow(%p, %d)", info, action);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return tcflow(hostinfo->filefd, action);
}

pid_t HostfsTcgetsid(struct VfsInfo *info) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsTcgetsid(%p)", info);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return tcgetsid(hostinfo->filefd);
}

pid_t HostfsTcgetpgrp(struct VfsInfo *info) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsTcgetpgrp(%p)", info);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return tcgetpgrp(hostinfo->filefd);
}

int HostfsTcsetpgrp(struct VfsInfo *info, pid_t pgrp) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsTcsetpgrp(%p, %d)", info, pgrp);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return tcsetpgrp(hostinfo->filefd, pgrp);
}

int HostfsSockatmark(struct VfsInfo *info) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsSockatmark(%p)", info);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return sockatmark(hostinfo->filefd);
}

int HostfsPipe(struct VfsInfo *infos[2]) {
  int i;
  int fds[2];
  VFS_LOGF("HostfsPipe(%p)", infos);
  if (infos == NULL) {
    return efault();
  }
  fds[0] = fds[1] = -1;
  infos[0] = infos[1] = NULL;
  if (pipe(fds) == -1) {
    return -1;
  }
  for (i = 0; i < 2; ++i) {
    if (HostfsWrapFd(fds[i], false, &infos[i]) == -1) {
      goto cleananddie;
    }
  }
  return 0;
cleananddie:
  for (i = 0; i < 2; ++i) {
    if (infos[i] != NULL) {
      unassert(!VfsFreeInfo(infos[i]));
    } else if (fds[i] != -1) {
      unassert(!close(fds[i]));
    }
  }
  return -1;
}

#ifdef HAVE_PIPE2
int HostfsPipe2(struct VfsInfo *infos[2], int flags) {
  int i;
  int fds[2];
  VFS_LOGF("HostfsPipe2(%p, %d)", infos, flags);
  if (infos == NULL) {
    return efault();
  }
  fds[0] = fds[1] = -1;
  infos[0] = infos[1] = NULL;
  if (pipe2(fds, flags) == -1) {
    return -1;
  }
  for (i = 0; i < 2; ++i) {
    if (HostfsWrapFd(fds[i], false, &infos[i]) == -1) {
      goto cleananddie;
    }
  }
  return 0;
cleananddie:
  for (i = 0; i < 2; ++i) {
    if (infos[i] != NULL) {
      unassert(!VfsFreeInfo(infos[i]));
    } else if (fds[i] != -1) {
      unassert(!close(fds[i]));
    }
  }
  return -1;
}
#endif

int HostfsSocket(int domain, int type, int protocol, struct VfsInfo **output) {
  int fd;
  VFS_LOGF("HostfsSocket(%d, %d, %d, %p)", domain, type, protocol, output);
  if (output == NULL) {
    return efault();
  }
  fd = -1;
  *output = NULL;
  fd = socket(domain, type, protocol);
  if (fd == -1) {
    goto cleananddie;
  }
  if (HostfsWrapFd(fd, false, output) == -1) {
    goto cleananddie;
  }
  ((struct HostfsInfo *)(*output)->data)->socketfamily = domain;
  return 0;
cleananddie:
  if (*output != NULL) {
    unassert(!VfsFreeInfo(*output));
  } else if (fd != -1) {
    unassert(!close(fd));
  }
  return -1;
}

int HostfsSocketpair(int domain, int type, int protocol,
                     struct VfsInfo *infos[2]) {
  int i;
  int fds[2];
  VFS_LOGF("HostfsSocketpair(%d, %d, %d, %p)", domain, type, protocol, infos);
  if (infos == NULL) {
    return efault();
  }
  fds[0] = fds[1] = -1;
  infos[0] = infos[1] = NULL;
  if (socketpair(domain, type, protocol, fds) == -1) {
    return -1;
  }
  for (i = 0; i < 2; ++i) {
    if (HostfsWrapFd(fds[i], false, &infos[i]) == -1) {
      goto cleananddie;
    }
    ((struct HostfsInfo *)infos[i]->data)->socketfamily = domain;
  }
  return 0;
cleananddie:
  for (i = 0; i < 2; ++i) {
    if (infos[i] != NULL) {
      unassert(!VfsFreeInfo(infos[i]));
    } else if (fds[i] != -1) {
      unassert(!close(fds[i]));
    }
  }
  return -1;
}

int HostfsFexecve(struct VfsInfo *info, char *const *argv, char *const *envp) {
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsFexecve(%p, %p, %p)", info, argv, envp);
  if (info == NULL) {
    return efault();
  }
  hostinfo = (struct HostfsInfo *)info->data;
  return fexecve(hostinfo->filefd, argv, envp);
}

struct VfsDevice g_anondevice = {
    .data = NULL,
    .dev = -1,
    .ops = &g_hostfs.ops,
    .mounts = NULL,
    .refcount = 1,
};

int HostfsWrapFd(int fd, bool dodup, struct VfsInfo **output) {
  struct stat st;
  struct HostfsInfo *hostinfo;
  VFS_LOGF("HostfsWrapFd(%d, %p)", fd, output);
  if (output == NULL) {
    return efault();
  }
  hostinfo = NULL;
  *output = NULL;
  if (dodup) {
    fd = dup(fd);
    if (fd == -1) {
      return -1;
    }
  }
  if (fstat(fd, &st) == -1) {
    goto cleananddie;
  }
  if (HostfsCreateInfo(&hostinfo) == -1) {
    goto cleananddie;
  }
  hostinfo->filefd = fd;
  hostinfo->mode = st.st_mode;
  if (VfsCreateInfo(output) == -1) {
    goto cleananddie;
  }
  (*output)->data = hostinfo;
  (*output)->parent = NULL;
  (*output)->dev = -1;
  (*output)->ino =
      HostfsHash(st.st_dev, (const char *)&st.st_ino, sizeof(st.st_ino));
  (*output)->mode = st.st_mode;
  (*output)->refcount = 1;
  unassert(!VfsAcquireDevice(&g_anondevice, &(*output)->device));
  return 0;
cleananddie:
  if (*output != NULL) {
    unassert(!VfsFreeInfo(*output));
  } else if (hostinfo != NULL) {
    unassert(!HostfsFreeInfo(hostinfo));
  } else if (dodup && fd != -1) {
    unassert(!close(fd));
  }
  return -1;
}

struct VfsSystem g_hostfs = {.name = "hostfs",
                             .ops = {
                                 .init = HostfsInit,
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
                                 .pipe = HostfsPipe,
#ifdef HAVE_PIPE2
                                 .pipe2 = HostfsPipe2,
#endif
                                 .socket = HostfsSocket,
                                 .socketpair = HostfsSocketpair,
                                 .tcgetattr = HostfsTcgetattr,
                                 .tcsetattr = HostfsTcsetattr,
                                 .tcflush = HostfsTcflush,
                                 .tcdrain = HostfsTcdrain,
                                 .tcsendbreak = HostfsTcsendbreak,
                                 .tcflow = HostfsTcflow,
                                 .tcgetsid = HostfsTcgetsid,
                                 .tcgetpgrp = HostfsTcgetpgrp,
                                 .tcsetpgrp = HostfsTcsetpgrp,
                                 .sockatmark = HostfsSockatmark,
                                 .fexecve = HostfsFexecve,
                             }};

#endif /* DISABLE_VFS */
