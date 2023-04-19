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
#include "blink/procfs.h"

#include <fcntl.h>
#include <stdlib.h>

#include "blink/atomic.h"
#include "blink/errno.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/timespec.h"
#include "blink/vfs.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef __CYGWIN__
#include <windows.h>
#include <winternl.h>
#endif

#ifndef DISABLE_VFS

#define PROCFS_NAME_MAX 16
#define PROCFS_READ_LEN 4096
#define PROCFS_DELETED  " (deleted)"

struct ProcfsInfo {
  u64 ino;
  u32 mode;
  u32 uid;
  u32 gid;
  u32 type;
  char name[PROCFS_NAME_MAX];
  struct timespec time;
  union {
    struct ProcfsOpenFile *openfile;
    struct ProcfsOpenDir *opendir;
  };
  union {
    int (*readdir)(struct VfsInfo *, struct dirent *);
    ssize_t (*readlink)(struct VfsInfo *, char **);
    int (*read)(struct VfsInfo *, struct ProcfsOpenFile *);
  };
};

struct ProcfsOpenFile {
  pthread_mutex_t_ lock;
  size_t index;
  off_t offset;
  int openflags;
  size_t readbufstart;
  size_t readbufend;
  char readbuf[PROCFS_READ_LEN];
};

struct ProcfsOpenDir {
  pthread_mutex_t_ lock;
  size_t index;
};

struct ProcfsDevice {
  struct timespec mounttime;
};

enum {
  PROCFS_NULL_INO,
  PROCFS_ROOT_INO,
  PROCFS_FILESYSTEMS_INO,
  PROCFS_MEMINFO_INO,
  PROCFS_MOUNTS_INO,
  PROCFS_SELF_INO,
  PROCFS_SYS_INO,
  PROCFS_UPTIME_INO,

  PROCFS_FIRST_PID_INO
};

enum {
  PROCFS_ROOT_TYPE,
  PROCFS_MEMINFO_TYPE,
  PROCFS_FILESYSTEMS_TYPE,
  PROCFS_MOUNTS_TYPE,
  PROCFS_SELF_TYPE,
  PROCFS_SYS_TYPE,
  PROCFS_UPTIME_TYPE,

  PROCFS_PIDDIR_TYPE,
  PROCFS_PIDDIR_EXE_TYPE,
  PROCFS_PIDDIR_CWD_TYPE,
  PROCFS_PIDDIR_ROOT_TYPE,
  PROCFS_PIDDIR_MOUNTS_TYPE,
  PROCFS_PIDDIR_FDDIR_TYPE,
  PROCFS_PIDDIR_LAST_TYPE = PROCFS_PIDDIR_FDDIR_TYPE
};

static int ProcfsRootReaddir(struct VfsInfo *, struct dirent *);
static ssize_t ProcfsSelfReadlink(struct VfsInfo *, char **);
static ssize_t ProcfsToSelfReadlink(struct VfsInfo *, char **);
static int ProcfsMeminfoRead(struct VfsInfo *, struct ProcfsOpenFile *);
static int ProcfsUptimeRead(struct VfsInfo *, struct ProcfsOpenFile *);
static int ProcfsFilesystemsRead(struct VfsInfo *, struct ProcfsOpenFile *);

static int ProcfsPiddirReaddir(struct VfsInfo *, struct dirent *);
static ssize_t ProcfsPiddirExeReadlink(struct VfsInfo *, char **);
static ssize_t ProcfsPiddirCwdReadlink(struct VfsInfo *, char **);
static ssize_t ProcfsPiddirRootReadlink(struct VfsInfo *, char **);
static int ProcfsPiddirMountsRead(struct VfsInfo *, struct ProcfsOpenFile *);

static struct ProcfsInfo g_defaultinfos[] = {
    [PROCFS_ROOT_INO] = {PROCFS_ROOT_INO, S_IFDIR | 0555, 0, 0,
                         PROCFS_ROOT_TYPE, "", .readdir = ProcfsRootReaddir},
    [PROCFS_FILESYSTEMS_INO] = {PROCFS_FILESYSTEMS_INO, S_IFREG | 0444, 0, 0,
                                PROCFS_FILESYSTEMS_TYPE, "filesystems",
                                .read = ProcfsFilesystemsRead},
    [PROCFS_MEMINFO_INO] = {PROCFS_MEMINFO_INO, S_IFREG | 0444, 0, 0,
                            PROCFS_MEMINFO_TYPE, "meminfo",
                            .read = ProcfsMeminfoRead},
    [PROCFS_MOUNTS_INO] = {PROCFS_MOUNTS_INO, S_IFLNK | 0777, 0, 0,
                           PROCFS_MOUNTS_TYPE, "mounts",
                           .readlink = ProcfsToSelfReadlink},
    [PROCFS_SELF_INO] = {PROCFS_SELF_INO, S_IFLNK | 0777, 0, 0,
                         PROCFS_SELF_TYPE, "self",
                         .readlink = ProcfsSelfReadlink},
    [PROCFS_SYS_INO] = {PROCFS_SYS_INO, S_IFDIR | 0555, 0, 0, PROCFS_SYS_TYPE,
                        "sys"},
    [PROCFS_UPTIME_INO] = {PROCFS_UPTIME_INO, S_IFREG | 0444, 0, 0,
                           PROCFS_UPTIME_TYPE, "uptime",
                           .read = ProcfsUptimeRead},
};

static struct ProcfsInfo g_piddirinfos[] = {
    [PROCFS_PIDDIR_TYPE -
     PROCFS_PIDDIR_TYPE] = {0, S_IFDIR | 0555, 0, 0, PROCFS_PIDDIR_TYPE, "",
                            .readdir = ProcfsPiddirReaddir},
    [PROCFS_PIDDIR_EXE_TYPE -
        PROCFS_PIDDIR_TYPE] = {0, S_IFLNK | 0777, 0, 0, PROCFS_PIDDIR_EXE_TYPE,
                               "exe", .readlink = ProcfsPiddirExeReadlink},
    [PROCFS_PIDDIR_CWD_TYPE -
        PROCFS_PIDDIR_TYPE] = {0, S_IFLNK | 0777, 0, 0, PROCFS_PIDDIR_CWD_TYPE,
                               "cwd", .readlink = ProcfsPiddirCwdReadlink},
    [PROCFS_PIDDIR_ROOT_TYPE -
        PROCFS_PIDDIR_TYPE] = {0, S_IFLNK | 0777, 0, 0, PROCFS_PIDDIR_ROOT_TYPE,
                               "root", .readlink = ProcfsPiddirRootReadlink},
    [PROCFS_PIDDIR_MOUNTS_TYPE -
        PROCFS_PIDDIR_TYPE] = {0, S_IFREG | 0444, 0, 0,
                               PROCFS_PIDDIR_MOUNTS_TYPE, "mounts",
                               .read = ProcfsPiddirMountsRead},
    [PROCFS_PIDDIR_FDDIR_TYPE - PROCFS_PIDDIR_TYPE] = {0, S_IFDIR | 0555, 0, 0,
                                                       PROCFS_PIDDIR_FDDIR_TYPE,
                                                       "fd"},
};

////////////////////////////////////////////////////////////////////////////////

static int ProcfsCreateInfo(struct ProcfsInfo **info) {
  *info = malloc(sizeof(struct ProcfsInfo));
  if (*info == NULL) {
    return enomem();
  }
  (*info)->ino = 0;
  (*info)->mode = 0;
  (*info)->type = 0;
  (*info)->name[0] = '\0';
  (*info)->openfile = NULL;
  return 0;
}

static int ProcfsCreateDefaultInfo(struct ProcfsInfo **info,
                                   struct ProcfsDevice *device, u64 ino) {
  *info = malloc(sizeof(struct ProcfsInfo));
  if (*info == NULL) {
    return enomem();
  }
  **info = g_defaultinfos[ino];
  (*info)->time = device->mounttime;
  return 0;
}

static int ProcfsCreatePiddirInfo(struct ProcfsInfo **info,
                                  struct ProcfsInfo *parent, i32 pid,
                                  u32 type) {
  *info = malloc(sizeof(struct ProcfsInfo));
  if (*info == NULL) {
    return enomem();
  }
  if (parent == NULL) {
    **info = g_piddirinfos[0];
    // TODO(trungnt): This should be dynamically allocated when we support
    // more processes.
    (*info)->ino = PROCFS_FIRST_PID_INO;
    sprintf((*info)->name, "%d", pid);
    // TODO(trungnt): On Linux, the UID and GID of these files
    // are set to the ones of the user who created the process.
    (*info)->uid = getuid();
    (*info)->gid = getgid();
    (*info)->time = GetTime();
  } else {
    **info = g_piddirinfos[type - PROCFS_PIDDIR_TYPE];
    (*info)->ino = parent->ino + (type - PROCFS_PIDDIR_TYPE);
    (*info)->uid = parent->uid;
    (*info)->gid = parent->gid;
    (*info)->time = GetTime();
  }
  return 0;
}

static int ProcfsFreeInfo(void *info) {
  if (info == NULL) {
    return 0;
  }
  struct ProcfsInfo *procinfo = (struct ProcfsInfo *)info;
  if (S_ISDIR(procinfo->mode)) {
    free(procinfo->opendir);
  } else if (S_ISREG(procinfo->mode)) {
    free(procinfo->openfile);
  }
  free(info);
  return 0;
}

static int ProcfsFreeDevice(void *device) {
  free(device);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

static int ProcfsInit(const char *source, u64 flags, const void *data,
                      struct VfsDevice **device, struct VfsMount **mount) {
  struct ProcfsDevice *procdevice = NULL;
  struct ProcfsInfo *procinfo = NULL;
  procdevice = malloc(sizeof(struct ProcfsDevice));
  if (procdevice == NULL) {
    return enomem();
  }
  procdevice->mounttime = GetTime();
  *device = NULL;
  if (VfsCreateDevice(device) == -1) {
    goto cleananddie;
  }
  (*device)->data = procdevice;
  (*device)->ops = &g_procfs.ops;
  *mount = malloc(sizeof(struct VfsMount));
  if (*mount == NULL) {
    goto cleananddie;
  }
  if (ProcfsCreateDefaultInfo(&procinfo, procdevice, PROCFS_ROOT_INO) == -1) {
    goto cleananddie;
  }
  if (VfsCreateInfo(&(*mount)->root) == -1) {
    goto cleananddie;
  }
  unassert(!VfsAcquireDevice(*device, &(*mount)->root->device));
  (*mount)->root->data = procinfo;
  (*mount)->root->mode = S_IFDIR;
  (*mount)->root->ino = 1;
  // Weak reference.
  (*device)->root = (*mount)->root;
  VFS_LOGF("Mounted a procfs device");
  return 0;
cleananddie:
  if (*device) {
    unassert(!VfsFreeDevice(*device));
  } else {
    free(procdevice);
  }
  if (*mount) {
    if ((*mount)->root) {
      unassert(!VfsFreeInfo((*mount)->root));
    } else {
      unassert(!ProcfsFreeInfo(procinfo));
    }
    free(*mount);
  }
  return -1;
}

////////////////////////////////////////////////////////////////////////////////

static int ProcfsReadmountentry(struct VfsDevice *device, char **spec,
                                char **type, char **mntops) {
  *spec = strdup("proc");
  if (*spec == NULL) {
    return enomem();
  }
  *type = strdup("proc");
  if (*type == NULL) {
    free(*spec);
    return enomem();
  }
  *mntops = NULL;
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

static int ProcfsFinddir(struct VfsInfo *parent, const char *name,
                         struct VfsInfo **output) {
  struct ProcfsInfo *procparent = (struct ProcfsInfo *)parent->data;
  struct ProcfsInfo *procoutput = NULL;
  int i, pid;
  VFS_LOGF("ProcfsFinddir(parent=%p (%s), name=\"%s\", output=%p)", parent,
           parent->name, name, output);
  if (strcmp(name, ".") == 0) {
    unassert(!VfsAcquireInfo(parent, output));
    return 0;
  }
  *output = NULL;
  switch (procparent->type) {
    case PROCFS_ROOT_TYPE:
      for (i = PROCFS_ROOT_INO + 1; i < PROCFS_FIRST_PID_INO; ++i) {
        if (!strcmp(name, g_defaultinfos[i].name)) {
          if (ProcfsCreateDefaultInfo(
                  &procoutput, (struct ProcfsDevice *)parent->device->data,
                  i) == -1) {
            goto cleananddie;
          }
          break;
        }
      }
      if (procoutput == NULL) {
        pid = -1;
        sscanf(name, "%d", &pid);
        if (pid == getpid()) {
          if (ProcfsCreatePiddirInfo(&procoutput, NULL, pid,
                                     PROCFS_PIDDIR_ROOT_TYPE) == -1) {
            goto cleananddie;
          }
        }
      }
      // TODO(trungnt): PID-specific directories of other processes.
      break;
    case PROCFS_PIDDIR_TYPE:
      for (i = 1; i <= PROCFS_PIDDIR_LAST_TYPE - PROCFS_PIDDIR_TYPE; ++i) {
        if (!strcmp(name, g_piddirinfos[i].name)) {
          if (ProcfsCreatePiddirInfo(&procoutput, procparent, -1,
                                     i + PROCFS_PIDDIR_TYPE) == -1) {
            goto cleananddie;
          }
          break;
        }
      }
      break;
  }
  if (procoutput == NULL) {
    enoent();
    goto cleananddie;
  }
  if (VfsCreateInfo(output) == -1) {
    goto cleananddie;
  }
  (*output)->data = procoutput;
  (*output)->ino = procoutput->ino;
  (*output)->mode = procoutput->mode;
  unassert(!VfsAcquireDevice(parent->device, &(*output)->device));
  unassert(!VfsAcquireInfo(parent, &(*output)->parent));
  (*output)->name = strdup(procoutput->name);
  if ((*output)->name == NULL) {
    goto cleananddie;
  }
  (*output)->namelen = strlen((*output)->name);
  return 0;
cleananddie:
  if (*output) {
    unassert(!VfsFreeInfo(*output));
  } else {
    unassert(!ProcfsFreeInfo(procoutput));
  }
  return -1;
}

////////////////////////////////////////////////////////////////////////////////

static ssize_t ProcfsReadlink(struct VfsInfo *info, char **output) {
  struct ProcfsInfo *procinfo = (struct ProcfsInfo *)info->data;
  VFS_LOGF("ProcfsReadlink(info=%p (%s), output=%p)", info, info->name, output);
  if (!S_ISLNK(procinfo->mode)) {
    return einval();
  }
  if (!procinfo->readlink) {
    return eperm();
  }
  return procinfo->readlink(info, output);
}

////////////////////////////////////////////////////////////////////////////////

static int ProcfsAccessImpl(struct ProcfsInfo *procinfo, mode_t mode) {
  uid_t uid;
  gid_t gid;
  int ret = 0;
  if (procinfo->ino < PROCFS_FIRST_PID_INO) {
    // Get fresher info as some files might have been chmod/chown'ed.
    procinfo = &g_defaultinfos[procinfo->ino];
  }
  if (mode != F_OK) {
    uid = getuid();
    // This implementation is incomplete as a user can be in multiple groups.
    gid = getgid();
    if (mode & R_OK) {
      if (uid == 0) {
        if (!(procinfo->mode & (S_IRUSR | S_IRGRP | S_IROTH))) {
          ret = eacces();
        }
      } else if (uid == procinfo->uid) {
        if (!(procinfo->mode & S_IRUSR)) {
          ret = eacces();
        }
      } else if (gid == procinfo->gid) {
        if (!(procinfo->mode & S_IRGRP)) {
          ret = eacces();
        }
      } else if (!(procinfo->mode & S_IROTH)) {
        ret = eacces();
      }
    }
    if (mode & W_OK) {
      if (uid == 0) {
        if (!(procinfo->mode & (S_IWUSR | S_IWGRP | S_IWOTH))) {
          ret = eacces();
        }
      } else if (uid == procinfo->uid) {
        if (!(procinfo->mode & S_IWUSR)) {
          ret = eacces();
        }
      } else if (gid == procinfo->gid) {
        if (!(procinfo->mode & S_IWGRP)) {
          ret = eacces();
        }
      } else if (!(procinfo->mode & S_IWOTH)) {
        ret = eacces();
      }
    }
    if (mode & X_OK) {
      if (uid == 0) {
        if (!(procinfo->mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
          ret = eacces();
        }
      } else if (uid == procinfo->uid) {
        if (!(procinfo->mode & S_IXUSR)) {
          ret = eacces();
        }
      } else if (gid == procinfo->gid) {
        if (!(procinfo->mode & S_IXGRP)) {
          ret = eacces();
        }
      } else if (!(procinfo->mode & S_IXOTH)) {
        ret = eacces();
      }
    }
  }
  return ret;
}

static int ProcfsAccess(struct VfsInfo *parent, const char *name, mode_t mode,
                        int flags) {
  struct VfsInfo *info = NULL;
  struct ProcfsInfo *procinfo;
  int ret = 0;
  if (ProcfsFinddir(parent, name, &info) == -1) {
    return -1;
  }
  procinfo = (struct ProcfsInfo *)info->data;
  ret = ProcfsAccessImpl(procinfo, mode);
  unassert(!VfsFreeInfo(info));
  return ret;
}

////////////////////////////////////////////////////////////////////////////////

static int ProcfsOpen(struct VfsInfo *parent, const char *name, int flags,
                      int mode, struct VfsInfo **output) {
  struct ProcfsInfo *procoutput = NULL;
  if (ProcfsFinddir(parent, name, output) == -1) {
    return -1;
  }
  procoutput = (struct ProcfsInfo *)(*output)->data;
  if (S_ISDIR(procoutput->mode)) {
    procoutput->opendir = malloc(sizeof(struct ProcfsOpenDir));
    if (procoutput->opendir == NULL) {
      enomem();
      goto cleananddie;
    }
    procoutput->opendir->index = 0;
    unassert(!pthread_mutex_init(&procoutput->opendir->lock, NULL));
  } else if (S_ISREG(procoutput->mode)) {
    if (flags & O_RDWR || flags & O_RDONLY) {
      if (ProcfsAccessImpl(procoutput, R_OK) == -1) {
        goto cleananddie;
      }
    }
    if (flags & O_RDWR || flags & O_WRONLY) {
      if (ProcfsAccessImpl(procoutput, W_OK) == -1) {
        goto cleananddie;
      }
    }
    procoutput->openfile = malloc(sizeof(struct ProcfsOpenFile));
    if (procoutput->openfile == NULL) {
      enomem();
      goto cleananddie;
    }
    procoutput->openfile->offset = 0;
    procoutput->openfile->index = 0;
    procoutput->openfile->openflags = flags;
    procoutput->openfile->readbufstart = procoutput->openfile->readbufend = 0;
    unassert(!pthread_mutex_init(&procoutput->openfile->lock, NULL));
  } else {
    einval();
    goto cleananddie;
  }
  return 0;
cleananddie:
  if (procoutput) {
    if (S_ISDIR(procoutput->mode)) {
      free(procoutput->opendir);
      procoutput->opendir = NULL;
    } else if (S_ISREG(procoutput->mode)) {
      free(procoutput->openfile);
      procoutput->openfile = NULL;
    }
  }
  unassert(!VfsFreeInfo(*output));
  return -1;
}

static int ProcfsClose(struct VfsInfo *info) {
  struct ProcfsInfo *procinfo = (struct ProcfsInfo *)info->data;
  void *opendata;
  if (S_ISDIR(procinfo->mode)) {
    opendata = atomic_exchange(&procinfo->opendir, NULL);
  } else if (S_ISREG(procinfo->mode)) {
    opendata = atomic_exchange(&procinfo->openfile, NULL);
  }
  LOCK((pthread_mutex_t_ *)opendata);
  UNLOCK((pthread_mutex_t_ *)opendata);
  unassert(!pthread_mutex_destroy((pthread_mutex_t_ *)opendata));
  free(opendata);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

static int ProcfsStatImpl(struct VfsDevice *device, struct ProcfsInfo *info,
                          struct stat *st) {
  st->st_dev = device->dev;
  st->st_ino = info->ino;
  st->st_mode = info->mode;
  st->st_nlink = 1;
  if (info->ino < PROCFS_FIRST_PID_INO) {
    st->st_uid = g_defaultinfos[info->type].uid;
    st->st_gid = g_defaultinfos[info->type].gid;
  } else {
    st->st_uid = info->uid;
    st->st_gid = info->gid;
  }
  st->st_uid = info->uid;
  st->st_gid = info->gid;
  st->st_rdev = 0;
  st->st_size = 0;
  st->st_blksize = PROCFS_READ_LEN;
  st->st_blocks = 0;
  st->st_atim = st->st_mtim = st->st_ctim = info->time;
  return 0;
}

static int ProcfsStat(struct VfsInfo *parent, const char *name, struct stat *st,
                      int flags) {
  struct VfsInfo *info;
  int ret;
  if (ProcfsFinddir(parent, name, &info) == -1) {
    return -1;
  }
  ret = ProcfsStatImpl(parent->device, (struct ProcfsInfo *)info->data, st);
  unassert(!VfsFreeInfo(info));
  return ret;
}

static int ProcfsFstat(struct VfsInfo *parent, struct stat *st) {
  return ProcfsStatImpl(parent->device, (struct ProcfsInfo *)parent->data, st);
}

////////////////////////////////////////////////////////////////////////////////

// On Linux, it is actually possible to chmod/chown some files in procfs.
// It is NOT possible to do so for files in pid-specific directories.
// We try to emulate it here, but this emulation suffers from the same
// limitations as the mount tables at the time of writing: It does not
// propagate to sibling and parent processes.

static int ProcfsFchmod(struct VfsInfo *info, mode_t mode) {
  struct ProcfsInfo *procinfo = (struct ProcfsInfo *)info->data;
  if (procinfo->ino >= PROCFS_FIRST_PID_INO) {
    return eperm();
  } else {
    procinfo->mode = (procinfo->mode & ~07777) | (mode & 07777);
    g_defaultinfos[procinfo->type].mode = procinfo->mode;
    return 0;
  }
}

static int ProcfsChmod(struct VfsInfo *parent, const char *name, mode_t mode,
                       int flags) {
  struct VfsInfo *info;
  int ret;
  // When we reach here all necessary symlink dereferences have already been
  // conducted.
  if (ProcfsFinddir(parent, name, &info) == -1) {
    return -1;
  }
  ret = ProcfsFchmod(info, mode);
  unassert(!VfsFreeInfo(info));
  return ret;
}

static int ProcfsFchown(struct VfsInfo *info, uid_t uid, gid_t gid) {
  struct ProcfsInfo *procinfo = (struct ProcfsInfo *)info->data;
  if (procinfo->ino >= PROCFS_FIRST_PID_INO) {
    return eperm();
  } else {
    if (uid != -1) {
      procinfo->uid = uid;
      g_defaultinfos[procinfo->type].uid = uid;
    }
    if (gid != -1) {
      procinfo->gid = gid;
      g_defaultinfos[procinfo->type].gid = gid;
    }
    return 0;
  }
}

static int ProcfsChown(struct VfsInfo *parent, const char *name, uid_t uid,
                       gid_t gid, int flags) {
  struct VfsInfo *info;
  int ret;
  if (ProcfsFinddir(parent, name, &info) == -1) {
    return -1;
  }
  ret = ProcfsFchown(info, uid, gid);
  unassert(!VfsFreeInfo(info));
  return ret;
}

////////////////////////////////////////////////////////////////////////////////

static ssize_t ProcfsReadImpl(struct VfsInfo *info, void *buf, size_t len,
                              off_t off, bool copy) {
  struct ProcfsInfo *procinfo = (struct ProcfsInfo *)info->data;
  struct ProcfsOpenFile tmpopenfile = *(procinfo->openfile);
  ssize_t ret = 0;
  size_t bytestoread = 0;
  if (off != -1) {
    tmpopenfile.readbufstart = tmpopenfile.readbufend = 0;
    tmpopenfile.index = 0;
    tmpopenfile.offset = 0;
  }
  while ((off > 0 || len > 0) &&
         tmpopenfile.readbufend <= sizeof(tmpopenfile.readbuf)) {
    // All bytes from the buffer has been consumed.
    if (tmpopenfile.readbufstart == tmpopenfile.readbufend) {
      if (!procinfo->read) {
        ret = eperm();
        break;
      }
      if (procinfo->read(info, &tmpopenfile) == -1) {
        ret = -1;
        break;
      }
    }
    if (off > 0) {
      bytestoread = MIN(off, tmpopenfile.readbufend - tmpopenfile.readbufstart);
      tmpopenfile.readbufstart += bytestoread;
      tmpopenfile.offset += bytestoread;
      off -= bytestoread;
    } else {
      bytestoread = MIN(len, tmpopenfile.readbufend - tmpopenfile.readbufstart);
      memcpy(buf, tmpopenfile.readbuf + tmpopenfile.readbufstart, bytestoread);
      tmpopenfile.readbufstart += bytestoread;
      tmpopenfile.offset += bytestoread;
      buf = (void *)((char *)buf + bytestoread);
      len -= bytestoread;
      ret += bytestoread;
    }
  }
  if (copy || (off == -1 && ret != -1)) {
    *(procinfo->openfile) = tmpopenfile;
  }
  return ret;
}

static ssize_t ProcfsPreadv(struct VfsInfo *info, const struct iovec *iov,
                            int iovcnt, off_t off) {
  struct ProcfsInfo *procinfo = (struct ProcfsInfo *)info->data;
  size_t len = 0;
  ssize_t ret;
  if (!S_ISREG(procinfo->mode) || procinfo->openfile == NULL) {
    return einval();
  }
  if (off < 0) return einval();
  LOCK(&procinfo->openfile->lock);
  for (int i = 0; i < iovcnt; ++i) {
    len += iov[i].iov_len;
    if (len > SSIZE_MAX) return einval();
  }
  len = 0;
  for (int i = 0; i < iovcnt; ++i) {
    ret = ProcfsReadImpl(info, iov[i].iov_base, iov[i].iov_len, off, false);
    if (ret == -1) {
      if (len == 0) len = -1;
      break;
    }
    len += ret;
    off += ret;
    procinfo->openfile->offset += ret;
    if (ret < iov[i].iov_len) {
      break;
    }
  }
  UNLOCK(&procinfo->openfile->lock);
  return len;
}

static ssize_t ProcfsReadv(struct VfsInfo *info, const struct iovec *iov,
                           int iovcnt) {
  struct ProcfsInfo *procinfo = (struct ProcfsInfo *)info->data;
  size_t len = 0;
  ssize_t ret;
  if (!S_ISREG(procinfo->mode) || procinfo->openfile == NULL) {
    return einval();
  }
  LOCK(&procinfo->openfile->lock);
  for (int i = 0; i < iovcnt; ++i) {
    len += iov[i].iov_len;
    if (len > SSIZE_MAX) return einval();
  }
  len = 0;
  for (int i = 0; i < iovcnt; ++i) {
    ret = ProcfsReadImpl(info, iov[i].iov_base, iov[i].iov_len, -1, false);
    if (ret == -1) {
      if (len == 0) len = -1;
      break;
    }
    len += ret;
    if (ret < iov[i].iov_len) {
      break;
    }
  }
  UNLOCK(&procinfo->openfile->lock);
  return len;
}

static ssize_t ProcfsPread(struct VfsInfo *info, void *buf, size_t len,
                           off_t off) {
  struct ProcfsInfo *procinfo = (struct ProcfsInfo *)info->data;
  ssize_t ret;
  if (!S_ISREG(procinfo->mode) || procinfo->openfile == NULL) {
    return einval();
  }
  if (off < 0) return einval();
  LOCK(&procinfo->openfile->lock);
  ret = ProcfsReadImpl(info, buf, len, off, false);
  UNLOCK(&procinfo->openfile->lock);
  return ret;
}

static ssize_t ProcfsRead(struct VfsInfo *info, void *buf, size_t len) {
  struct ProcfsInfo *procinfo = (struct ProcfsInfo *)info->data;
  ssize_t ret;
  if (!S_ISREG(procinfo->mode) || procinfo->openfile == NULL) {
    return einval();
  }
  LOCK(&procinfo->openfile->lock);
  ret = ProcfsReadImpl(info, buf, len, -1, false);
  UNLOCK(&procinfo->openfile->lock);
  return ret;
}

static ssize_t ProcfsWrite(struct VfsInfo *, const void *, size_t);
static ssize_t ProcfsPwrite(struct VfsInfo *, const void *, size_t, off_t);
static ssize_t ProcfsWritev(struct VfsInfo *, const struct iovec *, int);
static ssize_t ProcfsPwritev(struct VfsInfo *, const struct iovec *, int,
                             off_t);

////////////////////////////////////////////////////////////////////////////////

static off_t ProcfsSeekImpl(struct VfsInfo *info, off_t off, int whence) {
  struct ProcfsInfo *procinfo = (struct ProcfsInfo *)info->data;
  off_t newoff;
  if (S_ISDIR(procinfo->mode)) {
    if (procinfo->opendir == NULL) {
      return einval();
    }
    switch (whence) {
      case SEEK_SET:
        if (off < 0) return einval();
        procinfo->opendir->index = off;
        break;
      case SEEK_CUR:
        if (off < 0 && -off > procinfo->opendir->index) return einval();
        procinfo->opendir->index += off;
        break;
      case SEEK_END:
        return einval();
    }
    return procinfo->opendir->index;
  } else if (S_ISREG(procinfo->mode)) {
    if (procinfo->openfile == NULL) {
      return einval();
    }
    switch (whence) {
      case SEEK_SET:
        newoff = off;
        break;
      case SEEK_CUR:
        newoff = procinfo->openfile->offset + off;
        break;
      case SEEK_END:
        return einval();
    }
    if (newoff < 0) {
      return einval();
    }
    if (procinfo->openfile->readbufend <= sizeof(procinfo->openfile->readbuf) &&
        procinfo->openfile->offset / PROCFS_READ_LEN ==
            newoff / PROCFS_READ_LEN) {
      // We're still in the cached region.
      procinfo->openfile->readbufstart += newoff - procinfo->openfile->offset;
      procinfo->openfile->offset = newoff;
    } else {
      if ((procinfo->openfile->offset / PROCFS_READ_LEN >
           newoff / PROCFS_READ_LEN) ||
          (procinfo->openfile->readbufend >
           sizeof(procinfo->openfile->readbuf))) {
        // Reset the file if it has already ended or we're going back before the
        // buffer.
        procinfo->openfile->index = 0;
        procinfo->openfile->offset = 0;
        procinfo->openfile->readbufstart = procinfo->openfile->readbufend = 0;
      }
      ProcfsReadImpl(info, NULL, 0, newoff - procinfo->openfile->offset, true);
      procinfo->openfile->offset = newoff;
    }
    return procinfo->openfile->offset;
  } else {
    return einval();
  }
  return einval();
}

static off_t ProcfsSeek(struct VfsInfo *info, off_t off, int whence) {
  off_t ret;
  struct ProcfsInfo *procinfo = (struct ProcfsInfo *)info->data;
  LOCK(&procinfo->openfile->lock);
  ret = ProcfsSeekImpl(info, off, whence);
  UNLOCK(&procinfo->openfile->lock);
  return ret;
}

////////////////////////////////////////////////////////////////////////////////

static int ProcfsDup(struct VfsInfo *info, struct VfsInfo **newinfo) {
  struct ProcfsInfo *procinfo = (struct ProcfsInfo *)info->data, *newprocinfo;
  u32 openflags = 0;
  *newinfo = NULL;
  if (S_ISREG(info->mode)) {
    openflags = procinfo->openfile->openflags & ~(O_CREAT | O_EXCL | O_TRUNC);
  } else if (S_ISDIR(info->mode)) {
    openflags = O_DIRECTORY;
  } else {
    return einval();
  }
  if (ProcfsOpen(info->parent, info->name, openflags, 0, newinfo) == -1) {
    return -1;
  }
  newprocinfo = (struct ProcfsInfo *)(*newinfo)->data;
  if (S_ISDIR(info->mode)) {
    memcpy(newprocinfo->opendir, procinfo->opendir,
           sizeof(*newprocinfo->opendir));
  } else if (S_ISREG(info->mode)) {
    memcpy(newprocinfo->openfile, procinfo->openfile,
           sizeof(*newprocinfo->openfile));
  }
  unassert(!pthread_mutex_init((pthread_mutex_t *)newprocinfo->openfile, NULL));
  return 0;
}

#ifdef HAVE_DUP3
static int ProcfsDup3(struct VfsInfo *info, struct VfsInfo **newinfo, int) {
  // O_CLOEXEC is already handled by the syscall layer.
  return ProcfsDup(info, newinfo);
}
#endif

////////////////////////////////////////////////////////////////////////////////

static int ProcfsOpendir(struct VfsInfo *info, struct VfsInfo **output) {
  struct ProcfsInfo *procinfo = (struct ProcfsInfo *)info->data;
  if (!S_ISDIR(procinfo->mode) && !procinfo->opendir) {
    return einval();
  }
  unassert(!VfsAcquireInfo(info, output));
  return 0;
}

#ifdef HAVE_SEEKDIR
static void ProcfsSeekdir(struct VfsInfo *info, long offset) {
  struct ProcfsInfo *procinfo = (struct ProcfsInfo *)info->data;
  if (!S_ISDIR(procinfo->mode) && !procinfo->opendir) {
    einval();
    return;
  }
  procinfo->opendir->index = offset;
}

static long ProcfsTelldir(struct VfsInfo *info) {
  struct ProcfsInfo *procinfo = (struct ProcfsInfo *)info->data;
  if (!S_ISDIR(procinfo->mode) && !procinfo->opendir) {
    return einval();
  }
  return procinfo->opendir->index;
}
#endif

static struct dirent *ProcfsReaddir(struct VfsInfo *info) {
  static _Thread_local char buf[sizeof(struct dirent) + VFS_NAME_MAX];
  struct dirent *de = (struct dirent *)buf;
  struct ProcfsInfo *procinfo = (struct ProcfsInfo *)info->data;
  if (!S_ISDIR(procinfo->mode) && !procinfo->opendir) {
    einval();
    return NULL;
  }
  if (procinfo->readdir) {
    if (procinfo->readdir(info, de) == -1) {
      return NULL;
    }
    return de;
  }
  return NULL;
}

static void ProcfsRewinddir(struct VfsInfo *info) {
  return ProcfsSeekdir(info, 0);
}

static int ProcfsClosedir(struct VfsInfo *info) {
  unassert(!VfsFreeInfo(info));
  return ProcfsClose(info);
}

////////////////////////////////////////////////////////////////////////////////

static int ProcfsUtime(struct VfsInfo *, const char *, const struct timespec[2],
                       int) {
  return einval();
}

static int ProcfsFutime(struct VfsInfo *, const struct timespec[2]) {
  return einval();
}

////////////////////////////////////////////////////////////////////////////////

struct VfsInfo *g_selfexeinfo = NULL;

int ProcfsRegisterExe(i32 pid, const char *path) {
  struct VfsInfo *newinfo, *tmp;
  unassert(pid == getpid());
  unassert(!VfsTraverse(path, &newinfo, true));
  tmp = g_selfexeinfo;
  g_selfexeinfo = newinfo;
  unassert(!VfsFreeInfo(tmp));
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

static int ProcfsInfoToDirent(struct ProcfsInfo *info, struct dirent *de) {
  de->d_ino = info->ino;
#ifdef DT_UNKNOWN
  if (S_ISDIR(info->mode)) {
    de->d_type = DT_DIR;
  } else if (S_ISREG(info->mode)) {
    de->d_type = DT_REG;
  } else if (S_ISLNK(info->mode)) {
    de->d_type = DT_LNK;
  } else {
    de->d_type = DT_UNKNOWN;
  }
#endif
  strcpy(de->d_name, info->name);
  return 0;
}

static int ProcfsRootReaddir(struct VfsInfo *info, struct dirent *de) {
  struct ProcfsInfo *procinfo = (struct ProcfsInfo *)info->data;
  struct ProcfsOpenDir *dir = procinfo->opendir;
  int ret = 0;
  LOCK(&dir->lock);
  // The index value reserves two numbers for .. and .
  // The PROCFS_*_INO on the other hand uses the first two numbers
  // for the NULL node and the ROOT node.
  if (dir->index == 0) {
    de->d_ino = info->parent->ino;
#ifdef DT_DIR
    de->d_type = DT_DIR;
#endif
    strcpy(de->d_name, "..");
    ret = 0;
  } else if (dir->index == 1) {
    de->d_ino = info->ino;
#ifdef DT_DIR
    de->d_type = DT_DIR;
#endif
    strcpy(de->d_name, ".");
    ret = 0;
  } else if (dir->index >= PROCFS_FIRST_PID_INO) {
    if (dir->index == PROCFS_FIRST_PID_INO) {
      // In the limited version, there's only one PID visible here.
      // For the complete implementation, we have to query the PID table.
      de->d_ino = PROCFS_FIRST_PID_INO;
#ifdef DT_DIR
      de->d_type = DT_DIR;
#endif
      sprintf(de->d_name, "%d", getpid());
    } else {
      // TODO(trungnt): PID-specific directories for other processes
      ret = enoent();
    }
  } else {
    if (ProcfsInfoToDirent(&g_defaultinfos[dir->index], de) == -1) {
      ret = -1;
    }
    ret = 0;
  }
  ++dir->index;
  UNLOCK(&dir->lock);
  return ret;
}

static int ProcfsPiddirReaddir(struct VfsInfo *info, struct dirent *de) {
  struct ProcfsInfo *procinfo = (struct ProcfsInfo *)info->data;
  struct ProcfsOpenDir *dir = procinfo->opendir;
  int ret = 0;
  LOCK(&dir->lock);
  if (dir->index == 0) {
    de->d_ino = info->parent->ino;
#ifdef DT_DIR
    de->d_type = DT_DIR;
#endif
    strcpy(de->d_name, "..");
    ret = 0;
  } else if (dir->index == 1) {
    de->d_ino = info->ino;
#ifdef DT_DIR
    de->d_type = DT_DIR;
#endif
    strcpy(de->d_name, ".");
    ret = 0;
  } else if ((dir->index - 1) >
             (PROCFS_PIDDIR_LAST_TYPE - PROCFS_PIDDIR_TYPE)) {
    ret = enoent();
  } else {
    if (ProcfsInfoToDirent(&g_piddirinfos[dir->index - 1], de) == -1) {
      ret = -1;
    }
    ret = 0;
  }
  ++dir->index;
  UNLOCK(&dir->lock);
  return ret;
}

////////////////////////////////////////////////////////////////////////////////

static ssize_t ProcfsSelfReadlink(struct VfsInfo *info, char **buf) {
  *buf = malloc(16);
  if (!*buf) {
    return enomem();
  }
  snprintf(*buf, 16, "%d", getpid());
  return strlen(*buf);
}

static ssize_t ProcfsToSelfReadlink(struct VfsInfo *info, char **buf) {
  size_t len = strlen(info->name) + sizeof("self/");
  *buf = malloc(len);
  if (!*buf) {
    return enomem();
  }
  strcpy(*buf, "self/");
  strcpy(*buf + sizeof("self/") - 1, info->name);
  return len - 1;
}

static ssize_t ProcfsPiddirExeReadlink(struct VfsInfo *info, char **buf) {
  ssize_t len;
  char *tmp;
  struct stat st;
  VFS_LOGF("ProcfsPiddirExeReadlink(%p (%s), %p)", info, info->name, buf);
  if ((len = VfsPathBuildFull(g_selfexeinfo, NULL, buf)) == -1) {
    return -1;
  }
  if (VfsStat(AT_FDCWD, *buf, &st, 0) == -1) {
    len += sizeof(PROCFS_DELETED) - 1;
    tmp = realloc(*buf, len + 1);
    if (!tmp) {
      free(*buf);
      return enomem();
    }
    *buf = tmp;
    memcpy(*buf + len, PROCFS_DELETED, sizeof(PROCFS_DELETED));
  }
  return len;
}

static ssize_t ProcfsPiddirCwdReadlink(struct VfsInfo *info, char **buf) {
  return VfsPathBuildFull(g_cwdinfo, NULL, buf);
}

static ssize_t ProcfsPiddirRootReadlink(struct VfsInfo *info, char **buf) {
  return VfsPathBuildFull(g_rootinfo, g_actualrootinfo, buf);
}

////////////////////////////////////////////////////////////////////////////////

static int ProcfsMeminfoRead(struct VfsInfo *info,
                             struct ProcfsOpenFile *openfile) {
#ifdef __linux__
  int fd = open("/proc/meminfo", O_RDONLY);
  ssize_t bytesread;
  int ret = 0;
  if (fd == -1) {
    return -1;
  }
  if ((bytesread = pread(fd, openfile->readbuf, sizeof(openfile->readbuf),
                         openfile->index)) == -1) {
    ret = -1;
  }
  close(fd);
  if (ret != -1) {
    if (bytesread != 0) {
      openfile->index += bytesread;
      openfile->readbufstart = 0;
      openfile->readbufend = bytesread;
    } else {
      openfile->readbufstart = sizeof(openfile->readbuf) + 1;
      openfile->readbufend = sizeof(openfile->readbuf) + 1;
    }
  }
  return ret;
#else
  u64 memtotal = 0, memfree = 0;
  if (openfile->index > 0) {
    openfile->readbufstart = sizeof(openfile->readbuf) + 1;
    openfile->readbufend = sizeof(openfile->readbuf) + 1;
    return 0;
  }
#if defined(__CYGWIN__)
  MEMORYSTATUSEX memstat;
  memstat.dwLength = sizeof(memstat);
  if (!GlobalMemoryStatusEx(&memstat)) {
    return -1;
  }
  memtotal = memstat.ullTotalPhys;
  memfree = memstat.ullAvailPhys;
#elif defined(__EMSCRIPTEN__)
  // WASM is 32-bit so on a RAM-efficient browser running on a
  // device with a lot of RAM, we can reach the 4GB limit.
  memtotal = 4 * 1024 * 1024;
  memfree = memtotal - EM_ASM_INT(return HEAP8.length);
#endif
  openfile->readbufstart = 0;
  openfile->readbufend = snprintf(openfile->readbuf, sizeof(openfile->readbuf),
                                  "MemTotal: %llu kB\nMemFree: %llu kB\n",
                                  (unsigned long long)(memtotal / 1024),
                                  (unsigned long long)(memfree / 1024));
  openfile->index = 1;
  return 0;
#endif
}

static int ProcfsUptimeRead(struct VfsInfo *info,
                            struct ProcfsOpenFile *openfile) {
  double uptime = 0, idletime = 0;
  int ret = 0;
  if (openfile->index > 0) {
    openfile->readbufstart = sizeof(openfile->readbuf) + 1;
    openfile->readbufend = sizeof(openfile->readbuf) + 1;
    return 0;
  }
#ifdef __linux__
  FILE *fp = fopen("/proc/uptime", "r");
  if (!fp) {
    return -1;
  }
  if (!fp) {
    return -1;
  }
  if (fscanf(fp, "%lf %lf", &uptime, &idletime) != 2) {
    ret = -1;
  }
  fclose(fp);
#elif defined(__CYGWIN__)
  SYSTEM_PERFORMANCE_INFORMATION spi;
  SYSTEM_TIMEOFDAY_INFORMATION sti;
  if (!NT_SUCCESS(NtQuerySystemInformation(SystemPerformanceInformation, &spi,
                                           sizeof(spi), NULL))) {
    ret = -1;
  } else if (!NT_SUCCESS(NtQuerySystemInformation(SystemTimeOfDayInformation,
                                                  &sti, sizeof(sti), NULL))) {
    ret = -1;
  } else {
    uptime = (sti.CurrentTime.QuadPart - sti.BootTime.QuadPart) / 10000000.0;
    idletime = spi.IdleTime.QuadPart / 10000000.0;
  }
#elif defined(__EMSCRIPTEN__)
  uptime = EM_ASM_DOUBLE(return performance.now() / 1000);
#endif
  if (ret != -1) {
    openfile->readbufstart = 0;
    openfile->readbufend =
        snprintf(openfile->readbuf, sizeof(openfile->readbuf), "%lf %lf\n",
                 uptime, idletime);
    openfile->index = 1;
  }
  return ret;
}

static int ProcfsFilesystemsRead(struct VfsInfo *info,
                                 struct ProcfsOpenFile *openfile) {
  size_t byteswritten = 0;
  size_t bytesleft = sizeof(openfile->readbuf);
  size_t ret;
  struct Dll *e = NULL;
  int i;
  if (openfile->readbufend > sizeof(openfile->readbuf)) {
    return 0;
  }
  LOCK(&g_vfs.lock);
  for (i = 0, e = dll_first(g_vfs.systems); e;
       e = dll_next(g_vfs.systems, e), ++i) {
    struct VfsSystem *system = VFS_SYSTEM_CONTAINER(e);
    if (i < openfile->index) continue;
    ret = snprintf(openfile->readbuf + byteswritten, bytesleft, "%-8s%s\n",
                   system->nodev ? "nodev" : "", system->name);
    if (ret > bytesleft) {
      break;
    }
    byteswritten += ret;
    bytesleft -= ret;
    ++openfile->index;
  }
  UNLOCK(&g_vfs.lock);
  if (e == NULL && byteswritten == 0) {
    openfile->readbufstart = sizeof(openfile->readbuf) + 1;
    openfile->readbufend = sizeof(openfile->readbuf) + 1;
  } else {
    openfile->readbufstart = 0;
    openfile->readbufend = byteswritten;
  }
  return 0;
}

static int ProcfsMountsStringEscape(char **str) {
  size_t len, len1;
  char *tmp;
  for (len = 0, len1 = 0; (*str)[len]; ++len) {
    len1 += ((*str)[len] == ' ') || ((*str)[len] == '\t');
  }
  if (len1) {
    len1 = len + len1 * 3;
    tmp = realloc((*str), len1 + 1);
    if (!tmp) {
      return enomem();
    }
    (*str) = tmp;
    tmp = NULL;
    (*str)[len1] = '\0';
    for (; len > 0; --len) {
      if ((*str)[len - 1] == ' ') {
        (*str)[len1 - 1] = '0';
        (*str)[len1 - 2] = '4';
        (*str)[len1 - 3] = '0';
        (*str)[len1 - 4] = '\\';
        len1 -= 4;
      } else if ((*str)[len - 1] == '\t') {
        (*str)[len1 - 1] = '1';
        (*str)[len1 - 2] = '1';
        (*str)[len1 - 3] = '0';
        (*str)[len1 - 4] = '\\';
        len1 -= 4;
      } else {
        (*str)[len1 - 1] = (*str)[len - 1];
        --len1;
      }
    }
  }
  return 0;
}

static int ProcfsPiddirMountsRead(struct VfsInfo *info,
                                  struct ProcfsOpenFile *openfile) {
  size_t byteswritten = 0;
  size_t bytesleft = sizeof(openfile->readbuf);
  size_t ret, len, len1;
  struct Dll *e = NULL;
  char *spec, *file, *vfstype, *mntops, *mntops1, *tmp;
  int freq = 0, passno = 0;
  int i;
  if (openfile->readbufend > sizeof(openfile->readbuf)) {
    return 0;
  }
  LOCK(&g_vfs.lock);
  for (i = 0, e = dll_first(g_vfs.devices); e;
       e = dll_next(g_vfs.devices, e), ++i) {
    struct VfsDevice *device = VFS_DEVICE_CONTAINER(e);
    if (i < openfile->index) continue;
    spec = file = vfstype = mntops = mntops1 = tmp = NULL;
    ret = 0;
    // Probably the root device.
    if (device->root == NULL) {
      goto cleanandcontinue;
    }
    if (VfsPathBuildFull(device->root, NULL, &file) == -1) {
      goto cleanandcontinue;
    }
    mntops = strdup("defaults");
    if (!mntops) {
      goto cleanandcontinue;
    }
    if (!device->ops->Readmountentry ||
        device->ops->Readmountentry(device, &spec, &vfstype, &mntops1) == -1) {
      goto cleanandcontinue;
    }
    if (mntops1) {
      len = strlen(mntops);
      len1 = strlen(mntops1);
      tmp = realloc(mntops, len + 1 + len1 + 1);
      if (!tmp) {
        goto cleanandcontinue;
      }
      mntops = tmp;
      tmp = NULL;
      mntops[len] = ',';
      memcpy(mntops + len + 1, mntops1, len1 + 1);
    }
    if (spec) {
      if (ProcfsMountsStringEscape(&spec) == -1) {
        goto cleanandcontinue;
      }
    }
    if (file) {
      if (ProcfsMountsStringEscape(&file) == -1) {
        goto cleanandcontinue;
      }
    }
    ret = snprintf(openfile->readbuf + byteswritten, bytesleft,
                   "%s %s %s %s %d %d\n", spec, file, vfstype, mntops, freq,
                   passno);
  cleanandcontinue:
    free(spec);
    free(file);
    free(vfstype);
    free(mntops);
    free(mntops1);
    free(tmp);
    if (ret > bytesleft) {
      break;
    }
    byteswritten += ret;
    bytesleft -= ret;
    ++openfile->index;
  }
  UNLOCK(&g_vfs.lock);
  if (e == NULL && byteswritten == 0) {
    openfile->readbufstart = sizeof(openfile->readbuf) + 1;
    openfile->readbufend = sizeof(openfile->readbuf) + 1;
  } else {
    openfile->readbufstart = 0;
    openfile->readbufend = byteswritten;
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

struct VfsSystem g_procfs = {.name = "proc",
                             .nodev = true,
                             .ops = {
                                 .Init = ProcfsInit,
                                 .Freeinfo = ProcfsFreeInfo,
                                 .Freedevice = ProcfsFreeDevice,
                                 .Readmountentry = ProcfsReadmountentry,
                                 .Finddir = ProcfsFinddir,
                                 .Readlink = ProcfsReadlink,
                                 .Mkdir = NULL,
                                 .Mkfifo = NULL,
                                 .Open = ProcfsOpen,
                                 .Access = ProcfsAccess,
                                 .Stat = ProcfsStat,
                                 .Fstat = ProcfsFstat,
                                 .Chmod = ProcfsChmod,
                                 .Fchmod = ProcfsFchmod,
                                 .Chown = ProcfsChown,
                                 .Fchown = ProcfsFchown,
                                 .Ftruncate = NULL,
                                 .Close = ProcfsClose,
                                 .Link = NULL,
                                 .Unlink = NULL,
                                 .Read = ProcfsRead,
                                 //.Write = ProcfsWrite,
                                 .Pread = ProcfsPread,
                                 //.Pwrite = ProcfsPwrite,
                                 .Readv = ProcfsReadv,
                                 //.Writev = ProcfsWritev,
                                 .Preadv = ProcfsPreadv,
                                 //.Pwritev = ProcfsPwritev,
                                 .Seek = ProcfsSeek,
                                 .Fsync = NULL,
                                 .Fdatasync = NULL,
                                 .Flock = NULL,
                                 .Dup = ProcfsDup,
#ifdef HAVE_DUP3
                                 .Dup3 = ProcfsDup3,
#endif
                                 .Poll = NULL,
                                 .Opendir = ProcfsOpendir,
#ifdef HAVE_SEEKDIR
                                 .Seekdir = ProcfsSeekdir,
                                 .Telldir = ProcfsTelldir,
#endif
                                 .Readdir = ProcfsReaddir,
                                 .Rewinddir = ProcfsRewinddir,
                                 .Closedir = ProcfsClosedir,
                                 .Bind = NULL,
                                 .Connect = NULL,
                                 .Connectunix = NULL,
                                 .Accept = NULL,
                                 .Listen = NULL,
                                 .Shutdown = NULL,
                                 .Recvmsg = NULL,
                                 .Sendmsg = NULL,
                                 .Recvmsgunix = NULL,
                                 .Sendmsgunix = NULL,
                                 .Getsockopt = NULL,
                                 .Setsockopt = NULL,
                                 .Getsockname = NULL,
                                 .Getpeername = NULL,
                                 .Rename = NULL,
                                 .Utime = ProcfsUtime,
                                 .Futime = ProcfsFutime,
                                 .Symlink = NULL,
                                 .Mmap = NULL,
                                 .Munmap = NULL,
                                 .Mprotect = NULL,
                                 .Msync = NULL,
                                 .Pipe = NULL,
#ifdef HAVE_PIPE2
                                 .Pipe2 = NULL,
#endif
                                 .Socket = NULL,
                                 .Socketpair = NULL,
                                 .Tcgetattr = NULL,
                                 .Tcsetattr = NULL,
                                 .Tcflush = NULL,
                                 .Tcdrain = NULL,
                                 .Tcsendbreak = NULL,
                                 .Tcflow = NULL,
                                 .Tcgetsid = NULL,
                                 .Tcgetpgrp = NULL,
                                 .Tcsetpgrp = NULL,
#ifdef HAVE_SOCKATMARK
                                 .Sockatmark = NULL,
#endif
                                 .Fexecve = NULL,
                             }};

#endif /* DISABLE_VFS */
