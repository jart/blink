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

#include "blink/dis.h"
#include "blink/elf.h"
#include "blink/endian.h"
#include "blink/util.h"

bool g_disisprog_disable;

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

static void DisLoadElfLoads(struct Dis *d, struct Elf *elf) {
  long i, j, n;
  Elf64_Phdr_ *phdr;
  j = 0;
  n = Read16(elf->ehdr->phnum);
  if (d->loads.n < n) {
    d->loads.n = n;
    d->loads.p =
        (struct DisLoad *)realloc(d->loads.p, d->loads.n * sizeof(*d->loads.p));
  }
  for (i = 0; i < n; ++i) {
    phdr = GetElfSegmentHeaderAddress(elf->ehdr, elf->size, i);
    if (Read32(phdr->type) != PT_LOAD_) continue;
    d->loads.p[j].addr = Read64(phdr->vaddr) + elf->aslr;
    d->loads.p[j].size = Read64(phdr->memsz);
    d->loads.p[j].istext = (Read32(phdr->flags) & PF_X_) == PF_X_;
    ++j;
  }
  d->loads.i = j;
}

static void DisLoadElfSyms(struct Dis *d, struct Elf *elf) {
  int n;
  long i, j;
  i64 stablen;
  const Elf64_Sym_ *st;
  bool isabs, isweak, islocal, isprotected, isfunc, isobject;
  j = 0;
  if ((d->syms.stab = GetElfStringTable(elf->ehdr, elf->size))) {
    if ((st = GetElfSymbolTable(elf->ehdr, elf->size, &n))) {
      stablen = (intptr_t)elf->ehdr + elf->size - (intptr_t)d->syms.stab;
      if (d->syms.n < n) {
        d->syms.n = n;
        d->syms.p =
            (struct DisSym *)realloc(d->syms.p, d->syms.n * sizeof(*d->syms.p));
      }
      for (i = 0; i < n; ++i) {
        if (ELF64_ST_TYPE_(st[i].info) == STT_SECTION_ ||
            ELF64_ST_TYPE_(st[i].info) == STT_FILE_ || !Read32(st[i].name) ||
            startswith(d->syms.stab + Read32(st[i].name), "v_") ||
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
        d->syms.p[j].unique = i;
        d->syms.p[j].size = Read64(st[i].size);
        d->syms.p[j].name = Read32(st[i].name);
        d->syms.p[j].addr = Read64(st[i].value) + elf->aslr;
        ELF_LOGF("SYMBOL %" PRIx64 " %" PRIx64 " %s", elf->aslr,
                 d->syms.p[j].addr, d->syms.stab + d->syms.p[j].name);
        d->syms.p[j].rank =
            -islocal + -isweak + -isabs + isprotected + isobject + isfunc;
        d->syms.p[j].iscode =
            DisIsText(d, Read64(st[i].value)) ? !isobject : isfunc;
        d->syms.p[j].isabs = isabs;
        ++j;
      }
    } else {
      LOGF("could not load elf symbol table");
    }
  } else {
    LOGF("could not load elf string table");
  }
  d->syms.i = j;
}

static void DisSortSyms(struct Dis *d) {
  long i;
  qsort(d->syms.p, d->syms.i, sizeof(struct DisSym), DisSymCompare);
  for (i = 0; i < d->syms.i; ++i) {
    if (!strcmp("_end", d->syms.stab + d->syms.p[i].name)) {
      d->syms.i = i;
      break;
    }
  }
}

bool DisIsProg(struct Dis *d, i64 addr) {
  long i;
  if (g_disisprog_disable) return true;
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
    if (strcmp(s, d->syms.stab + d->syms.p[i].name) == 0) {
      return i;
    }
  }
  return -1;
}

void DisLoadElf(struct Dis *d, struct Elf *elf) {
  if (!elf || !elf->ehdr) return;
  DisLoadElfLoads(d, elf);
  DisLoadElfSyms(d, elf);
  DisSortSyms(d);
}
