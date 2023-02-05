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
    if (!e || !(g = realloc(r, (n + 2) * sizeof(*r)))) {
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
  for (i = 0; g_overlays[i]; ++i) {
    if (!g_overlays[i][0]) continue;
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
}

int OverlaysOpen(int dirfd, const char *path, int flags, int mode) {
  int fd;
  int err;
  size_t i;
  unassert(g_overlays);
  if (!path) return efault();
  if (!*path) return enoent();
  if (*path != '/') {
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
        if (errno == EINTR || errno == EMFILE || errno == ENFILE) {
          return -1;
        } else {
          LOGF("bad overlay %s: %s", g_overlays[i], DescribeHostErrno(errno));
          continue;
        }
      }
      if ((fd = openat(dirfd, path + 1, flags, mode)) != -1) {
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
