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
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/endian.h"
#include "blink/loader.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/memory.h"
#include "blink/util.h"

#define READ64(p) Read64((const uint8_t *)(p))
#define READ32(p) Read32((const uint8_t *)(p))

static void LoadElfLoadSegment(struct Machine *m, void *code, size_t codesize,
                               Elf64_Phdr *phdr) {
  int64_t align, bsssize;
  int64_t felf, fstart, fend, vstart, vbss, vend;
  align = MAX(Read64(phdr->p_align), 4096);
  unassert(0 == (Read64(phdr->p_vaddr) - Read64(phdr->p_offset)) % align);
  felf = (int64_t)(intptr_t)code;
  vstart = ROUNDDOWN(Read64(phdr->p_vaddr), align);
  vbss = ROUNDUP(Read64(phdr->p_vaddr) + Read64(phdr->p_filesz), align);
  vend = ROUNDUP(Read64(phdr->p_vaddr) + Read64(phdr->p_memsz), align);
  fstart = felf + ROUNDDOWN(Read64(phdr->p_offset), align);
  fend = felf + Read64(phdr->p_offset) + Read64(phdr->p_filesz);
  bsssize = vend - vbss;
  m->system->brk = MAX(m->system->brk, vend);
  unassert(vend >= vstart);
  unassert(fend >= fstart);
  unassert(felf <= fstart);
  unassert(vstart >= -0x800000000000);
  unassert(vend <= 0x800000000000);
  unassert(vend - vstart >= fstart - fend);
  unassert(Read64(phdr->p_filesz) <= Read64(phdr->p_memsz));
  unassert(felf + Read64(phdr->p_offset) - fstart ==
           Read64(phdr->p_vaddr) - vstart);
  if (ReserveVirtual(m, vstart, fend - fstart, 0x0207) == -1) {
    LOGF("ReserveVirtual failed");
    exit(200);
  }
  VirtualRecv(m, vstart, (void *)(uintptr_t)fstart, fend - fstart);
  if (bsssize) {
    if (ReserveVirtual(m, vbss, bsssize, 0x0207) == -1) {
      LOGF("ReserveVirtual failed");
      exit(200);
    }
  }
  if (Read64(phdr->p_memsz) - Read64(phdr->p_filesz) > bsssize) {
    VirtualSet(m, Read64(phdr->p_vaddr) + Read64(phdr->p_filesz), 0,
               Read64(phdr->p_memsz) - Read64(phdr->p_filesz) - bsssize);
  }
}

static void LoadElf(struct Machine *m, struct Elf *elf) {
  unsigned i;
  Elf64_Phdr *phdr;
  m->ip = elf->base = Read64(elf->ehdr->e_entry);
  for (i = 0; i < Read16(elf->ehdr->e_phnum); ++i) {
    phdr = GetElfSegmentHeaderAddress(elf->ehdr, elf->size, i);
    switch (Read32(phdr->p_type)) {
      case PT_LOAD:
        elf->base = MIN(elf->base, Read64(phdr->p_vaddr));
        LoadElfLoadSegment(m, elf->ehdr, elf->size, phdr);
        break;
      default:
        break;
    }
  }
}

static void LoadBin(struct Machine *m, intptr_t base, const char *prog,
                    void *code, size_t codesize) {
  Elf64_Phdr phdr;
  Write32(phdr.p_type, PT_LOAD);
  Write32(phdr.p_flags, PF_X | PF_R | PF_W);
  Write64(phdr.p_offset, 0);
  Write64(phdr.p_vaddr, base);
  Write64(phdr.p_paddr, base);
  Write64(phdr.p_filesz, codesize);
  Write64(phdr.p_memsz, ROUNDUP(codesize + 4 * 1024 * 1024, 4 * 1024 * 1024));
  Write64(phdr.p_align, 4096);
  LoadElfLoadSegment(m, code, codesize, &phdr);
  m->ip = base;
}

static void BootProgram(struct Machine *m, struct Elf *elf, size_t codesize) {
  m->ip = 0x7c00;
  elf->base = 0x7c00;
  if (ReserveReal(m, 0x00f00000) == -1) {
    exit(201);
  }
  memset(m->system->real.p, 0, 0x00f00000);
  Write16(m->system->real.p + 0x400, 0x3F8);
  Write16(m->system->real.p + 0x40E, 0xb0000 >> 4);
  Write16(m->system->real.p + 0x413, 0xb0000 / 1024);
  Write16(m->system->real.p + 0x44A, 80);
  Write64(m->cs, 0);
  Write64(m->dx, 0);
  memcpy(m->system->real.p + 0x7c00, elf->map, 512);
  if (memcmp(elf->map, "\177ELF", 4) == 0) {
    elf->ehdr = (void *)elf->map;
    elf->size = codesize;
    elf->base = Read64(elf->ehdr->e_entry);
  } else {
    elf->base = 0x7c00;
    elf->ehdr = NULL;
    elf->size = 0;
  }
}

static int GetElfHeader(char ehdr[64], const char *prog, const char *image) {
  int c, i;
  const char *p;
  for (p = image; p < image + 4096; ++p) {
    if (READ64(p) != READ64("printf '")) {
      continue;
    }
    for (i = 0, p += 8; p + 3 < image + 4096 && (c = *p++) != '\'';) {
      if (c == '\\') {
        if ('0' <= *p && *p <= '7') {
          c = *p++ - '0';
          if ('0' <= *p && *p <= '7') {
            c *= 8;
            c += *p++ - '0';
            if ('0' <= *p && *p <= '7') {
              c *= 8;
              c += *p++ - '0';
            }
          }
        }
      }
      if (i < 64) {
        ehdr[i++] = c;
      } else {
        LOGF("%s: ape printf elf header too long\n", prog);
        return -1;
      }
    }
    if (i != 64) {
      LOGF("%s: ape printf elf header too short\n", prog);
      return -1;
    }
    if (READ32(ehdr) != READ32("\177ELF")) {
      LOGF("%s: ape printf elf header didn't have elf magic\n", prog);
      return -1;
    }
    return 0;
  }
  LOGF("%s: printf statement not found in first 4096 bytes\n", prog);
  return -1;
}

void LoadProgram(struct Machine *m, char *prog, char **args, char **vars,
                 struct Elf *elf) {
  int fd;
  int64_t sp;
  char ehdr[64];
  struct stat st;
  unassert(prog);
  elf->prog = prog;
  if ((fd = open(prog, O_RDONLY)) == -1 ||
      (fstat(fd, &st) == -1 || !st.st_size)) {
    LOGF("%s: not found", prog);
    exit(201);
  }
  elf->mapsize = st.st_size;
  elf->map = mmap(0, elf->mapsize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (elf->map == MAP_FAILED) {
    LOGF("mmap failed: %s", strerror(errno));
    exit(200);
  }
  close(fd);
  ResetCpu(m);
  if ((m->mode & 3) == XED_MODE_REAL) {
    BootProgram(m, elf, elf->mapsize);
  } else {
    sp = 0x800000000000;
    Write64(m->sp, sp);
    m->system->cr3 = AllocateLinearPage(m);
    if (ReserveVirtual(m, sp - 0x100000, 0x100000, 0x0207) == -1) {
      LOGF("ReserveVirtual failed");
      exit(200);
    }
    LoadArgv(m, prog, args, vars);
    if (memcmp(elf->map, "\177ELF", 4) == 0) {
      elf->ehdr = (void *)elf->map;
      elf->size = elf->mapsize;
      LoadElf(m, elf);
    } else if ((READ64(elf->map) == READ64("MZqFpD='") ||
                READ64(elf->map) == READ64("jartsr='")) &&
               !GetElfHeader(ehdr, prog, elf->map)) {
      memcpy(elf->map, ehdr, 64);
      elf->ehdr = (void *)elf->map;
      elf->size = elf->mapsize;
      LoadElf(m, elf);
    } else {
      elf->base = 0x400000;
      elf->ehdr = NULL;
      elf->size = 0;
      LoadBin(m, elf->base, prog, elf->map, elf->mapsize);
    }
  }
}
