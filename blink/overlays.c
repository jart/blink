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
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/debug.h"
#include "blink/errno.h"
#include "blink/log.h"
#include "blink/overlays.h"
#include "blink/util.h"

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

void SetOverlays(const char *config) {
  static int once;
  char cwd[PATH_MAX];
  bool has_real_root;
  size_t i, n, cwdlen = 0;
  if (!once) {
    atexit(FreeOverlays);
    once = 1;
  }
  FreeOverlays();
  // should we use the default config?
  if (!config) {
    // when an absolute path $x is encountered
    config = ""    // use $x if it exists
             ":"   // otherwise
             "o";  // use o/$x if it exists
  }
  // load the overlay paths into an array
  unassert(g_overlays = SplitString(config, ':'));
  // make relative overlays absolute at startup time
  // just in case the app calls chdir() or something
  has_real_root = false;
  for (i = 0; g_overlays[i]; ++i) {
    if (!g_overlays[i][0]) {
      has_real_root = true;
      continue;
    }
    if (g_overlays[i][0] == '/') {
      if (!g_overlays[i][1]) {
        g_overlays[i][0] = 0;
        has_real_root = true;
      }
      continue;
    }
    if (!cwdlen) {
      if (!getcwd(cwd, sizeof(cwd))) break;
      cwdlen = strlen(cwd);
      cwd[cwdlen++] = '/';
    }
    if (cwdlen + (n = strlen(g_overlays[i])) >= sizeof(cwd)) {
      continue;
    }
    memcpy(cwd + cwdlen, g_overlays[i], n + 1);
    free(g_overlays[i]);
    g_overlays[i] = strdup(cwd);
  }
  unassert(g_overlays[0]);
  if (!has_real_root && g_overlays[1]) {
    WriteErrorString("if multiple overlays are specified, "
                     "one of them must be empty string");
    exit(1);
  }
}

// if we get these failures when opening a temporary dirfd of a user
// supplied overlay path, then it's definitely not a user error, and
// therefore not safe to continue.
static bool IsUnrecoverableErrno(void) {
  return errno == EINTR || errno == EMFILE || errno == ENFILE;
}

// if the user only specified a single overlay, then we treat it as
// chroot would unless of course the specified root is the real one
static bool IsRestrictedRoot(void) {
  return !g_overlays[1] && g_overlays[0][0];
}

char *OverlaysGetcwd(char *output, size_t size) {
  size_t n, m;
  char buf[PATH_MAX], *cwd;
  if (!(cwd = (getcwd)(buf, sizeof(buf)))) return 0;
  n = strlen(cwd);
  if (IsRestrictedRoot()) {
    m = strlen(g_overlays[0]);
    if (n == m && !memcmp(cwd, g_overlays[0], n)) {
      cwd = "/";
    } else if (n > m && !memcmp(cwd, g_overlays[0], m) && cwd[m] == '/') {
      cwd += m;
    } else {
      cwd = "(unreachable)/";
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
  if (IsRestrictedRoot() && path[0] == '/') {
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

int OverlaysOpen(int dirfd, const char *path, int flags, int mode) {
  int fd;
  int err;
  size_t i;
  if (!path) return efault();
  if (!*path) return enoent();
  if (path[0] != '/' && path[0]) {
    return openat(dirfd, path, flags, mode);
  }
  for (err = ENOENT, i = 0; g_overlays[i]; ++i) {
    if (!*g_overlays[i]) {
      if ((fd = open(path, flags, mode)) != -1) {
        return fd;
      }
      err = errno;
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
      if ((fd = openat(dirfd, !path[1] ? "." : path + 1, flags, mode)) != -1) {
        unassert(dup2(fd, dirfd) == dirfd);
        if (flags & O_CLOEXEC) {
          unassert(!fcntl(dirfd, F_SETFD, FD_CLOEXEC));
        }
        unassert(!close(fd));
        return dirfd;
      }
      err = errno;
      unassert(!close(dirfd));
      if (err != ENOENT && err != ENOTDIR) {
        return -1;
      }
    }
  }
  errno = err;
  return -1;
}

static int OverlaysGeneric(int dirfd, const char *path, void *args,
                           int fgenericat(int, const char *, void *)) {
  int err;
  size_t i;
  if (!path) return efault();
  if (!*path) return enoent();
  if (path[0] != '/' && path[0]) {
    return fgenericat(dirfd, path, args);
  }
  for (err = ENOENT, i = 0; g_overlays[i]; ++i) {
    if (!*g_overlays[i]) {
      if (!fgenericat(AT_FDCWD, path, args)) {
        return 0;
      }
      err = errno;
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
      if (!fgenericat(dirfd, !path[1] ? "." : path + 1, args)) {
        unassert(!close(dirfd));
        return 0;
      }
      err = errno;
      unassert(!close(dirfd));
      if (err != ENOENT && err != ENOTDIR) {
        return -1;
      }
    }
  }
  errno = err;
  return -1;
}

////////////////////////////////////////////////////////////////////////////////

struct Stat {
  struct stat *st;
  int flags;
};

static int Stat(int dirfd, const char *path, void *vargs) {
  struct Stat *args = (struct Stat *)vargs;
  return fstatat(dirfd, path, args->st, args->flags);
}

int OverlaysStat(int dirfd, const char *path, struct stat *st, int flags) {
  struct Stat args = {st, flags};
  return OverlaysGeneric(dirfd, path, &args, Stat);
}

////////////////////////////////////////////////////////////////////////////////

struct Access {
  int mode;
  int flags;
};

static int Access(int dirfd, const char *path, void *vargs) {
  struct Access *args = (struct Access *)vargs;
  return faccessat(dirfd, path, args->mode, args->flags);
}

int OverlaysAccess(int dirfd, const char *path, int mode, int flags) {
  struct Access args = {mode, flags};
  return OverlaysGeneric(dirfd, path, &args, Access);
}

////////////////////////////////////////////////////////////////////////////////

struct Unlink {
  int flags;
};

static int Unlink(int dirfd, const char *path, void *vargs) {
  struct Unlink *args = (struct Unlink *)vargs;
  return unlinkat(dirfd, path, args->flags);
}

int OverlaysUnlink(int dirfd, const char *path, int flags) {
  struct Unlink args = {flags};
  return OverlaysGeneric(dirfd, path, &args, Unlink);
}

////////////////////////////////////////////////////////////////////////////////

struct Mkdir {
  int mode;
};

static int Mkdir(int dirfd, const char *path, void *vargs) {
  struct Mkdir *args = (struct Mkdir *)vargs;
  return mkdirat(dirfd, path, args->mode);
}

int OverlaysMkdir(int dirfd, const char *path, int mode) {
  struct Mkdir args = {mode};
  return OverlaysGeneric(dirfd, path, &args, Mkdir);
}

////////////////////////////////////////////////////////////////////////////////

struct Chmod {
  int mode;
  int flags;
};

static int Chmod(int dirfd, const char *path, void *vargs) {
  struct Chmod *args = (struct Chmod *)vargs;
  return fchmodat(dirfd, path, args->mode, args->flags);
}

int OverlaysChmod(int dirfd, const char *path, int mode, int flags) {
  struct Chmod args = {mode, flags};
  return OverlaysGeneric(dirfd, path, &args, Chmod);
}

////////////////////////////////////////////////////////////////////////////////

struct Chown {
  uid_t uid;
  gid_t gid;
  int flags;
};

static int Chown(int dirfd, const char *path, void *vargs) {
  struct Chown *args = (struct Chown *)vargs;
  return fchownat(dirfd, path, args->uid, args->gid, args->flags);
}

int OverlaysChown(int dirfd, const char *path, uid_t uid, gid_t gid,
                  int flags) {
  struct Chown args = {uid, gid, flags};
  return OverlaysGeneric(dirfd, path, &args, Chown);
}
