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
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blink/elf.h"
#include "blink/endian.h"
#include "blink/log.h"
#include "blink/util.h"

void CheckElfAddress(const Elf64_Ehdr *elf, size_t mapsize, intptr_t addr,
                     size_t addrsize) {
  if (addr < (intptr_t)elf || addr + addrsize > (intptr_t)elf + mapsize) {
    LOGF("CheckElfAddress failed: %#" PRIxPTR "..%#" PRIxPTR " %p..%#" PRIxPTR,
         addr, addr + addrsize, (void *)elf, (intptr_t)elf + mapsize);
    exit(202);
  }
}

char *GetElfString(const Elf64_Ehdr *elf, size_t mapsize, const char *strtab,
                   u32 rva) {
  intptr_t addr = (intptr_t)strtab + rva;
  CheckElfAddress(elf, mapsize, addr, 0);
  CheckElfAddress(elf, mapsize, addr,
                  strnlen((char *)addr, (intptr_t)elf + mapsize - addr) + 1);
  return (char *)addr;
}

Elf64_Phdr *GetElfSegmentHeaderAddress(const Elf64_Ehdr *elf, size_t mapsize,
                                       u64 i) {
  intptr_t addr = ((intptr_t)elf + (intptr_t)Read64(elf->e_phoff) +
                   (intptr_t)Read16(elf->e_phentsize) * i);
  CheckElfAddress(elf, mapsize, addr, Read16(elf->e_phentsize));
  return (Elf64_Phdr *)addr;
}

void *GetElfSectionAddress(const Elf64_Ehdr *elf, size_t mapsize,
                           const Elf64_Shdr *shdr) {
  intptr_t addr, size;
  addr = (intptr_t)elf + (intptr_t)Read64(shdr->sh_offset);
  size = (intptr_t)Read64(shdr->sh_size);
  CheckElfAddress(elf, mapsize, addr, size);
  return (void *)addr;
}

char *GetElfSectionNameStringTable(const Elf64_Ehdr *elf, size_t mapsize) {
  if (!Read64(elf->e_shoff) || !Read16(elf->e_shentsize)) return NULL;
  return (char *)GetElfSectionAddress(
      elf, mapsize,
      GetElfSectionHeaderAddress(elf, mapsize, Read16(elf->e_shstrndx)));
}

const char *GetElfSectionName(const Elf64_Ehdr *elf, size_t mapsize,
                              Elf64_Shdr *shdr) {
  if (!elf || !shdr) return NULL;
  return GetElfString(elf, mapsize, GetElfSectionNameStringTable(elf, mapsize),
                      Read32(shdr->sh_name));
}

Elf64_Shdr *GetElfSectionHeaderAddress(const Elf64_Ehdr *elf, size_t mapsize,
                                       u16 i) {
  intptr_t addr;
  addr = ((intptr_t)elf + (intptr_t)Read64(elf->e_shoff) +
          (intptr_t)Read16(elf->e_shentsize) * i);
  CheckElfAddress(elf, mapsize, addr, Read16(elf->e_shentsize));
  return (Elf64_Shdr *)addr;
}

char *GetElfStringTable(const Elf64_Ehdr *elf, size_t mapsize) {
  int i;
  const char *name;
  Elf64_Shdr *shdr;
  for (i = 0; i < Read16(elf->e_shnum); ++i) {
    shdr = GetElfSectionHeaderAddress(elf, mapsize, i);
    if (Read32(shdr->sh_type) == SHT_STRTAB) {
      name = GetElfSectionName(elf, mapsize,
                               GetElfSectionHeaderAddress(elf, mapsize, i));
      if (name && !strcmp(name, ".strtab")) {
        return (char *)GetElfSectionAddress(elf, mapsize, shdr);
      }
    }
  }
  return NULL;
}

Elf64_Sym *GetElfSymbolTable(const Elf64_Ehdr *elf, size_t mapsize,
                             int *out_count) {
  int i;
  Elf64_Shdr *shdr;
  for (i = Read16(elf->e_shnum); i > 0; --i) {
    shdr = GetElfSectionHeaderAddress(elf, mapsize, i - 1);
    if (Read32(shdr->sh_type) == SHT_SYMTAB) {
      if (Read64(shdr->sh_entsize) != sizeof(Elf64_Sym)) continue;
      if (out_count) {
        *out_count = Read64(shdr->sh_size) / Read64(shdr->sh_entsize);
      }
      return (Elf64_Sym *)GetElfSectionAddress(elf, mapsize, shdr);
    }
  }
  return NULL;
}
