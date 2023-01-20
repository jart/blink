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

void CheckElfAddress(const Elf64_Ehdr_ *elf, size_t mapsize, intptr_t addr,
                     size_t addrsize) {
  if (addr < (intptr_t)elf || addr + addrsize > (intptr_t)elf + mapsize) {
    LOGF("CheckElfAddress failed: %#" PRIxPTR "..%#" PRIxPTR " %p..%#" PRIxPTR,
         addr, addr + addrsize, (void *)elf, (intptr_t)elf + mapsize);
    exit(202);
  }
}

char *GetElfString(const Elf64_Ehdr_ *elf, size_t mapsize, const char *strtab,
                   u32 rva) {
  intptr_t addr = (intptr_t)strtab + rva;
  CheckElfAddress(elf, mapsize, addr, 0);
  CheckElfAddress(elf, mapsize, addr,
                  strnlen((char *)addr, (intptr_t)elf + mapsize - addr) + 1);
  return (char *)addr;
}

Elf64_Phdr_ *GetElfSegmentHeaderAddress(const Elf64_Ehdr_ *elf, size_t mapsize,
                                        u64 i) {
  intptr_t addr = ((intptr_t)elf + (intptr_t)Read64(elf->phoff) +
                   (intptr_t)Read16(elf->phentsize) * i);
  CheckElfAddress(elf, mapsize, addr, Read16(elf->phentsize));
  return (Elf64_Phdr_ *)addr;
}

void *GetElfSectionAddress(const Elf64_Ehdr_ *elf, size_t mapsize,
                           const Elf64_Shdr_ *shdr) {
  intptr_t addr, size;
  addr = (intptr_t)elf + (intptr_t)Read64(shdr->offset);
  size = (intptr_t)Read64(shdr->size);
  CheckElfAddress(elf, mapsize, addr, size);
  return (void *)addr;
}

char *GetElfSectionNameStringTable(const Elf64_Ehdr_ *elf, size_t mapsize) {
  if (!Read64(elf->shoff) || !Read16(elf->shentsize)) return NULL;
  return (char *)GetElfSectionAddress(
      elf, mapsize,
      GetElfSectionHeaderAddress(elf, mapsize, Read16(elf->shstrndx)));
}

const char *GetElfSectionName(const Elf64_Ehdr_ *elf, size_t mapsize,
                              Elf64_Shdr_ *shdr) {
  if (!elf || !shdr) return NULL;
  return GetElfString(elf, mapsize, GetElfSectionNameStringTable(elf, mapsize),
                      Read32(shdr->name));
}

Elf64_Shdr_ *GetElfSectionHeaderAddress(const Elf64_Ehdr_ *elf, size_t mapsize,
                                        u16 i) {
  intptr_t addr;
  addr = ((intptr_t)elf + (intptr_t)Read64(elf->shoff) +
          (intptr_t)Read16(elf->shentsize) * i);
  CheckElfAddress(elf, mapsize, addr, Read16(elf->shentsize));
  return (Elf64_Shdr_ *)addr;
}

static char *GetElfStringTableImpl(const Elf64_Ehdr_ *elf, size_t mapsize,
                                   const char *kind) {
  int i;
  const char *name;
  Elf64_Shdr_ *shdr;
  for (i = 0; i < Read16(elf->shnum); ++i) {
    shdr = GetElfSectionHeaderAddress(elf, mapsize, i);
    if (Read32(shdr->type) == SHT_STRTAB_) {
      name = GetElfSectionName(elf, mapsize,
                               GetElfSectionHeaderAddress(elf, mapsize, i));
      if (name && !strcmp(name, kind)) {
        return (char *)GetElfSectionAddress(elf, mapsize, shdr);
      }
    }
  }
  return 0;
}

char *GetElfStringTable(const Elf64_Ehdr_ *elf, size_t mapsize) {
  char *res;
  if (!(res = GetElfStringTableImpl(elf, mapsize, ".strtab"))) {
    res = GetElfStringTableImpl(elf, mapsize, ".dynstr");
  }
  return res;
}

static Elf64_Sym_ *GetElfSymbolTableImpl(const Elf64_Ehdr_ *elf, size_t mapsize,
                                         int *out_count, int kind) {
  int i;
  Elf64_Shdr_ *shdr;
  for (i = Read16(elf->shnum); i > 0; --i) {
    shdr = GetElfSectionHeaderAddress(elf, mapsize, i - 1);
    if (Read32(shdr->type) == kind) {
      if (Read64(shdr->entsize) != sizeof(Elf64_Sym_)) continue;
      if (out_count) {
        *out_count = Read64(shdr->size) / Read64(shdr->entsize);
      }
      return (Elf64_Sym_ *)GetElfSectionAddress(elf, mapsize, shdr);
    }
  }
  return 0;
}

Elf64_Sym_ *GetElfSymbolTable(const Elf64_Ehdr_ *elf, size_t mapsize,
                              int *out_count) {
  Elf64_Sym_ *res;
  if (!(res = GetElfSymbolTableImpl(elf, mapsize, out_count, SHT_SYMTAB_))) {
    res = GetElfSymbolTableImpl(elf, mapsize, out_count, SHT_DYNSYM_);
  }
  return res;
}
