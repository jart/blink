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
#include "blink/dis.h"
#include "blink/dll.h"
#include "blink/elf.h"
#include "blink/endian.h"
#include "blink/fspath.h"
#include "blink/loader.h"
#include "blink/machine.h"
#include "blink/map.h"
#include "blink/overlays.h"
#include "blink/util.h"

#ifdef DISABLE_OVERLAYS
#define OverlaysOpen openat
#endif

#define READ32(p) Read32((const u8 *)(p))

static void LoadFileMapSymbols(struct System *s, struct FileMap *fm) {
  int fd;
  i64 base;
  void *map;
  char *path;
  int oflags;
  struct stat st;
  char pathdbg[PATH_MAX];
  if (fm->offset) return;
  oflags = O_RDONLY | O_CLOEXEC | O_NOCTTY;
  snprintf(pathdbg, sizeof(pathdbg), "%s.dbg", fm->path);
  if ((fd = OverlaysOpen(AT_FDCWD, (path = pathdbg), oflags, 0)) != -1 ||
      (fd = OverlaysOpen(AT_FDCWD, (path = fm->path), oflags, 0)) != -1) {
    if (fstat(fd, &st) != -1 &&
        (map = Mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0, "debug")) !=
            MAP_FAILED) {
      if (st.st_size >= sizeof(Elf64_Ehdr_) &&
          READ32(map) == READ32("\177ELF") &&
          GetElfMemorySize((Elf64_Ehdr_ *)map, st.st_size, &base) != -1) {
        DisLoadElf(s->dis, (Elf64_Ehdr_ *)map, st.st_size, fm->virt - base);
      } else {
        ELF_LOGF("%s: not a valid elf image", path);
      }
      unassert(!munmap(map, st.st_size));
    } else {
      ELF_LOGF("%s: mmap failed: %s", path, DescribeHostErrno(errno));
    }
    unassert(!close(fd));
  } else {
    ELF_LOGF("%s: open failed: %s", path, DescribeHostErrno(errno));
  }
}

void LoadDebugSymbols(struct System *s) {
  struct Dll *e;
  unassert(s->dis);
  s->onfilemap = LoadFileMapSymbols;
  for (e = dll_first(s->filemaps); e; e = dll_next(s->filemaps, e)) {
    LoadFileMapSymbols(s, FILEMAP_CONTAINER(e));
  }
}
