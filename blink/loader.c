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
#include "blink/overlays.h"
#include "blink/random.h"
#include "blink/tunables.h"
#include "blink/util.h"
#include "blink/x86.h"

#ifdef DISABLE_OVERLAYS
#define OverlaysOpen   openat
#define OverlaysAccess faccessat
#endif

#define READ64(p) Read64((const u8 *)(p))
#define READ32(p) Read32((const u8 *)(p))

static bool CanEmulateImpl(struct Machine *, char **, char ***, bool);

static dontdiscard bool Add(i64 x, i64 y, i64 *z) {
  u64 a, b, c;
  a = x, b = y, c = a + b;
  if (((c ^ a) & (c ^ b)) >> 63) return true;
  return *z = c, false;
}

static dontdiscard bool Sub(i64 x, i64 y, i64 *z) {
  u64 a, b, c;
  a = x, b = y, c = a - b;
  if (((c ^ a) & (a ^ b)) >> 63) return true;
  return *z = c, false;
}

static i64 LoadElfLoadSegment(struct Machine *m, const char *path, void *image,
                              size_t imagesize, const Elf64_Phdr_ *phdr,
                              i64 last_end, u64 aslr, int fd) {
  i64 bulk;
  struct System *s = m->system;
  u32 flags = Read32(phdr->flags);
  i64 vaddr = Read64(phdr->vaddr) + aslr;
  i64 memsz = Read64(phdr->memsz);
  i64 offset = Read64(phdr->offset);
  i64 filesz = Read64(phdr->filesz);
  long pagesize = HasLinearMapping(m) ? GetSystemPageSize() : 4096;
  i64 start = ROUNDDOWN(vaddr, pagesize);
  i64 end = ROUNDUP(vaddr + memsz, pagesize);
  long skew = vaddr & (pagesize - 1);
  u64 key = (flags & PF_R_ ? PAGE_U : 0) |   //
            (flags & PF_W_ ? PAGE_RW : 0) |  //
            (flags & PF_X_ ? 0 : PAGE_XD);

  SYS_LOGF("PT_LOAD %c%c%c [%" PRIx64 ",%" PRIx64 ") %s",  //
           (flags & PF_R_ ? 'R' : '.'),                    //
           (flags & PF_W_ ? 'W' : '.'),                    //
           (flags & PF_X_ ? 'X' : '.'),                    //
           vaddr, vaddr + memsz, path);

  ELF_LOGF("PROGRAM HEADER");
  ELF_LOGF("  aslr = %" PRIx64, aslr);
  ELF_LOGF("  flags = %s%s%s",          //
           (flags & PF_R_ ? "R" : ""),  //
           (flags & PF_W_ ? "W" : ""),  //
           (flags & PF_X_ ? "X" : ""));
  ELF_LOGF("  vaddr = %" PRIx64, vaddr);
  ELF_LOGF("  memsz = %" PRIx64, memsz);
  ELF_LOGF("  offset = %" PRIx64, offset);
  ELF_LOGF("  filesz = %" PRIx64, filesz);
  ELF_LOGF("  pagesize = %ld", pagesize);
  ELF_LOGF("  start = %" PRIx64, start);
  ELF_LOGF("  end = %" PRIx64, end);
  ELF_LOGF("  skew = %lx", skew);

  if (offset > imagesize) {
    ERRF("bad phdr offset");
    exit(127);
  }
  if (filesz > imagesize) {
    ERRF("bad phdr filesz");
    exit(127);
  }
  if (offset + filesz > imagesize) {
    ERRF("corrupt elf program header");
    exit(127);
  }
  if (end < last_end) {
    ERRF("program headers aren't ordered, expected %" PRIx64 " >= %" PRIx64,
         end, last_end);
    exit(127);
  }
  if (skew != (offset & (pagesize - 1))) {
    WriteErrorString(
        "p_vaddr p_offset skew unequal w.r.t. page size; try either "
        "(1) rebuilding your program using the linker flags: -static "
        "-Wl,-z,common-page-size=65536,-z,max-page-size=65536 or (2) "
        "using `blink -m` to disable the linear memory optimization\n");
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
      if (ReserveVirtual(s, start, pagesize, key, -1, 0, 0, 0) == -1) {
        ERRF("failed to allocate program header skew");
        exit(127);
      }
      start += pagesize;
    }
    ELF_LOGF("copy %" PRIx64 "-%" PRIx64 " from %" PRIx64 "-%" PRIx64, vaddr,
             vaddr + (pagesize - skew), offset,
             offset + MIN(filesz, pagesize - skew));
    unassert(!CopyToUser(m, vaddr, (u8 *)image + offset,
                         MIN(filesz, pagesize - skew)));
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
      if (ReserveVirtual(s, start, bulk, key, fd, offset, 0, 0) == -1) {
        ERRF("failed to map elf program header file data");
        exit(127);
      }
      unassert(AddFileMap(m->system, start, bulk, path, offset));
    }
    start += bulk;
    offset += bulk;
    filesz -= bulk;
    if (start < end) {
      // allocate .bss zero-initialized memory.
      ELF_LOGF("alloc %" PRIx64 "-%" PRIx64, start, end);
      if (ReserveVirtual(s, start, end - start, key, -1, 0, 0, 0) == -1) {
        ERRF("failed to allocate program header bss");
        exit(127);
      }
      // copy the tail skew.
      if (filesz) {
        ELF_LOGF("copy %" PRIx64 "-%" PRIx64 " from %" PRIx64 "-%" PRIx64,
                 start, start + filesz, offset, offset + filesz);
        unassert(!CopyToUser(m, start, (u8 *)image + offset, filesz));
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
    }
  }

  return end;
}

static bool IsFreebsdExecutable(Elf64_Ehdr_ *ehdr, size_t size) {
  // APE uses the FreeBSD OS ABI too, but never with ET_DYN
  return Read16(ehdr->type) == ET_DYN_ &&
         ehdr->ident[EI_OSABI_] == ELFOSABI_FREEBSD_;
}

static i64 GetElfMemorySize(const Elf64_Ehdr_ *ehdr, size_t size, i64 *base) {
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
        if (Add(x, Read64(phdr->memsz), &y)) return -1;
        lo = MIN(x, lo);
        hi = MAX(y, hi);
      }
    }
  }
  if (Sub(hi, lo, &res)) return -1;
  *base = lo;
  return res;
}

static bool IsOpenbsdExecutable(struct Elf64_Ehdr_ *ehdr, size_t size) {
  size_t off;
  unsigned i;
  Elf64_Phdr_ *phdr;
  if (Read64(ehdr->phoff) < size) {
    for (i = 0; i < Read16(ehdr->phnum); ++i) {
      off = Read64(ehdr->phoff) + Read16(ehdr->phentsize) * i;
      if (off + Read16(ehdr->phentsize) > size) {
        return false;
      }
      phdr = (Elf64_Phdr_ *)((u8 *)ehdr + off);
      if (Read32(phdr->type) == PT_OPENBSD_RANDOMIZE_) {
        return true;
      }
    }
  }
  return false;
}

static bool IsHaikuExecutable(Elf64_Ehdr_ *ehdr, size_t size) {
#ifdef __HAIKU__
  int i, n;
  bool res = false;
  const char *stab;
  const Elf64_Sym_ *st;
  if ((stab = GetElfStringTable(ehdr, size)) &&
      (st = GetElfSymbolTable(ehdr, size, &n))) {
    for (i = 0; i < n; ++i) {
      if (ELF64_ST_TYPE_(st[i].info) == STT_OBJECT_ &&
          !strcmp(stab + Read32(st[i].name), "_gSharedObjectHaikuVersion")) {
        res = true;
        break;
      }
    }
  }
  return res;
#else
  return false;
#endif
}

bool IsSupportedExecutable(const char *path, void *image, size_t size) {
  bool res;
  Elf64_Ehdr_ *ehdr;
  if (size >= sizeof(Elf64_Ehdr_) && READ32(image) == READ32("\177ELF")) {
    ehdr = (Elf64_Ehdr_ *)image;
    res = (Read16(ehdr->type) == ET_EXEC_ ||  //
           Read16(ehdr->type) == ET_DYN_) &&
          ehdr->ident[EI_CLASS_] == ELFCLASS64_ &&
          Read16(ehdr->machine) == EM_NEXGEN32E_ &&
          !IsFreebsdExecutable(ehdr, size) &&  //
          !IsOpenbsdExecutable(ehdr, size) &&  //
          !IsHaikuExecutable(ehdr, size);
#if defined(__ELF__) && !defined(__linux)
    if (res) LOGF("blink believes %s is an x86_64-linux executable", path);
#endif
    return res;
  }
  return (size >= 4096 && (READ64(image) == READ64("MZqFpD='") ||    //
                           READ64(image) == READ64("jartsr='"))) ||  //
         endswith(path, ".bin");
}

static void LoadFlatExecutable(struct Machine *m, intptr_t base,
                               const char *prog, void *image, size_t imagesize,
                               int fd) {
  Elf64_Phdr_ phdr;
  Write32(phdr.type, PT_LOAD_);
  Write32(phdr.flags, PF_X_ | PF_R_ | PF_W_);
  Write64(phdr.offset, 0);
  Write64(phdr.vaddr, base);
  Write64(phdr.filesz, imagesize);
  Write64(phdr.memsz, ROUNDUP(imagesize + kRealSize, 4096));
  LoadElfLoadSegment(m, prog, image, imagesize, &phdr, 0, 0, fd);
  m->ip = base;
}

static i64 ChooseAslr(const Elf64_Ehdr_ *ehdr, size_t size, i64 dflt,
                      i64 *base) {
  i64 aslr;
  if (GetElfMemorySize(ehdr, size, base) <= 0) {
    ERRF("couldn't determine boundaries of loaded executable");
    exit(127);
  }
  if (Read16(ehdr->type) == ET_DYN_ && !*base) {
    aslr = dflt;
    ELF_LOGF("choosing base skew %" PRIx64 " because dynamic", aslr);
  } else {
    aslr = 0;
    ELF_LOGF("won't skew base since not dynamic");
  }
  *base += aslr;
  if (!(*base & ~(GetSystemPageSize() - 1))) {
    ERRF("won't load program to null base address");
    exit(127);
  }
  return aslr;
}

static bool LoadElf(struct Machine *m, struct Elf *elf, int fd) {
  int i;
  Elf64_Phdr_ *phdr;
  i64 end = INT64_MIN;
  bool execstack = true;
  elf->aslr = ChooseAslr(elf->ehdr, elf->size, m->system->brk, &elf->base);
  m->ip = elf->at_entry = elf->aslr + Read64(elf->ehdr->entry);
  m->cs.sel = USER_CS_LINUX;
  m->ss.sel = USER_DS_LINUX;
  elf->at_phdr = elf->base + Read64(elf->ehdr->phoff);
  elf->at_phent = Read16(elf->ehdr->phentsize);
  elf->at_phnum = 0;
  for (i = 0; i < Read16(elf->ehdr->phnum); ++i) {
    ++elf->at_phnum;
    phdr = (Elf64_Phdr_ *)((u8 *)elf->ehdr + Read64(elf->ehdr->phoff) +
                           Read16(elf->ehdr->phentsize) * i);
    switch (Read32(phdr->type)) {
      case PT_GNU_STACK_:
        execstack = false;
        break;
      case PT_LOAD_:
        end = LoadElfLoadSegment(m, elf->execfn, elf->ehdr, elf->size, phdr,
                                 end, elf->aslr, fd);
        break;
      case PT_INTERP_:
        elf->interpreter = (char *)elf->ehdr + Read64(phdr->offset);
        if (elf->interpreter[Read64(phdr->filesz) - 1]) {
          ELF_LOGF("elf interpreter not nul terminated");
          exit(127);
        }
        break;
      default:
        break;
    }
  }
  if (elf->interpreter) {
    int fd;
    i64 aslr;
    char ibuf[21];
    struct stat st;
    Elf64_Ehdr_ *ehdr;
    end = INT64_MIN;
    ELF_LOGF("loading elf interpreter %s", elf->interpreter);
    errno = 0;
    SYS_LOGF("LoadInterpreter %s", elf->interpreter);
    if ((fd = OverlaysOpen(AT_FDCWD, elf->interpreter, O_RDONLY, 0)) == -1 ||
        (fstat(fd, &st) == -1 || !st.st_size) ||
        (ehdr = (Elf64_Ehdr_ *)Mmap(0, st.st_size, PROT_READ | PROT_WRITE,
                                    MAP_PRIVATE, fd, 0, "loader")) ==
            MAP_FAILED ||
        !IsSupportedExecutable(elf->interpreter, ehdr, st.st_size)) {
      WriteErrorString(elf->interpreter);
      WriteErrorString(": failed to load interpreter (errno ");
      FormatInt64(ibuf, errno);
      WriteErrorString(ibuf);
      WriteErrorString(")\n");
      exit(127);
    }
    aslr = ChooseAslr(
        ehdr, st.st_size,
        elf->aslr ? elf->aslr - (16 * 1024 * 1024) : FLAG_dyninterpaddr,
        &elf->at_base);
    m->ip = elf->at_base + Read64(ehdr->entry);
    for (i = 0; i < Read16(ehdr->phnum); ++i) {
      phdr = GetElfSegmentHeaderAddress(ehdr, st.st_size, i);
      CheckElfAddress(ehdr, st.st_size, (intptr_t)ehdr + Read64(phdr->offset),
                      Read64(phdr->filesz));
      switch (Read32(phdr->type)) {
        case PT_LOAD_:
          end = LoadElfLoadSegment(m, elf->interpreter, ehdr, st.st_size, phdr,
                                   end, aslr, fd);
          break;
        default:
          break;
      }
    }
    unassert(!Munmap(ehdr, st.st_size));
    unassert(!close(fd));
  }
  return execstack;
}

static void BootProgram(struct Machine *m, struct Elf *elf, size_t codesize) {
  m->cs.sel = m->cs.base = 0;
  m->ip = 0x7c00;
  elf->base = 0x7c00;
  memset(m->system->real, 0, 0x00f00000);
  Write16(m->system->real + 0x400, 0x3F8);
  Write16(m->system->real + 0x40E, 0xb0000 >> 4);
  Write16(m->system->real + 0x413, 0xb0000 / 1024);
  Write16(m->system->real + 0x44A, 80);
  Write64(m->dx, 0);
  memcpy(m->system->real + 0x7c00, elf->map, 512);
  if (READ32(elf->map) == READ32("\177ELF")) {
    elf->ehdr = (Elf64_Ehdr_ *)elf->map;
    elf->size = codesize;
    elf->base = Read64(elf->ehdr->entry);
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

static void FreeProgName(void) {
  free(g_progname);
}

static int CheckExecutableFile(const struct stat *st) {
  if (!S_ISREG(st->st_mode)) {
    LOGF("execve needs regular file");
    errno = EACCES;
    return -1;
  }
  if (!(st->st_mode & 0111)) {
    LOGF("execve needs chmod +x");
    errno = EACCES;
    return -1;
  }
  if (!st->st_size) {
    LOGF("executable file empty");
    errno = ENOEXEC;
    return -1;
  }
  return 0;
}

// SHELL RUNS
//   ./script.sh foo bar
// SCRIPT HAS
//   #!/bin/interp one arg
// BLINK DOES
//   AT_EXECFN = ./script.sh
//   argv[0] = /bin/interp
//   argv[1] = one arg
//   argv[2] = ./script.sh
//   argv[3] = foo
//   argv[4] = bar
static bool HasShebang(struct Machine *m, const char *p, size_t n, char **prog,
                       char **args) {
  char *b;
  size_t i, j, t;
  n = MIN(n, kMaxShebang);
  if (n < 4) return false;
  if (p[0] != '#') return false;
  if (p[1] != '!') return false;
  if (!(b = (char *)AddToFreeList(m, calloc(1, n + 1)))) return false;
  for (t = 0, j = 0, i = 2; i < n; ++i) {
    if (!(32 <= p[i] && p[i] < 0177) && p[i] != '\n') return false;
    if (!t) {
      // STATE 0: COPY INTERPRETER FILENAME
      if (p[i] == ' ') {
        *prog = b;
        b = 0;
        j = 0;
        t = 1;
      } else if (p[i] == '\n') {
        *prog = b;
        *args = 0;
        return true;
      } else {
        b[j++] = p[i];
        b[j] = 0;
      }
    } else if (t == 1) {
      // STATE 1: HANDLE SPACES BETWEEN INTERPRETER AND ITS ARGS
      if (p[i] == ' ') {
        // do nothing
      } else if (p[i] == '\n') {
        *args = 0;
        return true;
      } else {
        if (!(b = (char *)AddToFreeList(m, calloc(1, n + 1)))) return false;
        b[j++] = p[i];
        b[j] = 0;
        t = 2;
      }
    } else if (t == 2) {
      // STATE 2: COPY INTERPRETER ARGS AS A SINGLE ARGUMENT
      if (p[i] == '\n') {
        *args = b;
        return true;
      } else {
        b[j++] = p[i];
        b[j] = 0;
      }
    } else {
      __builtin_unreachable();
    }
  }
  return false;
}

static size_t CountStrList(char **a) {
  size_t n = 0;
  while (*a++) ++n;
  return n;
}

static char **ConcatStrLists(struct Machine *m, char **a, char **b) {
  size_t i, n;
  char **c, *e;
  if ((c = (char **)AddToFreeList(
           m, malloc(((n = CountStrList(a) + CountStrList(b)) + 2) *
                     sizeof(*c))))) {
    i = 0;
    while ((e = *a++)) c[i++] = e;
    while ((e = *b++)) c[i++] = e;
    unassert(i == n);
    c[i++] = 0;
    c[i] = 0;
  }
  return c;
}

static int CanEmulateData(struct Machine *m, char **prog, char ***argv,
                          bool isfirst, char *img, size_t imglen) {
  char **newargv;
  char *interp[3] = {0};
  if (IsSupportedExecutable(*prog, img, imglen)) {
    return 1;
  } else if (isfirst && HasShebang(m, img, imglen, interp, interp + 1) &&
             CanEmulateImpl(m, interp, 0, false) &&
             (newargv = ConcatStrLists(m, interp, *argv))) {
    newargv[1 + !!interp[1]] = *prog;
    *prog = interp[0];
    *argv = newargv;
    return 2;
  } else {
    return 0;
  }
}

void LoadProgram(struct Machine *m, char *execfn, char *prog, char **args,
                 char **vars) {
  int fd;
  i64 stack;
  int status;
  bool isfirst;
  char ibuf[21];
  char ehdr[64];
  long pagesize;
  bool execstack;
  struct stat st;
  struct Elf *elf;
  static bool once;
  if (!once) {
    atexit(FreeProgName);
    once = true;
  }
  elf = &m->system->elf;
  unassert(GetRandom(elf->rng, sizeof(elf->rng), 0) == sizeof(elf->rng));
  for (isfirst = true;;) {
    unassert(prog);
    elf->execfn = execfn;
    elf->prog = prog;
    elf->interpreter = 0;
    elf->at_phdr = 0;
    elf->at_base = -1;
    elf->at_phent = 56;
    free(g_progname);
    g_progname = strdup(prog);
    SYS_LOGF("LoadProgram %s", prog);
    if ((fd = OverlaysOpen(AT_FDCWD, prog, O_RDONLY, 0)) == -1 ||
        fstat(fd, &st) == -1 || CheckExecutableFile(&st) == -1 ||
        (elf->map = (char *)Mmap(0, (elf->mapsize = st.st_size),
                                 PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0,
                                 "loader")) == MAP_FAILED) {
      WriteErrorString(prog);
      WriteErrorString(": failed to load executable (errno ");
      FormatInt64(ibuf, errno);
      WriteErrorString(ibuf);
      WriteErrorString(")\n");
      exit(127);
    }
    status = CanEmulateData(m, &prog, &args, isfirst, elf->map, elf->mapsize);
    if (!status) {
      WriteErrorString("\
error: unsupported executable; we need:\n\
- x86_64-linux elf executables\n\
- flat executables (.bin files)\n\
- actually portable executables (MZqFpD/jartsr)\n\
- scripts with #!shebang meeting above criteria\n");
      exit(127);
    } else if (status == 1) {
      break;  // file is a real executable
    } else if (status == 2) {
      // turns out it's a shell script
      if (isfirst) {
        // start over using the shebang interpreter instead
        unassert(!munmap(elf->map, elf->mapsize));
        unassert(!close(fd));
        isfirst = false;
      } else {
        LOGF("shell scripts can't interpret shell scripts");
        exit(127);
      }
    } else {
      __builtin_unreachable();
    }
  }
  ResetCpu(m);
  m->system->codesize = 0;
  m->system->codestart = 0;
  m->system->brk = FLAG_imagestart;
  m->system->automap = FLAG_automapstart;
  if (HasLinearMapping(m)) {
    m->system->brk ^= Read64(elf->rng) & FLAG_aslrmask;
    m->system->automap ^= (Read64(elf->rng) & FLAG_aslrmask);
  }
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
    stack = HasLinearMapping(m) && FLAG_vabits <= 47 && !kSkew
                ? 0
                : kStackTop - kStackSize;
    if ((stack = ReserveVirtual(m->system, stack, kStackSize,
                                PAGE_U | PAGE_RW | (execstack ? 0 : PAGE_XD),
                                -1, 0, 0, 0)) != -1) {
      unassert(AddFileMap(m->system, stack, kStackSize, "[stack]", -1));
      Put64(m->sp, stack + kStackSize);
    } else {
      LOGF("failed to reserve stack memory");
      exit(127);
    }
    LoadArgv(m, execfn, prog, args, vars, elf->rng);
  }
  pagesize = GetSystemPageSize();
  pagesize = MAX(4096, pagesize);
  if (elf->interpreter) {
    elf->interpreter = strdup(elf->interpreter);
  }
  unassert(CheckMemoryInvariants(m->system));
  elf->execfn = strdup(elf->execfn);
  elf->prog = strdup(elf->prog);
  unassert(!close(fd));
}

static bool CanEmulateImpl(struct Machine *m, char **prog, char ***argv,
                           bool isfirst) {
  int fd;
  bool res;
  void *img;
  struct stat st;
  if ((fd = OverlaysOpen(AT_FDCWD, *prog, O_RDONLY | O_CLOEXEC, 0)) == -1) {
    return false;
  }
  unassert(!fstat(fd, &st));
  if (CheckExecutableFile(&st) == -1) {
    close(fd);
    return false;
  }
  img = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (img == MAP_FAILED) return false;
  res = !!CanEmulateData(m, prog, argv, isfirst, (char *)img, st.st_size);
  unassert(!munmap(img, st.st_size));
  return res;
}

bool CanEmulateExecutable(struct Machine *m, char **prog, char ***argv) {
  int err;
  bool res;
  err = errno;
  res = CanEmulateImpl(m, prog, argv, true);
  errno = err;
  return res;
}
