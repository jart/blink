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
#include "blink/builtin.h"
#include "blink/end.h"
#include "blink/endian.h"
#include "blink/loader.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/map.h"
#include "blink/util.h"

#define READ64(p) Read64((const u8 *)(p))
#define READ32(p) Read32((const u8 *)(p))

static i64 LoadElfLoadSegment(struct Machine *m, void *image, size_t imagesize,
                              const Elf64_Phdr_ *phdr, i64 last_end, int fd) {
  i64 bulk;
  struct System *s = m->system;
  u32 flags = Read32(phdr->p_flags);
  i64 vaddr = Read64(phdr->p_vaddr);
  i64 memsz = Read64(phdr->p_memsz);
  i64 offset = Read64(phdr->p_offset);
  i64 filesz = Read64(phdr->p_filesz);
  long pagesize = HasLinearMapping(m) ? GetSystemPageSize() : 4096;
  i64 start = ROUNDDOWN(vaddr, pagesize);
  i64 end = ROUNDUP(vaddr + memsz, pagesize);
  long skew = vaddr & (pagesize - 1);
  u64 key = (flags & PF_R_ ? PAGE_U : 0) |   //
            (flags & PF_W_ ? PAGE_RW : 0) |  //
            (flags & PF_X_ ? 0 : PAGE_XD);

  ELF_LOGF("PROGRAM HEADER");
  ELF_LOGF("  vaddr = %" PRIx64, vaddr);
  ELF_LOGF("  memsz = %" PRIx64, memsz);
  ELF_LOGF("  offset = %" PRIx64, offset);
  ELF_LOGF("  filesz = %" PRIx64, filesz);
  ELF_LOGF("  pagesize = %" PRIx64, pagesize);
  ELF_LOGF("  start = %" PRIx64, start);
  ELF_LOGF("  end = %" PRIx64, end);
  ELF_LOGF("  skew = %lx", skew);

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
  if (skew != (offset & (pagesize - 1))) {
    LOGF("p_vaddr p_offset skew unequal w.r.t. page size; try either "
         "(1) rebuilding your program using the linker flags: -static "
         "-Wl,-z,common-page-size=%ld,-z,max-page-size=%ld or (2) "
         "using `blink -m` to disable the linear memory optimization",
         pagesize, pagesize);
    exit(127);
  }

  // on systems with a page size greater than the elf executable (e.g.
  // apple m1) it's possible for the second program header load to end
  // up overlapping the previous one.
  if (start < last_end) {
    start += last_end - start;
  }
  if (start >= end) {
    return last_end;
  }

  // mmap() always returns an address that page-size aligned, but elf
  // program headers can start at any address. therefore the first page
  // needs to be loaded with special care, if the phdr is skewed. the
  // only real rule in this situation is that the skew of of the virtual
  // address and the file offset need to be the same.
  if (skew) {
    if (vaddr > start && start + pagesize <= end) {
      unassert(vaddr < start + pagesize);
      ELF_LOGF("alloc %" PRIx64 "-%" PRIx64, start, start + pagesize);
      if (ReserveVirtual(s, start, pagesize, key, -1, 0, 0) == -1) {
        LOGF("failed to allocate program header skew");
        exit(127);
      }
      start += pagesize;
    }
    ELF_LOGF("copy %" PRIx64 "-%" PRIx64 " from %" PRIx64 "-%" PRIx64, vaddr,
             vaddr + (pagesize - skew), offset,
             offset + MIN(filesz, pagesize - skew));
    CopyToUser(m, vaddr, (u8 *)image + offset, MIN(filesz, pagesize - skew));
    vaddr += pagesize - skew;
    offset += pagesize - skew;
    filesz -= MIN(filesz, pagesize - skew);
  }

  // load the aligned program header.
  unassert(start <= end);
  unassert(vaddr == start);
  unassert(vaddr + filesz <= end);
  unassert(!(vaddr & (pagesize - 1)));
  unassert(!(start & (pagesize - 1)));
  unassert(!(offset & (pagesize - 1)));
  if (start < end) {
    bulk = ROUNDDOWN(filesz, pagesize);
    unassert(bulk >= 0);
    if (bulk) {
      // map the bulk of .text directly into memory without copying.
      ELF_LOGF("load %" PRIx64 "-%" PRIx64 " from %" PRIx64 "-%" PRIx64, start,
               start + bulk, offset, offset + bulk);
      if (ReserveVirtual(s, start, bulk, key, fd, offset, 0) == -1) {
        LOGF("failed to map elf program header file data");
        exit(127);
      }
    }
    start += bulk;
    offset += bulk;
    filesz -= bulk;
    if (start < end) {
      // allocate .bss zero-initialized memory.
      ELF_LOGF("alloc %" PRIx64 "-%" PRIx64, start, end);
      if (ReserveVirtual(s, start, end - start, key, -1, 0, 0) == -1) {
        LOGF("failed to allocate program header bss");
        exit(127);
      }
      // copy the tail skew.
      if (filesz) {
        ELF_LOGF("copy %" PRIx64 "-%" PRIx64 " from %" PRIx64 "-%" PRIx64,
                 start, start + filesz, offset, offset + filesz);
        CopyToUser(m, start, (u8 *)image + offset, filesz);
      }
    } else {
      unassert(!filesz);
    }
  }

  s->brk = MAX(s->brk, end);

  if (flags & PF_X_) {
    if (!s->codesize) {
      s->codestart = vaddr;
      s->codesize = memsz;
    } else if (vaddr == s->codestart + s->codesize) {
      s->codesize += memsz;
    } else {
      LOGF("elf has multiple executable program headers at noncontiguous "
           "addresses; only the first region will benefit from jitting");
    }
  }

  return end;
}

bool IsSupportedExecutable(const char *path, void *image) {
  Elf64_Ehdr_ *ehdr;
  if (READ32(image) == READ32("\177ELF")) {
    ehdr = (Elf64_Ehdr_ *)image;
    return Read16(ehdr->e_type) == ET_EXEC_ &&
           ehdr->e_ident[EI_CLASS_] == ELFCLASS64_ &&
           Read16(ehdr->e_machine) == EM_NEXGEN32E_;
  }
  return READ64(image) == READ64("MZqFpD='") ||  //
         READ64(image) == READ64("jartsr='") ||  //
         endswith(path, ".bin");
}

static void LoadFlatExecutable(struct Machine *m, intptr_t base,
                               const char *prog, void *image, size_t imagesize,
                               int fd) {
  Elf64_Phdr_ phdr;
  Write32(phdr.p_type, PT_LOAD_);
  Write32(phdr.p_flags, PF_X_ | PF_R_ | PF_W_);
  Write64(phdr.p_offset, 0);
  Write64(phdr.p_vaddr, base);
  Write64(phdr.p_filesz, imagesize);
  Write64(phdr.p_memsz, ROUNDUP(imagesize + kRealSize, 4096));
  LoadElfLoadSegment(m, image, imagesize, &phdr, 0, fd);
  m->ip = base;
}

static bool LoadElf(struct Machine *m, struct Elf *elf, int fd) {
  int i;
  Elf64_Phdr_ *phdr;
  i64 end = INT64_MIN;
  bool execstack = true;
  m->ip = elf->base = Read64(elf->ehdr->e_entry);
  for (i = 0; i < Read16(elf->ehdr->e_phnum); ++i) {
    phdr = GetElfSegmentHeaderAddress(elf->ehdr, elf->size, i);
    switch (Read32(phdr->p_type)) {
      case PT_LOAD_:
        elf->base = MIN(elf->base, (i64)Read64(phdr->p_vaddr));
        end = LoadElfLoadSegment(m, elf->ehdr, elf->size, phdr, end, fd);
        break;
      case PT_GNU_STACK_:
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
    elf->ehdr = (Elf64_Ehdr_ *)elf->map;
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
#ifdef HAVE_JIT
  size_t n;
  free(m->system->fun);
  if (m->mode != XED_MODE_LONG ||        //
      IsJitDisabled(&m->system->jit) ||  //
      !(n = m->system->codesize) ||      //
      !(m->system->fun = (atomic_int *)AllocateBig(
            n * sizeof(atomic_int), PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0))) {
    m->system->fun = 0;
  }
  if (m->system->fun) {
    m->fun = m->system->fun - m->system->codestart;
    m->codestart = m->system->codestart;
    m->codesize = m->system->codesize;
  } else {
    m->fun = 0;
    m->codestart = 0;
    m->codesize = 0;
    m->system->codesize = 0;
    m->system->codestart = 0;
  }
#endif
}

void LoadProgram(struct Machine *m, char *prog, char **args, char **vars) {
  int fd;
  i64 sp;
  char ibuf[21];
  char ehdr[64];
  long pagesize;
  bool execstack;
  struct stat st;
  struct Elf *elf;
  unassert(prog);
  elf = &m->system->elf;
  elf->prog = prog;
  free(g_progname);
  g_progname = strdup(prog);
  if ((fd = open(prog, O_RDONLY)) == -1 ||
      (fstat(fd, &st) == -1 || !st.st_size) ||
      (elf->map =
           (char *)Mmap(0, (elf->mapsize = st.st_size), PROT_READ | PROT_WRITE,
                        MAP_PRIVATE, fd, 0, "loader")) == MAP_FAILED) {
    WriteErrorString(prog);
    WriteErrorString(": failed to load (errno ");
    FormatInt64(ibuf, errno);
    WriteErrorString(ibuf);
    WriteErrorString(")\n");
    exit(127);
  };
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
      elf->ehdr = (Elf64_Ehdr_ *)elf->map;
      elf->size = elf->mapsize;
      execstack = LoadElf(m, elf, fd);
    } else if (READ64(elf->map) == READ64("MZqFpD='") ||
               READ64(elf->map) == READ64("jartsr='")) {
      if (GetElfHeader(ehdr, prog, elf->map) == -1) exit(127);
      memcpy(elf->map, ehdr, 64);
      elf->ehdr = (Elf64_Ehdr_ *)elf->map;
      elf->size = elf->mapsize;
      execstack = LoadElf(m, elf, fd);
    } else {
      elf->base = 0x400000;
      elf->ehdr = NULL;
      elf->size = 0;
      LoadFlatExecutable(m, elf->base, prog, elf->map, elf->mapsize, fd);
      execstack = true;
    }
    sp = kStackTop;
    Put64(m->sp, sp);
    if (ReserveVirtual(m->system, sp - kStackSize, kStackSize,
                       PAGE_U | PAGE_RW | (execstack ? 0 : PAGE_XD), -1, 0,
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
  unassert(!close(fd));
}
