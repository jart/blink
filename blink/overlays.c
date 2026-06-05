/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
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
#include "blink/overlays.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/builtin.h"
#include "blink/debug.h"
#include "blink/errno.h"
#include "blink/fspath.h"
#include "blink/likely.h"
#include "blink/log.h"
#include "blink/syscall.h"
#include "blink/thompike.h"
#include "blink/util.h"

#ifndef DISABLE_OVERLAYS

#define UNREACHABLE "(unreachable)"

static char **g_overlays;

static void FreeStrings(char **ss) {
  size_t i;
  if (!ss) return;
  for (i = 0; ss[i]; ++i) {
    free(ss[i]);
  }
  free(ss);
}

static char **SplitString(const char *s, int c) {
  size_t n = 0;
  const char *t;
  char *e, **g, **r = 0;
  while (s) {
    if ((t = strchr(s, c))) {
      e = strndup(s, t - s);
    } else {
      e = strdup(s);
    }
    if (!e || !(g = (char **)realloc(r, (n + 2) * sizeof(*r)))) {
      free(e);
      FreeStrings(r);
      return 0;
    }
    g[n++] = e;
    g[n] = 0;
    s = t ? t + 1 : 0;
    r = g;
  }
  return r;
}

static void FreeOverlays(void) {
  FreeStrings(g_overlays);
  g_overlays = 0;
}

// strip all leading slashes from an absolute path to make it relative,
// or return "." if the path is just slashes (i.e. the root directory)
static const char *RelativizePath(const char *path) {
  while (*path == '/') ++path;
  return *path ? path : ".";
}

// blink confines guests with overlays (openat relative to the overlay dir)
// rather than a real chroot(), so the host kernel resolves an absolute
// symlink TARGET against the host root and escapes the overlay — e.g. a guest
// /usr/local/www/nginx -> /usr/local/www/nginx-dist symlink can't be followed.
// openat2(RESOLVE_IN_ROOT) makes "/" and absolute symlinks resolve relative to
// the overlay dir, exactly like chroot. We use it only as a fallback when the
// plain openat path fails with ENOENT/ENOTDIR (the escape symptom), so the
// common case keeps its existing behavior.
#if defined(__linux__) && defined(SYS_openat2)
#include <linux/openat2.h>
#ifndef RESOLVE_IN_ROOT
#define RESOLVE_IN_ROOT 0x10
#endif
#define HAVE_HOST_OPENAT2 1

static int g_openat2_broken;

static int OpenInRoot(int rootfd, const char *relpath, int flags, int mode) {
  int fd;
  struct open_how how;
  if (g_openat2_broken) {
    errno = ENOSYS;
    return -1;
  }
  memset(&how, 0, sizeof(how));
  how.flags = (unsigned)flags;
  how.mode = (flags & (O_CREAT | O_TMPFILE)) ? (unsigned)mode : 0;
  how.resolve = RESOLVE_IN_ROOT;
  fd = (int)syscall(SYS_openat2, rootfd, relpath, &how, sizeof(how));
  if (fd == -1 && errno == ENOSYS) g_openat2_broken = 1;
  return fd;
}

// Resolve the parent directory of relpath within the overlay root and write
// its real host path to out[]; *base points at the final path component.
// Intermediate absolute symlinks are resolved in-root; the final component is
// left for the caller's operation so its follow/create semantics are kept.
static int ResolveParentInRoot(int rootfd, const char *relpath, char *out,
                               size_t outsz, const char **base) {
  int pfd;
  ssize_t n;
  char dirbuf[PATH_MAX];
  char procpath[64];
  const char *slash = strrchr(relpath, '/');
  if (slash) {
    size_t dlen = (size_t)(slash - relpath);
    if (dlen == 0) dlen = 1;  // a leading-slash leftover; treat as root
    if (dlen >= sizeof(dirbuf)) {
      errno = ENAMETOOLONG;
      return -1;
    }
    memcpy(dirbuf, relpath, dlen);
    dirbuf[dlen] = 0;
    *base = slash + 1;
  } else {
    dirbuf[0] = '.';
    dirbuf[1] = 0;
    *base = relpath;
  }
  if ((pfd = OpenInRoot(rootfd, dirbuf, O_PATH | O_DIRECTORY | O_CLOEXEC, 0)) ==
      -1) {
    return -1;
  }
  snprintf(procpath, sizeof(procpath), "/proc/self/fd/%d", pfd);
  n = readlink(procpath, out, outsz - 1);
  close(pfd);
  if (n == -1) return -1;
  out[n] = 0;
  return 0;
}
#endif /* __linux__ && SYS_openat2 */

// if the user only specified a single overlay, then we treat it as
// chroot would unless of course the specified root is the real one
static bool IsRestrictedRoot(char **paths) {
  return !paths[1] && paths[0][0];
}

int SetOverlays(const char *config, bool cd_into_chroot) {
  size_t i, j;
  static int once;
  bool has_real_root;
  char *path, *path2, **paths;
  if (!config) return efault();
  if (!(paths = SplitString(config, ':'))) {
    return -1;
  }
  // normalize absolute paths at startup and
  // remove non-existent paths
  has_real_root = false;
  i = j = 0;
  do {
    path = paths[i++];
    if (path) {
      if (!path[0] || (path[0] == '/' && !path[1])) {
        path[0] = 0;
        has_real_root = true;
      } else {
        path2 = ExpandUser(path);
        free(path);
        path = path2;
        path2 = (char *)malloc(PATH_MAX + 1);
        if (!realpath(path, path2)) {
          free(path);
          continue;
        }
        free(path);
        path = path2;
      }
    }
    paths[j++] = path;
  } while (path);
  if (!paths[0]) {
    LOGF("blink overlays '%s' didn't have a path that exists", config);
    FreeStrings(paths);
    return einval();
  }
  if (!has_real_root && paths[1]) {
    LOGF("if multiple overlays are specified, "
         "one of them must be empty string");
    FreeStrings(paths);
    return einval();
  }
  if (cd_into_chroot && IsRestrictedRoot(paths)) {
    if (chdir(paths[0])) {
      LOGF("failed to cd into blink overlay: %s", DescribeHostErrno(errno));
      FreeStrings(paths);
      return -1;
    }
  }
  if (!once) {
    atexit(FreeOverlays);
    once = 1;
  }
  FreeOverlays();
  g_overlays = paths;
  return 0;
}

// if we get these failures when opening a temporary dirfd of a user
// supplied overlay path, then it's definitely not a user error, and
// therefore not safe to continue.
static bool IsUnrecoverableErrno(void) {
  return errno == EINTR || errno == EMFILE || errno == ENFILE;
}

char *OverlaysGetcwd(char *output, size_t size) {
  size_t n, m;
  char *cwd, buf[PATH_MAX];
  if (!(cwd = (getcwd)(buf, sizeof(buf)))) return 0;
  n = strlen(cwd);
  if (IsRestrictedRoot(g_overlays)) {
    m = strlen(g_overlays[0]);
    if (n == m && !memcmp(cwd, g_overlays[0], n)) {
      cwd[0] = '/';
      cwd[1] = 0;
    } else if (n > m && !memcmp(cwd, g_overlays[0], m) && cwd[m] == '/') {
      cwd += m;
    } else if (strlen(UNREACHABLE) + n + 1 < sizeof(buf)) {
      memmove(cwd + strlen(UNREACHABLE), cwd, n + 1);
      memcpy(cwd, UNREACHABLE, strlen(UNREACHABLE));
    } else {
      return 0;
    }
    n = strlen(cwd);
  }
  if (n + 1 > size) return 0;
  memcpy(output, cwd, n + 1);
  return output;
}

static int Chdir(const char *path) {
  return chdir(path);
}

int OverlaysChdir(const char *path) {
  size_t n, m;
  char buf[PATH_MAX];
  if (!path) return efault();
  if (!path[0]) return enoent();
  if (IsRestrictedRoot(g_overlays) && path[0] == '/') {
    if (!path[1]) {
      return Chdir(g_overlays[0]);
    } else {
      n = strlen(g_overlays[0]);
      m = strlen(path);
      if (n + m + 1 > sizeof(buf)) {
        errno = ENAMETOOLONG;
        return -1;
      }
      memcpy(buf, g_overlays[0], n);
      memcpy(buf + n, path, m);
      buf[n + m] = 0;
      return Chdir(buf);
    }
  }
  return Chdir(path);
}

// Standard device files that we'll satisfy from the host if the chroot
// doesn't have them. Apps assume these always exist (they're kernel-provided
// on a real system), but minimal chroots often skip them.
static bool IsStandardDevice(const char *path) {
  return !strcmp(path, "/dev/null") ||      //
         !strcmp(path, "/dev/zero") ||      //
         !strcmp(path, "/dev/full") ||      //
         !strcmp(path, "/dev/random") ||    //
         !strcmp(path, "/dev/urandom") ||   //
         !strcmp(path, "/dev/tty");
}

int OverlaysOpen(int dirfd, const char *path, int flags, int mode) {
  int fd;
  size_t i;
  int err = -1;
  if (!path) return efault();
  if (!*path) return enoent();
  if (path[0] != '/' && path[0]) {
    return openat(dirfd, path, flags, mode);
  }
  // Pty device paths are always satisfied from the host. The guest's chroot
  // typically lacks /dev/pts/, but the host's pty allocator (used by our
  // posix_openpt impl) names slaves /dev/pts/N — the guest must be able to
  // open that exact path back. This bypass is narrow and read/write-only by
  // file path; it doesn't broaden the chroot for anything else.
  if (!strncmp(path, "/dev/pts/", 9) || !strcmp(path, "/dev/ptmx")) {
    if ((fd = open(path, flags, mode)) != -1) return fd;
    if (errno != ENOENT && errno != ENOTDIR) return -1;
    // fall through to normal overlay search if host doesn't have it
  }
  // Standard character devices (/dev/null, /dev/zero, /dev/random, ...) are
  // always satisfied from the host — the kernel's char devices have no
  // useful in-chroot equivalent. Going through the overlay first is not
  // just wasteful: a read-only /dev in the chroot makes openat() return
  // EACCES (not ENOENT) for shell redirections like `>/dev/null` that
  // carry O_CREAT, which would bypass the ENOENT-only fallback at the
  // bottom. pkg PRE-INSTALL scripts hit this.
  if (IsStandardDevice(path)) {
    if ((fd = open(path, flags, mode)) != -1) return fd;
    if (errno != ENOENT && errno != ENOTDIR) return -1;
    // fall through to normal overlay search if host doesn't have it
  }
  for (i = 0; g_overlays[i]; ++i) {
    if (!*g_overlays[i]) {
      if ((fd = open(path, flags, mode)) != -1) {
        return fd;
      }
      if (errno != ENOENT && errno != ENOTDIR) {
        return -1;
      }
      if (err == -1) {
        err = errno;
      }
    } else {
      dirfd = open(g_overlays[i], O_RDONLY | O_DIRECTORY | O_CLOEXEC, 0);
      if (dirfd == -1) {
        if (IsUnrecoverableErrno()) {
          return -1;
        } else {
          LOGF("bad overlay %s: %s", g_overlays[i], DescribeHostErrno(errno));
          continue;
        }
      }
      fd = openat(dirfd, RelativizePath(path), flags, mode);
#ifdef HAVE_HOST_OPENAT2
      // The plain openat above can't follow an absolute symlink target back
      // into the overlay. Retry in-root so e.g. a /www -> /www-dist symlink
      // resolves within the chroot instead of escaping to the host root.
      if (fd == -1 && (errno == ENOENT || errno == ENOTDIR)) {
        int saved = errno;
        if ((fd = OpenInRoot(dirfd, RelativizePath(path), flags, mode)) == -1) {
          errno = saved;  // keep openat's errno so the overlay loop is unchanged
        }
      }
#endif
      if (fd != -1) {
        unassert(dup2(fd, dirfd) == dirfd);
        if (flags & O_CLOEXEC) {
          unassert(!fcntl(dirfd, F_SETFD, FD_CLOEXEC));
        }
        unassert(!close(fd));
        return dirfd;
      }
      if (err == -1) {
        err = errno;
      }
      unassert(!close(dirfd));
      if (errno != ENOENT && errno != ENOTDIR) {
        return -1;
      }
    }
  }
  // Standard device files: if no overlay had them, fall back to host. Lets
  // users skip the chore of symlinking /dev/null & co. into every chroot.
  if ((err == ENOENT || err == ENOTDIR) && IsStandardDevice(path)) {
    if ((fd = open(path, flags, mode)) != -1) return fd;
  }
  unassert(err != -1);
  errno = err;
  return -1;
}

static ssize_t OverlaysGeneric(int dirfd, const char *path, void *args,
                               ssize_t fgenericat(int, const char *, void *)) {
  _Static_assert(sizeof(ssize_t) >= sizeof(int), "");
  size_t i;
  ssize_t rc;
  int err = -1;
  if (!path) return efault();
  if (!*path) return enoent();
  if (path[0] != '/' && path[0]) {
    return fgenericat(dirfd, path, args);
  }
  for (i = 0; g_overlays[i]; ++i) {
    if (!*g_overlays[i]) {
      if ((rc = fgenericat(AT_FDCWD, path, args)) != -1) {
        return rc;
      }
      if (err == -1) {
        err = errno;
      }
      if (err != ENOENT && err != ENOTDIR) {
        return -1;
      }
    } else {
      dirfd = open(g_overlays[i], O_RDONLY | O_DIRECTORY | O_CLOEXEC, 0);
      if (dirfd == -1) {
        if (IsUnrecoverableErrno()) {
          return -1;
        } else {
          LOGF("bad overlay %s: %s", g_overlays[i], DescribeHostErrno(errno));
          continue;
        }
      }
      rc = fgenericat(dirfd, RelativizePath(path), args);
#ifdef HAVE_HOST_OPENAT2
      // Same absolute-symlink-escape fallback as OverlaysOpen: resolve the
      // path's parent within the overlay root, then retry on the resulting
      // host path so an intermediate /a -> /b symlink stays inside the chroot.
      if (rc == -1 && (errno == ENOENT || errno == ENOTDIR)) {
        int saved = errno;
        char hostparent[PATH_MAX], hostpath[PATH_MAX];
        const char *base;
        if (ResolveParentInRoot(dirfd, RelativizePath(path), hostparent,
                                sizeof(hostparent), &base) != -1) {
          size_t pl = strlen(hostparent), bl = strlen(base);
          if (pl + 1 + bl + 1 <= sizeof(hostpath)) {
            memcpy(hostpath, hostparent, pl);
            if (bl) {
              hostpath[pl] = '/';
              memcpy(hostpath + pl + 1, base, bl + 1);
            } else {
              hostpath[pl] = 0;
            }
            rc = fgenericat(AT_FDCWD, hostpath, args);
          }
        }
        if (rc == -1) errno = saved;
      }
#endif
      if (rc != -1) {
        unassert(!close(dirfd));
        return rc;
      }
      if (err == -1) {
        err = errno;
      }
      unassert(!close(dirfd));
      if (errno != ENOENT && errno != ENOTDIR) {
        return -1;
      }
    }
  }
  unassert(err != -1);
  errno = err;
  return -1;
}

////////////////////////////////////////////////////////////////////////////////

struct Stat {
  struct stat *st;
  int flags;
};

static ssize_t Stat(int dirfd, const char *path, void *vargs) {
  struct Stat *args = (struct Stat *)vargs;
  return fstatat(dirfd, path, args->st, args->flags);
}

int OverlaysStat(int dirfd, const char *path, struct stat *st, int flags) {
  struct Stat args = {st, flags};
  return OverlaysGeneric(dirfd, path, &args, Stat);
}

////////////////////////////////////////////////////////////////////////////////

struct Access {
  mode_t mode;
  int flags;
};

static ssize_t Access(int dirfd, const char *path, void *vargs) {
  struct Access *args = (struct Access *)vargs;
  return faccessat(dirfd, path, args->mode, args->flags);
}

int OverlaysAccess(int dirfd, const char *path, mode_t mode, int flags) {
  struct Access args = {mode, flags};
  return OverlaysGeneric(dirfd, path, &args, Access);
}

////////////////////////////////////////////////////////////////////////////////

struct Unlink {
  int flags;
};

static ssize_t Unlink(int dirfd, const char *path, void *vargs) {
  struct Unlink *args = (struct Unlink *)vargs;
  return unlinkat(dirfd, path, args->flags);
}

int OverlaysUnlink(int dirfd, const char *path, int flags) {
  struct Unlink args = {flags};
  return OverlaysGeneric(dirfd, path, &args, Unlink);
}

////////////////////////////////////////////////////////////////////////////////

struct Mkdir {
  mode_t mode;
};

static ssize_t Mkdir(int dirfd, const char *path, void *vargs) {
  struct Mkdir *args = (struct Mkdir *)vargs;
  return mkdirat(dirfd, path, args->mode);
}

int OverlaysMkdir(int dirfd, const char *path, mode_t mode) {
  struct Mkdir args = {mode};
  return OverlaysGeneric(dirfd, path, &args, Mkdir);
}

////////////////////////////////////////////////////////////////////////////////

struct Mkfifo {
  mode_t mode;
};

static ssize_t Mkfifo(int dirfd, const char *path, void *vargs) {
  struct Mkfifo *args = (struct Mkfifo *)vargs;
  return mkfifoat(dirfd, path, args->mode);
}

int OverlaysMkfifo(int dirfd, const char *path, mode_t mode) {
  struct Mkfifo args = {mode};
  return OverlaysGeneric(dirfd, path, &args, Mkfifo);
}

////////////////////////////////////////////////////////////////////////////////

struct Chmod {
  mode_t mode;
  int flags;
};

static ssize_t Chmod(int dirfd, const char *path, void *vargs) {
  struct Chmod *args = (struct Chmod *)vargs;
  return fchmodat(dirfd, path, args->mode, args->flags);
}

int OverlaysChmod(int dirfd, const char *path, mode_t mode, int flags) {
  struct Chmod args = {mode, flags};
  return OverlaysGeneric(dirfd, path, &args, Chmod);
}

////////////////////////////////////////////////////////////////////////////////

struct Chown {
  uid_t uid;
  gid_t gid;
  int flags;
};

static ssize_t Chown(int dirfd, const char *path, void *vargs) {
  struct Chown *args = (struct Chown *)vargs;
  return fchownat(dirfd, path, args->uid, args->gid, args->flags);
}

int OverlaysChown(int dirfd, const char *path, uid_t uid, gid_t gid,
                  int flags) {
  struct Chown args = {uid, gid, flags};
  return OverlaysGeneric(dirfd, path, &args, Chown);
}

////////////////////////////////////////////////////////////////////////////////

struct Symlink {
  const char *target;
};

static ssize_t Symlink(int dirfd, const char *path, void *vargs) {
  struct Symlink *args = (struct Symlink *)vargs;
  return symlinkat(args->target, dirfd, path);
}

int OverlaysSymlink(const char *target, int dirfd, const char *path) {
  struct Symlink args = {target};
  return OverlaysGeneric(dirfd, path, &args, Symlink);
}

////////////////////////////////////////////////////////////////////////////////

struct Readlink {
  char *buf;
  size_t size;
};

static ssize_t Readlink(int dirfd, const char *path, void *vargs) {
  struct Readlink *args = (struct Readlink *)vargs;
  return readlinkat(dirfd, path, args->buf, args->size);
}

ssize_t OverlaysReadlink(int dirfd, const char *path, char *buf, size_t size) {
  struct Readlink args = {buf, size};
  return OverlaysGeneric(dirfd, path, &args, Readlink);
}

////////////////////////////////////////////////////////////////////////////////

struct Utime {
  const struct timespec *times;
  int flags;
};

static ssize_t Utime(int dirfd, const char *path, void *vargs) {
  struct Utime *args = (struct Utime *)vargs;
  return utimensat(dirfd, path, args->times, args->flags);
}

int OverlaysUtime(int dirfd, const char *path, const struct timespec times[2],
                  int flags) {
  struct Utime args = {times, flags};
  return OverlaysGeneric(dirfd, path, &args, Utime);
}

////////////////////////////////////////////////////////////////////////////////

static ssize_t OverlaysGeneric2(int srcdirfd, const char *srcpath, int dstdirfd,
                                const char *dstpath, void *args,
                                ssize_t fgenericat(int, const char *, int,
                                                   const char *, void *)) {
  ssize_t rc;
  int err = -1;
  ssize_t i, j;
  const char *sp, *dp;
  int srccloseme, dstcloseme;
  if (!srcpath || !dstpath) return efault();
  if (!*srcpath || !*dstpath) return enoent();
  for (j = 0; j >= 0 && g_overlays[j]; ++j) {
    if (srcpath[0] != '/' && srcpath[0]) {
      j = -2;
      sp = srcpath;
      srccloseme = -1;
    } else if (!*g_overlays[j]) {
      srcdirfd = AT_FDCWD;
      srccloseme = -1;
      sp = srcpath;
    } else {
      sp = RelativizePath(srcpath);
      srccloseme = srcdirfd =
          open(g_overlays[j], O_RDONLY | O_DIRECTORY | O_CLOEXEC, 0);
      if (srcdirfd == -1) {
        if (IsUnrecoverableErrno()) {
          return -1;
        } else {
          LOGF("bad overlay %s: %s", g_overlays[j], DescribeHostErrno(errno));
          continue;
        }
      }
    }
    for (i = 0; i >= 0 && g_overlays[i]; ++i) {
      if (dstpath[0] != '/' && dstpath[0]) {
        i = -2;
        dp = dstpath;
        dstcloseme = -1;
      } else if (!*g_overlays[i]) {
        dstdirfd = AT_FDCWD;
        dstcloseme = -1;
        dp = dstpath;
      } else {
        dp = RelativizePath(dstpath);
        dstcloseme = dstdirfd =
            open(g_overlays[i], O_RDONLY | O_DIRECTORY | O_CLOEXEC, 0);
        if (dstdirfd == -1) {
          if (IsUnrecoverableErrno()) {
            if (srccloseme != -1) close(srccloseme);
            return -1;
          } else {
            LOGF("bad overlay %s: %s", g_overlays[i], DescribeHostErrno(errno));
            continue;
          }
        }
      }
      if ((rc = fgenericat(srcdirfd, sp, dstdirfd, dp, args)) != -1) {
        if (dstcloseme != -1) close(dstcloseme);
        if (srccloseme != -1) close(srccloseme);
        return rc;
      }
      if (err == -1) {
        err = errno;
      }
      if (dstcloseme != -1) close(dstcloseme);
      if (errno != ENOENT && errno != ENOTDIR) {
        if (srccloseme != -1) close(srccloseme);
        return -1;
      }
    }
    if (srccloseme != -1) close(srccloseme);
  }
  unassert(err != -1);
  errno = err;
  return -1;
}

////////////////////////////////////////////////////////////////////////////////

static ssize_t Rename(int srcdirfd, const char *srcpath, int dstdirfd,
                      const char *dstpath, void *vargs) {
  return renameat(srcdirfd, srcpath, dstdirfd, dstpath);
}

int OverlaysRename(int srcdirfd, const char *srcpath, int dstdirfd,
                   const char *dstpath) {
  return OverlaysGeneric2(srcdirfd, srcpath, dstdirfd, dstpath, 0, Rename);
}

////////////////////////////////////////////////////////////////////////////////

struct Link {
  int flags;
};

static ssize_t Link(int srcdirfd, const char *srcpath, int dstdirfd,
                    const char *dstpath, void *vargs) {
  struct Link *args = (struct Link *)vargs;
  return linkat(srcdirfd, srcpath, dstdirfd, dstpath, args->flags);
}

int OverlaysLink(int srcdirfd, const char *srcpath, int dstdirfd,
                 const char *dstpath, int flags) {
  struct Link args = {flags};
  return OverlaysGeneric2(srcdirfd, srcpath, dstdirfd, dstpath, &args, Link);
}

#endif /* DISABLE_OVERLAYS */
