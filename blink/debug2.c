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

#include "blink/assert.h"
#include "blink/builtin.h"
#include "blink/elf.h"
#include "blink/fspath.h"
#include "blink/loader.h"
#include "blink/machine.h"
#include "blink/map.h"
#include "blink/overlays.h"
#include "blink/util.h"

#ifdef DISABLE_OVERLAYS
#define OverlaysOpen openat
#endif

void LoadDebugSymbols(struct Elf *elf) {
  int fd, n;
  char *path;
  void *elfmap;
  struct stat st;
  bool ok = false;
  char buf[PATH_MAX];
  if (elf->ehdr && GetElfSymbolTable(elf->ehdr, elf->size, &n) && n) return;
  unassert(elf->prog);
  snprintf(buf, sizeof(buf), "%s.dbg", elf->prog);
  path = JoinPath(GetStartDir(), buf);
  if ((fd = OverlaysOpen(AT_FDCWD, path, O_RDONLY, 0)) != -1) {
    if (fstat(fd, &st) != -1 &&
        (elfmap = Mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0,
                       "debug")) != MAP_FAILED) {
      elf->ehdr = (Elf64_Ehdr_ *)elfmap;
      elf->size = st.st_size;
      ok = true;
    }
    close(fd);
  }
  if (!ok) {
    LOGF("LoadDebugSymbols(%s) failed: %s", path, strerror(errno));
  }
  free(path);
}
