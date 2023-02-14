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
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/builtin.h"
#include "blink/debug.h"
#include "blink/dis.h"
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/loader.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/map.h"
#include "blink/overlays.h"
#include "blink/stats.h"
#include "blink/thread.h"
#include "blink/tsan.h"
#include "blink/util.h"

#ifdef UNWIND
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#endif

#define MAX_BACKTRACE_LINES 64

#ifdef DISABLE_OVERLAYS
#define OverlaysOpen openat
#endif

#define APPEND(...) o += snprintf(b + o, n - o, __VA_ARGS__)

int asan_backtrace_index;
int asan_backtrace_buffer[1];
int ubsan_backtrace_memory[2];
char *ubsan_backtrace_pointer = ((char *)ubsan_backtrace_memory) + 1;

void PrintBacktrace(void) {
#ifdef UNWIND
  int o = 0;
  char b[2048];
  char sym[256];
  int n = sizeof(b);
  unw_cursor_t cursor;
  unw_context_t context;
  unw_word_t offset, pc;
  unw_getcontext(&context);
  unw_init_local(&cursor, &context);
  while (unw_step(&cursor) > 0) {
    unw_get_reg(&cursor, UNW_REG_IP, &pc);
    if (!pc) break;
    APPEND("\n\t%lx ", pc);
    if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0) {
      APPEND("%s+%ld", sym, offset);
    } else {
      APPEND("<unknown>");
    }
  }
  ERRF("blink backtrace%s", b);
#elif defined(__SANITIZE_ADDRESS__)
  volatile int x;
  x = asan_backtrace_buffer[asan_backtrace_index + 1];
  (void)x;
#elif defined(__SANITIZE_UNDEFINED__)
  *(int *)ubsan_backtrace_pointer = 0;
#endif
}

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

const char *DescribeProt(int prot) {
  char *p;
  bool gotsome;
  _Thread_local static char buf[64];
  if (!prot) return "PROT_NONE";
  p = buf;
  gotsome = false;
  if (prot & PROT_READ) {
    if (gotsome) *p++ = '|';
    p = stpcpy(p, "PROT_READ");
    prot &= ~PROT_READ;
    gotsome = true;
  }
  if (prot & PROT_WRITE) {
    if (gotsome) *p++ = '|';
    p = stpcpy(p, "PROT_WRITE");
    prot &= ~PROT_WRITE;
    gotsome = true;
  }
  if (prot & PROT_EXEC) {
    if (gotsome) *p++ = '|';
    p = stpcpy(p, "PROT_EXEC");
    prot &= ~PROT_EXEC;
    gotsome = true;
  }
  if (prot) {
    if (gotsome) *p++ = '|';
    p = FormatInt64(p, prot);
  }
  return buf;
}

const char *DescribeSignal(int sig) {
  char *p;
  _Thread_local static char buf[30];
  switch (sig) {
    case SIGHUP_LINUX:
      return "SIGHUP";
    case SIGINT_LINUX:
      return "SIGINT";
    case SIGQUIT_LINUX:
      return "SIGQUIT";
    case SIGILL_LINUX:
      return "SIGILL";
    case SIGTRAP_LINUX:
      return "SIGTRAP";
    case SIGABRT_LINUX:
      return "SIGABRT";
    case SIGBUS_LINUX:
      return "SIGBUS";
    case SIGFPE_LINUX:
      return "SIGFPE";
    case SIGKILL_LINUX:
      return "SIGKILL";
    case SIGUSR1_LINUX:
      return "SIGUSR1";
    case SIGSEGV_LINUX:
      return "SIGSEGV";
    case SIGUSR2_LINUX:
      return "SIGUSR2";
    case SIGPIPE_LINUX:
      return "SIGPIPE";
    case SIGALRM_LINUX:
      return "SIGALRM";
    case SIGTERM_LINUX:
      return "SIGTERM";
    case SIGSTKFLT_LINUX:
      return "SIGSTKFLT";
    case SIGCHLD_LINUX:
      return "SIGCHLD";
    case SIGCONT_LINUX:
      return "SIGCONT";
    case SIGSTOP_LINUX:
      return "SIGSTOP";
    case SIGTSTP_LINUX:
      return "SIGTSTP";
    case SIGTTIN_LINUX:
      return "SIGTTIN";
    case SIGTTOU_LINUX:
      return "SIGTTOU";
    case SIGURG_LINUX:
      return "SIGURG";
    case SIGXCPU_LINUX:
      return "SIGXCPU";
    case SIGXFSZ_LINUX:
      return "SIGXFSZ";
    case SIGVTALRM_LINUX:
      return "SIGVTALRM";
    case SIGPROF_LINUX:
      return "SIGPROF";
    case SIGWINCH_LINUX:
      return "SIGWINCH";
    case SIGIO_LINUX:
      return "SIGIO";
    case SIGSYS_LINUX:
      return "SIGSYS";
    case SIGINFO_LINUX:
      return "SIGINFO";
    case SIGPWR_LINUX:
      return "SIGPWR";
    default:
      break;
  }
  p = buf;
  if (SIGRTMIN_LINUX <= sig && sig <= SIGRTMAX_LINUX) {
    p = stpcpy(p, "SIGRTMIN+");
  }
  p = FormatInt64(p, sig);
  return buf;
}

const char *DescribeSyscallResult(i64 ax) {
  _Thread_local static char buf[22];
  if (ax != -1) {
    snprintf(buf, sizeof(buf), "%#" PRIx64, ax);
  } else {
    snprintf(buf, sizeof(buf), "-%s", DescribeHostErrno(errno));
  }
  return buf;
}

const char *DescribeHostErrno(int x) {
  _Thread_local static char buf[21];
  if (x == EPERM) return "EPERM";
  if (x == ENOENT) return "ENOENT";
  if (x == ESRCH) return "ESRCH";
  if (x == EINTR) return "EINTR";
  if (x == EIO) return "EIO";
  if (x == ENXIO) return "ENXIO";
  if (x == E2BIG) return "E2BIG";
  if (x == ENOEXEC) return "ENOEXEC";
  if (x == EBADF) return "EBADF";
  if (x == ECHILD) return "ECHILD";
  if (x == EAGAIN) return "EAGAIN";
#if EWOULDBLOCK != EAGAIN
  if (x == EWOULDBLOCK) return "EWOULDBLOCK";
#endif
  if (x == ENOMEM) return "ENOMEM";
  if (x == EACCES) return "EACCES";
  if (x == EFAULT) return "EFAULT";
#ifdef ENOTBLK
  if (x == ENOTBLK) return "ENOTBLK";
#endif
  if (x == EBUSY) return "EBUSY";
  if (x == EEXIST) return "EEXIST";
  if (x == EXDEV) return "EXDEV";
  if (x == ENODEV) return "ENODEV";
  if (x == ENOTDIR) return "ENOTDIR";
  if (x == EISDIR) return "EISDIR";
  if (x == EINVAL) return "EINVAL";
  if (x == ENFILE) return "ENFILE";
  if (x == EMFILE) return "EMFILE";
  if (x == ENOTTY) return "ENOTTY";
  if (x == ETXTBSY) return "ETXTBSY";
  if (x == EFBIG) return "EFBIG";
  if (x == ENOSPC) return "ENOSPC";
  if (x == ESPIPE) return "ESPIPE";
  if (x == EROFS) return "EROFS";
  if (x == EMLINK) return "EMLINK";
  if (x == EPIPE) return "EPIPE";
  if (x == EDOM) return "EDOM";
  if (x == ERANGE) return "ERANGE";
  if (x == EDEADLK) return "EDEADLK";
  if (x == ENAMETOOLONG) return "ENAMETOOLONG";
  if (x == ENOLCK) return "ENOLCK";
  if (x == ENOSYS) return "ENOSYS";
  if (x == ENOTEMPTY) return "ENOTEMPTY";
  if (x == ELOOP) return "ELOOP";
  if (x == ENOMSG) return "ENOMSG";
  if (x == EIDRM) return "EIDRM";
#ifdef EREMOTE
  if (x == EREMOTE) return "EREMOTE";
#endif
  if (x == EPROTO) return "EPROTO";
  if (x == EBADMSG) return "EBADMSG";
  if (x == EOVERFLOW) return "EOVERFLOW";
  if (x == EILSEQ) return "EILSEQ";
#ifdef EUSERS
  if (x == EUSERS) return "EUSERS";
#endif
  if (x == ENOTSOCK) return "ENOTSOCK";
  if (x == EDESTADDRREQ) return "EDESTADDRREQ";
  if (x == EMSGSIZE) return "EMSGSIZE";
  if (x == EPROTOTYPE) return "EPROTOTYPE";
  if (x == ENOPROTOOPT) return "ENOPROTOOPT";
  if (x == EPROTONOSUPPORT) return "EPROTONOSUPPORT";
#ifdef ESOCKTNOSUPPORT
  if (x == ESOCKTNOSUPPORT) return "ESOCKTNOSUPPORT";
#endif
  if (x == ENOTSUP) return "ENOTSUP";
#if EOPNOTSUPP != ENOTSUP
  if (x == EOPNOTSUPP) return "EOPNOTSUPP";
#endif
#ifdef EPFNOSUPPORT
  if (x == EPFNOSUPPORT) return "EPFNOSUPPORT";
#endif
  if (x == EAFNOSUPPORT) return "EAFNOSUPPORT";
  if (x == EADDRINUSE) return "EADDRINUSE";
  if (x == EADDRNOTAVAIL) return "EADDRNOTAVAIL";
  if (x == ENETDOWN) return "ENETDOWN";
  if (x == ENETUNREACH) return "ENETUNREACH";
  if (x == ENETRESET) return "ENETRESET";
  if (x == ECONNABORTED) return "ECONNABORTED";
  if (x == ECONNRESET) return "ECONNRESET";
  if (x == ENOBUFS) return "ENOBUFS";
  if (x == EISCONN) return "EISCONN";
  if (x == ENOTCONN) return "ENOTCONN";
#ifdef ESHUTDOWN
  if (x == ESHUTDOWN) return "ESHUTDOWN";
#endif
#ifdef ETOOMANYREFS
  if (x == ETOOMANYREFS) return "ETOOMANYREFS";
#endif
  if (x == ETIMEDOUT) return "ETIMEDOUT";
  if (x == ECONNREFUSED) return "ECONNREFUSED";
#ifdef EHOSTDOWN
  if (x == EHOSTDOWN) return "EHOSTDOWN";
#endif
  if (x == EHOSTUNREACH) return "EHOSTUNREACH";
  if (x == EALREADY) return "EALREADY";
  if (x == EINPROGRESS) return "EINPROGRESS";
  if (x == ESTALE) return "ESTALE";
  if (x == EDQUOT) return "EDQUOT";
  if (x == ECANCELED) return "ECANCELED";
  if (x == EOWNERDEAD) return "EOWNERDEAD";
  if (x == ENOTRECOVERABLE) return "ENOTRECOVERABLE";
#ifdef ETIME
  if (x == ETIME) return "ETIME";
#endif
#ifdef ENONET
  if (x == ENONET) return "ENONET";
#endif
#ifdef ERESTART
  if (x == ERESTART) return "ERESTART";
#endif
#ifdef ENOSR
  if (x == ENOSR) return "ENOSR";
#endif
#ifdef ENOSTR
  if (x == ENOSTR) return "ENOSTR";
#endif
#ifdef ENODATA
  if (x == ENODATA) return "ENODATA";
#endif
#ifdef EMULTIHOP
  if (x == EMULTIHOP) return "EMULTIHOP";
#endif
#ifdef ENOLINK
  if (x == ENOLINK) return "ENOLINK";
#endif
#ifdef ENOMEDIUM
  if (x == ENOMEDIUM) return "ENOMEDIUM";
#endif
#ifdef EMEDIUMTYPE
  if (x == EMEDIUMTYPE) return "EMEDIUMTYPE";
#endif
  FormatInt64(buf, x);
  return buf;
}

const char *DescribeFlags(int flags) {
  _Thread_local static char b[7];
  b[0] = (flags & OF) ? 'O' : '.';
  b[1] = (flags & SF) ? 'S' : '.';
  b[2] = (flags & ZF) ? 'Z' : '.';
  b[3] = (flags & AF) ? 'A' : '.';
  b[4] = (flags & PF) ? 'P' : '.';
  b[5] = (flags & CF) ? 'C' : '.';
  return b;
}

const char *DescribeOp(struct Machine *m, i64 pc) {
  _Thread_local static char b[256];
  int e, i, k, o = 0, n = sizeof(b);
  struct Dis d = {true};
  char spec[64];
  if (!(e = GetInstruction(m, pc, d.xedd))) {
    o = DisInst(&d, b, DisSpec(d.xedd, spec)) - b;
  }
  if (e != kMachineSegmentationFault) {
    k = MAX(8, d.xedd->length);
    for (i = 0; i < k; ++i) {
      APPEND(" %02x", d.xedd->bytes[i]);
    }
  } else {
    APPEND("segfault");
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
  if ((fd = OverlaysOpen(AT_FDCWD, buf, O_RDONLY, 0)) != -1) {
    if (fstat(fd, &st) != -1 &&
        (elfmap = Mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0,
                       "debug")) != MAP_FAILED) {
      elf->ehdr = (Elf64_Ehdr_ *)elfmap;
      elf->size = st.st_size;
    }
    close(fd);
  }
}

void PrintFds(struct Fds *fds) {
  struct Dll *e;
  LOGF("%-8s %-8s", "fildes", "oflags");
  for (e = dll_first(fds->list); e; e = dll_next(fds->list, e)) {
    LOGF("%-8d %-8x", FD_CONTAINER(e)->fildes, FD_CONTAINER(e)->oflags);
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
         m->cs.base + MaskAddress(m->mode, m->ip), DescribeOp(m, GetPc(m)),
         Get64(m->ax), Get64(m->cx), Get64(m->dx), Get64(m->bx), Get64(m->sp),
         Get64(m->bp), Get64(m->si), Get64(m->di), Get64(m->r8), Get64(m->r9),
         Get64(m->r10), Get64(m->r11), Get64(m->r12), Get64(m->r13),
         Get64(m->r14), Get64(m->r15), m->fs.base, m->gs.base,
         GET_COUNTER(instructions_decoded), GET_COUNTER(instructions_jitted),
         g_progname);

  rp = m->ip;
  bp = Get64(m->bp);
  sp = Get64(m->sp);
  for (i = 0; i < MAX_BACKTRACE_LINES;) {
    if (i) APPEND("\n\t");
    sym = DisFindSym(&dis, rp);
    APPEND("%012" PRIx64 " %012" PRIx64 " %s", m->ss.base + bp, rp,
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
    if (((m->ss.base + bp) & 0xfff) > 0xff0) break;
    if (!(r = LookupAddress(m, m->ss.base + bp))) {
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

// use cosmopolitan/tool/build/fastdiff.c
void LogCpu(struct Machine *m) {
  static FILE *f;
  if (!f) f = fopen("/tmp/cpu.log", "w");
  fprintf(f,
          "\n"
          "IP %" PRIx64 "\n"
          "AX %#" PRIx64 "\n"
          "CX %#" PRIx64 "\n"
          "DX %#" PRIx64 "\n"
          "BX %#" PRIx64 "\n"
          "SP %#" PRIx64 "\n"
          "BP %#" PRIx64 "\n"
          "SI %#" PRIx64 "\n"
          "DI %#" PRIx64 "\n"
          "R8 %#" PRIx64 "\n"
          "R9 %#" PRIx64 "\n"
          "R10 %#" PRIx64 "\n"
          "R11 %#" PRIx64 "\n"
          "R12 %#" PRIx64 "\n"
          "R13 %#" PRIx64 "\n"
          "R14 %#" PRIx64 "\n"
          "R15 %#" PRIx64 "\n"
          "FLAGS %s\n"
          "%s\n",
          m->ip, Read64(m->ax), Read64(m->cx), Read64(m->dx), Read64(m->bx),
          Read64(m->sp), Read64(m->bp), Read64(m->si), Read64(m->di),
          Read64(m->r8), Read64(m->r9), Read64(m->r10), Read64(m->r11),
          Read64(m->r12), Read64(m->r13), Read64(m->r14), Read64(m->r15),
          DescribeFlags(m->flags), DescribeOp(m, GetPc(m)));
}

bool CheckMemoryInvariants(struct System *s) {
  if (s->rss == s->memstat.tables + s->memstat.committed &&
      s->vss == s->memstat.committed + s->memstat.reserved) {
    return true;
  } else {
    ERRF("%-10s = %" PRIx64 " vs. %ld", "rss", s->rss,
         s->memstat.tables + s->memstat.committed);
    ERRF("%-10s = %" PRIx64 " vs. %ld", "vss", s->vss,
         s->memstat.committed + s->memstat.reserved);
    ERRF("%-10s = %ld", "tables", s->memstat.tables);
    ERRF("%-10s = %ld", "reserved", s->memstat.reserved);
    ERRF("%-10s = %ld", "committed", s->memstat.committed);
    return false;
  }
}
