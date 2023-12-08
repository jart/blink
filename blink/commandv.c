/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2022 Justine Alexandra Roberts Tunney                              │
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
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blink/builtin.h"
#include "blink/overlays.h"
#include "blink/util.h"
#include "blink/vfs.h"

struct PathSearcher {
  char *path;
  size_t pathlen;
  size_t namelen;
  const char *name;
  const char *syspath;
};

static char EndsWithIgnoreCase(const char *p, unsigned long n, const char *s) {
  unsigned long i, m;
  if (n >= (m = strlen(s))) {
    for (i = n - m; i < n; ++i) {
      if (tolower(p[i]) != *s++) {
        return 0;
      }
    }
    return 1;
  } else {
    return 0;
  }
}

static char IsComPath(struct PathSearcher *ps) {
  return EndsWithIgnoreCase(ps->name, ps->namelen, ".com") ||
         EndsWithIgnoreCase(ps->name, ps->namelen, ".exe") ||
         EndsWithIgnoreCase(ps->name, ps->namelen, ".com.dbg");
}

static char AccessCommand(struct PathSearcher *ps, const char *suffix,
                          unsigned long pathlen) {
  unsigned long suffixlen;
  suffixlen = strlen(suffix);
  if (pathlen + 1 + ps->namelen + suffixlen + 1 > ps->pathlen) return 0;
  if (pathlen && ps->path[pathlen - 1] != '/') ps->path[pathlen++] = '/';
  memcpy(ps->path + pathlen, ps->name, ps->namelen);
  memcpy(ps->path + pathlen + ps->namelen, suffix, suffixlen + 1);
  return !VfsAccess(AT_FDCWD, ps->path, X_OK, 0);
}

static char SearchPath(struct PathSearcher *ps, const char *suffix) {
  const char *p;
  unsigned long i;
  for (p = ps->syspath;;) {
    for (i = 0; p[i] && p[i] != ':'; ++i) {
      if (i < ps->pathlen) {
        ps->path[i] = p[i];
      }
    }
    if (AccessCommand(ps, suffix, i)) {
      return 1;
    } else if (p[i] == ':') {
      p += i + 1;
    } else {
      return 0;
    }
  }
}

static char FindCommand(struct PathSearcher *ps, const char *suffix) {
  if (memchr(ps->name, '/', ps->namelen) ||
      memchr(ps->name, '\\', ps->namelen)) {
    ps->path[0] = 0;
    return AccessCommand(ps, suffix, 0);
  } else {
    if (AccessCommand(ps, suffix, 0)) return 1;
  }
  return SearchPath(ps, suffix);
}

/**
 * Searches for command `name` on system `$PATH`.
 */
char *Commandv(const char *name, char *buf, size_t size) {
  struct PathSearcher ps;
  ps.path = buf;
  ps.pathlen = size;
  ps.syspath = getenv("PATH");
  if (!ps.syspath) ps.syspath = "/usr/local/bin:/bin:/usr/bin";
  if (!(ps.namelen = strlen((ps.name = name)))) return 0;
  if (ps.namelen + 1 > ps.pathlen) return 0;
  if (FindCommand(&ps, "") || (!IsComPath(&ps) && FindCommand(&ps, ".com"))) {
    return ps.path;
  } else {
    return 0;
  }
}
