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
#include <sys/mman.h>
#include <sys/stat.h>

#include "blink/assert.h"
#include "blink/atomic.h"
#include "blink/devfs.h"
#include "blink/errno.h"
#include "blink/hostfs.h"
#include "blink/linux.h"
#include "blink/log.h"
#include "blink/macros.h"
#include "blink/procfs.h"
#include "blink/vfs.h"

#ifndef DISABLE_VFS

#define VFS_UNREACHABLE        "(unreachable)"
#define VFS_TRAVERSE_MAX_LINKS 40

static int ProcfsInit(const char *, u64, const void *, struct VfsDevice **,
                      struct VfsMount **);

struct VfsFd {
  struct Dll elem;
  struct VfsInfo *data;
  int fd;
};

struct VfsMap {
  struct Dll elem;
  struct VfsInfo *data;
  void *addr;
  size_t len;
  off_t offset;
  int prot;
  int flags;
};

#define VFS_FD_CONTAINER(e)  DLL_CONTAINER(struct VfsFd, elem, (e))
#define VFS_MAP_CONTAINER(e) DLL_CONTAINER(struct VfsMap, elem, (e))

static struct VfsDevice g_rootdevice = {
    .data = NULL,
    .mounts = NULL,
    .ops = NULL,
    .dev = 0,
    .refcount = 1,
};

static struct VfsInfo g_initialrootinfo = {
    .device = &g_rootdevice,
    .data = NULL,
    .ino = 0,
    .dev = 0,
    .mode = S_IFDIR | 0755,
    .refcount = 1,
    .parent = NULL,
};

static struct VfsInfo *g_cwdinfo;
static struct VfsInfo *g_rootinfo;

static struct Vfs g_vfs = {
    .devices = NULL,
    .systems = NULL,
    .fds = NULL,
    .maps = NULL,
    .lock = PTHREAD_MUTEX_INITIALIZER_,
    .mapslock = PTHREAD_MUTEX_INITIALIZER_,
};

int VfsInit(const char *prefix) {
  struct stat st;
  char *cwd, hostcwd[PATH_MAX], *bprefix = NULL;
  struct VfsInfo *info;
  size_t hostcwdlen, prefixlen;

  // Register built-in filesystems
  unassert(!VfsRegister(&g_hostfs));
  unassert(!VfsRegister(&g_devfs));
  unassert(!VfsRegister(&g_procfs));

  dll_init(&g_rootdevice.elem);
  dll_make_first(&g_vfs.devices, &g_rootdevice.elem);

  // Initialize the root directory
  unassert(!VfsAcquireInfo(&g_initialrootinfo, &g_rootinfo));
  if (prefix) {
    bprefix = realpath(prefix, NULL);
  }
  if (bprefix) {
    if (stat(bprefix, &st) == -1) {
      ERRF("Failed to stat BLINK_PREFIX %s, %s", bprefix, strerror(errno));
      free(bprefix);
      bprefix = NULL;
    } else if (!S_ISDIR(st.st_mode)) {
      ERRF("BLINK_PREFIX %s is not a directory", bprefix);
      free(bprefix);
      bprefix = NULL;
    }
  }
  if (bprefix) {
    unassert(!VfsMount(bprefix, "/", "hostfs", 0, NULL));
  } else {
    unassert(!VfsMount("/", "/", "hostfs", 0, NULL));
  }
  unassert(!VfsFreeInfo(g_rootinfo));
  unassert(!VfsTraverse("/", &g_rootinfo, false));

  // Temporary cwd for syscalls to work during initialization
  unassert(!VfsChdir("/"));

  // Mount the host's root.
  if (VfsMkdir(AT_FDCWD, VFS_SYSTEM_ROOT_MOUNT, 0755) == -1 &&
      errno != EEXIST) {
    ERRF("Failed to create system root mount directory, %s", strerror(errno));
    goto cleananddie;
  }
  unassert(VfsMount("/", VFS_SYSTEM_ROOT_MOUNT, "hostfs", 0, NULL) != -1);

  // devfs, procfs
  if (VfsMkdir(AT_FDCWD, "/dev", 0755) == -1 && errno != EEXIST) {
    ERRF("Failed to create /dev, %s", strerror(errno));
    goto cleananddie;
  }
  unassert(!VfsMount("", "/dev", "devfs", 0, NULL));
  if (VfsMkdir(AT_FDCWD, "/proc", 0755) == -1 && errno != EEXIST) {
    ERRF("Failed to create /proc, %s", strerror(errno));
    goto cleananddie;
  }
  unassert(!VfsMount("", "/proc", "procfs", 0, NULL));

  // Initialize the current working directory
  unassert(getcwd(hostcwd, sizeof(hostcwd)));
  prefixlen = strlen(bprefix);
  if (bprefix && !strncmp(hostcwd, bprefix, prefixlen)) {
    hostcwdlen = strlen(hostcwd);
    cwd = malloc(hostcwdlen - prefixlen + 1);
    if (cwd == NULL) {
      enomem();
      goto cleananddie;
    }
    memcpy(cwd, hostcwd + prefixlen, hostcwdlen - prefixlen + 1);
  } else {
    cwd = malloc(PATH_MAX + sizeof(VFS_SYSTEM_ROOT_MOUNT));
    if (cwd == NULL) {
      enomem();
      goto cleananddie;
    }
    memcpy(cwd, VFS_SYSTEM_ROOT_MOUNT, sizeof(VFS_SYSTEM_ROOT_MOUNT));
    strcat(cwd, hostcwd);
  }

  unassert(!VfsChdir(cwd));
  free(cwd);
  free(bprefix);

  // stdin, stdout, stderr
  unassert(!HostfsWrapFd(0, false, &info));
  unassert(VfsAddFd(info) == 0);
  unassert(!HostfsWrapFd(1, false, &info));
  unassert(VfsAddFd(info) == 1);
  unassert(!HostfsWrapFd(2, false, &info));
  unassert(VfsAddFd(info) == 2);

  VFS_LOGF("Initialized VFS");

  return 0;
cleananddie:
  free(bprefix);
  return -1;
}

int VfsRegister(struct VfsSystem *system) {
  if (system == NULL) {
    efault();
    return -1;
  }
  LOCK(&g_vfs.lock);
  VFS_LOGF("Registering filesystem %s", system->name);
  dll_init(&system->elem);
  dll_make_last(&g_vfs.systems, &system->elem);
  UNLOCK(&g_vfs.lock);
  return 0;
}

int VfsMount(const char *source, const char *target, const char *fstype,
             u64 flags, const void *data) {
  struct VfsInfo *targetinfo;
  struct VfsSystem *newsystem, *s;
  struct VfsDevice *targetdevice = 0, *newdevice, *d;
  struct VfsMount *newmount;
  struct Dll *e;
  char *newname = 0;
  i64 nextdev;
  if (flags & MS_SILENT_LINUX) {
    flags &= ~MS_SILENT_LINUX;
  }
  if (flags) {
    // Theoretically, we can support a lot of the Linux flags without
    // changing the current design. However, as the major intended
    // usecase is to simply mount hostfs, this is currently not supported.
    LOGF("Unsupported mount flags: 0x%llx", (unsigned long long)flags);
  }
  if (VfsTraverse(target, &targetinfo, true) == -1) {
    return -1;
  }
  if (!S_ISDIR(targetinfo->mode)) {
    return enotdir();
  }
  LOCK(&g_vfs.lock);
  newsystem = NULL;
  for (e = dll_first(g_vfs.systems); e; e = dll_next(g_vfs.systems, e)) {
    s = VFS_SYSTEM_CONTAINER(e);
    if (strcmp(s->name, fstype) == 0) {
      newsystem = s;
      break;
    }
  }
  if (newsystem == NULL) {
    VFS_LOGF("Unknown filesystem type: %s", fstype);
    UNLOCK(&g_vfs.lock);
    return enodev();
  }
  for (e = dll_first(g_vfs.devices); e; e = dll_next(g_vfs.devices, e)) {
    d = VFS_DEVICE_CONTAINER(e);
    if (d->dev == targetinfo->dev) {
      targetdevice = d;
      break;
    }
  }
  if (targetdevice == NULL) {
    // Might have been unmounted after VfsTraverse by another thread
    UNLOCK(&g_vfs.lock);
    return enoent();
  }
  if (targetinfo->name != NULL) {
    newname = strdup(targetinfo->name);
    if (newname == NULL) {
      UNLOCK(&g_vfs.lock);
      return enomem();
    }
  }
  if (newsystem->ops.init(source, flags, data, &newdevice, &newmount) == -1) {
    UNLOCK(&g_vfs.lock);
    return -1;
  }
  nextdev = 0;
  for (e = dll_first(g_vfs.devices); e; e = dll_next(g_vfs.devices, e)) {
    d = VFS_DEVICE_CONTAINER(e);
    if (d->dev == nextdev) {
      ++nextdev;
    } else {
      break;
    }
  }
  newdevice->dev = nextdev;
  if (e == NULL) {
    dll_make_last(&g_vfs.devices, &newdevice->elem);
  } else {
    dll_splice_after(dll_prev(g_vfs.devices, e), &newdevice->elem);
  }
  newdevice->flags = flags;
  newmount->baseino = targetinfo->ino;
  newmount->root->dev = nextdev;
  newmount->root->name = newname;
  unassert(!VfsAcquireInfo(targetinfo->parent, &newmount->root->parent));
  dll_init(&newmount->elem);
  dll_make_last(&targetdevice->mounts, &newmount->elem);
  UNLOCK(&g_vfs.lock);
  unassert(!VfsFreeInfo(targetinfo));
  VFS_LOGF("Mounted a new device at %s, dev=%ld", target, nextdev);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int VfsCreateDevice(struct VfsDevice **output) {
  struct VfsDevice *device;
  if (output == NULL) {
    return efault();
  }
  device = malloc(sizeof(struct VfsDevice));
  if (device == NULL) {
    return enomem();
  }
  device->dev = 0;
  device->refcount = 1;
  device->data = NULL;
  device->ops = NULL;
  device->mounts = NULL;
  device->refcount = 1;
  unassert(!pthread_mutex_init(&device->lock, NULL));
  dll_init(&device->elem);
  *output = device;
  return 0;
}

int VfsFreeDevice(struct VfsDevice *device) {
  struct Dll *e;
  struct VfsMount *mount;
  int rc;
  if (device == NULL) {
    return 0;
  }
  if ((rc = atomic_fetch_sub(&device->refcount, 1)) != 1) {
    return 0;
  }
  unassert(!pthread_mutex_lock(&device->lock));
  if (device->ops && device->ops->freedevice) {
    if (device->ops->freedevice(device->data) == -1) {
      unassert(!pthread_mutex_unlock(&device->lock));
      return -1;
    }
  }
  device->data = NULL;
  device->ops = NULL;
  // Don't free root, it's a weak reference.
  device->root = NULL;
  for (e = dll_first(device->mounts); e; e = dll_next(device->mounts, e)) {
    mount = VFS_MOUNT_CONTAINER(e);
    unassert(!VfsFreeInfo(mount->root));
    mount->root = NULL;
    free(mount);
  }
  unassert(!pthread_mutex_unlock(&device->lock));
  unassert(!pthread_mutex_destroy(&device->lock));
  free(device);
  return 0;
}

int VfsAcquireDevice(struct VfsDevice *device, struct VfsDevice **output) {
  int rc;
  if (output == NULL) {
    return efault();
  }
  if (device == NULL) {
    *output = NULL;
    return 0;
  }
  rc = atomic_fetch_add(&device->refcount, 1);
  unassert(rc > 0);
  *output = device;
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int VfsCreateInfo(struct VfsInfo **output) {
  struct VfsInfo *info;
  if (output == NULL) {
    return efault();
  }
  info = malloc(sizeof(struct VfsInfo));
  if (info == NULL) {
    return enomem();
  }
  info->parent = NULL;
  info->device = NULL;
  info->name = NULL;
  info->data = NULL;
  info->ino = 0;
  info->dev = 0;
  info->mode = 0;
  info->refcount = 1;
  *output = info;
  return 0;
}

int VfsAcquireInfo(struct VfsInfo *info, struct VfsInfo **output) {
  int rc;
  if (output == NULL) {
    return efault();
  }
  if (info == NULL) {
    *output = NULL;
    return 0;
  }
  rc = atomic_fetch_add(&info->refcount, 1);
  unassert(rc > 0);
  VFS_LOGF("Acquired VfsInfo %p, refcount now %i.", info, rc + 1);
  *output = info;
  return 0;
}

int VfsFreeInfo(struct VfsInfo *info) {
  int rc;
  if (info == NULL) {
    return 0;
  }
  if ((rc = atomic_fetch_sub(&info->refcount, 1)) != 1) {
    VFS_LOGF("Freeing VfsInfo %p, refcount now %i.", info, rc - 1);
    return 0;
  }
  if (VfsFreeInfo(info->parent) == -1) {
    return -1;
  }
  info->parent = NULL;
  if (info->device->ops && info->device->ops->freeinfo) {
    if (info->device->ops->freeinfo(info->data) == -1) {
      return -1;
    }
  }
  unassert(!VfsFreeDevice(info->device));
  info->data = NULL;
  if (info->name) {
    free((void *)info->name);
    info->name = NULL;
  }
  free(info);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

static int VfsTraverseMount(struct VfsInfo **info, char **childname) {
  struct VfsMount *mount;
  struct Dll *e;
  char *p;
  if (info == NULL) {
    return efault();
  }
  if (!S_ISDIR((*info)->mode)) {
    return 0;
  }
  LOCK(&g_vfs.lock);
  for (e = dll_first((*info)->device->mounts); e;
       e = dll_next((*info)->device->mounts, e)) {
    mount = VFS_MOUNT_CONTAINER(e);
    if (!childname) {
      // Checking info itself.
      if (mount->baseino == (*info)->ino) {
        VFS_LOGF("VfsTraverseMount: switching from dev=%d to dev=%d",
                 (*info)->dev, mount->root->dev);
        unassert(!VfsFreeInfo(*info));
        unassert(!VfsAcquireInfo(mount->root, info));
        break;
      }
    } else {
      // Checking child node "childname" of info.
      VFS_LOGF("VfsTraverseMount: checking mount %s", mount->root->name);
      if (mount->root->parent && mount->root->parent->ino == (*info)->ino &&
          !strcmp(mount->root->name, *childname)) {
        unassert(mount->root->parent->dev == (*info)->dev);
        VFS_LOGF("VfsTraverseMount: switching from dev=%d to dev=%d",
                 (*info)->dev, mount->root->dev);
        p = realloc(*childname, 2);
        if (p == NULL) {
          UNLOCK(&g_vfs.lock);
          return enomem();
        }
        *childname = p;
        strcpy(*childname, ".");
        unassert(!VfsFreeInfo(*info));
        unassert(!VfsAcquireInfo(mount->root, info));
        break;
      }
    }
  }
  UNLOCK(&g_vfs.lock);
  return 0;
}

static int VfsTraverseStackBuild(struct VfsInfo **stack, const char *path,
                                 struct VfsInfo *root, bool follow, int level) {
  struct VfsInfo *next, *origin;
  const char *end, *filename;
  char *link;
  VFS_LOGF("VfsTraverseStackBuild(%p, \"%s\", %p, %d)", stack, path, root,
           follow);
  if (stack == NULL || path == NULL) {
    return efault();
  }
  if (level > VFS_TRAVERSE_MAX_LINKS) {
    return eloop();
  }
  origin = *stack;
  if (*stack == NULL) {
    unassert(!VfsAcquireInfo(&g_initialrootinfo, stack));
  }
  if (root == NULL) {
    root = &g_initialrootinfo;
  }
  while (*path) {
    if (!S_ISDIR((*stack)->mode)) {
      enotdir();
      goto cleananddie;
    }
    unassert(!VfsTraverseMount(stack, NULL));
    while (*path == '/') {
      ++path;
    }
    end = path;
    while (end[0] && end[0] != '/') {
      ++end;
    }
    if (end == path) {
      break;
    }
    filename = strndup(path, end - path);
    path = end;
    if (!strcmp(filename, ".")) {
      free((void *)filename);
      continue;
    }
    if (!strcmp(filename, "..")) {
      free((void *)filename);
      if (*stack != root && (*stack)->parent != NULL) {
        unassert(!VfsAcquireInfo((*stack)->parent, &next));
        unassert(!VfsFreeInfo(*stack));
        *stack = next;
      }
      continue;
    }
    if ((*stack)->device->ops->finddir(*stack, filename, &next) == -1) {
      free((void *)filename);
      goto cleananddie;
    }
    unassert(!VfsFreeInfo(*stack));
    *stack = next;
    free((void *)filename);
    if (follow) {
      while (S_ISLNK((*stack)->mode)) {
        if ((*stack)->device->ops->readlink(*stack, &link) == -1) {
          goto cleananddie;
        }
        VFS_LOGF("VfsTraverseStackBuild: symlink to %s", link);
        if (link[0] == '/') {
          unassert(!VfsFreeInfo(*stack));
          unassert(!VfsAcquireInfo(root, stack));
        } else {
          unassert(!VfsAcquireInfo((*stack)->parent, &next));
          unassert(!VfsFreeInfo(*stack));
          *stack = next;
        }
        if (VfsTraverseStackBuild(stack, link, root, follow, level + 1) == -1) {
          free(link);
          goto cleananddie;
        }
        free(link);
      }
    }
  }
  unassert(!VfsTraverseMount(stack, NULL));
  return 0;
cleananddie:
  while (*stack != origin) {
    unassert(!VfsAcquireInfo((*stack)->parent, &next));
    unassert(!VfsFreeInfo(*stack));
    *stack = next;
  }
  return -1;
}

int VfsTraverse(const char *path, struct VfsInfo **output, bool follow) {
  struct VfsInfo *root;
  VFS_LOGF("VfsTraverse(\"%s\", %p, %d)", path, output, follow);
  if (path == NULL || output == NULL) {
    efault();
    return -1;
  }
  root = NULL;
  if (path[0] != '/') {
    unassert(!VfsAcquireInfo(g_cwdinfo, output));
  } else {
    unassert(!VfsAcquireInfo(g_rootinfo, output));
  }
  if (VfsTraverseStackBuild(output, path, root, follow, 0) == -1) {
    unassert(!VfsFreeInfo(*output));
    return -1;
  }
  return 0;
}

int VfsPathBuild(struct VfsInfo *info, struct VfsInfo *root, char **output) {
  struct VfsInfo *current;
  size_t len, currentlen;
  len = 0;
  current = info;
  if (root == NULL) {
    root = g_rootinfo;
  }
  if (current->dev == root->dev && current->ino == root->ino) {
    *output = strdup("/");
    if (*output == NULL) {
      return enomem();
    }
    return 0;
  }
  while (current && (current->dev != root->dev || current->ino != root->ino)) {
    len += strlen(current->name) + 1;
    current = current->parent;
  }
  if (current == NULL) {
    *output = strdup(VFS_UNREACHABLE);
    if (*output == NULL) {
      return enomem();
    }
    return 0;
  }
  *output = malloc(len + 1);
  if (*output == NULL) {
    return enomem();
  }
  (*output)[len] = '\0';
  current = info;
  while (current && (current->dev != root->dev || current->ino != root->ino)) {
    currentlen = strlen(current->name);
    len -= currentlen;
    memcpy((void *)(*output + len), current->name, currentlen);
    if (len != 0) {
      --len;
      (*output)[len] = '/';
    }
    current = current->parent;
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int VfsAddFd(struct VfsInfo *data) {
  struct VfsFd *vfsfd;
  struct Dll *e;
  int nextfd = 0;
  vfsfd = malloc(sizeof(*vfsfd));
  if (vfsfd == NULL) {
    return -1;
  }
  vfsfd->data = data;
  LOCK(&g_vfs.lock);
  for (e = dll_first(g_vfs.fds); e; e = dll_next(g_vfs.fds, e)) {
    if (VFS_FD_CONTAINER(e)->fd == nextfd) {
      ++nextfd;
    } else {
      break;
    }
  }
  vfsfd->fd = nextfd;
  VFS_LOGF("VfsAddFd -> %d", vfsfd->fd);
  dll_init(&vfsfd->elem);
  if (e == NULL) {
    dll_make_last(&g_vfs.fds, &vfsfd->elem);
  } else {
    e = dll_prev(g_vfs.fds, e);
    if (e == NULL) {
      dll_make_first(&g_vfs.fds, &vfsfd->elem);
    } else {
      dll_splice_after(e, &vfsfd->elem);
    }
  }
  UNLOCK(&g_vfs.lock);
  return vfsfd->fd;
}

/**
 * Closes the emulated file descriptor fd and returns the data associated with
 * it.
 */
int VfsFreeFd(int fd, struct VfsInfo **data) {
  struct Dll *e;
  struct VfsFd *vfsfd;
  LOCK(&g_vfs.lock);
  for (e = dll_first(g_vfs.fds); e;) {
    vfsfd = VFS_FD_CONTAINER(e);
    if (VFS_FD_CONTAINER(e)->fd == fd) {
      *data = vfsfd->data;
      dll_remove(&g_vfs.fds, &vfsfd->elem);
      free(vfsfd);
      VFS_LOGF("VfsFreeFd(%d)", fd);
      UNLOCK(&g_vfs.lock);
      return 0;
    }
    e = dll_next(g_vfs.fds, e);
  }
  UNLOCK(&g_vfs.lock);
  return ebadf();
}

int VfsGetFd(int fd, struct VfsInfo **output) {
  struct Dll *e;
  LOCK(&g_vfs.lock);
  for (e = dll_first(g_vfs.fds); e; e = dll_next(g_vfs.fds, e)) {
    if (VFS_FD_CONTAINER(e)->fd == fd) {
      UNLOCK(&g_vfs.lock);
      unassert(!VfsAcquireInfo(VFS_FD_CONTAINER(e)->data, output));
      return 0;
    }
  }
  UNLOCK(&g_vfs.lock);
  return ebadf();
}

int VfsSetFd(int fd, struct VfsInfo *data) {
  struct Dll *e;
  struct VfsFd *vfsfd;
  LOCK(&g_vfs.lock);
  for (e = dll_first(g_vfs.fds); e; e = dll_next(g_vfs.fds, e)) {
    if (VFS_FD_CONTAINER(e)->fd == fd) {
      unassert(!VfsFreeInfo(VFS_FD_CONTAINER(e)->data));
      VFS_FD_CONTAINER(e)->data = data;
      UNLOCK(&g_vfs.lock);
      return 0;
    } else if (VFS_FD_CONTAINER(e)->fd > fd) {
      break;
    }
  }
  vfsfd = malloc(sizeof(*vfsfd));
  if (vfsfd == NULL) {
    UNLOCK(&g_vfs.lock);
    return enomem();
  }
  vfsfd->data = data;
  vfsfd->fd = fd;
  dll_init(&vfsfd->elem);
  if (e == NULL) {
    dll_make_last(&g_vfs.fds, &vfsfd->elem);
  } else if ((e = dll_prev(g_vfs.fds, e))) {
    dll_splice_after(e, &vfsfd->elem);
  } else {
    dll_make_first(&g_vfs.fds, &vfsfd->elem);
  }
  UNLOCK(&g_vfs.lock);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int VfsChdir(const char *path) {
  struct VfsInfo *info;
  int ret = 0;
  VFS_LOGF("VfsChdir(\"%s\")", path);
  if (path == NULL) {
    return efault();
  }
  if (*path == '\0') {
    return enoent();
  }
  if (VfsTraverse(path, &info, true) == -1) {
    return -1;
  }
  if (!S_ISDIR(info->mode)) {
    ret = enotdir();
  } else {
    info = atomic_exchange(&g_cwdinfo, info);
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsFchdir(int fd) {
  struct VfsInfo *info;
  int ret = 0;
  VFS_LOGF("VfsFchdir(%d)", fd);
  if (VfsGetFd(fd, &info) != 0) {
    return -1;
  }
  if (!S_ISDIR(info->mode)) {
    ret = enotdir();
  } else {
    info = atomic_exchange(&g_cwdinfo, info);
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsChroot(const char *path) {
  struct VfsInfo *info;
  int ret = 0;
  VFS_LOGF("VfsChroot(\"%s\")", path);
  if (path == NULL) {
    return efault();
  }
  if (*path == '\0') {
    return enoent();
  }
  if (VfsTraverse(path, &info, true) != 0) {
    return -1;
  }
  if (!S_ISDIR(info->mode)) {
    ret = enotdir();
  } else {
    info = atomic_exchange(&g_rootinfo, info);
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

char *VfsGetcwd(char *buf, size_t size) {
  char *p;
  if (buf == NULL) {
    efault();
    return NULL;
  }
  if (size == 0) {
    return buf;
  }
  if (VfsPathBuild(g_cwdinfo, NULL, &p) == -1) {
    return NULL;
  }
  strncpy(buf, p, size);
  buf[size - 1] = '\0';
  free(p);
  return buf;
}

static int VfsHandleDirfdName(int dirfd, const char *name,
                              struct VfsInfo **parent, char **leaf) {
  struct VfsInfo *dir;
  const char *p, *q;
  char *parentname = NULL;
  if (name == NULL || parent == NULL || leaf == NULL) {
    return efault();
  }
  if (name[0] == '/') {
    unassert(!VfsAcquireInfo(g_rootinfo, &dir));
  } else if (dirfd == AT_FDCWD) {
    unassert(!VfsAcquireInfo(g_cwdinfo, &dir));
  } else {
    if (VfsGetFd(dirfd, &dir) == -1) {
      goto cleananddie;
    }
  }
  if (!strcmp(name, "/")) {
    *leaf = strdup(".");
    if (*leaf == NULL) {
      goto cleananddie;
    }
    *parent = dir;
    return 0;
  }
  if (!S_ISDIR(dir->mode)) {
    enotdir();
    goto cleananddie;
  }
  for (p = name, q = name; *p; ++p) {
    if (*p == '/' && p[1]) {
      q = p + 1;
    }
  }
  parentname = strndup(name, q - name);
  if (parentname == NULL) {
    goto cleananddie;
  }
  if (VfsTraverseStackBuild(&dir, parentname, g_rootinfo, true, 0) == -1) {
    goto cleananddie;
  }

  *parent = dir;
  *leaf = strdup(q);
  if (*leaf == NULL) {
    goto cleananddie;
  }
  free(parentname);
  return 0;
cleananddie:
  free(parentname);
  unassert(!VfsFreeInfo(dir));
  return -1;
}

static int VfsHandleDirfdSymlink(struct VfsInfo **dir, char **name) {
  int level = 0;
  char *buf, *newname, *s;
  struct VfsInfo *tmp;
  VFS_LOGF("VfsHandleDirfdSymlink(%p, %p)", dir, name);
  buf = NULL;
  tmp = NULL;
  while (true) {
    if (++level > VFS_TRAVERSE_MAX_LINKS) {
      return eloop();
    }
    if (!(*dir)->device->ops->finddir || !(*dir)->device->ops->readlink) {
      return eopnotsupp();
    }
    if ((*dir)->device->ops->finddir(*dir, *name, &tmp) == -1) {
      if (errno != ENOENT) {
        return -1;
      } else {
        break;
      }
    }
    if (!S_ISLNK(tmp->mode)) {
      unassert(!VfsFreeInfo(tmp));
      break;
    }
    if ((*dir)->device->ops->readlink(tmp, &buf) == -1) {
      goto cleananddie;
    }
    unassert(!VfsFreeInfo(tmp));
    tmp = NULL;
    if (buf[0] == '/') {
      if (VfsHandleDirfdName(AT_FDCWD, buf, &tmp, &s) == -1) {
        goto cleananddie;
      }
      free(*name);
      *name = s;
      unassert(!VfsFreeInfo(*dir));
      *dir = tmp;
    } else {
      newname = buf;
      for (s = buf; *s; ++s) {
        if (*s == '/') {
          newname = s;
          break;
        }
      }
      if (newname != buf) {
        *newname = '\0';
        ++newname;
        newname = strdup(newname);
        if (newname == NULL) {
          enomem();
          goto cleananddie;
        }
        if (VfsTraverseStackBuild(dir, buf, NULL, true, 0) == -1) {
          free(newname);
          goto cleananddie;
        }
      } else {
        // release control to newname.
        buf = NULL;
      }
      free(*name);
      *name = newname;
    }
    free(buf);
    buf = NULL;
  }
  return 0;
cleananddie:
  free(buf);
  unassert(!VfsFreeInfo(tmp));
  return -1;
}

int VfsUnlink(int dirfd, const char *name, int flags) {
  struct VfsInfo *dir;
  char *newname;
  int ret;
  VFS_LOGF("VfsUnlink(%d, \"%s\", %d)", dirfd, name, flags);
  if (name == NULL) {
    return efault();
  }
  if (!*name) {
    return enoent();
  }
  if (VfsHandleDirfdName(dirfd, name, &dir, &newname) == -1) {
    return -1;
  }
  unassert(!VfsTraverseMount(&dir, &newname));
  if (dir->device->ops->unlink) {
    ret = dir->device->ops->unlink(dir, newname, flags);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(dir));
  free(newname);
  return ret;
}

int VfsMkdir(int dirfd, const char *name, mode_t mode) {
  struct VfsInfo *dir;
  char *newname;
  int ret;
  VFS_LOGF("VfsMkdir(%d, \"%s\", %d)", dirfd, name, mode);
  if (name == NULL) {
    return efault();
  }
  if (!*name) {
    return enoent();
  }
  if (VfsHandleDirfdName(dirfd, name, &dir, &newname) == -1) {
    return -1;
  }
  unassert(!VfsTraverseMount(&dir, &newname));
  if (dir->device->ops->mkdir) {
    ret = dir->device->ops->mkdir(dir, newname, mode);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(dir));
  free(newname);
  return ret;
}

int VfsMkfifo(int dirfd, const char *name, mode_t mode) {
  struct VfsInfo *dir;
  char *newname;
  int ret;
  VFS_LOGF("VfsMkfifo(%d, \"%s\", %d)", dirfd, name, mode);
  if (name == NULL) {
    return efault();
  }
  if (!*name) {
    return enoent();
  }
  if (VfsHandleDirfdName(dirfd, name, &dir, &newname) == -1) {
    return -1;
  }
  unassert(!VfsTraverseMount(&dir, &newname));
  if (dir->device->ops->mkfifo) {
    ret = dir->device->ops->mkfifo(dir, newname, mode);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(dir));
  free(newname);
  return ret;
}

int VfsOpen(int dirfd, const char *name, int flags, int mode) {
  struct VfsInfo *dir, *out;
  char *newname;
  int ret = 0;
  VFS_LOGF("VfsOpen(%d, \"%s\", %d, %d)", dirfd, name, flags, mode);
  if (name == NULL) {
    return efault();
  }
  if (!*name) {
    return enoent();
  }
  if (VfsHandleDirfdName(dirfd, name, &dir, &newname) == -1) {
    return -1;
  }
  if (!(flags & O_NOFOLLOW)) {
    ret = VfsHandleDirfdSymlink(&dir, &newname);
  }
  unassert(!VfsTraverseMount(&dir, &newname));
  if (ret != -1) {
    if (dir->device->ops->open) {
      if (dir->device->ops->open(dir, newname, flags, mode, &out) == -1) {
        ret = -1;
      } else {
        ret = VfsAddFd(out);
      }
    } else {
      ret = eopnotsupp();
    }
  }
  unassert(!VfsFreeInfo(dir));
  free(newname);
  return ret;
}

int VfsChmod(int dirfd, const char *name, mode_t mode, int flags) {
  struct VfsInfo *dir;
  char *newname;
  int ret = 0;
  VFS_LOGF("VfsChmod(%d, \"%s\", %d, %d)", dirfd, name, mode, flags);
  if (name == NULL) {
    return efault();
  }
  if (!*name) {
    return enoent();
  }
  if (VfsHandleDirfdName(dirfd, name, &dir, &newname) == -1) {
    return -1;
  }
  if (!(flags & AT_SYMLINK_NOFOLLOW)) {
    ret = VfsHandleDirfdSymlink(&dir, &newname);
  }
  unassert(!VfsTraverseMount(&dir, &newname));
  if (ret != -1) {
    if (dir->device->ops->chmod) {
      ret = dir->device->ops->chmod(dir, newname, mode, flags);
    } else {
      ret = eopnotsupp();
    }
  }
  unassert(!VfsFreeInfo(dir));
  free(newname);
  return ret;
}

int VfsFchmod(int fd, mode_t mode) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsFchmod(%d, %d)", fd, mode);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->fchmod) {
    ret = info->device->ops->fchmod(info, mode);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsAccess(int dirfd, const char *name, mode_t mode, int flags) {
  struct VfsInfo *dir;
  char *newname;
  int ret = 0;
  VFS_LOGF("VfsAccess(%d, \"%s\", %d, %d)", dirfd, name, mode, flags);
  if (name == NULL) {
    return efault();
  }
  if (!*name) {
    return enoent();
  }
  if (VfsHandleDirfdName(dirfd, name, &dir, &newname) == -1) {
    return -1;
  }
  if (!(flags & AT_SYMLINK_NOFOLLOW)) {
    ret = VfsHandleDirfdSymlink(&dir, &newname);
  }
  unassert(!VfsTraverseMount(&dir, &newname));
  if (ret != -1) {
    if (dir->device->ops->access) {
      ret = dir->device->ops->access(dir, newname, mode, flags);
    } else {
      ret = eopnotsupp();
    }
  }
  unassert(!VfsFreeInfo(dir));
  free(newname);
  return ret;
}

int VfsSymlink(const char *target, int dirfd, const char *name) {
  struct VfsInfo *dir;
  char *newname;
  int ret;
  VFS_LOGF("VfsSymlink(\"%s\", %d, \"%s\")", target, dirfd, name);
  if (target == NULL || name == NULL) {
    return efault();
  }
  if (!*name) {
    return enoent();
  }
  if (VfsHandleDirfdName(dirfd, name, &dir, &newname) == -1) {
    return -1;
  }
  unassert(!VfsTraverseMount(&dir, &newname));
  if (dir->device->ops->symlink) {
    ret = dir->device->ops->symlink(target, dir, newname);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(dir));
  free(newname);
  return ret;
}

int VfsStat(int dirfd, const char *name, struct stat *st, int flags) {
  struct VfsInfo *dir;
  char *newname;
  int ret = 0;
  VFS_LOGF("VfsStat(%d, \"%s\", %p, %d)", dirfd, name, st, flags);
  if (name == NULL || st == NULL) {
    return efault();
  }
  if (VfsHandleDirfdName(dirfd, name, &dir, &newname) == -1) {
    return -1;
  }
  if (!(flags & AT_SYMLINK_NOFOLLOW)) {
    ret = VfsHandleDirfdSymlink(&dir, &newname);
  }
  unassert(!VfsTraverseMount(&dir, &newname));
  if (ret != -1) {
    if (dir->device->ops->stat) {
      ret = dir->device->ops->stat(dir, newname, st, flags);
    } else {
      ret = eopnotsupp();
    }
  }
  unassert(!VfsFreeInfo(dir));
  free(newname);
  return ret;
}

int VfsChown(int dirfd, const char *name, uid_t uid, gid_t gid, int flags) {
  struct VfsInfo *dir;
  char *newname;
  int ret = 0;
  VFS_LOGF("VfsChown(%d, \"%s\", %d, %d, %d)", dirfd, name, uid, gid, flags);
  if (name == NULL) {
    return efault();
  }
  if (VfsHandleDirfdName(dirfd, name, &dir, &newname) == -1) {
    return -1;
  }
  if (!(flags & AT_SYMLINK_NOFOLLOW)) {
    ret = VfsHandleDirfdSymlink(&dir, &newname);
  }
  unassert(!VfsTraverseMount(&dir, &newname));
  if (ret != -1) {
    if (dir->device->ops->chown) {
      ret = dir->device->ops->chown(dir, newname, uid, gid, flags);
    } else {
      ret = eopnotsupp();
    }
  }
  unassert(!VfsFreeInfo(dir));
  free(newname);
  return ret;
}

int VfsFchown(int fd, uid_t uid, gid_t gid) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsFchown(%d, %d, %d)", fd, uid, gid);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->fchown) {
    ret = info->device->ops->fchown(info, uid, gid);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsRename(int olddirfd, const char *oldname, int newdirfd,
              const char *newname) {
  struct VfsInfo *olddir, *newdir;
  char *newoldname, *newnewname;
  int ret;
  VFS_LOGF("VfsRename(%d, \"%s\", %d, \"%s\")", olddirfd, oldname, newdirfd,
           newname);
  if (oldname == NULL || newname == NULL) {
    return efault();
  }
  if (!*oldname || !*newname) {
    return enoent();
  }
  if (VfsHandleDirfdName(olddirfd, oldname, &olddir, &newoldname) == -1) {
    return -1;
  }
  if (VfsHandleDirfdName(newdirfd, newname, &newdir, &newnewname) == -1) {
    unassert(!VfsFreeInfo(olddir));
    free(newoldname);
    return -1;
  }
  if (olddir->device->ops->rename) {
    ret = olddir->device->ops->rename(olddir, newoldname, newdir, newnewname);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(olddir));
  unassert(!VfsFreeInfo(newdir));
  free(newoldname);
  free(newnewname);
  return ret;
}

ssize_t VfsReadlink(int dirfd, const char *name, char *buf, size_t bufsiz) {
  struct VfsInfo *dir, *file;
  ssize_t ret;
  char *tmp;
  char *newname;
  VFS_LOGF("VfsReadlink(%d, \"%s\", %p, %zu)", dirfd, name, buf, bufsiz);
  if (name == NULL || buf == NULL) {
    return efault();
  }
  newname = NULL;
  if (*name == '\0') {
    dir = NULL;
    if (VfsGetFd(dirfd, &file) == -1) {
      return -1;
    }
  } else {
    if (VfsHandleDirfdName(dirfd, name, &dir, &newname) == -1) {
      return -1;
    }
    if (!dir->device->ops->finddir ||
        dir->device->ops->finddir(dir, newname, &file) == -1) {
      unassert(!VfsFreeInfo(dir));
      free(newname);
      return -1;
    }
  }
  if (file->device->ops->readlink) {
    ret = file->device->ops->readlink(file, &tmp);
    if (ret != -1) {
      memcpy(buf, tmp, MIN(ret, bufsiz));
      ret = MIN(ret, bufsiz);
      free(tmp);
    }
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(dir));
  unassert(!VfsFreeInfo(file));
  free(newname);
  return ret;
}

int VfsLink(int olddirfd, const char *oldname, int newdirfd,
            const char *newname, int flags) {
  struct VfsInfo *olddir, *newdir;
  char *newoldname, *newnewname;
  int ret;
  VFS_LOGF("VfsLink(%d, \"%s\", %d, \"%s\", %d)", olddirfd, oldname, newdirfd,
           newname, flags);
  if (oldname == NULL || newname == NULL) {
    return efault();
  }
  if (!*oldname || !*newname) {
    return enoent();
  }
  // AT_EMPTY_PATH currently not supported by blink's syscall subsystem.
  if (VfsHandleDirfdName(olddirfd, oldname, &olddir, &newoldname) == -1) {
    return -1;
  }
  if (flags & AT_SYMLINK_FOLLOW) {
    if (VfsHandleDirfdName(olddirfd, oldname, &olddir, &newoldname) == -1) {
      unassert(!VfsFreeInfo(olddir));
      free(newoldname);
      return -1;
    }
  }
  if (VfsHandleDirfdName(newdirfd, newname, &newdir, &newnewname) == -1) {
    unassert(!VfsFreeInfo(olddir));
    free(newoldname);
    return -1;
  }
  if (olddir->device != newdir->device) {
    ret = exdev();
  } else if (olddir->device->ops->link) {
    ret = olddir->device->ops->link(olddir, newoldname, newdir, newnewname,
                                    flags);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(olddir));
  unassert(!VfsFreeInfo(newdir));
  free(newoldname);
  free(newnewname);
  return ret;
}

int VfsUtime(int dirfd, const char *name, const struct timespec times[2],
             int flags) {
  struct VfsInfo *dir;
  char *newname;
  int ret = -1;
  VFS_LOGF("VfsUtime(%d, \"%s\", %p, %d)", dirfd, name, times, flags);
  if (name == NULL) {
    return efault();
  }
  if (!*name) {
    return enoent();
  }
  if (VfsHandleDirfdName(dirfd, name, &dir, &newname) == -1) {
    return -1;
  }
  if (!(flags & AT_SYMLINK_NOFOLLOW)) {
    ret = VfsHandleDirfdSymlink(&dir, &newname);
  }
  unassert(!VfsTraverseMount(&dir, &newname));
  if (ret != -1) {
    if (dir->device->ops->utime) {
      ret = dir->device->ops->utime(dir, newname, times, flags);
    } else {
      ret = eopnotsupp();
    }
  }
  unassert(!VfsFreeInfo(dir));
  free(newname);
  return ret;
}

int VfsFutime(int fd, const struct timespec times[2]) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsFutime(%d, %p)", fd, times);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->futime) {
    ret = info->device->ops->futime(info, times);
  } else {
    unassert(!VfsFreeInfo(info));
    return eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsFstat(int fd, struct stat *st) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsFstat(%d, %p)", fd, st);
  if (st == NULL) {
    return efault();
  }
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->fstat) {
    ret = info->device->ops->fstat(info, st);
  } else {
    unassert(!VfsFreeInfo(info));
    return eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsFtruncate(int fd, off_t length) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsFtruncate(%d, %ld)", fd, length);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->ftruncate) {
    ret = info->device->ops->ftruncate(info, length);
  } else {
    unassert(!VfsFreeInfo(info));
    return eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsClose(int fd) {
  struct VfsInfo *info;
  VFS_LOGF("VfsClose(%d)", fd);
  if (VfsFreeFd(fd, &info) == -1) {
    return -1;
  }
  unassert(!VfsFreeInfo(info));
  return 0;
}

ssize_t VfsRead(int fd, void *buf, size_t nbyte) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsRead(%d, %p, %zu)", fd, buf, nbyte);
  if (buf == NULL) {
    return efault();
  }
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->read) {
    ret = info->device->ops->read(info, buf, nbyte);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

ssize_t VfsWrite(int fd, const void *buf, size_t nbyte) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsWrite(%d, %p, %zu)", fd, buf, nbyte);
  if (buf == NULL) {
    return efault();
  }
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->write) {
    ret = info->device->ops->write(info, buf, nbyte);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

ssize_t VfsPread(int fd, void *buf, size_t nbyte, off_t offset) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsPread(%d, %p, %zu, %ld)", fd, buf, nbyte, offset);
  if (buf == NULL) {
    return efault();
  }
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->pread) {
    ret = info->device->ops->pread(info, buf, nbyte, offset);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

ssize_t VfsPwrite(int fd, const void *buf, size_t nbyte, off_t offset) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsPwrite(%d, %p, %zu, %ld)", fd, buf, nbyte, offset);
  if (buf == NULL) {
    return efault();
  }
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->pwrite) {
    ret = info->device->ops->pwrite(info, buf, nbyte, offset);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

ssize_t VfsReadv(int fd, const struct iovec *iov, int iovcnt) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsReadv(%d, %p, %d)", fd, iov, iovcnt);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->readv) {
    ret = info->device->ops->readv(info, iov, iovcnt);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

ssize_t VfsWritev(int fd, const struct iovec *iov, int iovcnt) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsWritev(%d, %p, %d)", fd, iov, iovcnt);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->writev) {
    ret = info->device->ops->writev(info, iov, iovcnt);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

ssize_t VfsPreadv(int fd, const struct iovec *iov, int iovcnt, off_t offset) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsPreadv(%d, %p, %d, %ld)", fd, iov, iovcnt, offset);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->preadv) {
    ret = info->device->ops->preadv(info, iov, iovcnt, offset);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

ssize_t VfsPwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsPwritev(%d, %p, %d, %ld)", fd, iov, iovcnt, offset);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->pwritev) {
    ret = info->device->ops->pwritev(info, iov, iovcnt, offset);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

off_t VfsSeek(int fd, off_t offset, int whence) {
  struct VfsInfo *info;
  off_t ret;
  VFS_LOGF("VfsSeek(%d, %ld, %d)", fd, offset, whence);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->seek) {
    ret = info->device->ops->seek(info, offset, whence);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsFsync(int fd) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsFsync(%d)", fd);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->fsync) {
    ret = info->device->ops->fsync(info);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsFdatasync(int fd) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsFdatasync(%d)", fd);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->fdatasync) {
    ret = info->device->ops->fdatasync(info);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsFlock(int fd, int operation) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsFlock(%d, %d)", fd, operation);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->flock) {
    ret = info->device->ops->flock(info, operation);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsFcntl(int fd, int cmd, ...) {
  struct VfsInfo *info;
  int ret, tmp = -1;
  va_list ap;
  VFS_LOGF("VfsFcntl(%d, %d, ...)", fd, cmd);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  switch (cmd) {
    case F_DUPFD:
      ret = VfsDup(fd);
      break;
    case F_DUPFD_CLOEXEC:
      ret = VfsDup(fd);
      if (ret != -1) {
        tmp = ret;
        ret = VfsFcntl(ret, F_SETFD, FD_CLOEXEC);
      }
      if (ret == -1) {
        unassert(!VfsClose(tmp));
      }
      ret = tmp;
      break;
    default:
      if (info->device->ops->fcntl) {
        va_start(ap, cmd);
        ret = info->device->ops->fcntl(info, cmd, ap);
        va_end(ap);
      } else {
        ret = eopnotsupp();
      }
      break;
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsIoctl(int fd, unsigned long request, const void *arg) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsIoctl(%d, %lu, %p)", fd, request, arg);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->ioctl) {
    ret = info->device->ops->ioctl(info, request, arg);
  } else {
    errno = ENOTTY;
    ret = -1;
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsDup(int fd) {
  struct VfsInfo *info, *newinfo;
  int ret;
  VFS_LOGF("VfsDup(%d)", fd);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->dup) {
    ret = info->device->ops->dup(info, &newinfo);
    if (ret != -1) {
      ret = VfsAddFd(newinfo);
      if (ret == -1) {
        unassert(!VfsFreeInfo(newinfo));
      }
    }
  } else {
    unassert(!VfsFreeInfo(info));
    return eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsDup2(int fd, int newfd) {
  struct VfsInfo *info, *newinfo;
  int ret;
  VFS_LOGF("VfsDup2(%d, %d)", fd, newfd);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (VfsFreeFd(newfd, &newinfo) == 0) {
    unassert(!VfsFreeInfo(newinfo));
  }
  if (info->device->ops->dup) {
    ret = info->device->ops->dup(info, &newinfo);
    if (ret != -1) {
      ret = VfsSetFd(newfd, newinfo);
      if (ret != -1) {
        ret = newfd;
      }
    }
  } else {
    unassert(!VfsFreeInfo(info));
    return eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

#ifdef HAVE_DUP3
int VfsDup3(int fd, int newfd, int flags) {
  struct VfsInfo *info, *newinfo;
  int ret;
  VFS_LOGF("VfsDup3(%d, %d, %d)", fd, newfd, flags);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->dup3) {
    ret = info->device->ops->dup3(info, &newinfo, flags);
    if (ret != -1) {
      ret = VfsSetFd(newfd, newinfo);
      if (ret != -1) {
        ret = newfd;
      }
    }
  } else {
    unassert(!VfsFreeInfo(info));
    return eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}
#endif

int VfsPoll(struct pollfd *fds, nfds_t nfds, int timeout) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsPoll(%p, %lld, %d)", fds, (long long)nfds, timeout);
  // Currently, blink only uses poll with nfds = 1 and timeout = 0
  unassert(nfds == 1);
  unassert(timeout == 0);
  if (VfsGetFd(fds[0].fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->poll) {
    ret = info->device->ops->poll(&info, fds, 1, timeout);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
              struct timespec *timeout, sigset_t *sigmask) {
  VFS_LOGF("VfsSelect(%d, %p, %p, %p, %p, %p)", nfds, readfds, writefds,
           exceptfds, timeout, sigmask);
  return enosys();
}

DIR *VfsOpendir(int fd) {
  struct VfsInfo *info;
  DIR *ret;
  VFS_LOGF("VfsOpendir(%d)", fd);
  if (VfsGetFd(fd, &info) == -1) {
    return NULL;
  }
  if (info->device->ops->opendir) {
    if (info->device->ops->opendir(info, (struct VfsInfo **)&ret) == -1) {
      ret = NULL;
    }
  } else {
    eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

#ifdef HAVE_SEEKDIR
void VfsSeekdir(DIR *dir, long loc) {
  struct VfsInfo *info;
  VFS_LOGF("VfsSeekdir(%p, %ld)", dir, loc);
  info = (struct VfsInfo *)dir;
  if (info->device->ops->seekdir) {
    info->device->ops->seekdir(info, loc);
  } else {
    VFS_LOGF("seekdir() not supported by device %p.", info->device);
    eopnotsupp();
    return;
  }
}

long VfsTelldir(DIR *dir) {
  struct VfsInfo *info;
  long ret;
  VFS_LOGF("VfsTelldir(%p)", dir);
  info = (struct VfsInfo *)dir;
  if (info->device->ops->telldir) {
    ret = info->device->ops->telldir(info);
  } else {
    VFS_LOGF("telldir() not supported by device %p.", info->device);
    return eopnotsupp();
  }
  return ret;
}
#endif

struct dirent *VfsReaddir(DIR *dir) {
  struct VfsInfo *info;
  struct dirent *ret;
  VFS_LOGF("VfsReaddir(%p)", dir);
  info = (struct VfsInfo *)dir;
  if (info->device->ops->readdir) {
    ret = info->device->ops->readdir(info);
  } else {
    VFS_LOGF("readdir() not supported by device %p.", info->device);
    eopnotsupp();
    ret = NULL;
  }
  return ret;
}

void VfsRewinddir(DIR *dir) {
  struct VfsInfo *info;
  VFS_LOGF("VfsRewindDir(%p)", dir);
  info = (struct VfsInfo *)dir;
  if (info->device->ops->rewinddir) {
    info->device->ops->rewinddir(info);
  } else {
    VFS_LOGF("rewinddir() not supported by device %p.", info->device);
    eopnotsupp();
    return;
  }
}

int VfsClosedir(DIR *dir) {
  struct VfsInfo *info;
  struct Dll *e;
  struct VfsFd *vfsfd;
  int ret;
  VFS_LOGF("VfsClosedir(%p)", dir);
  info = (struct VfsInfo *)dir;
  if (info->device->ops->closedir) {
    ret = info->device->ops->closedir(info);
    if (ret != -1) {
      LOCK(&g_vfs.lock);
      for (e = dll_first(g_vfs.fds); e;) {
        vfsfd = VFS_FD_CONTAINER(e);
        e = dll_next(g_vfs.fds, e);
        if (vfsfd->data == info) {
          unassert(!VfsFreeInfo(vfsfd->data));
          dll_remove(&g_vfs.fds, &vfsfd->elem);
          free(vfsfd);
          break;
        }
      }
      UNLOCK(&g_vfs.lock);
    }
  } else {
    ret = eopnotsupp();
  }
  return ret;
}

int VfsBind(int fd, const struct sockaddr *addr, socklen_t addrlen) {
  struct VfsInfo *info, *dir;
  char *newname;
  int ret;
  VFS_LOGF("VfsBind(%d, %p, %d)", fd, addr, addrlen);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (addr->sa_family != AF_UNIX) {
    ret = HostfsBind(info, addr, addrlen);
  } else {
    if (VfsHandleDirfdName(AT_FDCWD, addr->sa_data, &dir, &newname) == -1) {
      ret = -1;
    } else {
      unassert(!VfsAcquireInfo(dir, &info->parent));
      unassert(!VfsAcquireDevice(dir->device, &info->device));
      info->name = newname;
      // The new driver should know the info's parent, name and device.
      // The data still belongs to hostfs, which should be freed and replaced
      // with its own.
      ret = dir->device->ops->bind(info, addr, addrlen);
      if (ret == -1) {
        unassert(!VfsFreeInfo(info->parent));
        info->parent = NULL;
        unassert(!VfsFreeDevice(info->device));
        info->device = NULL;
        free((void *)info->name);
        info->name = NULL;
      }
    }
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsConnect(int fd, const struct sockaddr *addr, socklen_t addrlen) {
  struct VfsInfo *info, *sock;
  const char *name;
  int ret;
  VFS_LOGF("VfsConnect(%d, %p, %d)", fd, addr, addrlen);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (addr->sa_family != AF_UNIX) {
    ret = HostfsConnect(info, addr, addrlen);
  } else {
    name = ((struct sockaddr_un *)addr)->sun_path;
    if (VfsTraverse(name, &sock, false) == -1) {
      ret = -1;
    } else {
      if (info->device->ops->connectunix) {
        ret = info->device->ops->connectunix(
            sock, info, (const struct sockaddr_un *)addr, addrlen);
      } else {
        ret = eopnotsupp();
      }
      unassert(!VfsFreeInfo(sock));
    }
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsAccept(int fd, struct sockaddr *addr, socklen_t *addrlen) {
  struct VfsInfo *info, *newinfo;
  int ret;
  VFS_LOGF("VfsAccept(%d, %p, %p)", fd, addr, addrlen);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (addr->sa_family != AF_UNIX) {
    ret = HostfsAccept(info, addr, addrlen, &newinfo);
  } else {
    if (info->device->ops->accept) {
      ret = info->device->ops->accept(info, addr, addrlen, &newinfo);
    } else {
      ret = eopnotsupp();
    }
  }
  if (ret != -1) {
    ret = VfsAddFd(newinfo);
    if (ret == -1) {
      unassert(!VfsFreeInfo(newinfo));
    }
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsListen(int fd, int backlog) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsListen(%d, %d)", fd, backlog);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->listen) {
    ret = info->device->ops->listen(info, backlog);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsShutdown(int fd, int how) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsShutdown(%d, %d)", fd, how);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->shutdown) {
    ret = info->device->ops->shutdown(info, how);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

ssize_t VfsRecvmsg(int fd, struct msghdr *msg, int flags) {
  struct VfsInfo *info, *sock;
  const char *name;
  int ret;
  VFS_LOGF("VfsRecvmsg(%d, %p, %d)", fd, msg, flags);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (msg->msg_name &&
      ((struct sockaddr *)msg->msg_name)->sa_family == AF_UNIX) {
    name = ((struct sockaddr_un *)msg->msg_name)->sun_path;
    if (VfsTraverse(name, &sock, false) == -1) {
      ret = -1;
    } else {
      if (sock->device->ops->recvmsgunix) {
        ret = sock->device->ops->recvmsgunix(sock, info, msg, flags);
      } else {
        ret = eopnotsupp();
      }
      unassert(!VfsFreeInfo(sock));
    }
  }
  if (info->device->ops->recvmsg) {
    ret = info->device->ops->recvmsg(info, msg, flags);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

ssize_t VfsSendmsg(int fd, const struct msghdr *msg, int flags) {
  struct VfsInfo *info, *sock;
  const char *name;
  int ret;
  VFS_LOGF("VfsSendmsg(%d, %p, %d)", fd, msg, flags);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (msg->msg_name &&
      ((struct sockaddr *)msg->msg_name)->sa_family == AF_UNIX) {
    name = ((struct sockaddr_un *)msg->msg_name)->sun_path;
    if (VfsTraverse(name, &sock, false) == -1) {
      ret = -1;
    } else {
      if (sock->device->ops->sendmsgunix) {
        ret = sock->device->ops->sendmsgunix(sock, info, msg, flags);
      } else {
        ret = eopnotsupp();
      }
      unassert(!VfsFreeInfo(sock));
    }
  } else {
    if (info->device->ops->sendmsg) {
      ret = info->device->ops->sendmsg(info, msg, flags);
    } else {
      ret = eopnotsupp();
    }
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsGetsockopt(int fd, int level, int optname, void *optval,
                  socklen_t *optlen) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsGetsockopt(%d, %d, %d, %p, %p)", fd, level, optname, optval,
           optlen);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->getsockopt) {
    ret = info->device->ops->getsockopt(info, level, optname, optval, optlen);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsSetsockopt(int fd, int level, int optname, const void *optval,
                  socklen_t optlen) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsSetsockopt(%d, %d, %d, %p, %u)", fd, level, optname, optval,
           optlen);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->setsockopt) {
    ret = info->device->ops->setsockopt(info, level, optname, optval, optlen);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsGetsockname(int fd, struct sockaddr *addr, socklen_t *addrlen) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsGetsockname(%d, %p, %p)", fd, addr, addrlen);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->getsockname) {
    ret = info->device->ops->getsockname(info, addr, addrlen);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsGetpeername(int fd, struct sockaddr *addr, socklen_t *addrlen) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsGetpeername(%d, %p, %p)", fd, addr, addrlen);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->getpeername) {
    ret = info->device->ops->getpeername(info, addr, addrlen);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

////////////////////////////////////////////////////////////////////////////////

static bool VfsMemoryRangeOverlap(void *a, size_t alen, void *b, size_t blen) {
  return (a <= b && b < a + alen) || (b <= a && a < b + blen);
}

static bool VfsMemoryRangeContains(void *a, size_t alen, void *b, size_t blen) {
  return a <= b && b + blen <= a + alen;
}

static int VfsMapFree(struct VfsMap *map) {
  if (map == NULL) {
    return 0;
  }
  if (VfsFreeInfo(map->data) == -1) {
    return -1;
  }
  free(map);
  return 0;
}

static int VfsMapSplit(struct VfsMap *map, void *addr, size_t len,
                       struct VfsMap **left, struct VfsMap **right) {
  struct VfsMap *l, *r;
  void *end, *mapend;
  l = r = NULL;
  unassert(VfsMemoryRangeOverlap(map->addr, map->len, addr, len));
  if (map->addr + map->len > addr && map->addr < addr) {
    l = malloc(sizeof(*l));
    if (!l) return enomem();
    l->addr = map->addr;
    l->len = addr - map->addr;
    l->offset = map->offset;
    l->prot = map->prot;
    l->flags = map->flags;
    unassert(!VfsAcquireInfo(map->data, &l->data));
    dll_init(&l->elem);
  }
  if (addr + len > map->addr && addr + len < map->addr + map->len) {
    r = malloc(sizeof(*r));
    if (!r) {
      unassert(!VfsMapFree(l));
      return enomem();
    }
    r->addr = addr + len;
    r->len = map->addr + map->len - (addr + len);
    r->offset = map->offset + (addr + len - map->addr);
    r->prot = map->prot;
    r->flags = map->flags;
    unassert(!VfsAcquireInfo(map->data, &r->data));
    dll_init(&r->elem);
  }
  end = addr + len;
  mapend = map->addr + map->len;
  map->addr = MAX(map->addr, addr);
  map->len = MIN(mapend, end) - map->addr;
  *left = l;
  *right = r;
  return 0;
}

static int VfsMapListExtractAffectedRange(struct Dll **maps, void *addr,
                                          size_t len, struct Dll **original,
                                          struct Dll **modified,
                                          struct Dll **before) {
  struct Dll *e, *f;
  struct VfsMap *map, *newmap, *newmapleft = 0, *newmapright = 0;
  e = dll_first(*maps);
  while (e && !VfsMemoryRangeOverlap(VFS_MAP_CONTAINER(e)->addr,
                                     VFS_MAP_CONTAINER(e)->len, addr, len)) {
    e = dll_next(*maps, e);
  }
  if (!e) {
    *original = *modified = *before = NULL;
    return 0;
  }
  *original = *modified = NULL;
  *before = dll_prev(*maps, e);
  while (e && (map = VFS_MAP_CONTAINER(e)) &&
         VfsMemoryRangeOverlap(map->addr, map->len, addr, len)) {
    newmap = malloc(sizeof(*newmap));
    if (!newmap) return enomem();
    newmap->addr = map->addr;
    newmap->len = map->len;
    newmap->offset = map->offset;
    newmap->prot = map->prot;
    newmap->flags = map->flags;
    unassert(!VfsAcquireInfo(map->data, &newmap->data));
    dll_init(&newmap->elem);
    if (VfsMapSplit(newmap, addr, len, &newmapleft, &newmapright) == -1) {
      unassert(!VfsMapFree(newmap));
      if (before == NULL) {
        e = dll_first(*original);
        dll_remove(original, e);
        dll_make_first(maps, e);
        dll_splice_after(e, dll_first(*original));
      } else {
        dll_splice_after(*before, dll_first(*original));
      }
      for (e = dll_first(*modified); e;) {
        map = VFS_MAP_CONTAINER(e);
        e = dll_next(*modified, e);
        dll_remove(modified, &map->elem);
        unassert(!VfsMapFree(map));
      }
      return -1;
    }
    if (newmapleft) {
      dll_make_last(modified, &newmapleft->elem);
    }
    dll_make_last(modified, &newmap->elem);
    if (newmapright) {
      dll_make_last(modified, &newmapright->elem);
    }
    f = dll_next(*maps, e);
    dll_remove(maps, e);
    dll_make_last(original, e);
    e = f;
  }
  return 0;
}

static void VfsMapListJoin(struct Dll **maps, struct Dll **tojoin,
                           struct Dll *before) {
  struct Dll *e;
  if (*tojoin == NULL) {
    return;
  }
  if (before == NULL) {
    e = dll_first(*tojoin);
    dll_remove(tojoin, e);
    dll_make_first(&g_vfs.maps, e);
    if (!dll_is_empty(*tojoin)) {
      dll_splice_after(e, dll_first(*tojoin));
    }
  } else {
    dll_splice_after(before, dll_first(*tojoin));
  }
}

static void VfsMapListFree(struct Dll **tofree) {
  struct Dll *e;
  struct VfsMap *map;
  for (e = dll_first(*tofree); e;) {
    map = VFS_MAP_CONTAINER(e);
    e = dll_next(*tofree, e);
    dll_remove(tofree, &map->elem);
    unassert(!VfsMapFree(map));
  }
}

////////////////////////////////////////////////////////////////////////////////

void *VfsMmap(void *addr, size_t len, int prot, int flags, int fd,
              off_t offset) {
  struct VfsInfo *info;
  struct VfsMap *map, *newmap;
  void *ret;
  struct Dll *e, *original, *modified, *before;
  VFS_LOGF("VfsMmap(%p, %zu, %d, %d, %d, %ld)", addr, len, prot, flags, fd,
           offset);
  LOCK(&g_vfs.mapslock);
  info = NULL;
  original = modified = NULL;
  newmap = NULL;
  if (VfsMapListExtractAffectedRange(&g_vfs.maps, addr, len, &original,
                                     &modified, &before) == -1) {
    goto cleananddie;
  }
#ifdef MAP_ANONYMOUS
  if (flags & MAP_ANONYMOUS) {
    if ((ret = mmap(addr, len, prot, flags, -1, 0)) == MAP_FAILED) {
      goto cleananddie;
    }
  } else {
#else
  {
#endif
    if (VfsGetFd(fd, &info) == -1) {
      goto cleananddie;
    }
    newmap = malloc(sizeof(*newmap));
    if (!newmap) {
      goto cleananddie;
    }
    newmap->len = len;
    newmap->offset = offset;
    newmap->prot = prot;
    newmap->flags = flags;
    unassert(!VfsAcquireInfo(info, &newmap->data));
    dll_init(&newmap->elem);
    if (info->device->ops->mmap) {
      if ((ret = info->device->ops->mmap(info, addr, len, prot, flags,
                                         offset)) == MAP_FAILED) {
        goto cleananddie;
      }
      newmap->addr = ret;
    } else {
      enodev();
      goto cleananddie;
    }
  }
  if (flags & MAP_FIXED) {
    // Successful MAP_FIXED means that the old mappings were unmapped
    for (e = dll_first(modified); e;) {
      map = VFS_MAP_CONTAINER(e);
      if (!VfsMemoryRangeContains(addr, len, map->addr, map->len)) {
        unassert(!VfsMemoryRangeOverlap(addr, len, map->addr, map->len));
        e = dll_next(modified, e);
        continue;
      }
      if (map->data->device->ops->munmap) {
        unassert(
            !map->data->device->ops->munmap(map->data, map->addr, map->len));
      }
      e = dll_next(modified, e);
      dll_remove(&modified, &map->elem);
      unassert(!VfsMapFree(map));
    }
  }
  if (newmap) {
    e = dll_last(modified);
    while (e && (map = VFS_MAP_CONTAINER(e)) &&
           map->addr + map->len > newmap->addr) {
      unassert(!VfsMemoryRangeOverlap(map->addr, map->len, newmap->addr,
                                      newmap->len));
      e = dll_prev(modified, e);
    }
    if (e) {
      dll_splice_after(e, &newmap->elem);
    } else {
      dll_make_first(&modified, &newmap->elem);
    }
  }
  VfsMapListFree(&original);
  VfsMapListJoin(&g_vfs.maps, &modified, before);
  VfsFreeInfo(info);
  UNLOCK(&g_vfs.mapslock);
  return ret;
cleananddie:
  VfsMapListJoin(&g_vfs.maps, &original, before);
  VfsMapListFree(&modified);
  VfsFreeInfo(info);
  UNLOCK(&g_vfs.mapslock);
  unassert(!VfsMapFree(newmap));
  return MAP_FAILED;
}

int VfsMunmap(void *addr, size_t len) {
  struct Dll *e;
  struct VfsMap *map;
  struct Dll *original, *modified, *before;
  VFS_LOGF("VfsMunmap(%p, %zu)", addr, len);
  original = modified = NULL;
  LOCK(&g_vfs.mapslock);
  if (VfsMapListExtractAffectedRange(&g_vfs.maps, addr, len, &original,
                                     &modified, &before) == -1) {
    goto cleananddie;
  }
  if (munmap(addr, len) == -1) {
    goto cleananddie;
  }
  for (e = dll_first(modified); e;) {
    map = VFS_MAP_CONTAINER(e);
    if (!VfsMemoryRangeContains(addr, len, map->addr, map->len)) {
      unassert(!VfsMemoryRangeOverlap(addr, len, map->addr, map->len));
      e = dll_next(modified, e);
      continue;
    }
    if (map->data->device->ops->munmap) {
      unassert(!map->data->device->ops->munmap(map->data, map->addr, map->len));
    }
    e = dll_next(modified, e);
    dll_remove(&modified, &map->elem);
    unassert(!VfsMapFree(map));
  }
  VfsMapListFree(&original);
  VfsMapListJoin(&g_vfs.maps, &modified, before);
  UNLOCK(&g_vfs.mapslock);
  return 0;
cleananddie:
  VfsMapListJoin(&g_vfs.maps, &original, before);
  VfsMapListFree(&modified);
  UNLOCK(&g_vfs.mapslock);
  return -1;
}

int VfsMprotect(void *addr, size_t len, int prot) {
  struct Dll *e;
  struct VfsMap *map;
  struct Dll *original, *modified, *before;
  VFS_LOGF("VfsMprotect(%p, %zu, %d)", addr, len, prot);
  original = modified = NULL;
  LOCK(&g_vfs.mapslock);
  if (VfsMapListExtractAffectedRange(&g_vfs.maps, addr, len, &original,
                                     &modified, &before) == -1) {
    goto cleananddie;
  }
  for (e = dll_first(modified); e;) {
    map = VFS_MAP_CONTAINER(e);
    if (!VfsMemoryRangeContains(addr, len, map->addr, map->len)) {
      unassert(!VfsMemoryRangeOverlap(addr, len, map->addr, map->len));
      e = dll_next(modified, e);
      continue;
    }
    if (map->data->device->ops->mprotect) {
      if (map->data->device->ops->mprotect(map->data, map->addr, map->len,
                                           prot) == -1) {
        goto cleananddie;
      }
      map->prot = prot;
    }
    e = dll_next(modified, e);
  }
  if (mprotect(addr, len, prot) == -1) {
    goto cleananddie;
  }
  VfsMapListFree(&original);
  VfsMapListJoin(&g_vfs.maps, &modified, before);
  UNLOCK(&g_vfs.mapslock);
  return 0;
cleananddie:
  if (original != NULL) {
    for (e = dll_first(original); e;) {
      map = VFS_MAP_CONTAINER(e);
      if (map->data->device->ops->mprotect) {
        unassert(!map->data->device->ops->mprotect(map->data, map->addr,
                                                   map->len, map->prot));
      }
    }
  }
  VfsMapListJoin(&g_vfs.maps, &original, before);
  VfsMapListFree(&modified);
  UNLOCK(&g_vfs.mapslock);
  return -1;
}

int VfsMsync(void *addr, size_t len, int flags) {
  struct Dll *e;
  struct VfsMap *map;
  void *currentaddr;
  size_t currentlen;
  VFS_LOGF("VfsMsync(%p, %zu, %d)", addr, len, flags);
  if (msync(addr, len, flags) == -1) {
    return -1;
  }
  LOCK(&g_vfs.mapslock);
  for (e = dll_first(g_vfs.maps); e; e = dll_next(g_vfs.maps, e)) {
    map = VFS_MAP_CONTAINER(e);
    if (map->addr + map->len <= addr || map->addr >= addr + len) {
      continue;
    }
    if (!map->data->device->ops->msync) {
      UNLOCK(&g_vfs.mapslock);
      return enodev();
    }
    currentaddr = map->addr;
    currentlen = map->len;
    if (currentaddr < addr) {
      currentlen -= addr - currentaddr;
      currentaddr = addr;
    }
    if (currentaddr + currentlen > addr + len) {
      currentlen = addr + len - currentaddr;
    }
    if (map->data->device->ops->msync(map->data, currentaddr, currentlen,
                                      flags) == -1) {
      UNLOCK(&g_vfs.mapslock);
      return -1;
    }
  }
  UNLOCK(&g_vfs.mapslock);
  return 0;
}

int VfsTcgetattr(int fd, struct termios *termios_p) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsTcgetattr(%d, %p)", fd, termios_p);
  if (termios_p == NULL) {
    return efault();
  }
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->tcgetattr) {
    ret = info->device->ops->tcgetattr(info, termios_p);
  } else {
    errno = ENOTTY;
    ret = -1;
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsTcsetattr(int fd, int optional_actions,
                 const struct termios *termios_p) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsTcsetattr(%d, %d, %p)", fd, optional_actions, termios_p);
  if (termios_p == NULL) {
    return efault();
  }
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->tcsetattr) {
    ret = info->device->ops->tcsetattr(info, optional_actions, termios_p);
  } else {
    errno = ENOTTY;
    ret = -1;
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsTcflush(int fd, int queue_selector) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsTcflush(%d, %d)", fd, queue_selector);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->tcflush) {
    ret = info->device->ops->tcflush(info, queue_selector);
  } else {
    errno = ENOTTY;
    ret = -1;
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsTcdrain(int fd) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsTcdrain(%d)", fd);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->tcdrain) {
    ret = info->device->ops->tcdrain(info);
  } else {
    errno = ENOTTY;
    ret = -1;
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsTcsendbreak(int fd, int duration) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsTcsendbreak(%d, %d)", fd, duration);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->tcsendbreak) {
    ret = info->device->ops->tcsendbreak(info, duration);
  } else {
    errno = ENOTTY;
    ret = -1;
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsTcflow(int fd, int action) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsTcflow(%d, %d)", fd, action);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->tcflow) {
    ret = info->device->ops->tcflow(info, action);
  } else {
    errno = ENOTTY;
    ret = -1;
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

pid_t VfsTcgetsid(int fd) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsTcgetsid(%d)", fd);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->tcgetsid) {
    ret = info->device->ops->tcgetsid(info);
  } else {
    errno = ENOTTY;
    ret = -1;
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

pid_t VfsTcgetpgrp(int fd) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsTcgetpgrp(%d)", fd);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->tcgetpgrp) {
    ret = info->device->ops->tcgetpgrp(info);
  } else {
    errno = ENOTTY;
    ret = -1;
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsTcsetpgrp(int fd, pid_t pgrp_id) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsTcsetpgrp(%d, %d)", fd, pgrp_id);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->tcsetpgrp) {
    ret = info->device->ops->tcsetpgrp(info, pgrp_id);
  } else {
    errno = ENOTTY;
    ret = -1;
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsSockatmark(int fd) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsSockatmark(%d)", fd);
  if (VfsGetFd(fd, &info) == -1) {
    return -1;
  }
  if (info->device->ops->sockatmark) {
    ret = info->device->ops->sockatmark(info);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

int VfsExecve(const char *pathname, char *const *argv, char *const *envp) {
  struct VfsInfo *info;
  int ret;
  VFS_LOGF("VfsExecve(\"%s\", %p, %p)", pathname, argv, envp);
  info = NULL;
  if ((ret = VfsOpen(AT_FDCWD, pathname, O_RDONLY | O_CLOEXEC, 0)) == -1) {
    return -1;
  }
  if (VfsFreeFd(ret, &info) == -1) {
    return -1;
  }
  if (info->device->ops->fexecve) {
    ret = info->device->ops->fexecve(info, argv, envp);
  } else {
    ret = eopnotsupp();
  }
  unassert(!VfsFreeInfo(info));
  return ret;
}

////////////////////////////////////////////////////////////////////////////////

int VfsPipe(int fds[2]) {
  struct VfsInfo *infos[2];
  VFS_LOGF("VfsPipe(%p)", fds);
  if (fds == NULL) {
    return efault();
  }
  if (HostfsPipe(infos) == -1) {
    return -1;
  }
  if ((fds[0] = VfsAddFd(infos[0])) == -1) {
    VfsFreeInfo(infos[0]);
    VfsFreeInfo(infos[1]);
    return -1;
  }
  if ((fds[1] = VfsAddFd(infos[1])) == -1) {
    VfsFreeFd(fds[0], &infos[0]);
    VfsFreeInfo(infos[0]);
    VfsFreeInfo(infos[1]);
    return -1;
  }
  return 0;
}

int VfsPipe2(int fds[2], int flags) {
  struct VfsInfo *infos[2];
  VFS_LOGF("VfsPipe2(%p, %d)", fds, flags);
  if (fds == NULL) {
    return efault();
  }
  if (HostfsPipe2(infos, flags) == -1) {
    return -1;
  }
  if ((fds[0] = VfsAddFd(infos[0])) == -1) {
    VfsFreeInfo(infos[0]);
    VfsFreeInfo(infos[1]);
    return -1;
  }
  if ((fds[1] = VfsAddFd(infos[1])) == -1) {
    VfsFreeFd(fds[0], &infos[0]);
    VfsFreeInfo(infos[0]);
    VfsFreeInfo(infos[1]);
    return -1;
  }
  return 0;
}

int VfsSocket(int domain, int type, int protocol) {
  struct VfsInfo *info;
  int fd;
  VFS_LOGF("VfsSocket(%d, %d, %d)", domain, type, protocol);
  if (HostfsSocket(domain, type, protocol, &info) == -1) {
    return -1;
  }
  if ((fd = VfsAddFd(info)) == -1) {
    unassert(!VfsFreeInfo(info));
    return -1;
  }
  return fd;
}

int VfsSocketpair(int domain, int type, int protocol, int fds[2]) {
  struct VfsInfo *infos[2];
  VFS_LOGF("VfsSocketPair(%d, %d, %d, %p)", domain, type, protocol, fds);
  if (fds == NULL) {
    return efault();
  }
  if (HostfsSocketpair(domain, type, protocol, infos) == -1) {
    return -1;
  }
  if ((fds[0] = VfsAddFd(infos[0])) == -1) {
    VfsFreeInfo(infos[0]);
    VfsFreeInfo(infos[1]);
    return -1;
  }
  if ((fds[1] = VfsAddFd(infos[1])) == -1) {
    VfsFreeFd(fds[0], &infos[0]);
    VfsFreeInfo(infos[0]);
    VfsFreeInfo(infos[1]);
    return -1;
  }
  return 0;
}

#endif /* DISABLE_VFS */
