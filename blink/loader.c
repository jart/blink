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
#include "blink/loader.h"

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
#include "blink/biosrom.h"
#include "blink/builtin.h"
#include "blink/end.h"
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/map.h"
#include "blink/overlays.h"
#include "blink/procfs.h"
#include "blink/random.h"
#include "blink/tunables.h"
#include "blink/util.h"
#include "blink/vfs.h"
#include "blink/x86.h"

#define READ64(p) Read64((const u8 *)(p))
#define READ32(p) Read32((const u8 *)(p))

#ifndef __COSMOPOLITAN__
#define IsWindows() 0
#endif

static bool CanEmulateImpl(struct Machine *, char **, char ***, bool);

// If SHELL inherited from the host points to a binary that doesn't exist in
// the FreeBSD chroot (e.g. SHELL=/bin/bash on a Linux host, but no bash in
// the chroot), rewrite it. Without this mc's subshell mode silently
// fails-open: blink can't find /bin/bash in the chroot so SysExecve falls
// through to host execve(), and mc ends up "talking" to the host's Linux
// bash through a pipe. The protocol desyncs and mc hangs in sigsuspend
// after clearing the screen. Preferred replacement is the FreeBSD bash
// installed via pkg; fall back to /bin/sh otherwise.
static void FixupShellEnv(char **vars) {
  size_t i;
  if (!vars) return;
  for (i = 0; vars[i]; ++i) {
    if (strncmp(vars[i], "SHELL=", 6) != 0) continue;
    {
      const char *val = vars[i] + 6;
      int fd;
      // Already resolves inside the chroot? leave it.
      if ((fd = VfsOpen(AT_FDCWD, val, O_RDONLY | O_CLOEXEC, 0)) != -1) {
        close(fd);
        return;
      }
      // Try /usr/local/bin/bash, then /bin/sh.
      const char *candidates[] = {
          "SHELL=/usr/local/bin/bash",
          "SHELL=/bin/sh",
          NULL,
      };
      int j;
      for (j = 0; candidates[j]; ++j) {
        if ((fd = VfsOpen(AT_FDCWD, candidates[j] + 6, O_RDONLY | O_CLOEXEC,
                          0)) != -1) {
          close(fd);
          // vars[] was passed in from main() — these strings live in the
          // process env. Don't mutate them; allocate a fresh string.
          vars[i] = strdup(candidates[j]);
          LOGF("FreeBSD chroot: rewriting SHELL=%s -> %s", val,
               candidates[j] + 6);
          return;
        }
      }
      return;
    }
  }
}

// Pre-seed /var/run/ld-elf.so.hints inside the chroot if it's missing. On a
// real FreeBSD system this file is created by `service ldconfig start` at
// boot, which blink doesn't simulate; without it, ld-elf.so.1 doesn't know
// about /usr/local/lib and pkg-installed binaries fail to find their shared
// objects. We write the exact byte format that ldconfig(8) produces, listing
// the standard library directories.
static void EnsureFreebsdLdHints(void) {
  static const char path[] = "/var/run/ld-elf.so.hints";
  static const char dirs[] =
      "/usr/local/lib:/usr/local/lib/compat/pkg:/usr/lib/compat";
  unsigned char buf[128 + sizeof(dirs)];
  size_t dirlen = sizeof(dirs) - 1;
  int fd;
  if ((fd = VfsOpen(AT_FDCWD, path, O_RDONLY | O_CLOEXEC, 0)) != -1) {
    char m[4];
    bool ok = read(fd, m, 4) == 4 && !memcmp(m, "Ehnt", 4);
    close(fd);
    if (ok) return;  // valid hints file already present
  }
  memset(buf, 0, sizeof(buf));
  Write32(buf + 0, 0x746e6845);   // magic "Ehnt"
  Write32(buf + 4, 1);            // version
  Write32(buf + 8, 128);          // strtab offset
  Write32(buf + 12, dirlen + 1);  // strsize (includes trailing NUL)
  Write32(buf + 16, 0);           // dirlist offset within strtab
  Write32(buf + 20, dirlen);      // dirlistlen (excludes NUL)
  memcpy(buf + 128, dirs, dirlen + 1);
  (void)VfsMkdir(AT_FDCWD, "/var", 0755);
  (void)VfsMkdir(AT_FDCWD, "/var/run", 0755);
  fd = VfsOpen(AT_FDCWD, path,
               O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0444);
  if (fd >= 0) {
    (void)!write(fd, buf, 128 + dirlen + 1);
    close(fd);
  }
}

static void LoaderCopy(struct Machine *m, i64 vaddr, size_t amt, void *image,
                       i64 offset, int prot) {
  i64 base;
  bool memory_might_be_write_protected;
  if (!amt) return;
  ELF_LOGF("copy %" PRIx64 "-%" PRIx64 " from %" PRIx64 "-%" PRIx64, vaddr,
           vaddr + amt, offset, offset + amt);
  base = ROUNDDOWN(vaddr, 4096);
  if ((memory_might_be_write_protected =
           (prot & (PROT_READ | PROT_WRITE)) != (PROT_READ | PROT_WRITE) ||
           (!IsJitDisabled(&m->system->jit) &&
            prot == (PROT_READ | PROT_WRITE | PROT_EXEC)))) {
    unassert(!ProtectVirtual(m->system, base, vaddr + amt - base,
                             PROT_READ | PROT_WRITE, false));
  }
  unassert(!CopyToUser(m, vaddr, (u8 *)image + offset, amt));
  if (memory_might_be_write_protected) {
    unassert(!ProtectVirtual(m->system, base, vaddr + amt - base, prot, false));
  }
}

static i64 LoadElfLoadSegment(struct Machine *m, const char *path, void *image,
                              size_t imagesize, const Elf64_Phdr_ *phdr,
                              i64 last_end, int *last_prot, u64 aslr, int fd) {
  i64 bulk;
  size_t amt;
  void *blank;
  int overprot;
  struct System *s = m->system;
  u32 flags = Read32(phdr->flags);
  i64 vaddr = Read64(phdr->vaddr) + aslr;
  i64 memsz = Read64(phdr->memsz);
  i64 offset = Read64(phdr->offset);
  i64 filesz = Read64(phdr->filesz);
  long pagesize = HasLinearMapping() ? FLAG_pagesize : 4096;
  i64 start = ROUNDDOWN(vaddr, pagesize);
  i64 end = ROUNDUP(vaddr + memsz, pagesize);
  long skew = vaddr & (pagesize - 1);
  u64 key = (flags & PF_R_ ? PAGE_U : 0) |   //
            (flags & PF_W_ ? PAGE_RW : 0) |  //
            (flags & PF_X_ ? 0 : PAGE_XD);
  int prot = (flags & PF_R_ ? PROT_READ : 0) |   //
             (flags & PF_W_ ? PROT_WRITE : 0) |  //
             (flags & PF_X_ ? PROT_EXEC : 0);

  SYS_LOGF("PT_LOAD %c%c%c [%" PRIx64 ",%" PRIx64 ") %s",  //
           (flags & PF_R_ ? 'R' : '.'),                    //
           (flags & PF_W_ ? 'W' : '.'),                    //
           (flags & PF_X_ ? 'X' : '.'),                    //
           vaddr, vaddr + memsz, path);

  ELF_LOGF("PROGRAM HEADER");
  ELF_LOGF("  path = %s", path);
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

  if (!memsz) {
    return last_end;
  }
  if (offset > imagesize) {
    ERRF("bad phdr offset");
    exit(EXIT_FAILURE_EXEC_FAILED);
  }
  if (filesz > imagesize) {
    ERRF("bad phdr filesz");
    exit(EXIT_FAILURE_EXEC_FAILED);
  }
  if (filesz && offset + filesz > imagesize) {
    ERRF("corrupt elf program header");
    exit(EXIT_FAILURE_EXEC_FAILED);
  }
  if (end < last_end) {
    ERRF("program headers aren't ordered, expected %" PRIx64 " >= %" PRIx64,
         end, last_end);
    exit(EXIT_FAILURE_EXEC_FAILED);
  }
  if (skew != (offset & (pagesize - 1))) {
    WriteErrorString(
        "p_vaddr p_offset skew unequal w.r.t. page size; try either "
        "(1) rebuilding your program using the linker flags: -static "
        "-Wl,-z,common-page-size=65536,-z,max-page-size=65536 or (2) "
        "using `blink -m` to disable the linear memory optimization\n");
    exit(EXIT_FAILURE_EXEC_FAILED);
  }

  // on systems with a page size greater than the elf executable (e.g.
  // apple m1) it's possible for the second program header load to end
  // up overlapping the previous one.
  if (HasLinearMapping() && start < last_end) {
    unassert(pagesize > 4096);
    unassert(vaddr < last_end);
    unassert(start == last_end - pagesize);
    unassert(skew + (last_end - vaddr) == pagesize);
    overprot = prot | *last_prot;
    unassert(!ProtectVirtual(m->system, start, pagesize, overprot, false));
    amt = MIN(filesz, last_end - vaddr);
    LoaderCopy(m, vaddr, amt, image, offset, overprot);
    filesz -= amt;
    offset += last_end - vaddr;
    vaddr += last_end - vaddr;
    start = last_end;
    skew = 0;
  }

  // if there's still a skew then the elf program header is documenting
  // the precise byte offset within the file where this program starts.
  // in that case, it's harmless to just round down the mmap request to
  // ingest the previous bytes which shouldn't even need to be cleared.
  if (skew) {
    unassert(skew < pagesize);
    vaddr -= skew;
    offset -= skew;
    if (filesz) {
      filesz += skew;
    }
  }

  // associate the file name with the memory mapping
  // this makes it possible for debug symbols to be loaded
  if (filesz) {
    key |= PAGE_FILE;
    unassert(AddFileMap(m->system, start, filesz, path, offset));
  }

  // load the aligned program header
  unassert(start <= end);
  unassert(vaddr == start);
  unassert(vaddr + filesz <= end);
  unassert(!(vaddr & (pagesize - 1)));
  unassert(!(start & (pagesize - 1)));
  unassert(!(offset & (pagesize - 1)));
  if (start < end) {
    // it's also harmless to extend the size to the next host page
    // boundary when mapping a file. if some extra bytes go beyond
    // the end of the file then they'll be zero'd and sigbus won't
    // be raised if they're used. if extra file content does exist
    // beyond filesz, then we shall manually bzero() it afterwards
    if ((bulk = ROUNDUP(filesz, pagesize))) {
      // map the bulk of .text directly into memory without copying.
      ELF_LOGF("load %" PRIx64 "-%" PRIx64 " from %" PRIx64 "-%" PRIx64, start,
               start + bulk, offset, offset + bulk);
      if (ReserveVirtual(s, start, bulk, key, fd, offset, 0, 0) == -1) {
        ERRF("failed to map elf program header file data");
        exit(EXIT_FAILURE_EXEC_FAILED);
      }
      if ((amt = bulk - filesz)) {
        ELF_LOGF("note: next copy is actually bzero() kludge");
        unassert(blank = calloc(1, amt));
        LoaderCopy(m, start + filesz, amt, blank, 0, prot);
        free(blank);
      }
    }
    start += bulk;
    // allocate any remaining zero-initialized memory
    if (start < end) {
      ELF_LOGF("alloc %" PRIx64 "-%" PRIx64, start, end);
      if (ReserveVirtual(s, start, end - start, key, -1, 0, 0, 0) == -1) {
        ERRF("failed to allocate program header bss");
        exit(EXIT_FAILURE_EXEC_FAILED);
      }
    }
  }

  *last_prot = prot;
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
  return (Read16(ehdr->type) == ET_DYN_ || Read16(ehdr->type) == ET_EXEC_) &&
         ehdr->ident[EI_OSABI_] == ELFOSABI_FREEBSD_ &&
         ehdr->ident[EI_VERSION_] == 1;
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

static bool IsShebangExecutable(void *image, size_t size) {
  return size >= 2 && ((char *)image)[0] == '#' && ((char *)image)[1] == '!';
}

static void ExplainWhyItCantBeEmulated(const char *path, const char *reason) {
  LOGF("%s: can't emulate: %s", path, reason);
}

static bool IsBinFile(const char *prog) {
  return EndsWith(prog, ".bin") ||  //
         EndsWith(prog, ".BIN") ||  //
         EndsWith(prog, ".img") ||  //
         EndsWith(prog, ".IMG") ||  //
         EndsWith(prog, ".raw") ||  //
         EndsWith(prog, ".RAW");
}

bool IsSupportedExecutable(const char *path, void *image, size_t size) {
  Elf64_Ehdr_ *ehdr;
  if (IsBinFile(path)) {
    return true;
  }
  if (size >= sizeof(Elf64_Ehdr_) && READ32(image) == READ32("\177ELF")) {
    ehdr = (Elf64_Ehdr_ *)image;
    if (Read16(ehdr->type) != ET_EXEC_ &&  //
        Read16(ehdr->type) != ET_DYN_) {
      ExplainWhyItCantBeEmulated(path, "ELF is neither ET_EXEC or ET_DYN");
      return false;
    }
    if (ehdr->ident[EI_CLASS_] == ELFCLASS32_) {
      ExplainWhyItCantBeEmulated(path, "32-bit ELF not supported");
      return false;
    }
    if (Read16(ehdr->machine) != EM_NEXGEN32E_) {
      ExplainWhyItCantBeEmulated(path, "ELF is not AMD64");
      return false;
    }
    // FreeBSD executables are now supported
    // if (IsFreebsdExecutable(ehdr, size)) {
    //   ExplainWhyItCantBeEmulated(path, "ELF is FreeBSD executable");
    //   return false;
    // }
    if (IsOpenbsdExecutable(ehdr, size)) {
      ExplainWhyItCantBeEmulated(path, "ELF is OpenBSD executable");
      return false;
    }
    if (IsHaikuExecutable(ehdr, size)) {
      ExplainWhyItCantBeEmulated(path, "ELF is Haiku executable");
      return false;
    }
#if defined(__ELF__) && !defined(__linux)
    LOGF("blink believes %s is an x86_64-linux executable", path);
#endif
    return true;
  }
  if (size >= 4096 && (READ64(image) == READ64("MZqFpD='") ||
                       READ64(image) == READ64("jartsr='"))) {
    return true;
  }
  if (!IsShebangExecutable(image, size)) {
    ExplainWhyItCantBeEmulated(path, "not ELF, not APE, and not a .bin file");
  }
  return false;
}

static void LoadFlatExecutable(struct Machine *m, uintptr_t base,
                               const char *prog, void *image, size_t imagesize,
                               int fd) {
  int prot = 0;
  Elf64_Phdr_ phdr;
  i64 end = INT64_MIN;
  Write32(phdr.type, PT_LOAD_);
  Write32(phdr.flags, PF_X_ | PF_R_ | PF_W_);
  Write64(phdr.offset, 0);
  Write64(phdr.vaddr, base);
  Write64(phdr.filesz, imagesize);
  Write64(phdr.memsz, ROUNDUP(imagesize + kRealSize, 4096));
  LoadElfLoadSegment(m, prog, image, imagesize, &phdr, end, &prot, 0, fd);
  m->ip = base;
}

static i64 ChooseAslr(const Elf64_Ehdr_ *ehdr, size_t size, i64 dflt,
                      i64 *base) {
  i64 aslr;
  if (GetElfMemorySize(ehdr, size, base) <= 0) {
    ERRF("couldn't determine boundaries of loaded executable");
    exit(EXIT_FAILURE_EXEC_FAILED);
  }
  if (Read16(ehdr->type) == ET_DYN_ && !*base) {
    aslr = dflt;
    ELF_LOGF("choosing base skew %" PRIx64 " because dynamic", aslr);
  } else {
    aslr = 0;
    ELF_LOGF("won't skew base since not dynamic");
  }
  *base += aslr;
  if (!(*base & ~(FLAG_pagesize - 1))) {
    ERRF("won't load program to null base address");
    exit(EXIT_FAILURE_EXEC_FAILED);
  }
  return aslr;
}

static bool LoadElf(struct Machine *m,  //
                    struct Elf *elf,    //
                    Elf64_Ehdr_ *ehdr,  //
                    size_t esize,       //
                    int fd) {
  int i, prot;
  Elf64_Phdr_ *phdr;
  i64 end = INT64_MIN;
  bool execstack = true;
  elf->aslr = ChooseAslr(ehdr, esize, m->system->brk, &elf->base);
  m->ip = elf->at_entry = elf->aslr + Read64(ehdr->entry);
  /* fprintf(stderr, "LoadElf: type=%d base=%#" PRIx64 " aslr=%#" PRIx64 " entry=%#" PRIx64 " interp=%s\n",
          Read16(ehdr->type), elf->base, elf->aslr, (u64)m->ip,
          elf->interpreter ? "(pending)" : "(none)"); */
  m->cs.sel = USER_CS_LINUX;
  m->ss.sel = USER_DS_LINUX;
  elf->at_phdr = elf->base + Read64(ehdr->phoff);
  elf->at_phent = Read16(ehdr->phentsize);
  elf->at_phnum = 0;
  for (prot = i = 0; i < Read16(ehdr->phnum); ++i) {
    ++elf->at_phnum;
    phdr = (Elf64_Phdr_ *)((u8 *)ehdr + Read64(ehdr->phoff) +
                           Read16(ehdr->phentsize) * i);
    switch (Read32(phdr->type)) {
      case PT_GNU_STACK_:
        execstack = Read32(phdr->flags) & PF_X_;
        break;
      case PT_LOAD_:
        end = LoadElfLoadSegment(m, elf->execfn, ehdr, esize, phdr, end, &prot,
                                 elf->aslr, fd);
        break;
      case PT_INTERP_:
        elf->interpreter = (char *)ehdr + Read64(phdr->offset);
        if (elf->interpreter[Read64(phdr->filesz) - 1]) {
          ELF_LOGF("elf interpreter not nul terminated");
          exit(EXIT_FAILURE_EXEC_FAILED);
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
    Elf64_Ehdr_ *ehdri;
    end = INT64_MIN;
    ELF_LOGF("loading elf interpreter %s", elf->interpreter);
    errno = 0;
    SYS_LOGF("LoadInterpreter %s", elf->interpreter);
    if ((fd = VfsOpen(AT_FDCWD, elf->interpreter, O_RDONLY, 0)) == -1 ||
        (VfsFstat(fd, &st) == -1 || !st.st_size) ||
        (ehdri = (Elf64_Ehdr_ *)Mmap(0, st.st_size, PROT_READ | PROT_WRITE,
                                     MAP_PRIVATE, fd, 0, "loader")) ==
            MAP_FAILED ||
        !IsSupportedExecutable(elf->interpreter, ehdri, st.st_size)) {
      WriteErrorString(elf->interpreter);
      WriteErrorString(": failed to load interpreter (errno ");
      FormatInt64(ibuf, errno);
      WriteErrorString(ibuf);
      WriteErrorString(")\n");
      exit(EXIT_FAILURE_EXEC_FAILED);
    }
    aslr = ChooseAslr(
        ehdri, st.st_size,
        elf->aslr ? elf->aslr - (16 * 1024 * 1024) : FLAG_dyninterpaddr,
        &elf->at_base);
    m->ip = elf->at_base + Read64(ehdri->entry);
    /* fprintf(stderr, "LoadInterp: at_base=%#" PRIx64 " ip=%#" PRIx64 "\n", elf->at_base, (u64)m->ip); */
    for (prot = i = 0; i < Read16(ehdri->phnum); ++i) {
      phdr = GetElfProgramHeaderAddress(ehdri, st.st_size, i);
      switch (Read32(phdr->type)) {
        case PT_LOAD_:
          end = LoadElfLoadSegment(m, elf->interpreter, ehdri, st.st_size, phdr,
                                   end, &prot, aslr, fd);
          break;
        default:
          break;
      }
    }
    unassert(!Munmap(ehdri, st.st_size));
    unassert(!VfsClose(fd));
  }
  return execstack;
}

void BootProgram(struct Machine *m,  //
                 struct Elf *elf,    //
                 u8 bootdrive) {
  int fd;
  SetDefaultBiosIntVectors(m);
  memset(m->beg, 0, sizeof(m->beg));  // reinitialize registers
  memset(m->seg, 0, sizeof(m->seg));
  m->flags = SetFlag(m->flags, FLAGS_IF, 1);
  m->ip = 0x7c00;
  elf->base = 0x7c00;
  Write64(m->sp, 0x6f00);  // following QEMU
  m->dl = bootdrive;
  SetDefaultBiosDataArea(m);
  memset(m->system->real + 0x500, 0, kBiosOptBase - 0x500);
  memset(m->system->real + 0x00100000, 0, kRealSize - 0x00100000);
  if ((fd = VfsOpen(AT_FDCWD, m->system->elf.prog, O_RDONLY, 0)) == -1 ||
      VfsRead(fd, m->system->real + 0x7c00, 512) <= 0) {
    // if we failed to load the boot sector for whatever reason, then...
    // ...arrange to invoke int 0x18 (diskless boot hook)
    // TODO: maybe error out more quickly?
    Write16(m->system->real + 0x7c00, 0x18CD);
  }
  VfsClose(fd);
  SetWriteAddr(m, 0x7c00, 512);
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
        ERRF("%s: ape printf elf header too long\n", prog);
        return -1;
      }
    }
    if (i != 64) {
      ERRF("%s: ape printf elf header too short\n", prog);
      return -1;
    }
    if (READ32(ehdr) != READ32("\177ELF")) {
      ERRF("%s: ape printf elf header didn't have elf magic\n", prog);
      return -1;
    }
    return 0;
  }
  ERRF("%s: printf statement not found in first 4096 bytes\n", prog);
  return -1;
}

static void FreeProgName(void) {
  free(g_progname);
}

static int CheckExecutableFile(const char *prog, const struct stat *st) {
  if (!S_ISREG(st->st_mode)) {
    LOGF("execve needs regular file");
    errno = EACCES;
    return -1;
  }
  if (!IsWindows() && !(st->st_mode & 0111) && !IsBinFile(prog)) {
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

static bool IsApeBinary(const char *path) {
  int fd;
  ssize_t rc;
  char buf[8];
  fd = open(path, O_RDONLY | O_CLOEXEC | O_NOCTTY);
  if (fd == -1) return false;
  rc = pread(fd, buf, 8, 0);
  close(fd);
  if (rc != 8) return false;
  return !memcmp(buf, "MZqFpD='", 8) ||  //
         !memcmp(buf, "jartsr='", 8);
}

void LoadProgram(struct Machine *m, char *execfn, char *prog, char **args,
                 char **vars, const char *biosprog) {
  int fd;
  i64 stack;
  void *map;
  int status;
  char tmp[64];
  bool isfirst;
  long pagesize;
  size_t mapsize;
  bool execstack;
  struct stat st;
  struct Elf *elf;
  static bool once;
  if (!once) {
    atexit(FreeProgName);
    once = true;
  }
  elf = &m->system->elf;
  m->system->loaded = false;
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
    if ((fd = VfsOpen(AT_FDCWD, prog, O_RDONLY, 0)) == -1 ||
        VfsFstat(fd, &st) == -1 || CheckExecutableFile(prog, &st) == -1 ||
        (map = Mmap(0, (mapsize = st.st_size), PROT_READ | PROT_WRITE,
                    MAP_PRIVATE, fd, 0, "loader")) == MAP_FAILED) {
      WriteErrorString(prog);
      WriteErrorString(": failed to load executable (errno ");
      FormatInt64(tmp, errno);
      WriteErrorString(tmp);
      WriteErrorString(")\n");
      exit(EXIT_FAILURE_EXEC_FAILED);
    }
    status = CanEmulateData(m, &prog, &args, isfirst, (char *)map, mapsize);
    if (!status) {
      WriteErrorString("\
error: unsupported executable; we need:\n\
- x86_64-linux elf executables\n\
- flat executables (.bin files)\n\
- actually portable executables (MZqFpD/jartsr)\n\
- scripts with #!shebang meeting above criteria\n");
      exit(EXIT_FAILURE_EXEC_FAILED);
    } else if (status == 1) {
      break;  // file is a real executable
    } else if (status == 2) {
      // turns out it's a shell script
      if (isfirst) {
        // start over using the shebang interpreter instead
        unassert(!VfsMunmap(map, mapsize));
        unassert(!VfsClose(fd));
        isfirst = false;
      } else {
        LOGF("shell scripts can't interpret shell scripts");
        exit(EXIT_FAILURE_EXEC_FAILED);
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
  if (HasLinearMapping()) {
    m->system->brk ^= Read64(elf->rng) & FLAG_aslrmask;
    m->system->automap ^= (Read64(elf->rng) & FLAG_aslrmask);
  }
  if (m->mode.genmode == XED_GEN_MODE_REAL) {
    LoadBios(m, biosprog);
    if (IsApeBinary(prog)) {
      // cosmo convention (see also binbase)
      AddFileMap(m->system, 4 * 1024 * 1024, 512, prog, 0);
    } else {
      // sectorlisp convention
      AddFileMap(m->system, 0, 512, prog, 0);
    }
  } else {
    m->flags = SetFlag(m->flags, FLAGS_IF, 1);
    m->system->cr0 = CR0_PE | CR0_MP | CR0_ET | CR0_PG;
    m->system->cr3 = AllocatePageTable(m->system);
    if (READ32(map) == READ32("\177ELF")) {
      if (IsFreebsdExecutable((Elf64_Ehdr_*)map, mapsize)) {
        m->system->isfreebsd = true;
      }
      execstack = LoadElf(m, elf, (Elf64_Ehdr_ *)map, mapsize, fd);
    } else if (READ64(map) == READ64("MZqFpD='") ||
               READ64(map) == READ64("jartsr='")) {
      m->system->iscosmo = true;
      // Cosmopolitan programs pretty much require at least 47-bit virtual
      // addresses; if the host lacks these, then emulate them w/ software
      if (FLAG_vabits < 47) FLAG_nolinear = true;
      if (GetElfHeader(tmp, prog, (const char *)map) == -1) exit(EXIT_FAILURE_EXEC_FAILED);
      memcpy(map, tmp, 64);
      execstack = LoadElf(m, elf, (Elf64_Ehdr_ *)map, mapsize, fd);
    } else if (IsBinFile(prog)) {
      elf->base = 0x400000;
      LoadFlatExecutable(m, elf->base, prog, map, mapsize, fd);
      execstack = true;
    } else {
      unassert(!"impossible condition");
    }
    stack = HasLinearMapping() && FLAG_vabits <= 47 && !kSkew
                ? 0
                : kStackTop - kStackSize;
    if ((stack = ReserveVirtual(
             m->system, stack, kStackSize,
             PAGE_FILE | PAGE_U | PAGE_RW | (execstack ? 0 : PAGE_XD), -1, 0, 0,
             0)) != -1) {
      unassert(AddFileMap(m->system, stack, kStackSize, "[stack]", -1));
      Put64(m->sp, stack + kStackSize);
    } else {
      LOGF("failed to reserve stack memory");
      exit(EXIT_FAILURE_EXEC_FAILED);
    }
    m->system->loaded = true;  // in case rwx stack is smc write-protected :'(
    // FreeBSD doesn't use sa_restorer like Linux. The kernel provides a
    // signal trampoline via PS_STRINGS. We don't allocate a real page for
    // it — instead we pick a magic address that ExecuteInstruction()
    // intercepts at fetch time and dispatches to SigRestore directly.
    // sigaction() at syscall.c:7982 wires this address into hand.restorer
    // for every FreeBSD signal handler; DeliverSignal pushes it as the
    // handler's return address. When the handler ret's, control "jumps"
    // here and the interpreter recognizes it. This avoids the JIT/SMC
    // dance that fails for a real RWX guest page under -m mode.
    if (m->system->isfreebsd && !m->system->sigtramp) {
      m->system->sigtramp = 0x7fff0000;
      EnsureFreebsdLdHints();
    }
    if (m->system->isfreebsd) FixupShellEnv(vars);
    LoadArgv(m, execfn, prog, args, vars, elf->rng);
  }
  pagesize = FLAG_pagesize;
  pagesize = MAX(4096, pagesize);
  if (elf->interpreter) {
    elf->interpreter = strdup(elf->interpreter);
  }
  unassert(CheckMemoryInvariants(m->system));
  elf->execfn = strdup(elf->execfn);
  elf->prog = strdup(elf->prog);
  unassert(!VfsMunmap(map, mapsize));
  unassert(!VfsClose(fd));
  m->system->loaded = true;
#ifndef DISABLE_VFS
  unassert(!ProcfsRegisterExe(getpid(), elf->prog));
#endif
}

static bool CanEmulateImpl(struct Machine *m, char **prog, char ***argv,
                           bool isfirst) {
  int fd;
  bool res;
  void *img;
  struct stat st;
  if ((fd = VfsOpen(AT_FDCWD, *prog, O_RDONLY | O_CLOEXEC, 0)) == -1) {
  CantEmulate:
    LOGF("%s: can't emulate: %s", *prog, strerror(errno));
    return false;
  }
  unassert(!VfsFstat(fd, &st));
  if (CheckExecutableFile(*prog, &st) == -1) {
    VfsClose(fd);
    return false;
  }
  img = VfsMmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
  VfsClose(fd);
  if (img == MAP_FAILED) goto CantEmulate;
  res = !!CanEmulateData(m, prog, argv, isfirst, (char *)img, st.st_size);
  unassert(!VfsMunmap(img, st.st_size));
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
