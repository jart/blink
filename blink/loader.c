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
#include "blink/map.h"
#include "blink/memory.h"
#include "blink/util.h"

#define READ64(p) Read64((const u8 *)(p))
#define READ32(p) Read32((const u8 *)(p))

static void LoadElfLoadSegment(struct Machine *m, void *image, size_t imagesize,
                               const Elf64_Phdr *phdr, i64 *inout_mincode,
                               i64 *inout_maxcode, i64 *inout_brk) {

  u32 flags = Read32(phdr->p_flags);
  i64 vaddr = Read64(phdr->p_vaddr);
  i64 memsz = Read64(phdr->p_memsz);
  i64 offset = Read64(phdr->p_offset);
  i64 filesz = Read64(phdr->p_filesz);

  if (offset > imagesize) {
    LOGF("bad phdr offset");
    exit(200);
  }
  if (filesz > imagesize) {
    LOGF("bad phdr filesz");
    exit(200);
  }
  if (offset + filesz > imagesize) {
    LOGF("corrupt elf program header");
    exit(200);
  }

  *inout_brk = MAX(*inout_brk, ROUNDUP(vaddr + memsz, 4096));

  if (ReserveVirtual(m->system, ROUNDDOWN(vaddr, 4096),
                     ROUNDUP(vaddr + memsz, 4096) - ROUNDDOWN(vaddr, 4096),
                     PAGE_RSRV | PAGE_U | PAGE_RW | PAGE_V |
                         (flags & PF_X ? 0 : PAGE_XD)) == -1) {
    LOGF("failed to reserve virtual memory for elf program header");
    exit(200);
  }

  CopyToUser(m, vaddr, (u8 *)image + offset, filesz);

  if (flags & PF_X) {
    i64 mincode = vaddr;
    i64 maxcode = vaddr + memsz;
    if (!*inout_mincode) {
      *inout_mincode = mincode;
      *inout_maxcode = maxcode;
    } else {
      *inout_mincode = MIN(*inout_mincode, mincode);
      *inout_maxcode = MAX(*inout_maxcode, maxcode);
    }
  }
}

static void LoadElf(struct Machine *m, struct Elf *elf) {
  int i;
  i64 mincode = 0;
  i64 maxcode = 0;
  i64 brk = kMinBrk;
  Elf64_Phdr *phdr;
  if (elf->ehdr->e_ident[EI_CLASS] != ELFCLASS64 ||
      Read16(elf->ehdr->e_type) != ET_EXEC ||
      Read16(elf->ehdr->e_machine) != EM_NEXGEN32E) {
    WriteErrorString("error: not a statically-linked x86_64 executable\n");
    exit(200);
  }
  m->ip = elf->base = Read64(elf->ehdr->e_entry);
  for (i = 0; i < Read16(elf->ehdr->e_phnum); ++i) {
    phdr = GetElfSegmentHeaderAddress(elf->ehdr, elf->size, i);
    switch (Read32(phdr->p_type)) {
      case PT_LOAD:
        elf->base = MIN(elf->base, (i64)Read64(phdr->p_vaddr));
        LoadElfLoadSegment(m, elf->ehdr, elf->size, phdr, &mincode, &maxcode,
                           &brk);
        break;
      default:
        break;
    }
  }
  m->system->brk = brk;
  m->system->codestart = mincode;
  m->system->codesize = maxcode - mincode;
}

static void LoadBin(struct Machine *m, intptr_t base, const char *prog,
                    void *code, size_t codesize) {
  Elf64_Phdr phdr;
  i64 mincode = 0;
  i64 maxcode = 0;
  i64 brk = kMinBrk;
  Write32(phdr.p_type, PT_LOAD);
  Write32(phdr.p_flags, PF_X | PF_R | PF_W);
  Write64(phdr.p_offset, 0);
  Write64(phdr.p_vaddr, base);
  Write64(phdr.p_paddr, base);
  Write64(phdr.p_filesz, codesize);
  Write64(phdr.p_memsz, ROUNDUP(codesize + 4 * 1024 * 1024, 4 * 1024 * 1024));
  Write64(phdr.p_align, 4096);
  LoadElfLoadSegment(m, code, codesize, &phdr, &mincode, &maxcode, &brk);
  m->system->codesize = maxcode - mincode;
  m->system->codestart = mincode;
  m->system->brk = brk;
  m->ip = base;
}

static void BootProgram(struct Machine *m, struct Elf *elf, size_t codesize) {
  m->cs = 0;
  m->ip = 0x7c00;
  elf->base = 0x7c00;
  if (ReserveReal(m->system, 0x00f00000) == -1) {
    exit(201);
  }
  memset(m->system->real.p, 0, 0x00f00000);
  Write16(m->system->real.p + 0x400, 0x3F8);
  Write16(m->system->real.p + 0x40E, 0xb0000 >> 4);
  Write16(m->system->real.p + 0x413, 0xb0000 / 1024);
  Write16(m->system->real.p + 0x44A, 80);
  Write64(m->dx, 0);
  memcpy(m->system->real.p + 0x7c00, elf->map, 512);
  if (memcmp(elf->map, "\177ELF", 4) == 0) {
    elf->ehdr = (Elf64_Ehdr *)elf->map;
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

static void SetupDispatch(struct Machine *m) {
  size_t n;
  unsigned long i;
  free(m->system->fun);
  if ((n = m->system->codesize)) {
    unassert((m->system->fun =
                  (_Atomic(nexgen32e_f) *)calloc(n, sizeof(nexgen32e_f))));
    m->fun = m->system->fun - m->system->codestart;
    for (i = 0; i < n; ++i) {
      atomic_store_explicit(m->system->fun + i, GeneralDispatch,
                            memory_order_relaxed);
    }
  }
}

void LoadProgram(struct Machine *m, char *prog, char **args, char **vars) {
  int fd;
  i64 sp;
  char ehdr[64];
  struct stat st;
  struct Elf *elf;
  unassert(prog);
  elf = &m->system->elf;
  elf->prog = prog;
  g_progname = prog;
  if ((fd = open(prog, O_RDONLY)) == -1 ||
      (fstat(fd, &st) == -1 || !st.st_size)) {
    LOGF("%s: not found", prog);
    exit(201);
  }
  elf->mapsize = st.st_size;
  elf->map = (char *)Mmap(0, elf->mapsize, PROT_READ | PROT_WRITE, MAP_PRIVATE,
                          fd, 0, "loader");
  if (elf->map == MAP_FAILED) {
    LOGF("mmap failed: %s", strerror(errno));
    exit(200);
  }
  close(fd);
  ResetCpu(m);
  if (m->mode == XED_MODE_REAL) {
    BootProgram(m, elf, elf->mapsize);
  } else {
    sp = kStackTop;
    Write64(m->sp, sp);
    m->system->cr3 = AllocateLinearPage(m->system);
    if (ReserveVirtual(m->system, sp - kStackSize, kStackSize,
                       PAGE_RSRV | PAGE_U | PAGE_RW | PAGE_V) == -1) {
      LOGF("ReserveVirtual failed");
      exit(200);
    }
    LoadArgv(m, prog, args, vars);
    if (READ32(elf->map) == READ32("\177ELF")) {
      elf->ehdr = (Elf64_Ehdr *)elf->map;
      elf->size = elf->mapsize;
      LoadElf(m, elf);
    } else if ((READ64(elf->map) == READ64("MZqFpD='") ||
                READ64(elf->map) == READ64("jartsr='")) &&
               !GetElfHeader(ehdr, prog, elf->map)) {
      memcpy(elf->map, ehdr, 64);
      elf->ehdr = (Elf64_Ehdr *)elf->map;
      elf->size = elf->mapsize;
      LoadElf(m, elf);
    } else {
      elf->base = 0x400000;
      elf->ehdr = NULL;
      elf->size = 0;
      LoadBin(m, elf->base, prog, elf->map, elf->mapsize);
    }
  }
  SetupDispatch(m);
}
