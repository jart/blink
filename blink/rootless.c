#include "blink/rootless.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include "blink/endian.h"

static unsigned rootless_ids[2][4];

static pthread_mutex_t credentials_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t credentials_once = PTHREAD_ONCE_INIT;
static unsigned *rootless_groups;
static int rootless_ngroups;

static void LockCredentials(void) { pthread_mutex_lock(&credentials_lock); }
static void UnlockCredentials(void) { pthread_mutex_unlock(&credentials_lock); }
static void InitCredentials(void) {
  pthread_atfork(LockCredentials, UnlockCredentials, UnlockCredentials);
}

/* IDs are bookkeeping state with privileged transitions, like the rest of
 * the rootless backend. Host credentials are never modified. */
void RootlessGetIds(int group, unsigned ids[4]) {
  pthread_once(&credentials_once, InitCredentials);
  LockCredentials();
  memcpy(ids, rootless_ids[!!group], sizeof(rootless_ids[0]));
  UnlockCredentials();
}

unsigned RootlessGetId(int group, enum RootlessId kind) {
  unsigned ids[4];
  RootlessGetIds(group, ids);
  return ids[kind];
}

int RootlessSetResId(int group, unsigned real, unsigned effective, unsigned saved) {
  const unsigned unchanged = (unsigned)-1;
  pthread_once(&credentials_once, InitCredentials);
  LockCredentials();
  unsigned *ids = rootless_ids[!!group];
  if (!((real == unchanged || real == ids[ROOTLESS_REAL]) &&
        (effective == unchanged || (effective == ids[ROOTLESS_EFFECTIVE] &&
                                    effective == ids[ROOTLESS_FS])) &&
        (saved == unchanged || saved == ids[ROOTLESS_SAVED]))) {
    if (real != unchanged) ids[ROOTLESS_REAL] = real;
    if (effective != unchanged) ids[ROOTLESS_EFFECTIVE] = effective;
    if (saved != unchanged) ids[ROOTLESS_SAVED] = saved;
    ids[ROOTLESS_FS] = ids[ROOTLESS_EFFECTIVE];
  }
  UnlockCredentials();
  return 0;
}

int RootlessSetId(int group, unsigned id) {
  if (id == (unsigned)-1) { errno = EINVAL; return -1; }
  return RootlessSetResId(group, id, id, id);
}

int RootlessSetReId(int group, unsigned real, unsigned effective) {
  const unsigned unchanged = (unsigned)-1;
  pthread_once(&credentials_once, InitCredentials);
  LockCredentials();
  unsigned *ids = rootless_ids[!!group];
  unsigned oldreal = ids[ROOTLESS_REAL];
  if (real != unchanged) ids[ROOTLESS_REAL] = real;
  if (effective != unchanged) ids[ROOTLESS_EFFECTIVE] = effective;
  if (real != unchanged || (effective != unchanged && effective != oldreal)) {
    ids[ROOTLESS_SAVED] = ids[ROOTLESS_EFFECTIVE];
  }
  ids[ROOTLESS_FS] = ids[ROOTLESS_EFFECTIVE];
  UnlockCredentials();
  return 0;
}

unsigned RootlessSetFsId(int group, unsigned id) {
  pthread_once(&credentials_once, InitCredentials);
  LockCredentials();
  unsigned *ids = rootless_ids[!!group];
  unsigned old = ids[ROOTLESS_FS];
  if (id != (unsigned)-1) ids[ROOTLESS_FS] = id;
  UnlockCredentials();
  return old;
}

static int CompareGroup(const void *a, const void *b) {
  unsigned x = *(const unsigned *)a, y = *(const unsigned *)b;
  return (x > y) - (x < y);
}

int RootlessGetgroups(int size, unsigned *groups) {
  int count;
  pthread_once(&credentials_once, InitCredentials);
  LockCredentials();
  count = rootless_ngroups;
  if (size && size < count) {
    UnlockCredentials();
    errno = EINVAL;
    return -1;
  }
  if (size && count) memcpy(groups, rootless_groups, count * sizeof(*groups));
  UnlockCredentials();
  return count;
}

int RootlessSetgroups(int count, const unsigned *groups) {
  unsigned *copy = NULL;
  if (count < 0 || count > 65536) { errno = EINVAL; return -1; }
  if (count) {
    if (!(copy = malloc(count * sizeof(*copy)))) return -1;
    memcpy(copy, groups, count * sizeof(*copy));
    for (int i = 0; i < count; ++i) {
      if (copy[i] == (unsigned)-1) { free(copy); errno = EINVAL; return -1; }
    }
    qsort(copy, count, sizeof(*copy), CompareGroup);
  }
  pthread_once(&credentials_once, InitCredentials);
  LockCredentials();
  free(rootless_groups);
  rootless_groups = copy;
  rootless_ngroups = count;
  UnlockCredentials();
  return 0;
}

/* Linux fs/open.c:chown_common and fs/attr.c:setattr_should_drop_sgid.
 * This backend provides privileged metadata operations, not a capability model.
 * Directories keep their bits; privileged non-executable SGID files keep SGID.
 */
static unsigned ChownMode(unsigned mode) {
  if (S_ISDIR(mode)) return mode;
  mode &= ~S_ISUID;
  if (mode & S_IXGRP) mode &= ~S_ISGID;
  return mode;
}

struct RootlessAnonData {
  _Atomic(unsigned) uid, gid, mode;
};

struct RootlessAnon {
  // Each process owns its mappings and local descriptor references. The data
  // mapping is shared across fork so fchown through either process is visible.
  _Atomic(unsigned) refs;
  struct RootlessAnonData *data;
};

struct RootlessAnon *RootlessAnonNew(unsigned mode) {
  struct RootlessAnon *a = malloc(sizeof(*a));
  if (!a) return NULL;
  a->data = mmap(NULL, sizeof(*a->data), PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (a->data == MAP_FAILED) { free(a); return NULL; }
  atomic_init(&a->refs, 1);
  atomic_init(&a->data->uid, RootlessGetId(0, ROOTLESS_FS));
  atomic_init(&a->data->gid, RootlessGetId(1, ROOTLESS_FS));
  atomic_init(&a->data->mode, mode & 07777);
  return a;
}

struct RootlessAnon *RootlessAnonAcquire(struct RootlessAnon *a) {
  if (a) atomic_fetch_add(&a->refs, 1);
  return a;
}

void RootlessAnonFree(struct RootlessAnon *a) {
  if (a && atomic_fetch_sub(&a->refs, 1) == 1) {
    munmap(a->data, sizeof(*a->data));
    free(a);
  }
}

void RootlessAnonStat(struct RootlessAnon *a, struct stat *st) {
  st->st_uid = atomic_load(&a->data->uid);
  st->st_gid = atomic_load(&a->data->gid);
  st->st_mode = (st->st_mode & S_IFMT) | atomic_load(&a->data->mode);
}

int RootlessAnonChown(struct RootlessAnon *a, unsigned uid, unsigned gid) {
  if (uid != (unsigned)-1) atomic_store(&a->data->uid, uid);
  if (gid != (unsigned)-1) atomic_store(&a->data->gid, gid);
  unsigned old = atomic_load(&a->data->mode);
  while (!atomic_compare_exchange_weak(&a->data->mode, &old, ChownMode(old))) {}
  return 0;
}

int RootlessAnonChmod(struct RootlessAnon *a, unsigned mode) {
  atomic_store(&a->data->mode, mode & 07777);
  return 0;
}

int RootlessEnabled(void) {
#ifdef __APPLE__
  const char *value = getenv("BLINK_ROOTLESS");
  return value && !strcmp(value, "1");
#else
  return 0;
#endif
}

#ifdef __APPLE__
#include <sys/xattr.h>
#include <limits.h>
#define OWNER_ATTRIBUTE "org.image-forge.owner.v1"
#define MODE_ATTRIBUTE "org.image-forge.mode.v1"

static int OwnerPath(int fd, const char *name, char path[PATH_MAX]) {
  int n;
  if (*name == '/' || fd == AT_FDCWD) {
    n = snprintf(path, PATH_MAX, "%s", name);
  } else {
    if (fcntl(fd, F_GETPATH, path) == -1) return -1;
    size_t size = strlen(path);
    n = snprintf(path + size, PATH_MAX - size, "/%s", name);
    if (n >= 0) n += size;
  }
  if (n < 0 || n >= PATH_MAX) { errno = ENAMETOOLONG; return -1; }
  return 0;
}

static int ReadOwner(int fd, const char *name, unsigned *uid, unsigned *gid,
                     int flags) {
  char path[PATH_MAX];
  unsigned char value[8];
  ssize_t n;
  if (name) {
    if (OwnerPath(fd, name, path)) return -1;
    n = getxattr(path, OWNER_ATTRIBUTE, value, sizeof(value), 0,
                 flags & AT_SYMLINK_NOFOLLOW ? XATTR_NOFOLLOW : 0);
  } else {
    n = fgetxattr(fd, OWNER_ATTRIBUTE, value, sizeof(value), 0, 0);
  }
  if (n == -1 && errno == ENOATTR) { *uid = *gid = 0; return 0; }
  if (n == -1) return -1;
  if (n != sizeof(value)) { errno = EIO; return -1; }
  *uid = Read32(value);
  *gid = Read32(value + 4);
  return 0;
}
#endif

int RootlessStat(int fd, const char *name, struct stat *st, int flags) {
#ifdef __APPLE__
  unsigned uid, gid;
  if (RootlessEnabled()) {
    if (ReadOwner(fd, name, &uid, &gid, flags)) return -1;
    st->st_uid = uid;
    st->st_gid = gid;
    // Linux symlink permissions are always 0777; Darwin may report 0755.
    if (S_ISLNK(st->st_mode)) st->st_mode = (st->st_mode & S_IFMT) | 0777;
    char path[PATH_MAX];
    unsigned char mode[4];
    ssize_t n;
    if (name) {
      if (OwnerPath(fd, name, path)) return -1;
      n = getxattr(path, MODE_ATTRIBUTE, mode, sizeof(mode), 0,
                   flags & AT_SYMLINK_NOFOLLOW ? XATTR_NOFOLLOW : 0);
    } else {
      n = fgetxattr(fd, MODE_ATTRIBUTE, mode, sizeof(mode), 0, 0);
    }
    if (n == -1 && errno != ENOATTR) return -1;
    if (n >= 0) {
      if (n != sizeof(mode)) { errno = EIO; return -1; }
      st->st_mode = (st->st_mode & S_IFMT) | (Read32(mode) & 07777);
    }
  }
#endif
  return 0;
}

int RootlessChmod(int fd, const char *name, unsigned mode, int flags) {
#ifdef __APPLE__
  char path[PATH_MAX];
  struct stat st;
  unsigned char value[4];
  int rc;
  if (name) {
    if (OwnerPath(fd, name, path)) return -1;
    if (fstatat(fd, name, &st, flags)) return -1;
  } else if (fstat(fd, &st)) {
    return -1;
  }
  Write32(value, mode & 07777);
  // Host access must remain possible for metadata bookkeeping and snapshots.
  // Guest permission enforcement is not provided by this optional backend.
  unsigned hostmode = (mode & 0777) | (S_ISDIR(st.st_mode) ? 0700 : 0600);
  if (name) {
    rc = fchmodat(fd, name, hostmode, flags);
    if (!rc) rc = setxattr(path, MODE_ATTRIBUTE, value, sizeof(value), 0,
                            flags & AT_SYMLINK_NOFOLLOW ? XATTR_NOFOLLOW : 0);
  } else {
    rc = fchmod(fd, hostmode);
    if (!rc) rc = fsetxattr(fd, MODE_ATTRIBUTE, value, sizeof(value), 0, 0);
  }
  return rc;
#else
  errno = ENOSYS;
  return -1;
#endif
}

int RootlessChown(int fd, const char *name, unsigned uid, unsigned gid, int flags) {
#ifdef __APPLE__
  char path[PATH_MAX];
  struct stat st;
  int rc;
  unsigned olduid, oldgid;
  unsigned char value[8];
  if (name ? fstatat(fd, name, &st, flags) : fstat(fd, &st)) return -1;
  if (RootlessStat(fd, name, &st, flags)) return -1;
  if (uid == (unsigned)-1 || gid == (unsigned)-1) {
    if (ReadOwner(fd, name, &olduid, &oldgid, flags)) return -1;
    if (uid == (unsigned)-1) uid = olduid;
    if (gid == (unsigned)-1) gid = oldgid;
  }
  Write32(value, uid);
  Write32(value + 4, gid);
  if (!name) {
    rc = fsetxattr(fd, OWNER_ATTRIBUTE, value, sizeof(value), 0, 0);
  } else {
    if (OwnerPath(fd, name, path)) return -1;
    rc = setxattr(path, OWNER_ATTRIBUTE, value, sizeof(value), 0,
                  flags & AT_SYMLINK_NOFOLLOW ? XATTR_NOFOLLOW : 0);
  }
  if (!rc && ChownMode(st.st_mode) != st.st_mode) {
    rc = RootlessChmod(fd, name, ChownMode(st.st_mode), flags);
  }
  return rc;
#else
  errno = ENOSYS;
  return -1;
#endif
}

int RootlessPrepare(int fd, const char *name, struct RootlessOwner *owner) {
  memset(owner, 0, sizeof(*owner));
  if (!RootlessEnabled()) return 0;
  owner->uid = RootlessGetId(0, ROOTLESS_FS);
  owner->gid = RootlessGetId(1, ROOTLESS_FS);
#ifdef __APPLE__
  char path[PATH_MAX], *slash;
  struct stat parent;
  if (OwnerPath(fd, name, path)) return -1;
  slash = strrchr(path, '/');
  if (!slash) {
    strcpy(path, ".");
  } else if (slash == path) {
    slash[1] = 0;
  } else {
    *slash = 0;
  }
  if (stat(path, &parent) || RootlessStat(AT_FDCWD, path, &parent, 0)) return -1;
  if (parent.st_mode & S_ISGID) {
    owner->gid = parent.st_gid;
    owner->setgid = 1;
  }
#endif
  return 0;
}

int RootlessCreate(int fd, const char *name, int flags,
                    const struct RootlessOwner *owner, unsigned mode) {
  if (!RootlessEnabled()) return 0;
  struct stat st;
  if (name ? fstatat(fd, name, &st, flags) : fstat(fd, &st)) return -1;
  // dpkg creates files/directories with mode 000 before filling them. Darwin
  // denies xattr updates on these, even through an already-open descriptor.
  // Retain the guest mode, then permit host metadata access before writing it.
  if (!S_ISLNK(st.st_mode)) {
    unsigned hostmode = (st.st_mode & 0777) | (S_ISDIR(st.st_mode) ? 0700 : 0600);
    if (name ? fchmodat(fd, name, hostmode, flags) : fchmod(fd, hostmode)) return -1;
  }
  if (RootlessChown(fd, name, owner->uid, owner->gid, flags)) return -1;
  if (S_ISLNK(st.st_mode)) return 0;
  return RootlessChmod(fd, name, (st.st_mode & 0777) | (mode & 07000), flags);
}
