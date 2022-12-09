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
#include "blink/util.h"

#define READ64(p) Read64((const u8 *)(p))
#define READ32(p) Read32((const u8 *)(p))

static bool CouldJit(struct Machine *m) {
  return m->mode == XED_MODE_LONG;
}

static i64 LoadElfLoadSegment(struct Machine *m, void *image, size_t imagesize,
                              const Elf64_Phdr *phdr, i64 last_end) {
  u32 flags = Read32(phdr->p_flags);
  i64 vaddr = Read64(phdr->p_vaddr);
  i64 memsz = Read64(phdr->p_memsz);
  i64 offset = Read64(phdr->p_offset);
  i64 filesz = Read64(phdr->p_filesz);
  long pagesize = GetSystemPageSize();
  i64 start = ROUNDDOWN(vaddr, pagesize);
  i64 end = ROUNDUP(vaddr + memsz, pagesize);

  if (offset > imagesize) {
    LOGF("bad phdr offset");
    exit(127);
  }
  if (filesz > imagesize) {
    LOGF("bad phdr filesz");
    exit(127);
  }
  if (offset + filesz > imagesize) {
    LOGF("corrupt elf program header");
    exit(127);
  }
  if (end < last_end) {
    LOGF("program headers aren't ordered");
    exit(127);
  }
  if (memsz <= 0) {
    return last_end;
  }

  // on systems with a page size greater than the elf executable (e.g.
  // apple m1) it's possible for the second program header load to end
  // up overlapping the previous one.
  if (memsz > 0 && start < last_end) {
    start += last_end - start;
    memsz = end - start;
  }

  if (memsz > 0) {
    if (ReserveVirtual(m->system, start, end - start,
                       (flags & PF_R ? PAGE_U : 0) |
                           (flags & PF_W ? PAGE_RW : 0) |
                           (flags & PF_X ? 0 : PAGE_XD),
                       -1, false) == -1) {
      LOGF("failed to reserve virtual memory for elf program header");
      exit(127);
    }
  }

  CopyToUser(m, vaddr, (u8 *)image + offset, filesz);
  m->system->brk = MAX(m->system->brk, end);

  if ((flags & PF_X) && CouldJit(m)) {
    if (!m->system->codesize) {
      m->system->codestart = vaddr;
      m->system->codesize = memsz;
    } else if (vaddr == m->system->codestart + m->system->codesize) {
      m->system->codesize += memsz;
    } else {
      LOGF("elf has multiple executable program headers at noncontiguous "
           "addresses; only the first region will benefit from jitting");
    }
  }

  return end;
}

bool IsSupportedExecutable(const char *path, void *image) {
  Elf64_Ehdr *ehdr;
  if (READ32(image) == READ32("\177ELF")) {
    ehdr = (Elf64_Ehdr *)image;
    return Read16(ehdr->e_type) == ET_EXEC &&
           ehdr->e_ident[EI_CLASS] == ELFCLASS64 &&
           Read16(ehdr->e_machine) == EM_NEXGEN32E;
  }
  return READ64(image) == READ64("MZqFpD='") ||  //
         READ64(image) == READ64("jartsr='") ||  //
         endswith(path, ".bin");
}

static void LoadFlatExecutable(struct Machine *m, intptr_t base,
                               const char *prog, void *image,
                               size_t imagesize) {
  Elf64_Phdr phdr;
  Write32(phdr.p_type, PT_LOAD);
  Write32(phdr.p_flags, PF_X | PF_R | PF_W);
  Write64(phdr.p_offset, 0);
  Write64(phdr.p_vaddr, base);
  Write64(phdr.p_filesz, imagesize);
  Write64(phdr.p_memsz, ROUNDUP(imagesize + kRealSize, 4096));
  LoadElfLoadSegment(m, image, imagesize, &phdr, 0);
  m->ip = base;
}

static bool LoadElf(struct Machine *m, struct Elf *elf) {
  int i;
  Elf64_Phdr *phdr;
  i64 end = INT64_MIN;
  bool execstack = true;
  m->ip = elf->base = Read64(elf->ehdr->e_entry);
  for (i = 0; i < Read16(elf->ehdr->e_phnum); ++i) {
    phdr = GetElfSegmentHeaderAddress(elf->ehdr, elf->size, i);
    switch (Read32(phdr->p_type)) {
      case PT_LOAD:
        elf->base = MIN(elf->base, (i64)Read64(phdr->p_vaddr));
        end = LoadElfLoadSegment(m, elf->ehdr, elf->size, phdr, end);
        break;
      case PT_GNU_STACK:
        execstack = false;
        break;
      default:
        break;
    }
  }
  return execstack;
}

static void BootProgram(struct Machine *m, struct Elf *elf, size_t codesize) {
  m->cs = 0;
  m->ip = 0x7c00;
  elf->base = 0x7c00;
  memset(m->system->real, 0, 0x00f00000);
  Write16(m->system->real + 0x400, 0x3F8);
  Write16(m->system->real + 0x40E, 0xb0000 >> 4);
  Write16(m->system->real + 0x413, 0xb0000 / 1024);
  Write16(m->system->real + 0x44A, 80);
  Write64(m->dx, 0);
  memcpy(m->system->real + 0x7c00, elf->map, 512);
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
  size_t i, n;
  free(m->system->fun);
  if ((n = m->system->codesize)) {
    if ((m->system->fun =
             (_Atomic(nexgen32e_f) *)calloc(n, sizeof(nexgen32e_f)))) {
      for (i = 0; i < n; ++i) {
        atomic_store_explicit(m->system->fun + i, GeneralDispatch,
                              memory_order_relaxed);
      }
    } else {
      m->system->codestart = 0;
      m->system->codesize = 0;
    }
  } else {
    m->system->codestart = 0;
    m->system->fun = 0;
  }
  m->fun = m->system->fun - m->system->codestart;
  m->codestart = m->system->codestart;
  m->codesize = m->system->codesize;
}

void LoadProgram(struct Machine *m, char *prog, char **args, char **vars) {
  int fd;
  i64 sp;
  char ehdr[64];
  long pagesize;
  bool execstack;
  struct stat st;
  struct Elf *elf;
  unassert(prog);
  elf = &m->system->elf;
  elf->prog = prog;
  g_progname = prog;
  if ((fd = open(prog, O_RDONLY)) == -1 ||
      (fstat(fd, &st) == -1 || !st.st_size) ||
      (elf->map =
           (char *)Mmap(0, (elf->mapsize = st.st_size), PROT_READ | PROT_WRITE,
                        MAP_PRIVATE, fd, 0, "loader")) == MAP_FAILED) {
    WriteErrorString(prog);
    WriteErrorString(": ");
    WriteErrorString(strerror(errno));
    WriteErrorString("\n");
    exit(127);
  };
  unassert(!close(fd));
  if (!IsSupportedExecutable(prog, elf->map)) {
    WriteErrorString("\
error: unsupported executable; we need:\n\
- flat executables (.bin files)\n\
- actually portable executables (MZqFpD/jartsr)\n\
- statically-linked x86_64-linux elf executables\n");
    exit(127);
  }
  ResetCpu(m);
  m->system->codesize = 0;
  m->system->codestart = 0;
  m->system->brk = kMinBrk;
  if (m->mode == XED_MODE_REAL) {
    BootProgram(m, elf, elf->mapsize);
  } else {
    m->system->cr3 = AllocatePageTable(m->system);
    if (READ32(elf->map) == READ32("\177ELF")) {
      elf->ehdr = (Elf64_Ehdr *)elf->map;
      elf->size = elf->mapsize;
      execstack = LoadElf(m, elf);
    } else if (READ64(elf->map) == READ64("MZqFpD='") ||
               READ64(elf->map) == READ64("jartsr='")) {
      if (GetElfHeader(ehdr, prog, elf->map) == -1) exit(127);
      memcpy(elf->map, ehdr, 64);
      elf->ehdr = (Elf64_Ehdr *)elf->map;
      elf->size = elf->mapsize;
      execstack = LoadElf(m, elf);
    } else {
      elf->base = 0x400000;
      elf->ehdr = NULL;
      elf->size = 0;
      LoadFlatExecutable(m, elf->base, prog, elf->map, elf->mapsize);
      execstack = true;
    }
    sp = kStackTop;
    Put64(m->sp, sp);
    if (ReserveVirtual(m->system, sp - kStackSize, kStackSize,
                       PAGE_U | PAGE_RW | (execstack ? 0 : PAGE_XD), -1,
                       false) == -1) {
      LOGF("failed to reserve stack memory");
      exit(127);
    }
    LoadArgv(m, prog, args, vars);
  }
  SetupDispatch(m);
  pagesize = sysconf(_SC_PAGESIZE);
  pagesize = MAX(4096, pagesize);
  m->system->brk = ROUNDUP(m->system->brk, pagesize);
}
