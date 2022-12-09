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
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "blink/address.h"
#include "blink/assert.h"
#include "blink/dis.h"
#include "blink/endian.h"
#include "blink/loader.h"
#include "blink/log.h"
#include "blink/map.h"
#include "blink/stats.h"
#include "blink/tsan.h"
#include "blink/util.h"

#define MAX_BACKTRACE_LINES 64

#define APPEND(...) o += snprintf(b + o, n - o, __VA_ARGS__)

static i64 ReadWord(struct Machine *m, u8 *p) {
  switch (2 << m->mode) {
    default:
    case 8:
      return Read64(p);
    case 4:
      return Read32(p);
    case 2:
      return Read16(p);
  }
}

int GetInstruction(struct Machine *m, i64 pc, struct XedDecodedInst *x) {
  int i, err;
  u8 copy[15], *toil, *addr;
  if ((addr = LookupAddress(m, pc))) {
    if ((i = 4096 - (pc & 4095)) >= 15) {
      if (!DecodeInstruction(x, addr, 15, m->mode)) {
        return 0;
      } else {
        return kMachineDecodeError;
      }
    } else if ((toil = LookupAddress(m, pc + i))) {
      memcpy(copy, addr, i);
      memcpy(copy + i, toil, 15 - i);
      if (!DecodeInstruction(x, copy, 15, m->mode)) {
        return 0;
      } else {
        return kMachineDecodeError;
      }
    } else if (!(err = DecodeInstruction(x, addr, i, m->mode))) {
      return 0;
    } else if (err == XED_ERROR_BUFFER_TOO_SHORT) {
      return kMachineSegmentationFault;
    } else {
      return kMachineDecodeError;
    }
  } else {
    memset(x, 0, sizeof(*x));
    return kMachineSegmentationFault;
  }
}

const char *DescribeOp(struct Machine *m) {
  _Thread_local static char b[256];
  int i, o = 0, n = sizeof(b);
  struct Dis d = {true};
  char spec[64];
  if (!GetInstruction(m, GetPc(m), d.xedd)) {
    DisInst(&d, b + o, DisSpec(d.xedd, spec));
  } else if (d.xedd->length) {
    for (i = 0; i < d.xedd->length; ++i) {
      if (i) {
        APPEND(",");
      } else {
        APPEND(".byte\t");
      }
      APPEND("0x%02x", d.xedd->bytes[i]);
    }
  } else {
    APPEND("indescribable");
  }
  DisFree(&d);
  return b;
}

void LoadDebugSymbols(struct Elf *elf) {
  int fd, n;
  void *elfmap;
  char buf[1024];
  struct stat st;
  if (elf->ehdr && GetElfSymbolTable(elf->ehdr, elf->size, &n) && n) return;
  unassert(elf->prog);
  snprintf(buf, sizeof(buf), "%s.dbg", elf->prog);
  if ((fd = open(buf, O_RDONLY)) != -1) {
    if (fstat(fd, &st) != -1 &&
        (elfmap = Mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0,
                       "debug")) != MAP_FAILED) {
      elf->ehdr = (Elf64_Ehdr *)elfmap;
      elf->size = st.st_size;
    }
    close(fd);
  }
}

void PrintFds(struct Fds *fds) {
  struct Dll *e;
  LOGF("%-8s %-8s %-8s %-8s", "fildes", "systemfd", "oflags", "cloexec");
  for (e = dll_first(fds->list); e; e = dll_next(fds->list, e)) {
    LOGF("%-8d %-8d %-8x %-8s", FD_CONTAINER(e)->fildes,
         FD_CONTAINER(e)->systemfd, FD_CONTAINER(e)->oflags,
         FD_CONTAINER(e)->cloexec ? "true" : "false");
  }
}

const char *GetBacktrace(struct Machine *m) {
  _Thread_local static char b[4096];
  u8 *r;
  int i;
  int o = 0;
  int n = sizeof(b);
  i64 sym, sp, bp, rp;
  struct Dis dis = {true};
  char kAlignmentMask[] = {3, 3, 15};
  LoadDebugSymbols(&m->system->elf);
  DisLoadElf(&dis, &m->system->elf);

  APPEND(" PC %" PRIx64 " %s\n\t"
         " AX %016" PRIx64 " "
         " CX %016" PRIx64 " "
         " DX %016" PRIx64 " "
         " BX %016" PRIx64 "\n\t"
         " SP %016" PRIx64 " "
         " BP %016" PRIx64 " "
         " SI %016" PRIx64 " "
         " DI %016" PRIx64 "\n\t"
         " R8 %016" PRIx64 " "
         " R9 %016" PRIx64 " "
         "R10 %016" PRIx64 " "
         "R11 %016" PRIx64 "\n\t"
         "R12 %016" PRIx64 " "
         "R13 %016" PRIx64 " "
         "R14 %016" PRIx64 " "
         "R15 %016" PRIx64 "\n\t"
         " FS %016" PRIx64 " "
         " GS %016" PRIx64 " "
         "OPS %-16ld "
         "JIT %-16ld\n\t"
         "%s\n\t",
         m->cs + MaskAddress(m->mode, m->ip), DescribeOp(m), Get64(m->ax),
         Get64(m->cx), Get64(m->dx), Get64(m->bx), Get64(m->sp), Get64(m->bp),
         Get64(m->si), Get64(m->di), Get64(m->r8), Get64(m->r9), Get64(m->r10),
         Get64(m->r11), Get64(m->r12), Get64(m->r13), Get64(m->r14),
         Get64(m->r15), m->fs, m->gs, GET_COUNTER(instructions_decoded),
         GET_COUNTER(instructions_jitted), g_progname);

  rp = m->ip;
  bp = Get64(m->bp);
  sp = Get64(m->sp);
  for (i = 0; i < MAX_BACKTRACE_LINES;) {
    if (i) APPEND("\n\t");
    sym = DisFindSym(&dis, rp);
    APPEND("%012" PRIx64 " %012" PRIx64 " %s", m->ss + bp, rp,
           sym != -1 ? dis.syms.stab + dis.syms.p[sym].name : "UNKNOWN");
    if (sym != -1 && rp != dis.syms.p[sym].addr) {
      APPEND("+%#" PRIx64 "", rp - dis.syms.p[sym].addr);
    }
    if (!bp) break;
    if (bp < sp) {
      APPEND(" [STRAY]");
    } else if (bp - sp <= 0x1000) {
      APPEND(" %" PRId64 " bytes", bp - sp);
    }
    if (bp & kAlignmentMask[m->mode] && i) {
      APPEND(" [MISALIGN]");
    }
    ++i;
    if (((m->ss + bp) & 0xfff) > 0xff0) break;
    if (!(r = LookupAddress(m, m->ss + bp))) {
      APPEND(" [CORRUPT FRAME POINTER]");
      break;
    }
    sp = bp;
    bp = ReadWord(m, r + 0);
    rp = ReadWord(m, r + 8);
  }

  DisFree(&dis);
  return b;
}
