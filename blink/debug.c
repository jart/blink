/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
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
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/loader.h"
#include "blink/log.h"

void LoadDebugSymbols(struct Elf *elf) {
  int fd, n;
  void *elfmap;
  char buf[1024];
  struct stat st;
  if (elf->ehdr && GetElfSymbolTable(elf->ehdr, elf->size, &n) && n) return;
  unassert(elf->prog);
  snprintf(buf, sizeof(buf), "%s.dbg", elf->prog);
  if ((fd = open(buf, O_RDONLY)) != -1) {
    if (fstat(fd, &st) != -1 &&
        (elfmap = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) !=
            MAP_FAILED) {
      elf->ehdr = (Elf64_Ehdr *)elfmap;
      elf->size = st.st_size;
    }
    close(fd);
  }
}

void PrintFds(struct Fds *fds) {
  dll_element *e;
  LOGF("%-8s %-8s %-8s %-8s", "fildes", "systemfd", "oflags", "cloexec");
  for (e = dll_first(fds->list); e; e = dll_next(fds->list, e)) {
    LOGF("%-8d %-8d %-8x %-8s", FD_CONTAINER(e)->fildes,
         FD_CONTAINER(e)->systemfd, FD_CONTAINER(e)->oflags,
         FD_CONTAINER(e)->cloexec ? "true" : "false");
  }
}
