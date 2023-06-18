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
#include <stdlib.h>
#include <string.h>

#include "blink/assert.h"
#include "blink/dis.h"
#include "blink/elf.h"
#include "blink/endian.h"
#include "blink/log.h"
#include "blink/util.h"

static int DisSymCompare(const void *p1, const void *p2) {
  const struct DisSym *a = (const struct DisSym *)p1;
  const struct DisSym *b = (const struct DisSym *)p2;
  if (a->addr != b->addr) {
    if (a->addr < b->addr) return -1;
    if (a->addr > b->addr) return +1;
  }
  if (a->rank != b->rank) {
    if (a->rank > b->rank) return -1;
    if (a->rank < b->rank) return +1;
  }
  if (a->unique != b->unique) {
    if (a->unique < b->unique) return -1;
    if (a->unique > b->unique) return +1;
  }
  return 0;
}

static void DisLoadElfLoads(struct Dis *d, Elf64_Ehdr_ *ehdr, size_t esize,
                            i64 eskew) {
  long i;
  Elf64_Phdr_ *phdr;
  for (i = 0; i < Read16(ehdr->phnum); ++i) {
    phdr = GetElfProgramHeaderAddress(ehdr, esize, i);
    if (Read32(phdr->type) != PT_LOAD_) continue;
    if (d->loads.i == d->loads.n) {
      d->loads.n += 2;
      d->loads.n += d->loads.n >> 1;
      unassert(d->loads.p = (struct DisLoad *)realloc(
                   d->loads.p, d->loads.n * sizeof(*d->loads.p)));
    }
    d->loads.p[d->loads.i].addr = Read64(phdr->vaddr) + eskew;
    d->loads.p[d->loads.i].size = Read64(phdr->memsz);
    d->loads.p[d->loads.i].istext = (Read32(phdr->flags) & PF_X_) == PF_X_;
    ++d->loads.i;
  }
}

static void DisLoadElfSyms(struct Dis *d, Elf64_Ehdr_ *ehdr, size_t esize,
                           i64 eskew) {
  int n;
  long i;
  char *stab;
  i64 stablen;
  const Elf64_Sym_ *st;
  bool isabs, isweak, islocal, isprotected, isfunc, isobject;
  if ((stab = GetElfStringTable(ehdr, esize))) {
    if ((st = GetElfSymbolTable(ehdr, esize, &n))) {
      stablen = (uintptr_t)ehdr + esize - (uintptr_t)stab;
      for (i = 0; i < n; ++i) {
        if (ELF64_ST_TYPE_(st[i].info) == STT_SECTION_ ||
            ELF64_ST_TYPE_(st[i].info) == STT_FILE_ || !Read32(st[i].name) ||
            startswith(stab + Read32(st[i].name), "v_") ||
            !(0 <= Read32(st[i].name) && Read32(st[i].name) < stablen) ||
            !Read64(st[i].value) ||
            !(-0x800000000000 <= (i64)Read64(st[i].value) &&
              (i64)Read64(st[i].value) < 0x800000000000)) {
          continue;
        }
        isabs = Read16(st[i].shndx) == SHN_ABS_;
        isweak = ELF64_ST_BIND_(st[i].info) == STB_WEAK_;
        islocal = ELF64_ST_BIND_(st[i].info) == STB_LOCAL_;
        isprotected = st[i].other == STV_PROTECTED_;
        isfunc = ELF64_ST_TYPE_(st[i].info) == STT_FUNC_;
        isobject = ELF64_ST_TYPE_(st[i].info) == STT_OBJECT_;
        if (d->syms.i == d->syms.n) {
          d->syms.n += 2;
          d->syms.n += d->syms.n >> 1;
          unassert(d->syms.p = (struct DisSym *)realloc(
                       d->syms.p, d->syms.n * sizeof(*d->syms.p)));
        }
        d->syms.p[d->syms.i].unique = i;
        d->syms.p[d->syms.i].size = Read64(st[i].size);
        unassert(d->syms.p[d->syms.i].name = strdup(stab + Read32(st[i].name)));
        d->syms.p[d->syms.i].addr = Read64(st[i].value) + eskew;
        d->syms.p[d->syms.i].rank =
            -islocal + -isweak + -isabs + isprotected + isobject + isfunc;
        d->syms.p[d->syms.i].iscode =
            DisIsText(d, Read64(st[i].value)) ? !isobject : isfunc;
        d->syms.p[d->syms.i].isabs = isabs;
        ++d->syms.i;
      }
    } else {
      LOGF("could not load elf symbol table");
    }
  } else {
    LOGF("could not load elf string table");
  }
}

static void DisSortSyms(struct Dis *d) {
  qsort(d->syms.p, d->syms.i, sizeof(struct DisSym), DisSymCompare);
}

bool DisIsProg(struct Dis *d, i64 addr) {
  long i;
  for (i = 0; i < d->loads.i; ++i) {
    if (addr >= d->loads.p[i].addr &&
        addr < d->loads.p[i].addr + d->loads.p[i].size) {
      return true;
    }
  }
  return false;
}

bool DisIsText(struct Dis *d, i64 addr) {
  long i;
  for (i = 0; i < d->loads.i; ++i) {
    if (addr >= d->loads.p[i].addr &&
        addr < d->loads.p[i].addr + d->loads.p[i].size) {
      return d->loads.p[i].istext;
    }
  }
  return false;
}

long DisFindSym(struct Dis *d, i64 addr) {
  long l, r, m;
  if (DisIsProg(d, addr)) {
    l = 0;
    r = d->syms.i;
    while (l < r) {
      m = (l + r) >> 1;
      if (d->syms.p[m].addr > addr) {
        r = m;
      } else {
        l = m + 1;
      }
    }
    // TODO(jart): This was <256 but that broke SectorLISP debugging
    //             Why did the Cosmo binbase bootloader need this?
    if (r && d->syms.p[r - 1].addr < 32) {
      return -1;
    }
    if (r && (addr == d->syms.p[r - 1].addr ||
              (addr > d->syms.p[r - 1].addr &&
               (addr <= d->syms.p[r - 1].addr + d->syms.p[r - 1].size ||
                !d->syms.p[r - 1].size)))) {
      return r - 1;
    }
  }
  return -1;
}

long DisFindSymByName(struct Dis *d, const char *s) {
  long i;
  for (i = 0; i < d->syms.i; ++i) {
    if (strcmp(s, d->syms.p[i].name) == 0) {
      return i;
    }
  }
  return -1;
}

void DisLoadElf(struct Dis *d, Elf64_Ehdr_ *ehdr, size_t esize, i64 eskew) {
  DisLoadElfLoads(d, ehdr, esize, eskew);
  DisLoadElfSyms(d, ehdr, esize, eskew);
  DisSortSyms(d);
}
