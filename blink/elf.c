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
#include "blink/elf.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blink/builtin.h"
#include "blink/checked.h"
#include "blink/endian.h"
#include "blink/log.h"
#include "blink/macros.h"
#include "blink/map.h"
#include "blink/util.h"

i64 GetElfMemorySize(const Elf64_Ehdr_ *ehdr, size_t size, i64 *base) {
  size_t off;
  unsigned i;
  i64 x, y, lo, hi, res;
  const Elf64_Phdr_ *phdr;
  lo = INT64_MAX;
  hi = INT64_MIN;
  if (Read64(ehdr->phoff) < size) {
    for (i = 0; i < Read16(ehdr->phnum); ++i) {
      off = Read64(ehdr->phoff) + Read16(ehdr->phentsize) * i;
      if (off + Read16(ehdr->phentsize) > size) return -1;
      phdr = (const Elf64_Phdr_ *)((const u8 *)ehdr + off);
      if (Read32(phdr->type) == PT_LOAD_) {
        x = Read64(phdr->vaddr);
        if (ckd_add(&y, x, (i64)Read64(phdr->memsz))) return -1;
        lo = MIN(x, lo);
        hi = MAX(y, hi);
      }
    }
  }
  lo &= -FLAG_pagesize;
  if (ckd_sub(&res, hi, lo)) return -1;
  *base = lo;
  return res;
}

char *GetElfString(const Elf64_Ehdr_ *elf,  // validated
                   size_t mapsize,          // validated
                   const char *strtab,      // validated
                   size_t rva) {            // foreign
  uint64_t addr;
  if (!strtab) return 0;
  if (ckd_add(&addr, (uintptr_t)strtab, rva)) return 0;
  if (addr >= (uintptr_t)elf + mapsize) return 0;
  if (addr != (uintptr_t)addr) return 0;
  if (!memchr((char *)(uintptr_t)addr, 0, (uintptr_t)elf + mapsize - addr)) {
    return 0;
  }
  return (char *)(uintptr_t)addr;
}

Elf64_Phdr_ *GetElfProgramHeaderAddress(const Elf64_Ehdr_ *elf,  //
                                        size_t mapsize,          //
                                        u16 i) {                 //
  uint64_t off;
  if (i >= Read16(elf->phnum)) return 0;
  if (Read64(elf->phoff) <= 0) return 0;
  if (Read64(elf->phoff) >= mapsize) return 0;
  if (Read16(elf->phentsize) < sizeof(Elf64_Phdr_)) return 0;
  off = Read64(elf->phoff) + (unsigned)i * Read16(elf->phentsize);
  if (off > mapsize) return 0;
  return (Elf64_Phdr_ *)((char *)elf + off);
}

Elf64_Shdr_ *GetElfSectionHeaderAddress(const Elf64_Ehdr_ *elf,  //
                                        size_t mapsize,          //
                                        u16 i) {                 //
  uint64_t off;
  if (i >= Read16(elf->shnum)) return 0;
  if (Read64(elf->shoff) <= 0) return 0;
  if (Read64(elf->shoff) >= mapsize) return 0;
  if (Read16(elf->shentsize) < sizeof(Elf64_Shdr_)) return 0;
  off = Read64(elf->shoff) + (unsigned)i * Read16(elf->shentsize);
  if (off > mapsize) return 0;
  return (Elf64_Shdr_ *)((char *)elf + off);
}

// note: should not be used on bss section
void *GetElfSectionAddress(const Elf64_Ehdr_ *elf,     // validated
                           size_t mapsize,             // validated
                           const Elf64_Shdr_ *shdr) {  // foreign
  uint64_t last;
  if (!shdr) return 0;
  if (!Read64(shdr->size)) return 0;
  if (ckd_add(&last, Read64(shdr->offset), Read64(shdr->size))) return 0;
  if (last > mapsize) return 0;
  return (char *)elf + Read64(shdr->offset);
}

char *GetElfSectionNameStringTable(const Elf64_Ehdr_ *elf,  //
                                   size_t mapsize) {        //
  return (char *)GetElfSectionAddress(
      elf, mapsize,
      GetElfSectionHeaderAddress(elf, mapsize, Read16(elf->shstrndx)));
}

const char *GetElfSectionName(const Elf64_Ehdr_ *elf,  //
                              size_t mapsize,          //
                              Elf64_Shdr_ *shdr) {     //
  if (!shdr) return 0;
  return GetElfString(elf, mapsize, GetElfSectionNameStringTable(elf, mapsize),
                      Read32(shdr->name));
}

static char *GetElfStringTableImpl(const Elf64_Ehdr_ *elf,  //
                                   size_t mapsize,          //
                                   const char *kind) {      //
  int i;
  const char *name;
  Elf64_Shdr_ *shdr;
  for (i = 0; i < Read16(elf->shnum); ++i) {
    if ((shdr = GetElfSectionHeaderAddress(elf, mapsize, i)) &&
        Read32(shdr->type) == SHT_STRTAB_ &&
        (name = GetElfSectionName(elf, mapsize, shdr)) && !strcmp(name, kind)) {
      return (char *)GetElfSectionAddress(elf, mapsize, shdr);
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

static Elf64_Sym_ *GetElfSymbolTableImpl(const Elf64_Ehdr_ *elf,  //
                                         size_t mapsize,          //
                                         int *out_count,          //
                                         u32 kind) {              //
  int i;
  Elf64_Shdr_ *shdr;
  for (i = Read16(elf->shnum); i > 0; --i) {
    if ((shdr = GetElfSectionHeaderAddress(elf, mapsize, i - 1)) &&
        Read64(shdr->entsize) == sizeof(Elf64_Sym_) &&
        Read32(shdr->type) == kind) {
      if (out_count) {
        *out_count = Read64(shdr->size) / sizeof(Elf64_Sym_);
      }
      return (Elf64_Sym_ *)GetElfSectionAddress(elf, mapsize, shdr);
    }
  }
  return 0;
}

Elf64_Sym_ *GetElfSymbolTable(const Elf64_Ehdr_ *elf,  //
                              size_t mapsize,          //
                              int *out_count) {        //
  Elf64_Sym_ *res;
  if (!(res = GetElfSymbolTableImpl(elf, mapsize, out_count, SHT_SYMTAB_))) {
    res = GetElfSymbolTableImpl(elf, mapsize, out_count, SHT_DYNSYM_);
  }
  return res;
}
