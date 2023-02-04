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
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/elf.h"
#include "blink/endian.h"
#include "blink/util.h"

#define READ32(p) Read32((const u8 *)(p))

// Returns true if file is almost certainly a Haiku executable.
bool IsHaikuExecutable(int fd) {
  int i, n;
  struct stat fi;
  bool res = false;
  const char *stab;
  Elf64_Ehdr_ *ehdr;
  const Elf64_Sym_ *st;
  if (fstat(fd, &fi) == -1) return false;
  ehdr = (Elf64_Ehdr_ *)mmap(0, fi.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if ((void *)ehdr == MAP_FAILED) return false;
  if (READ32(ehdr) == READ32("\177ELF") &&
      (stab = GetElfStringTable(ehdr, fi.st_size)) &&
      (st = GetElfSymbolTable(ehdr, fi.st_size, &n))) {
    for (i = 0; i < n; ++i) {
      if (ELF64_ST_TYPE_(st[i].info) == STT_OBJECT_ &&
          !strcmp(stab + Read32(st[i].name), "_gSharedObjectHaikuVersion")) {
        res = true;
        break;
      }
    }
  }
  unassert(!munmap(ehdr, fi.st_size));
  return res;
}
