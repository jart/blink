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
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include "blink/assert.h"
#include <wctype.h>

#include "blink/address.h"
#include "blink/breakpoint.h"
#include "blink/builtin.h"
#include "blink/case.h"
#include "blink/cga.h"
#include "blink/cp437.h"
#include "blink/dis.h"
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/fpu.h"
#include "blink/high.h"
#include "blink/loader.h"
#include "blink/log.h"
#include "blink/macros.h"
#include "blink/mda.h"
#include "blink/memory.h"
#include "blink/modrm.h"
#include "blink/panel.h"
#include "blink/pml4t.h"
#include "blink/pty.h"
#include "blink/signal.h"
#include "blink/strwidth.h"
#include "blink/termios.h"
#include "blink/thompike.h"
#include "blink/tpenc.h"
#include "blink/util.h"
#include "blink/xlat.h"
#include "blink/xmmtype.h"

#define USAGE \
  " [-?HhrRstv] [ROM] [ARGS...]\r\n\
\r\n\
DESCRIPTION\r\n\
\r\n\
  Emulates x86 Linux Programs w/ Dense Machine State Visualization\r\n\
  Please keep still and only watchen astaunished das blinkenlights\r\n\
\r\n\
FLAGS\r\n\
\r\n\
  -h        help\r\n\
  -z        zoom\r\n\
  -v        verbosity\r\n\
  -r        real mode\r\n\
  -s        statistics\r\n\
  -H        disable highlight\r\n\
  -t        disable tui mode\r\n\
  -R        disable reactive\r\n\
  -b ADDR   push a breakpoint\r\n\
  -L PATH   log file location\r\n\
\r\n\
ARGUMENTS\r\n\
\r\n\
  ROM files can be ELF or a flat αcτµαlly pδrταblε εxεcµταblε.\r\n\
  It should use x86_64 in accordance with the System Five ABI.\r\n\
  The SYSCALL ABI is defined as it is written in Linux Kernel.\r\n\
\r\n\
FEATURES\r\n\
\r\n\
  8086, 8087, i386, x86_64, SSE3, SSSE3, POPCNT, MDA, CGA, TTY\r\n\
  Type ? for keyboard shortcuts and CLI flags inside emulator.\r\n\
\r\n"

#define HELP \
  "\033[1mBLINKENLIGHTS v1.o\033[22m\
                 https://justine.lol/blinkenlights/\n\
\n\
KEYBOARD SHORTCUTS                 CLI FLAGS\n\
\n\
ctrl-c  interrupt                  -t       tui mode\n\
s       step                       -r       real mode\n\
n       next                       -s       statistics\n\
c       continue                   -b ADDR  push breakpoint\n\
q       quit                       -L PATH  log file location\n\
f       finish                     -R       reactive tui mode\n\
R       restart                    -H       disable highlighting\n\
x       hex                        -v       increase verbosity\n\
?       help                       -?       help\n\
t       sse type\n\
w       sse width\n\
B       pop breakpoint\n\
ctrl-t  turbo\n\
alt-t   slowmo"

#define MAXZOOM    16
#define DUMPWIDTH  64
#define DISPWIDTH  80
#define WHEELDELTA 1

#define RESTART  0x001
#define REDRAW   0x002
#define CONTINUE 0x004
#define STEP     0x008
#define NEXT     0x010
#define FINISH   0x020
#define FAILURE  0x040
#define WINCHED  0x080
#define INT      0x100
#define QUIT     0x200
#define EXIT     0x400
#define ALARM    0x800

#define kXmmDecimal 0
#define kXmmHex     1
#define kXmmChar    2

#define kMouseLeftDown      0
#define kMouseMiddleDown    1
#define kMouseRightDown     2
#define kMouseLeftUp        4
#define kMouseMiddleUp      5
#define kMouseRightUp       6
#define kMouseLeftDrag      32
#define kMouseMiddleDrag    33
#define kMouseRightDrag     34
#define kMouseWheelUp       64
#define kMouseWheelDown     65
#define kMouseCtrlWheelUp   80
#define kMouseCtrlWheelDown 81

#define kAsanScale              3
#define kAsanMagic              0x7fff8000
#define kAsanHeapFree           -1
#define kAsanStackFree          -2
#define kAsanRelocated          -3
#define kAsanHeapUnderrun       -4
#define kAsanHeapOverrun        -5
#define kAsanGlobalOverrun      -6
#define kAsanGlobalUnregistered -7
#define kAsanStackUnderrun      -8
#define kAsanStackOverrun       -9
#define kAsanAllocaUnderrun     -10
#define kAsanAllocaOverrun      -11
#define kAsanUnscoped           -12
#define kAsanUnmapped           -13

#define Ctrl(C) ((C) ^ 0100)

#ifdef IUTF8
#define HR L'─'
#else
#define HR '-'
#endif

struct MemoryView {
  int64_t start;
  int zoom;
};

struct Panels {
  union {
    struct {
      struct Panel disassembly;
      struct Panel breakpointshr;
      struct Panel breakpoints;
      struct Panel mapshr;
      struct Panel maps;
      struct Panel frameshr;
      struct Panel frames;
      struct Panel displayhr;
      struct Panel display;
      struct Panel registers;
      struct Panel ssehr;
      struct Panel sse;
      struct Panel codehr;
      struct Panel code;
      struct Panel readhr;
      struct Panel readdata;
      struct Panel writehr;
      struct Panel writedata;
      struct Panel stackhr;
      struct Panel stack;
      struct Panel status;
    };
    struct Panel p[21];
  };
};

static const char kRegisterNames[16][4] = {
    "RAX", "RCX", "RDX", "RBX", "RSP", "RBP", "RSI", "RDI",
    "R8",  "R9",  "R10", "R11", "R12", "R13", "R14", "R15",
};

extern char **environ;

static bool react;
static bool tuimode;
static bool alarmed;
static bool mousemode;
static bool printstats;
static bool showhighsse;

static int tyn;
static int txn;
static int tick;
static int speed;
static int vidya;
static int ttyin;
static int focus;
static int ttyout;
static int opline;
static int action;
static int xmmdisp;
static int verbose;
static int exitcode;

static long ips;
static char *dialog;
static char *codepath;
static void *onbusted;
static int64_t oldlen;
static struct Pty *pty;
static int64_t opstart;
static struct Machine *m;
static int64_t mapsstart;
static uint64_t readaddr;
static uint64_t readsize;
static uint64_t writeaddr;
static uint64_t writesize;
static char *statusmessage;
static int64_t framesstart;
static uint64_t last_opcount;
static unsigned long opcount;
static int64_t breakpointsstart;

static struct Panels pan;
static struct Signals signals;
static struct Breakpoints breakpoints;
static struct MemoryView codeview;
static struct MemoryView readview;
static struct MemoryView writeview;
static struct MemoryView stackview;
static struct MachineState laststate;
static struct MachineMemstat lastmemstat;
static struct XmmType xmmtype;
static struct Elf elf[1];
static struct Dis dis[1];

static long double last_seconds;
static long double statusexpires;
static struct termios oldterm;
static char systemfailure[128];
static struct sigaction oldsig[4];

static void SetupDraw(void);
static void Redraw(void);

const char kXedErrorNames[] = "\
none\0\
buffer too short\0\
general error\0\
invalid for chip\0\
bad register\0\
bad lock prefix\0\
bad rep prefix\0\
bad legacy prefix\0\
bad rex prefix\0\
bad evex ubit\0\
bad map\0\
bad evex v prime\0\
bad evex z no masking\0\
no output pointer\0\
no agen call back registered\0\
bad memop index\0\
callback problem\0\
gather regs\0\
instr too long\0\
invalid mode\0\
bad evex ll\0\
unimplemented\0\
";

static char *FormatDouble(char buf[32], long double x) {
  snprintf(buf, 32, "%Lg", x);
  return buf;
}

static int64_t SignExtend(uint64_t x, char b) {
  char k;
  assert(1 <= b && b <= 64);
  k = 64 - b;
  return (int64_t)(x << k) >> k;
}

static void SetCarry(bool cf) {
  m->flags = SetFlag(m->flags, FLAGS_CF, cf);
}

static bool IsCall(void) {
  int dispatch = m->xedd->op.map << 8 | m->xedd->op.opcode;
  return (dispatch == 0x0E8 ||
          (dispatch == 0x0FF && ModrmReg(m->xedd->op.rde) == 2));
}

static bool IsDebugBreak(void) {
  return m->xedd->op.map == XED_ILD_MAP0 && m->xedd->op.opcode == 0xCC;
}

static bool IsRet(void) {
  switch (m->xedd->op.map << 8 | m->xedd->op.opcode) {
    case 0xC2:
    case 0xC3:
    case 0xCA:
    case 0xCB:
    case 0xCF:
      return true;
    default:
      return false;
  }
}

static int GetXmmTypeCellCount(int r) {
  switch (xmmtype.type[r]) {
    case kXmmIntegral:
      return 16 / xmmtype.size[r];
    case kXmmFloat:
      return 4;
    case kXmmDouble:
      return 2;
    default:
      __builtin_unreachable();
  }
}

static uint8_t CycleXmmType(uint8_t t) {
  switch (t) {
    default:
    case kXmmIntegral:
      return kXmmFloat;
    case kXmmFloat:
      return kXmmDouble;
    case kXmmDouble:
      return kXmmIntegral;
  }
}

static uint8_t CycleXmmDisp(uint8_t t) {
  switch (t) {
    default:
    case kXmmDecimal:
      return kXmmHex;
    case kXmmHex:
      return kXmmChar;
    case kXmmChar:
      return kXmmDecimal;
  }
}

static uint8_t CycleXmmSize(uint8_t w) {
  switch (w) {
    default:
    case 1:
      return 2;
    case 2:
      return 4;
    case 4:
      return 8;
    case 8:
      return 1;
  }
}

static int GetPointerWidth(void) {
  return 2 << (m->mode & 3);
}

static int64_t GetIp(void) {
  switch (GetPointerWidth()) {
    default:
    case 8:
      return m->ip;
    case 4:
      return Read64(m->cs) + (m->ip & 0xffff);
    case 2:
      return Read64(m->cs) + (m->ip & 0xffff);
  }
}

static int64_t GetSp(void) {
  switch (GetPointerWidth()) {
    default:
    case 8:
      return Read64(m->sp);
    case 4:
      return Read64(m->ss) + Read32(m->sp);
    case 2:
      return Read64(m->ss) + Read16(m->sp);
  }
}

static int64_t ReadWord(uint8_t *p) {
  switch (GetPointerWidth()) {
    default:
    case 8:
      return Read64(p);
    case 4:
      return Read32(p);
    case 2:
      return Read16(p);
  }
}

static void CopyMachineState(struct MachineState *ms) {
  ms->ip = m->ip;
  memcpy(ms->cs, m->cs, sizeof(m->cs));
  memcpy(ms->ss, m->ss, sizeof(m->ss));
  memcpy(ms->es, m->es, sizeof(m->es));
  memcpy(ms->ds, m->ds, sizeof(m->ds));
  memcpy(ms->fs, m->fs, sizeof(m->fs));
  memcpy(ms->gs, m->gs, sizeof(m->gs));
  memcpy(ms->reg, m->reg, sizeof(m->reg));
  memcpy(ms->xmm, m->xmm, sizeof(m->xmm));
  memcpy(&ms->fpu, &m->fpu, sizeof(m->fpu));
  memcpy(&ms->mxcsr, &m->mxcsr, sizeof(m->mxcsr));
}

/**
 * Handles file mapped page faults in valid page but past eof.
 */
static void OnSigBusted(int sig) {
  longjmp(onbusted, 1);
}

/**
 * Returns true if 𝑣 is a shadow memory virtual address.
 */
static bool IsShadow(int64_t v) {
  return 0x7fff8000 <= v && v < 0x100080000000;
}

/**
 * Returns glyph representing byte at virtual address 𝑣.
 */
static int VirtualBing(int64_t v) {
  int rc;
  uint8_t *p;
  jmp_buf busted;
  onbusted = busted;
  if ((p = FindReal(m, v))) {
    if (!setjmp(busted)) {
      rc = kCp437[p[0] & 255];
    } else {
      rc = L'≀';
    }
  } else {
    rc = L'⋅';
  }
  onbusted = NULL;
  return rc;
}

/**
 * Returns ASAN shadow uint8 concomitant to address 𝑣 or -1.
 */
static int VirtualShadow(int64_t v) {
  int rc;
  char *p;
  jmp_buf busted;
  if (IsShadow(v)) return -2;
  onbusted = busted;
  if ((p = FindReal(m, (v >> 3) + 0x7fff8000))) {
    if (!setjmp(busted)) {
      rc = p[0] & 0xff;
    } else {
      rc = -1;
    }
  } else {
    rc = -1;
  }
  onbusted = NULL;
  return rc;
}

static void ScrollOp(struct Panel *p, int64_t op) {
  int64_t n;
  opline = op;
  if ((n = p->bottom - p->top) > 1) {
    if (!(opstart + 1 <= op && op < opstart + n)) {
      opstart = MIN(MAX(0, op - n / 8), MAX(0, dis->ops.i - n));
    }
  }
}

static int TtyWriteString(const char *s) {
  return write(ttyout, s, strlen(s));
}

static void OnFeed(void) {
  TtyWriteString("\033[K\033[2J");
}

static void HideCursor(void) {
  TtyWriteString("\033[?25l");
}

static void ShowCursor(void) {
  TtyWriteString("\033[?25h");
}

static void EnableMouseTracking(void) {
  mousemode = true;
  TtyWriteString("\033[?1000;1002;1015;1006h");
}

static void DisableMouseTracking(void) {
  mousemode = false;
  TtyWriteString("\033[?1000;1002;1015;1006l");
}

static void ToggleMouseTracking(void) {
  if (mousemode) {
    DisableMouseTracking();
  } else {
    EnableMouseTracking();
  }
}

static void LeaveScreen(void) {
  char buf[64];
  sprintf(buf, "\033[%d;%dH\033[S\r\n", tyn, txn);
  TtyWriteString(buf);
}

static void GetTtySize(int fd) {
  struct winsize wsize;
  wsize.ws_row = tyn;
  wsize.ws_col = txn;
  ioctl(fd, TIOCGWINSZ, &wsize);
  tyn = wsize.ws_row;
  txn = wsize.ws_col;
}

static void TuiRejuvinate(void) {
  struct termios term;
  struct sigaction sa;
  GetTtySize(ttyout);
  HideCursor();
  memcpy(&term, &oldterm, sizeof(term));
  term.c_cc[VMIN] = 1;
  term.c_cc[VTIME] = 1;
  term.c_iflag &= ~(IXOFF | INPCK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR |
                    ICRNL | IXON | IGNPAR);
  term.c_iflag |= IGNBRK | ISIG;
  term.c_lflag &=
      ~(IEXTEN | ICANON | ECHO | ECHOE | ECHONL | NOFLSH | TOSTOP | ECHOK);
  term.c_cflag &= ~(CSIZE | PARENB);
  term.c_cflag |= CS8 | CREAD;
  term.c_oflag &= ~OPOST;
#ifdef IMAXBEL
  term.c_iflag &= ~IMAXBEL;
#endif
#ifdef ECHOK
  term.c_lflag &= ~ECHOK;
#endif
#ifdef PENDIN
  term.c_lflag &= ~PENDIN;
#endif
#ifdef IUTF8
  term.c_iflag |= IUTF8;
#endif
  ioctl(ttyout, TCSETS, &term);
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = OnSigBusted;
  sa.sa_flags = SA_NODEFER;
  sigaction(SIGBUS, &sa, 0);
  EnableMouseTracking();
}

static void OnQ(void) {
  action |= EXIT;
}

static void OnV(void) {
  vidya = !vidya;
}

static void OnSignal(int sig, siginfo_t *si, void *uc) {
  EnqueueSignal(m, &signals, sig, si->si_code);
}

static void OnSigWinch(int sig, siginfo_t *si, void *uc) {
  EnqueueSignal(m, &signals, sig, si->si_code);
  action |= WINCHED;
}

static void OnSigInt(int sig, siginfo_t *si, void *uc) {
  action |= INT;
}

static void OnSigAlrm(int sig, siginfo_t *si, void *uc) {
  EnqueueSignal(m, &signals, sig, si->si_code);
  action |= ALARM;
}

static void OnSigCont(int sig, siginfo_t *si, void *uc) {
  if (tuimode) {
    TuiRejuvinate();
    Redraw();
  }
  EnqueueSignal(m, &signals, sig, si->si_code);
}

static void TtyRestore1(void) {
  ShowCursor();
  TtyWriteString("\033[0m");
}

static void TtyRestore2(void) {
  ioctl(ttyout, TCSETS, &oldterm);
  DisableMouseTracking();
}

static void TuiCleanup(void) {
  sigaction(SIGCONT, oldsig + 2, 0);
  TtyRestore1();
  DisableMouseTracking();
  tuimode = false;
  LeaveScreen();
}

static void ResolveBreakpoints(void) {
  long i, sym;
  for (i = 0; i < breakpoints.i; ++i) {
    if (breakpoints.p[i].symbol && !breakpoints.p[i].addr) {
      if ((sym = DisFindSymByName(dis, breakpoints.p[i].symbol)) != -1) {
        breakpoints.p[i].addr = dis->syms.p[sym].addr;
      } else {
        fprintf(
            stderr,
            "error: breakpoint not found: %s (out of %zu loaded symbols)\r\n",
            breakpoints.p[i].symbol, dis->syms.i);
        exit(1);
      }
    }
  }
}

static void BreakAtNextInstruction(void) {
  struct Breakpoint b;
  memset(&b, 0, sizeof(b));
  b.addr = GetIp() + m->xedd->length;
  b.oneshot = true;
  PushBreakpoint(&breakpoints, &b);
}

static void LoadSyms(void) {
  LoadDebugSymbols(elf);
  DisLoadElf(dis, elf);
}

static int DrainInput(int fd) {
  char buf[32];
  struct pollfd fds[1];
  for (;;) {
    fds[0].fd = fd;
    fds[0].events = POLLIN;
    if (poll(fds, ARRAYLEN(fds), 0) == -1) return -1;
    if (!(fds[0].revents & POLLIN)) break;
    if (read(fd, buf, sizeof(buf)) == -1) return -1;
  }
  return 0;
}

static int ReadCursorPosition(int *out_y, int *out_x) {
  int y, x;
  char *p, buf[32];
  if (readansi(ttyin, buf, sizeof(buf)) == 1) return -1;
  p = buf;
  if (*p == 033) ++p;
  if (*p == '[') ++p;
  y = strtol(p, &p, 10);
  if (*p == ';') ++p;
  x = strtol(p, &p, 10);
  if (*p != 'R') {
    errno = EBADMSG;
    return -1;
  }
  if (out_y) *out_y = MAX(1, y) - 1;
  if (out_x) *out_x = MAX(1, x) - 1;
  return 0;
}

static int GetCursorPosition(int *out_y, int *out_x) {
  TtyWriteString("\033[6n");
  return ReadCursorPosition(out_y, out_x);
}

void CommonSetup(void) {
  static bool once;
  if (!once) {
    if (tuimode || breakpoints.i) {
      LoadSyms();
      ResolveBreakpoints();
    }
    once = true;
  }
}

void TuiSetup(void) {
  int y;
  bool report;
  char buf[64];
  static bool once;
  struct itimerval it;
  struct sigaction sa;
  report = false;
  if (!once) {
    /* LOGF("loaded program %s\r\n%s", codepath, gc(FormatPml4t(m))); */
    CommonSetup();
    ioctl(ttyout, TCGETS, &oldterm);
    atexit(TtyRestore2);
    once = true;
    report = true;
  }
  memset(&it, 0, sizeof(it));
  setitimer(ITIMER_REAL, &it, 0);
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = OnSigCont;
  sa.sa_flags = SA_RESTART | SA_NODEFER | SA_SIGINFO;
  sigaction(SIGCONT, &sa, oldsig + 2);
  CopyMachineState(&laststate);
  TuiRejuvinate();
  if (report) {
    DrainInput(ttyin);
    y = 0;
    if (GetCursorPosition(&y, NULL) != -1) {
      sprintf(buf, "\033[%dS", y);
      TtyWriteString(buf);
    }
  }
}

static void ExecSetup(void) {
  struct itimerval it;
  CommonSetup();
  it.it_interval.tv_sec = 0;
  it.it_interval.tv_usec = 1. / 60 * 1e6;
  it.it_value.tv_sec = 0;
  it.it_value.tv_usec = 1. / 60 * 1e6;
  setitimer(ITIMER_REAL, &it, NULL);
}

static void AppendPanel(struct Panel *p, int64_t line, const char *s) {
  if (0 <= line && line < p->bottom - p->top) {
    AppendStr(&p->lines[line], s);
  }
}

static void pcmpeqb(uint8_t x[16], const uint8_t y[16]) {
  for (int i = 0; i < 16; ++i) x[i] = -(x[i] == y[i]);
}

static unsigned pmovmskb(const uint8_t p[16]) {
  unsigned i, m;
  for (m = i = 0; i < 16; ++i) m |= !!(p[i] & 0x80) << i;
  return m;
}

static bool IsXmmNonZero(int64_t start, int64_t end) {
  int64_t i;
  uint8_t v1[16], vz[16];
  for (i = start; i < end; ++i) {
    memset(vz, 0, 16);
    memcpy(v1, m->xmm[i], 16);
    pcmpeqb(v1, vz);
    if (pmovmskb(v1) != 0xffff) {
      return true;
    }
  }
  return false;
}

static bool IsSegNonZero(void) {
  unsigned i;
  for (i = 0; i < 6; ++i) {
    if (Read64(GetSegment(m, 0, i))) {
      return true;
    }
  }
  return false;
}

static int PickNumberOfXmmRegistersToShow(void) {
  if (IsXmmNonZero(0, 8) || IsXmmNonZero(8, 16)) {
    if (showhighsse || IsXmmNonZero(8, 16)) {
      showhighsse = true;
      return 16;
    } else {
      return 8;
    }
  } else {
    showhighsse = false;
    return 0;
  }
}

void SetupDraw(void) {
  int i, j, n, a, b, c, yn, cpuy, ssey, dx[2], c2y[3], c3y[5];

  cpuy = 9;
  if (IsSegNonZero()) cpuy += 2;
  ssey = PickNumberOfXmmRegistersToShow();
  if (ssey) ++ssey;

  a = 12 + 1 + DUMPWIDTH;
  b = DISPWIDTH + 1;
  c = txn - a - b;
  if (c > DISPWIDTH) c = DISPWIDTH;
  if (c > 0) {
    dx[1] = txn >= a + b ? txn - a : txn;
  } else {
    dx[1] = txn - a;
  }
  dx[0] = txn >= a + c ? txn - a - c : dx[1];

  yn = tyn - 1;
  a = 1 / 8. * yn;
  b = 3 / 8. * yn;
  c2y[0] = a * .7;
  c2y[1] = a * 2;
  c2y[2] = a * 2 + b;
  if (yn - c2y[2] > 26) {
    c2y[1] -= yn - c2y[2] - 26;
    c2y[2] = yn - 26;
  }
  if (yn - c2y[2] < 26) {
    c2y[2] = yn - 26;
  }

  a = (yn - (cpuy + ssey) - 3) / 4;
  c3y[0] = cpuy;
  c3y[1] = cpuy + ssey;
  c3y[2] = cpuy + ssey + 1 + 1 + a * 1;
  c3y[3] = cpuy + ssey + 1 + 1 + a * 2 + 1;
  c3y[4] = cpuy + ssey + 1 + 1 + 1 + a * 3 + 1;

  /* COLUMN #1: DISASSEMBLY */

  pan.disassembly.top = 0;
  pan.disassembly.left = 0;
  pan.disassembly.bottom = yn;
  pan.disassembly.right = dx[0] - 1;

  /* COLUMN #2: BREAKPOINTS, MEMORY MAPS, BACKTRACE, DISPLAY */

  pan.breakpointshr.top = 0;
  pan.breakpointshr.left = dx[0];
  pan.breakpointshr.bottom = 1;
  pan.breakpointshr.right = dx[1] - 1;

  pan.breakpoints.top = 1;
  pan.breakpoints.left = dx[0];
  pan.breakpoints.bottom = c2y[0];
  pan.breakpoints.right = dx[1] - 1;

  pan.mapshr.top = c2y[0];
  pan.mapshr.left = dx[0];
  pan.mapshr.bottom = c2y[0] + 1;
  pan.mapshr.right = dx[1] - 1;

  pan.maps.top = c2y[0] + 1;
  pan.maps.left = dx[0];
  pan.maps.bottom = c2y[1];
  pan.maps.right = dx[1] - 1;

  pan.frameshr.top = c2y[1];
  pan.frameshr.left = dx[0];
  pan.frameshr.bottom = c2y[1] + 1;
  pan.frameshr.right = dx[1] - 1;

  pan.frames.top = c2y[1] + 1;
  pan.frames.left = dx[0];
  pan.frames.bottom = c2y[2];
  pan.frames.right = dx[1] - 1;

  pan.displayhr.top = c2y[2];
  pan.displayhr.left = dx[0];
  pan.displayhr.bottom = c2y[2] + 1;
  pan.displayhr.right = dx[1] - 1;

  pan.display.top = c2y[2] + 1;
  pan.display.left = dx[0];
  pan.display.bottom = yn;
  pan.display.right = dx[1] - 1;

  /* COLUMN #3: REGISTERS, VECTORS, CODE, MEMORY READS, MEMORY WRITES, STACK */

  pan.registers.top = 0;
  pan.registers.left = dx[1];
  pan.registers.bottom = c3y[0];
  pan.registers.right = txn;

  pan.ssehr.top = c3y[0];
  pan.ssehr.left = dx[1];
  pan.ssehr.bottom = c3y[0] + (ssey ? 1 : 0);
  pan.ssehr.right = txn;

  pan.sse.top = c3y[0] + (ssey ? 1 : 0);
  pan.sse.left = dx[1];
  pan.sse.bottom = c3y[1];
  pan.sse.right = txn;

  pan.codehr.top = c3y[1];
  pan.codehr.left = dx[1];
  pan.codehr.bottom = c3y[1] + 1;
  pan.codehr.right = txn;

  pan.code.top = c3y[1] + 1;
  pan.code.left = dx[1];
  pan.code.bottom = c3y[2];
  pan.code.right = txn;

  pan.readhr.top = c3y[2];
  pan.readhr.left = dx[1];
  pan.readhr.bottom = c3y[2] + 1;
  pan.readhr.right = txn;

  pan.readdata.top = c3y[2] + 1;
  pan.readdata.left = dx[1];
  pan.readdata.bottom = c3y[3];
  pan.readdata.right = txn;

  pan.writehr.top = c3y[3];
  pan.writehr.left = dx[1];
  pan.writehr.bottom = c3y[3] + 1;
  pan.writehr.right = txn;

  pan.writedata.top = c3y[3] + 1;
  pan.writedata.left = dx[1];
  pan.writedata.bottom = c3y[4];
  pan.writedata.right = txn;

  pan.stackhr.top = c3y[4];
  pan.stackhr.left = dx[1];
  pan.stackhr.bottom = c3y[4] + 1;
  pan.stackhr.right = txn;

  pan.stack.top = c3y[4] + 1;
  pan.stack.left = dx[1];
  pan.stack.bottom = yn;
  pan.stack.right = txn;

  pan.status.top = yn;
  pan.status.left = 0;
  pan.status.bottom = yn + 1;
  pan.status.right = txn;

  for (i = 0; i < ARRAYLEN(pan.p); ++i) {
    if (pan.p[i].left > pan.p[i].right) {
      pan.p[i].left = pan.p[i].right = 0;
    }
    if (pan.p[i].top > pan.p[i].bottom) {
      pan.p[i].top = pan.p[i].bottom = 0;
    }
    n = pan.p[i].bottom - pan.p[i].top;
    if (n == pan.p[i].n) {
      for (j = 0; j < n; ++j) {
        pan.p[i].lines[j].i = 0;
      }
    } else {
      for (j = 0; j < pan.p[i].n; ++j) {
        free(pan.p[i].lines[j].p);
      }
      free(pan.p[i].lines);
      pan.p[i].lines = calloc(n, sizeof(struct Buffer));
      pan.p[i].n = n;
    }
  }

  PtyResize(pty, pan.display.bottom - pan.display.top,
            pan.display.right - pan.display.left);
}

static int64_t Disassemble(void) {
  int64_t lines;
  lines = pan.disassembly.bottom - pan.disassembly.top * 2;
  if (Dis(dis, m, GetIp(), m->ip, lines) != -1) {
    return DisFind(dis, GetIp());
  } else {
    return -1;
  }
}

static int64_t GetDisIndex(void) {
  int64_t i;
  if ((i = DisFind(dis, GetIp())) == -1) {
    i = Disassemble();
  }
  while (i + 1 < dis->ops.i && !dis->ops.p[i].size) ++i;
  return i;
}

static void DrawDisassembly(struct Panel *p) {
  int64_t i, j;
  for (i = 0; i < p->bottom - p->top; ++i) {
    j = opstart + i;
    if (0 <= j && j < dis->ops.i) {
      if (j == opline) AppendPanel(p, i, "\033[7m");
      AppendPanel(p, i, DisGetLine(dis, m, j));
      if (j == opline) AppendPanel(p, i, "\033[27m");
    }
  }
}

static void DrawHr(struct Panel *p, const char *s) {
  int64_t i, wp, ws, wl, wr;
  if (p->bottom - p->top < 1) return;
  wp = p->right - p->left;
  ws = 8;
  wl = wp / 4 - ws / 2;
  wr = wp - (wl + strwidth(s, 0));
  for (i = 0; i < wl; ++i) AppendWide(&p->lines[0], HR);
  AppendStr(&p->lines[0], s);
  for (i = 0; i < wr; ++i) AppendWide(&p->lines[0], HR);
  AppendStr(&p->lines[0], "\033[0m");
}

static void DrawTerminalHr(struct Panel *p) {
  int64_t i, wp, ws, wl;
  struct itimerval it;
  if (p->bottom == p->top) return;
  if (pty->conf & kPtyBell) {
    if (!alarmed) {
      alarmed = true;
      it.it_interval.tv_sec = 0;
      it.it_interval.tv_usec = 0;
      it.it_value.tv_sec = 0;
      it.it_value.tv_usec = 800000;
      setitimer(ITIMER_REAL, &it, 0);
    }
    AppendStr(&p->lines[0], "\033[1m");
  }
  wp = p->right - p->left;
  ws = 8;
  wl = wp / 4 - ws / 2;
  for (i = 0; i < wl; ++i) AppendWide(&p->lines[0], HR);
#ifdef IUTF8
  AppendFmt(&p->lines[0], "TELETYPEWRITER──%s──%s──%s──%s",
            (pty->conf & kPtyLed1) ? "\033[1;31m◎\033[0m" : "○",
            (pty->conf & kPtyLed2) ? "\033[1;32m◎\033[0m" : "○",
            (pty->conf & kPtyLed3) ? "\033[1;33m◎\033[0m" : "○",
            (pty->conf & kPtyLed4) ? "\033[1;34m◎\033[0m" : "○");
#else
  AppendFmt(&p->lines[0], "TELETYPEWRITER--%s--%s--%s--%s",
            (pty->conf & kPtyLed1) ? "\033[1;31m@\033[0m" : "o",
            (pty->conf & kPtyLed2) ? "\033[1;32m@\033[0m" : "o",
            (pty->conf & kPtyLed3) ? "\033[1;33m@\033[0m" : "o",
            (pty->conf & kPtyLed4) ? "\033[1;34m@\033[0m" : "o");
#endif
  for (i = 26 + wl; i < p->right - p->left; ++i) {
    AppendWide(&p->lines[0], HR);
  }
}

static void DrawTerminal(struct Panel *p) {
  int64_t y, yn;
  if (p->top == p->bottom) return;
  for (yn = MIN(pty->yn, p->bottom - p->top), y = 0; y < yn; ++y) {
    PtyAppendLine(pty, p->lines + y, y);
    AppendStr(p->lines + y, "\033[0m");
  }
}

static void DrawDisplay(struct Panel *p) {
  switch (vidya) {
    case 7:
      DrawHr(&pan.displayhr, "MONOCHROME DISPLAY ADAPTER");
      if (0xb0000 + 25 * 80 * 2 > m->system->real.n) return;
      DrawMda(p, (void *)(m->system->real.p + 0xb0000));
      break;
    case 3:
      DrawHr(&pan.displayhr, "COLOR GRAPHICS ADAPTER");
      if (0xb8000 + 25 * 80 * 2 > m->system->real.n) return;
      DrawCga(p, (void *)(m->system->real.p + 0xb8000));
      break;
    default:
      DrawTerminalHr(&pan.displayhr);
      DrawTerminal(p);
      break;
  }
}

static void DrawFlag(struct Panel *p, int64_t i, char name, bool value) {
  char str[3] = "  ";
  if (value) str[1] = name;
  AppendPanel(p, i, str);
}

static void DrawRegister(struct Panel *p, int64_t i, int64_t r) {
  char buf[32];
  uint64_t value, previous;
  value = Read64(m->reg[r]);
  previous = Read64(laststate.reg[r]);
  if (value != previous) AppendPanel(p, i, "\033[7m");
  snprintf(buf, sizeof(buf), "%-3s", kRegisterNames[r]);
  AppendPanel(p, i, buf);
  AppendPanel(p, i, " ");
  snprintf(buf, sizeof(buf), "0x%016" PRIx64, value);
  AppendPanel(p, i, buf);
  if (value != previous) AppendPanel(p, i, "\033[27m");
  AppendPanel(p, i, "  ");
}

static void DrawSegment(struct Panel *p, int64_t i, const uint8_t seg[8],
                        const uint8_t last[8], const char *name) {
  char buf[32];
  uint64_t value, previous;
  value = Read64(seg);
  previous = Read64(last);
  if (value != previous) AppendPanel(p, i, "\033[7m");
  snprintf(buf, sizeof(buf), "%-3s", name);
  AppendPanel(p, i, buf);
  AppendPanel(p, i, " ");
  snprintf(buf, sizeof(buf), "0x%016" PRIx64, value);
  AppendPanel(p, i, buf);
  if (value != previous) AppendPanel(p, i, "\033[27m");
  AppendPanel(p, i, "  ");
}

static void DrawSt(struct Panel *p, int64_t i, int64_t r) {
  char buf[32];
  bool isempty, chg;
  long double value;
  isempty = FpuGetTag(m, r) == kFpuTagEmpty;
  if (isempty) AppendPanel(p, i, "\033[38;5;241m");
  value = m->fpu.st[(r + ((m->fpu.sw & kFpuSwSp) >> 11)) & 7];
  chg = value != laststate.fpu.st[(r + ((m->fpu.sw & kFpuSwSp) >> 11)) & 7];
  if (!isempty && chg) AppendPanel(p, i, "\033[7m");
  snprintf(buf, sizeof(buf), "ST%" PRId64 " ", r);
  AppendPanel(p, i, buf);
  AppendPanel(p, i, FormatDouble(buf, value));
  if (chg) AppendPanel(p, i, "\033[27m");
  AppendPanel(p, i, "  ");
  if (isempty) AppendPanel(p, i, "\033[39m");
}

static void DrawCpu(struct Panel *p) {
  char buf[48];
  if (p->top == p->bottom) return;
  DrawRegister(p, 0, 7), DrawRegister(p, 0, 0), DrawSt(p, 0, 0);
  DrawRegister(p, 1, 6), DrawRegister(p, 1, 3), DrawSt(p, 1, 1);
  DrawRegister(p, 2, 2), DrawRegister(p, 2, 5), DrawSt(p, 2, 2);
  DrawRegister(p, 3, 1), DrawRegister(p, 3, 4), DrawSt(p, 3, 3);
  DrawRegister(p, 4, 8), DrawRegister(p, 4, 12), DrawSt(p, 4, 4);
  DrawRegister(p, 5, 9), DrawRegister(p, 5, 13), DrawSt(p, 5, 5);
  DrawRegister(p, 6, 10), DrawRegister(p, 6, 14), DrawSt(p, 6, 6);
  DrawRegister(p, 7, 11), DrawRegister(p, 7, 15), DrawSt(p, 7, 7);
  snprintf(buf, sizeof(buf), "RIP 0x%016" PRIx64 "  FLG", m->ip);
  AppendPanel(p, 8, buf);
  DrawFlag(p, 8, 'C', GetFlag(m->flags, FLAGS_CF));
  DrawFlag(p, 8, 'P', GetFlag(m->flags, FLAGS_PF));
  DrawFlag(p, 8, 'A', GetFlag(m->flags, FLAGS_AF));
  DrawFlag(p, 8, 'Z', GetFlag(m->flags, FLAGS_ZF));
  DrawFlag(p, 8, 'S', GetFlag(m->flags, FLAGS_SF));
  DrawFlag(p, 8, 'I', GetFlag(m->flags, FLAGS_IF));
  DrawFlag(p, 8, 'D', GetFlag(m->flags, FLAGS_DF));
  DrawFlag(p, 8, 'O', GetFlag(m->flags, FLAGS_OF));
  AppendPanel(p, 8, "    ");
  if (m->fpu.sw & kFpuSwIe) AppendPanel(p, 8, " IE");
  if (m->fpu.sw & kFpuSwDe) AppendPanel(p, 8, " DE");
  if (m->fpu.sw & kFpuSwZe) AppendPanel(p, 8, " ZE");
  if (m->fpu.sw & kFpuSwOe) AppendPanel(p, 8, " OE");
  if (m->fpu.sw & kFpuSwUe) AppendPanel(p, 8, " UE");
  if (m->fpu.sw & kFpuSwPe) AppendPanel(p, 8, " PE");
  if (m->fpu.sw & kFpuSwSf) AppendPanel(p, 8, " SF");
  if (m->fpu.sw & kFpuSwEs) AppendPanel(p, 8, " ES");
  if (m->fpu.sw & kFpuSwC0) AppendPanel(p, 8, " C0");
  if (m->fpu.sw & kFpuSwC1) AppendPanel(p, 8, " C1");
  if (m->fpu.sw & kFpuSwC2) AppendPanel(p, 8, " C2");
  if (m->fpu.sw & kFpuSwBf) AppendPanel(p, 8, " BF");
  DrawSegment(p, 9, m->fs, laststate.fs, "FS");
  DrawSegment(p, 9, m->ds, laststate.ds, "DS");
  DrawSegment(p, 9, m->cs, laststate.cs, "CS");
  DrawSegment(p, 10, m->gs, laststate.gs, "GS");
  DrawSegment(p, 10, m->es, laststate.es, "ES");
  DrawSegment(p, 10, m->ss, laststate.ss, "SS");
}

static void DrawXmm(struct Panel *p, int64_t i, int64_t r) {
  float f;
  double d;
  wint_t ival;
  char buf[32];
  bool changed;
  uint64_t itmp;
  uint8_t xmm[16];
  int64_t j, k, n;
  int cells, left, cellwidth, panwidth;
  memcpy(xmm, m->xmm[r], sizeof(xmm));
  changed = memcmp(xmm, laststate.xmm[r], sizeof(xmm)) != 0;
  if (changed) AppendPanel(p, i, "\033[7m");
  left = sprintf(buf, "XMM%-2" PRId64 "", r);
  AppendPanel(p, i, buf);
  cells = GetXmmTypeCellCount(r);
  panwidth = p->right - p->left;
  cellwidth = MIN(MAX(0, (panwidth - left) / cells - 1), sizeof(buf) - 1);
  for (j = 0; j < cells; ++j) {
    AppendPanel(p, i, " ");
    switch (xmmtype.type[r]) {
      case kXmmFloat:
        memcpy(&f, xmm + j * sizeof(f), sizeof(f));
        FormatDouble(buf, f);
        break;
      case kXmmDouble:
        memcpy(&d, xmm + j * sizeof(d), sizeof(d));
        FormatDouble(buf, d);
        break;
      case kXmmIntegral:
        ival = 0;
        for (k = 0; k < xmmtype.size[r]; ++k) {
          itmp = xmm[j * xmmtype.size[r] + k] & 0xff;
          itmp <<= k * 8;
          ival |= itmp;
        }
        if (xmmdisp == kXmmHex || xmmdisp == kXmmChar) {
          if (xmmdisp == kXmmChar && iswalnum(ival)) {
            sprintf(buf, "%lc", ival);
          } else {
            sprintf(buf, "%.*x", xmmtype.size[r] * 8 / 4, ival);
          }
        } else {
          sprintf(buf, "%" PRId64, SignExtend(ival, xmmtype.size[r] * 8));
        }
        break;
      default:
        __builtin_unreachable();
    }
    buf[cellwidth] = '\0';
    AppendPanel(p, i, buf);
    n = cellwidth - strlen(buf);
    for (k = 0; k < n; ++k) {
      AppendPanel(p, i, " ");
    }
  }
  if (changed) AppendPanel(p, i, "\033[27m");
}

static void DrawSse(struct Panel *p) {
  int64_t i, n;
  n = p->bottom - p->top;
  if (n > 0) {
    for (i = 0; i < MIN(16, n); ++i) {
      DrawXmm(p, i, i);
    }
  }
}

static void ScrollMemoryView(struct Panel *p, struct MemoryView *v, int64_t a) {
  int64_t i, n, w;
  w = DUMPWIDTH * (1ull << v->zoom);
  n = p->bottom - p->top;
  i = a / w;
  if (!(v->start <= i && i < v->start + n)) {
    v->start = i - n / 4;
  }
}

static void ZoomMemoryView(struct MemoryView *v, int64_t y, int64_t x, int dy) {
  int64_t a, b, i, s;
  s = v->start;
  a = v->zoom;
  b = MIN(MAXZOOM, MAX(0, a + dy));
  i = y * DUMPWIDTH - x;
  s *= DUMPWIDTH * (1L << a);
  s += i * (1L << a) - i * (1L << b);
  s /= DUMPWIDTH * (1L << b);
  v->zoom = b;
  v->start = s;
}

static void ScrollMemoryViews(void) {
  ScrollMemoryView(&pan.code, &codeview, GetIp());
  ScrollMemoryView(&pan.readdata, &readview, readaddr);
  ScrollMemoryView(&pan.writedata, &writeview, writeaddr);
  ScrollMemoryView(&pan.stack, &stackview, GetSp());
}

static void ZoomMemoryViews(struct Panel *p, int y, int x, int dy) {
  if (p == &pan.code) {
    ZoomMemoryView(&codeview, y, x, dy);
  } else if (p == &pan.readdata) {
    ZoomMemoryView(&readview, y, x, dy);
  } else if (p == &pan.writedata) {
    ZoomMemoryView(&writeview, y, x, dy);
  } else if (p == &pan.stack) {
    ZoomMemoryView(&stackview, y, x, dy);
  }
}

static void DrawMemoryUnzoomed(struct Panel *p, struct MemoryView *view,
                               int64_t histart, int64_t hiend) {
  int c, s, x, sc;
  int64_t i, j, k;
  bool high, changed;
  high = false;
  for (i = 0; i < p->bottom - p->top; ++i) {
    AppendFmt(&p->lines[i], "%012" PRIx64 " ",
              ((view->start + i) * DUMPWIDTH) & 0xffffffffffff);
    for (j = 0; j < DUMPWIDTH; ++j) {
      k = (view->start + i) * DUMPWIDTH + j;
      c = VirtualBing(k);
      s = VirtualShadow(k);
      if (s != -1) {
        if (s == -2) {
          /* grey for shadow memory */
          x = 235;
        } else {
          sc = (signed char)s;
          if (sc > 7) {
            x = 129; /* PURPLE: shadow corruption */
          } else if (sc == kAsanHeapFree) {
            x = 20; /* BLUE: heap freed */
          } else if (sc == kAsanRelocated) {
            x = 16; /* BLACK: heap relocated */
          } else if (sc == kAsanHeapUnderrun || sc == kAsanAllocaUnderrun) {
            x = 53; /* RED+PURPLETINGE: heap underrun */
          } else if (sc == kAsanHeapOverrun || sc == kAsanAllocaOverrun) {
            x = 52; /* RED: heap overrun */
          } else if (sc < 0) {
            x = 52; /* RED: uncategorized invalid */
          } else if (sc > 0 && (k & 7) >= sc) {
            x = 88; /* BRIGHTRED: invalid address (skew) */
          } else if (!sc || (sc > 0 && (k & 7) < sc)) {
            x = 22; /* GREEN: valid address */
          } else {
            abort();
          }
        }
        AppendFmt(&p->lines[i], "\033[38;5;253;48;5;%dm", x);
      }
      changed = histart <= k && k < hiend;
      if (changed && !high) {
        high = true;
        AppendStr(&p->lines[i], "\033[7m");
      } else if (!changed && high) {
        AppendStr(&p->lines[i], "\033[27m");
        high = false;
      }
      AppendWide(&p->lines[i], c);
      if (s != -1) {
        AppendStr(&p->lines[i], "\033[39;49m");
      }
    }
    if (high) {
      AppendStr(&p->lines[i], "\033[27m");
      high = false;
    }
  }
}

static void DrawMemory(struct Panel *p, struct MemoryView *view,
                       int64_t histart, int64_t hiend) {
  if (p->top == p->bottom) return;
  DrawMemoryUnzoomed(p, view, histart, hiend);
}

static void DrawMaps(struct Panel *p) {
  int i;
  char *text, *p1, *p2;
  if (p->top == p->bottom) return;
  p1 = text = FormatPml4t(m);
  for (i = 0; p1; ++i, p1 = p2) {
    if ((p2 = strchr(p1, '\n'))) *p2++ = '\0';
    if (i >= mapsstart) {
      AppendPanel(p, i - mapsstart, p1);
    }
  }
  free(text);
}

static void DrawBreakpoints(struct Panel *p) {
  int64_t addr;
  const char *name;
  char *s, buf[256];
  int64_t i, line, sym;
  if (p->top == p->bottom) return;
  for (line = 0, i = breakpoints.i; i--;) {
    if (breakpoints.p[i].disable) continue;
    if (line >= breakpointsstart) {
      addr = breakpoints.p[i].addr;
      sym = DisFindSym(dis, addr);
      name = sym != -1 ? dis->syms.stab + dis->syms.p[sym].name : "UNKNOWN";
      s = buf;
      s += sprintf(s, "%012" PRIx64 " ", addr);
      strcpy(s, name);
      AppendPanel(p, line - breakpointsstart, buf);
      if (sym != -1 && addr != dis->syms.p[sym].addr) {
        snprintf(buf, sizeof(buf), "+%#" PRIx64, addr - dis->syms.p[sym].addr);
        AppendPanel(p, line, buf);
      }
    }
    ++line;
  }
}

static int GetPreferredStackAlignmentMask(void) {
  switch (m->mode & 3) {
    case XED_MODE_LONG:
      return 15;
    case XED_MODE_LEGACY:
      return 3;
    case XED_MODE_REAL:
      return 3;
    default:
      __builtin_unreachable();
  }
}

static void DrawFrames(struct Panel *p) {
  int i;
  int64_t sym;
  uint8_t *r;
  const char *name;
  char *s, line[256];
  int64_t sp, bp, rp;
  if (p->top == p->bottom) return;
  rp = m->ip;
  bp = Read64(m->bp);
  sp = Read64(m->sp);
  for (i = 0; i < p->bottom - p->top;) {
    sym = DisFindSym(dis, rp);
    name = sym != -1 ? dis->syms.stab + dis->syms.p[sym].name : "UNKNOWN";
    s = line;
    s += sprintf(s, "%012" PRIx64 " %012" PRIx64 " ", Read64(m->ss) + bp, rp);
    strcpy(s, name);
    AppendPanel(p, i - framesstart, line);
    if (sym != -1 && rp != dis->syms.p[sym].addr) {
      snprintf(line, sizeof(line), "+%#" PRIx64 "", rp - dis->syms.p[sym].addr);
      AppendPanel(p, i - framesstart, line);
    }
    if (!bp) break;
    if (bp < sp) {
      AppendPanel(p, i - framesstart, " [STRAY]");
    } else if (bp - sp <= 0x1000) {
      snprintf(line, sizeof(line), " %" PRId64 " bytes", bp - sp);
      AppendPanel(p, i - framesstart, line);
    }
    if (bp & GetPreferredStackAlignmentMask() && i) {
      AppendPanel(p, i - framesstart, " [MISALIGN]");
    }
    ++i;
    if (((Read64(m->ss) + bp) & 0xfff) > 0xff0) break;
    if (!(r = FindReal(m, Read64(m->ss) + bp))) {
      AppendPanel(p, i - framesstart, "CORRUPT FRAME POINTER");
      break;
    }
    sp = bp;
    bp = ReadWord(r + 0);
    rp = ReadWord(r + 8);
  }
}

static void CheckFramePointerImpl(void) {
  uint8_t *r;
  int64_t bp, rp;
  static int64_t lastbp;
  bp = Read64(m->bp);
  if (bp && bp == lastbp) return;
  lastbp = bp;
  rp = m->ip;
  while (bp) {
    if (!(r = FindReal(m, Read64(m->ss) + bp))) {
      LOGF("corrupt frame: %012" PRIx64 "", bp);
      ThrowProtectionFault(m);
    }
    bp = Read64(r + 0) - 0;
    rp = Read64(r + 8) - 1;
    if (!bp && !(m->bofram[0] <= rp && rp <= m->bofram[1])) {
      LOGF("bad frame !(%012" PRIx64 " <= %012" PRIx64 " <= %012" PRIx64 ")",
           m->bofram[0], rp, m->bofram[1]);
      ThrowProtectionFault(m);
    }
  }
}

static void CheckFramePointer(void) {
  if (m->bofram[0]) {
    CheckFramePointerImpl();
  }
}

static bool IsExecuting(void) {
  return (action & (CONTINUE | STEP | NEXT | FINISH)) && !(action & FAILURE);
}

static int AppendStat(struct Buffer *b, const char *name, int64_t value,
                      bool changed) {
  int width;
  AppendChar(b, ' ');
  if (changed) AppendStr(b, "\033[31m");
  width = AppendFmt(b, "%8" PRId64 " %s", value, name);
  if (changed) AppendStr(b, "\033[39m");
  return 1 + width;
}

static long double nowl(void) {
  long double secs;
  struct timespec tv;
  clock_gettime(CLOCK_REALTIME, &tv);
  secs = tv.tv_nsec;
  secs *= 1 / 1e9;
  secs += tv.tv_sec;
  return secs;
}

static long double dsleep(long double secs) {
  struct timespec dur, rem;
  dur.tv_sec = secs;
  dur.tv_nsec = secs * 1e9;
  dur.tv_nsec = dur.tv_nsec % 1000000000;
  if (secs > 1e-6) {
    nanosleep(&dur, &rem);
    secs = rem.tv_nsec;
    secs *= 1 / 1e9;
    secs += rem.tv_sec;
    return secs;
  } else {
    return 0;
  }
}

static void DrawStatus(struct Panel *p) {
  int yn, xn, rw;
  struct Buffer *s;
  struct MachineMemstat *a, *b;
  yn = p->top - p->bottom;
  xn = p->right - p->left;
  if (!yn || !xn) return;
  rw = 0;
  a = &m->system->memstat;
  b = &lastmemstat;
  s = malloc(sizeof(struct Buffer));
  memset(s, 0, sizeof(*s));
  if (ips > 0) rw += AppendStat(s, "ips", ips, false);
  rw += AppendStat(s, "kb", m->system->real.n / 1024, false);
  rw += AppendStat(s, "reserve", a->reserved, a->reserved != b->reserved);
  rw += AppendStat(s, "commit", a->committed, a->committed != b->committed);
  rw += AppendStat(s, "freed", a->freed, a->freed != b->freed);
  rw += AppendStat(s, "tables", a->pagetables, a->pagetables != b->pagetables);
  rw += AppendStat(s, "fds", m->system->fds.i, false);
  AppendFmt(&p->lines[0], "\033[7m%-*s%s\033[0m", xn - rw,
            statusmessage && nowl() < statusexpires ? statusmessage
                                                    : "das blinkenlights",
            s->p);
  free(s->p);
  free(s);
  memcpy(b, a, sizeof(*a));
}

static void PreventBufferbloat(void) {
  long double now, rate;
  static long double last;
  now = nowl();
  rate = 1. / 60;
  if (now - last < rate) {
    dsleep(rate - (now - last));
  }
  last = now;
}

static void Redraw(void) {
  int i, j;
  oldlen = m->xedd->length;
  if (!IsShadow(m->readaddr) && !IsShadow(m->readaddr + m->readsize)) {
    readaddr = m->readaddr;
    readsize = m->readsize;
  }
  if (!IsShadow(m->writeaddr) && !IsShadow(m->writeaddr + m->writesize)) {
    writeaddr = m->writeaddr;
    writesize = m->writesize;
  }
  ScrollOp(&pan.disassembly, GetDisIndex());
  if (last_opcount) {
    ips = (opcount - last_opcount) / (nowl() - last_seconds);
  }
  SetupDraw();
  for (i = 0; i < ARRAYLEN(pan.p); ++i) {
    for (j = 0; j < pan.p[i].bottom - pan.p[i].top; ++j) {
      pan.p[i].lines[j].i = 0;
    }
  }
  DrawDisassembly(&pan.disassembly);
  DrawDisplay(&pan.display);
  DrawCpu(&pan.registers);
  DrawSse(&pan.sse);
  DrawHr(&pan.breakpointshr, "BREAKPOINTS");
  DrawHr(&pan.mapshr, "PML4T");
  DrawHr(&pan.frameshr, m->bofram[0] ? "PROTECTED FRAMES" : "FRAMES");
  DrawHr(&pan.ssehr, "SSE");
  DrawHr(&pan.codehr, "CODE");
  DrawHr(&pan.readhr, "READ");
  DrawHr(&pan.writehr, "WRITE");
  DrawHr(&pan.stackhr, "STACK");
  DrawMaps(&pan.maps);
  DrawFrames(&pan.frames);
  DrawBreakpoints(&pan.breakpoints);
  DrawMemory(&pan.code, &codeview, GetIp(), GetIp() + m->xedd->length);
  DrawMemory(&pan.readdata, &readview, readaddr, readaddr + readsize);
  DrawMemory(&pan.writedata, &writeview, writeaddr, writeaddr + writesize);
  DrawMemory(&pan.stack, &stackview, GetSp(), GetSp() + GetPointerWidth());
  DrawStatus(&pan.status);
  PreventBufferbloat();
  PrintPanels(ttyout, ARRAYLEN(pan.p), pan.p, tyn, txn);
  last_opcount = opcount;
  last_seconds = nowl();
}

static void ReactiveDraw(void) {
  if (tuimode) {
    Redraw();
    tick = speed;
  }
}

static void HandleAlarm(void) {
  alarmed = false;
  action &= ~ALARM;
  pty->conf &= ~kPtyBell;
  free(statusmessage);
  statusmessage = NULL;
}

static void HandleAppReadInterrupt(void) {
  if (action & ALARM) {
    HandleAlarm();
  }
  if (action & WINCHED) {
    GetTtySize(ttyout);
    action &= ~WINCHED;
  }
  if (action & INT) {
    action &= ~INT;
    if (action & CONTINUE) {
      action &= ~CONTINUE;
    } else {
      if (tuimode) {
        LeaveScreen();
        TuiCleanup();
      }
      exit(0);
    }
  }
}

static int OnPtyFdClose(int fd) {
  return close(fd);
}

static bool HasPendingInput(int fd) {
  struct pollfd fds[1];
  fds[0].fd = fd;
  fds[0].events = POLLIN;
  fds[0].revents = 0;
  poll(fds, ARRAYLEN(fds), 0);
  return fds[0].revents & (POLLIN | POLLERR);
}

static ssize_t ReadPtyFdDirect(int fd, void *data, size_t size) {
  char *buf;
  ssize_t rc;
  buf = malloc(4096);
  pty->conf |= kPtyBlinkcursor;
  if (tuimode) DisableMouseTracking();
  for (;;) {
    ReactiveDraw();
    if ((rc = read(fd, buf, 4096)) != -1) break;
    /* CHECK_EQ(EINTR, errno); */
    HandleAppReadInterrupt();
  }
  if (tuimode) EnableMouseTracking();
  pty->conf &= ~kPtyBlinkcursor;
  if (rc > 0) {
    PtyWriteInput(pty, buf, rc);
    ReactiveDraw();
    rc = PtyRead(pty, data, size);
  }
  free(buf);
  return rc;
}

static ssize_t OnPtyFdReadv(int fd, const struct iovec *iov, int iovlen) {
  int i;
  ssize_t rc;
  void *data;
  size_t size;
  for (size = i = 0; i < iovlen; ++i) {
    if (iov[i].iov_len) {
      data = iov[i].iov_base;
      size = iov[i].iov_len;
      break;
    }
  }
  if (size) {
    if (!(rc = PtyRead(pty, data, size))) {
      rc = ReadPtyFdDirect(fd, data, size);
    }
    return rc;
  } else {
    return 0;
  }
}

static ssize_t OnPtyFdWritev(int fd, const struct iovec *iov, int iovlen) {
  int i;
  size_t size;
  for (size = i = 0; i < iovlen; ++i) {
    PtyWrite(pty, iov[i].iov_base, iov[i].iov_len);
    size += iov[i].iov_len;
  }
  return size;
}

static int OnPtyFdTiocgwinsz(int fd, struct winsize *ws) {
  ws->ws_row = pty->yn;
  ws->ws_col = pty->xn;
  return 0;
}

static int OnPtyFdTcgets(int fd, struct termios *c) {
  memset(c, 0, sizeof(*c));
  if (!(pty->conf & kPtyNocanon)) c->c_iflag |= ICANON;
  if (!(pty->conf & kPtyNoecho)) c->c_iflag |= ECHO;
  if (!(pty->conf & kPtyNoopost)) c->c_oflag |= OPOST;
  return 0;
}

static int OnPtyFdTcsets(int fd, uint64_t request, struct termios *c) {
  if (c->c_iflag & ICANON) {
    pty->conf &= ~kPtyNocanon;
  } else {
    pty->conf |= kPtyNocanon;
  }
  if (c->c_iflag & ECHO) {
    pty->conf &= ~kPtyNoecho;
  } else {
    pty->conf |= kPtyNoecho;
  }
  if (c->c_oflag & OPOST) {
    pty->conf &= ~kPtyNoopost;
  } else {
    pty->conf |= kPtyNoopost;
  }
  return 0;
}

static int OnPtyFdIoctl(int fd, uint64_t request, void *memory) {
  if (request == TIOCGWINSZ) {
    return OnPtyFdTiocgwinsz(fd, memory);
  } else if (request == TCGETS) {
    return OnPtyFdTcgets(fd, memory);
  } else if (request == TCSETS || request == TCSETSW || request == TCSETSF) {
    return OnPtyFdTcsets(fd, request, memory);
  } else {
    errno = EINVAL;
    return -1;
  }
}

static const struct MachineFdCb kMachineFdCbPty = {
    .close = OnPtyFdClose,
    .readv = OnPtyFdReadv,
    .writev = OnPtyFdWritev,
    .ioctl = (void *)OnPtyFdIoctl,
};

static void LaunchDebuggerReactively(void) {
  LOGF("%s", systemfailure);
  if (tuimode) {
    action |= FAILURE;
  } else {
    if (react) {
      tuimode = true;
      action |= FAILURE;
    } else {
      fprintf(stderr, "ERROR: %s\r\n", systemfailure);
      exit(1);
    }
  }
}

static void OnDebug(void) {
  strcpy(systemfailure, "IT'S A TRAP");
  LaunchDebuggerReactively();
}

static void OnSegmentationFault(void) {
  snprintf(systemfailure, sizeof(systemfailure),
           "SEGMENTATION FAULT %012" PRIx64, m->faultaddr);
  LaunchDebuggerReactively();
}

static void OnProtectionFault(void) {
  strcpy(systemfailure, "PROTECTION FAULT");
  LaunchDebuggerReactively();
}

static void OnSimdException(void) {
  strcpy(systemfailure, "SIMD FAULT");
  LaunchDebuggerReactively();
}

static void OnUndefinedInstruction(void) {
  strcpy(systemfailure, "UNDEFINED INSTRUCTION");
  LaunchDebuggerReactively();
}

static void OnDecodeError(void) {
  strcpy(stpcpy(systemfailure, "DECODE: "),
         doublenul(kXedErrorNames, m->xedd->op.error));
  LaunchDebuggerReactively();
}

static void OnDivideError(void) {
  strcpy(systemfailure, "DIVIDE BY ZERO OR BANE");
  LaunchDebuggerReactively();
}

static void OnFpuException(void) {
  strcpy(systemfailure, "FPU EXCEPTION");
  LaunchDebuggerReactively();
}

static void OnExit(int rc) {
  exitcode = rc;
  action |= EXIT;
}

static size_t GetLastIndex(size_t size, unsigned unit, int i, unsigned limit) {
  unsigned q, r;
  if (!size) return 0;
  q = size / unit;
  r = size % unit;
  if (!r) --q;
  q += i;
  if (q > limit) q = limit;
  return q;
}

static void OnDiskServiceReset(void) {
  m->ax[1] = 0x00;
  SetCarry(false);
}

static void OnDiskServiceBadCommand(void) {
  m->ax[1] = 0x01;
  SetCarry(true);
}

static void OnDiskServiceGetParams(void) {
  size_t lastsector, lastcylinder, lasthead;
  lastcylinder = GetLastIndex(elf->mapsize, 512 * 63 * 255, 0, 1023);
  lasthead = GetLastIndex(elf->mapsize, 512 * 63, 0, 255);
  lastsector = GetLastIndex(elf->mapsize, 512, 1, 63);
  m->dx[0] = 1;
  m->dx[1] = lasthead;
  m->cx[0] = lastcylinder >> 8 << 6 | lastsector;
  m->cx[1] = lastcylinder;
  m->ax[1] = 0;
  Write64(m->es, 0);
  Write16(m->di, 0);
  SetCarry(false);
}

static void OnDiskServiceReadSectors(void) {
  uint64_t addr, size;
  int64_t sectors, drive, head, cylinder, sector, offset;
  sectors = m->ax[0];
  drive = m->dx[0];
  head = m->dx[1];
  cylinder = (m->cx[0] & 192) << 2 | m->cx[1];
  sector = (m->cx[0] & 63) - 1;
  size = sectors * 512;
  offset = sector * 512 + head * 512 * 63 + cylinder * 512 * 63 * 255;
  (void)drive;
  LOGF("bios read sectors %" PRId64 " "
       "@ sector %" PRId64 " cylinder %" PRId64 " head %" PRId64
       " drive %" PRId64 " offset %#" PRIx64 "",
       sectors, sector, cylinder, head, drive, offset);
  if (0 <= sector && offset + size <= elf->mapsize) {
    addr = Read64(m->es) + Read16(m->bx);
    if (addr + size <= m->system->real.n) {
      SetWriteAddr(m, addr, size);
      memcpy(m->system->real.p + addr, elf->map + offset, size);
      m->ax[1] = 0x00;
      SetCarry(false);
    } else {
      m->ax[0] = 0x00;
      m->ax[1] = 0x02;
      SetCarry(true);
    }
  } else {
    LOGF("bios read sector failed 0 <= %" PRId64 " && %" PRIx64 " + %" PRIx64
         " <= %" PRIx64 "",
         sector, offset, size, elf->mapsize);
    m->ax[0] = 0x00;
    m->ax[1] = 0x0d;
    SetCarry(true);
  }
}

static void OnDiskService(void) {
  switch (m->ax[1]) {
    case 0x00:
      OnDiskServiceReset();
      break;
    case 0x02:
      OnDiskServiceReadSectors();
      break;
    case 0x08:
      OnDiskServiceGetParams();
      break;
    default:
      OnDiskServiceBadCommand();
      break;
  }
}

static void OnVidyaServiceSetMode(void) {
  if (FindReal(m, 0xB0000)) {
    vidya = m->ax[0];
  } else {
    LOGF("maybe you forgot -r flag");
  }
}

static void OnVidyaServiceGetMode(void) {
  m->ax[0] = vidya;
  m->ax[1] = 80;  // columns
  m->bx[1] = 0;   // page
}

static void OnVidyaServiceSetCursorPosition(void) {
  PtySetY(pty, m->dx[1]);
  PtySetX(pty, m->dx[0]);
}

static void OnVidyaServiceGetCursorPosition(void) {
  m->dx[1] = pty->y;
  m->dx[0] = pty->x;
  m->cx[1] = 5;  // cursor ▂ scan lines 5..7 of 0..7
  m->cx[0] = 7 | !!(pty->conf & kPtyNocursor) << 5;
}

static int GetVidyaByte(unsigned char b) {
  if (0x20 <= b && b <= 0x7F) return b;
  if (b == 0xFF) b = 0x00;
  return kCp437[b];
}

static void OnVidyaServiceWriteCharacter(void) {
  int i;
  uint64_t w;
  char *p, buf[32];
  p = buf;
  p += FormatCga(m->bx[0], p);
  p = stpcpy(p, "\0337");
  w = tpenc(GetVidyaByte(m->ax[0]));
  do {
    *p++ = w;
  } while ((w >>= 8));
  p = stpcpy(p, "\0338");
  for (i = Read16(m->cx); i--;) {
    PtyWrite(pty, buf, p - buf);
  }
}

static wint_t VidyaServiceXlatTeletype(uint8_t c) {
  switch (c) {
    case '\a':
    case '\b':
    case '\r':
    case '\n':
    case 0177:
      return c;
    default:
      return GetVidyaByte(c);
  }
}

static void OnVidyaServiceTeletypeOutput(void) {
  int n;
  uint64_t w;
  char buf[12];
  n = FormatCga(m->bx[0], buf);
  w = tpenc(VidyaServiceXlatTeletype(m->ax[0]));
  do {
    buf[n++] = w;
  } while ((w >>= 8));
  PtyWrite(pty, buf, n);
}

static void OnVidyaService(void) {
  switch (m->ax[1]) {
    case 0x00:
      OnVidyaServiceSetMode();
      break;
    case 0x02:
      OnVidyaServiceSetCursorPosition();
      break;
    case 0x03:
      OnVidyaServiceGetCursorPosition();
      break;
    case 0x09:
      OnVidyaServiceWriteCharacter();
      break;
    case 0x0E:
      OnVidyaServiceTeletypeOutput();
      break;
    case 0x0F:
      OnVidyaServiceGetMode();
      break;
    default:
      break;
  }
}

static void OnKeyboardServiceReadKeyPress(void) {
  uint8_t b;
  ssize_t rc;
  pty->conf |= kPtyBlinkcursor;
  if (tuimode) DisableMouseTracking();
  for (;;) {
    ReactiveDraw();
    if ((rc = read(0, &b, 1)) != -1) break;
    if (errno != EINTR) abort();
    HandleAppReadInterrupt();
  }
  if (tuimode) EnableMouseTracking();
  pty->conf &= ~kPtyBlinkcursor;
  ReactiveDraw();
  if (b == 0x7F) b = '\b';
  m->ax[0] = b;
  m->ax[1] = 0;
}

static void OnKeyboardService(void) {
  switch (m->ax[1]) {
    case 0x00:
      OnKeyboardServiceReadKeyPress();
      break;
    default:
      break;
  }
}

static void OnApmService(void) {
  if (Read16(m->ax) == 0x5300 && Read16(m->bx) == 0x0000) {
    Write16(m->bx, 'P' << 8 | 'M');
    SetCarry(false);
  } else if (Read16(m->ax) == 0x5301 && Read16(m->bx) == 0x0000) {
    SetCarry(false);
  } else if (Read16(m->ax) == 0x5307 && m->bx[0] == 1 && m->cx[0] == 3) {
    LOGF("APM SHUTDOWN");
    exit(0);
  } else {
    SetCarry(true);
  }
}

static void OnE820(void) {
  uint8_t p[20];
  uint64_t addr;
  addr = Read64(m->es) + Read16(m->di);
  if (Read32(m->dx) == 0x534D4150 && Read32(m->cx) == 24 &&
      addr + sizeof(p) <= m->system->real.n) {
    if (!Read32(m->bx)) {
      Write64(p + 0, 0);
      Write64(p + 8, m->system->real.n);
      Write32(p + 16, 1);
      memcpy(m->system->real.p + addr, p, sizeof(p));
      SetWriteAddr(m, addr, sizeof(p));
      Write32(m->cx, sizeof(p));
      Write32(m->bx, 1);
    } else {
      Write32(m->bx, 0);
      Write32(m->cx, 0);
    }
    Write32(m->ax, 0x534D4150);
    SetCarry(false);
  } else {
    SetCarry(true);
  }
}

static void OnInt15h(void) {
  if (Read32(m->ax) == 0xE820) {
    OnE820();
  } else if (m->ax[1] == 0x53) {
    OnApmService();
  } else {
    SetCarry(true);
  }
}

static bool OnHalt(int interrupt) {
  ReactiveDraw();
  switch (interrupt) {
    case 1:
    case 3:
      OnDebug();
      return false;
    case 0x13:
      OnDiskService();
      return true;
    case 0x10:
      OnVidyaService();
      return true;
    case 0x15:
      OnInt15h();
      return true;
    case 0x16:
      OnKeyboardService();
      return true;
    case kMachineSegmentationFault:
      OnSegmentationFault();
      return false;
    case kMachineProtectionFault:
      OnProtectionFault();
      return false;
    case kMachineSimdException:
      OnSimdException();
      return false;
    case kMachineUndefinedInstruction:
      OnUndefinedInstruction();
      return false;
    case kMachineDecodeError:
      OnDecodeError();
      return false;
    case kMachineDivideError:
      OnDivideError();
      return false;
    case kMachineFpuException:
      OnFpuException();
      return false;
    case kMachineExit:
    case kMachineHalt:
    default:
      OnExit(interrupt);
      return false;
  }
}

static void OnBinbase(struct Machine *m) {
  unsigned i;
  int64_t skew;
  skew = m->xedd->op.disp * 512;
  LOGF("skew binbase %" PRId64 " @ %012" PRIx64 "", skew, GetIp());
  for (i = 0; i < dis->syms.i; ++i) dis->syms.p[i].addr += skew;
  for (i = 0; i < dis->loads.i; ++i) dis->loads.p[i].addr += skew;
  for (i = 0; i < breakpoints.i; ++i) breakpoints.p[i].addr += skew;
  Disassemble();
}

static void OnLongBranch(struct Machine *m) {
  if (tuimode) {
    Disassemble();
  }
}

static void OnPageUp(void) {
  opstart -= tyn / 2;
}

static void OnPageDown(void) {
  opstart += tyn / 2;
}

static void SetStatus(const char *fmt, ...) {
  char *s;
  va_list va;
  struct itimerval it;
  va_start(va, fmt);
  unassert(vasprintf(&s, fmt, va) >= 0);
  va_end(va);
  free(statusmessage);
  statusmessage = s;
  statusexpires = nowl() + 1;
  it.it_interval.tv_sec = 0;
  it.it_interval.tv_usec = 0;
  it.it_value.tv_sec = 1;
  it.it_value.tv_usec = 0;
  setitimer(ITIMER_REAL, &it, 0);
}

static int ClampSpeed(int s) {
  return MAX(-0x1000, MIN(0x40000000, s));
}

static void OnTurbo(void) {
  if (!speed || speed == -1) {
    speed = 1;
  } else if (speed > 0) {
    speed = ClampSpeed(speed << 1);
  } else {
    speed = ClampSpeed(speed >> 1);
  }
  SetStatus("speed %d", speed);
}

static void OnSlowmo(void) {
  if (!speed || speed == 1) {
    speed = -1;
  } else if (speed > 0) {
    speed = ClampSpeed(speed >> 1);
  } else {
    speed = ClampSpeed(speed << 1);
  }
  SetStatus("speed %d", speed);
}

static void OnUpArrow(void) {
  --opstart;
}

static void OnDownArrow(void) {
  ++opstart;
}

static void OnHome(void) {
  opstart = 0;
}

static void OnEnd(void) {
  opstart = dis->ops.i - (pan.disassembly.bottom - pan.disassembly.top);
}

static void OnEnter(void) {
  action &= ~FAILURE;
}

static void OnTab(void) {
  focus = (focus + 1) % ARRAYLEN(pan.p);
}

static void OnUp(void) {
}

static void OnDown(void) {
}

static void OnStep(void) {
  if (action & FAILURE) return;
  action |= STEP;
  action &= ~NEXT;
  action &= ~CONTINUE;
}

static void OnNext(void) {
  if (action & FAILURE) return;
  action ^= NEXT;
  action &= ~STEP;
  action &= ~FINISH;
  action &= ~CONTINUE;
}

static void OnFinish(void) {
  if (action & FAILURE) return;
  action ^= FINISH;
  action &= ~NEXT;
  action &= ~FAILURE;
  action &= ~CONTINUE;
}

static void OnContinueTui(void) {
  action ^= CONTINUE;
  action &= ~STEP;
  action &= ~NEXT;
  action &= ~FINISH;
  action &= ~FAILURE;
}

static void OnContinueExec(void) {
  tuimode = false;
  action |= CONTINUE;
  action &= ~STEP;
  action &= ~NEXT;
  action &= ~FINISH;
  action &= ~FAILURE;
}

static void OnRestart(void) {
  action |= RESTART;
}

static void OnXmmType(void) {
  uint8_t t;
  unsigned i;
  t = CycleXmmType(xmmtype.type[0]);
  for (i = 0; i < 16; ++i) {
    xmmtype.type[i] = t;
  }
}

static void SetXmmSize(int bytes) {
  unsigned i;
  for (i = 0; i < 16; ++i) {
    xmmtype.size[i] = bytes;
  }
}

static void SetXmmDisp(int disp) {
  xmmdisp = disp;
}

static void OnXmmSize(void) {
  SetXmmSize(CycleXmmSize(xmmtype.size[0]));
}

static void OnXmmDisp(void) {
  SetXmmDisp(CycleXmmDisp(xmmdisp));
}

static bool HasPendingKeyboard(void) {
  return HasPendingInput(ttyin);
}

static void Sleep(int ms) {
  poll((struct pollfd[]){{ttyin, POLLIN}}, 1, ms);
}

static void OnMouseWheelUp(struct Panel *p, int y, int x) {
  if (p == &pan.disassembly) {
    opstart -= WHEELDELTA;
  } else if (p == &pan.code) {
    codeview.start -= WHEELDELTA;
  } else if (p == &pan.readdata) {
    readview.start -= WHEELDELTA;
  } else if (p == &pan.writedata) {
    writeview.start -= WHEELDELTA;
  } else if (p == &pan.stack) {
    stackview.start -= WHEELDELTA;
  } else if (p == &pan.maps) {
    mapsstart = MAX(0, mapsstart - 1);
  } else if (p == &pan.frames) {
    framesstart = MAX(0, framesstart - 1);
  } else if (p == &pan.breakpoints) {
    breakpointsstart = MAX(0, breakpointsstart - 1);
  }
}

static void OnMouseWheelDown(struct Panel *p, int y, int x) {
  if (p == &pan.disassembly) {
    opstart += WHEELDELTA;
  } else if (p == &pan.code) {
    codeview.start += WHEELDELTA;
  } else if (p == &pan.readdata) {
    readview.start += WHEELDELTA;
  } else if (p == &pan.writedata) {
    writeview.start += WHEELDELTA;
  } else if (p == &pan.stack) {
    stackview.start += WHEELDELTA;
  } else if (p == &pan.maps) {
    mapsstart += 1;
  } else if (p == &pan.frames) {
    framesstart += 1;
  } else if (p == &pan.breakpoints) {
    breakpointsstart += 1;
  }
}

static void OnMouseCtrlWheelUp(struct Panel *p, int y, int x) {
  ZoomMemoryViews(p, y, x, -1);
}

static void OnMouseCtrlWheelDown(struct Panel *p, int y, int x) {
  ZoomMemoryViews(p, y, x, +1);
}

static struct Panel *LocatePanel(int y, int x) {
  int i;
  for (i = 0; i < ARRAYLEN(pan.p); ++i) {
    if ((pan.p[i].left <= x && x < pan.p[i].right) &&
        (pan.p[i].top <= y && y < pan.p[i].bottom)) {
      return &pan.p[i];
    }
  }
  return NULL;
}

static void OnMouse(char *p) {
  int e, x, y;
  struct Panel *ep;
  e = strtol(p, &p, 10);
  if (*p == ';') ++p;
  x = strtol(p, &p, 10);
  x = MIN(txn, MAX(1, x)) - 1;
  if (*p == ';') ++p;
  y = strtol(p, &p, 10);
  y = MIN(tyn, MAX(1, y)) - 1;
  e |= (*p == 'm') << 2;
  if ((ep = LocatePanel(y, x))) {
    y -= ep->top;
    x -= ep->left;
    switch (e) {
      CASE(kMouseWheelUp, OnMouseWheelUp(ep, y, x));
      CASE(kMouseWheelDown, OnMouseWheelDown(ep, y, x));
      CASE(kMouseCtrlWheelUp, OnMouseCtrlWheelUp(ep, y, x));
      CASE(kMouseCtrlWheelDown, OnMouseCtrlWheelDown(ep, y, x));
      default:
        break;
    }
  }
}

static void OnHelp(void) {
  dialog = HELP;
}

static void ReadKeyboard(void) {
  char buf[64], *p = buf;
  memset(buf, 0, sizeof(buf));
  dialog = NULL;
  if (readansi(ttyin, buf, sizeof(buf)) == -1) {
    if (errno == EINTR) {
      return;
    }
    fprintf(stderr, "ReadKeyboard failed: %s\r\n", strerror(errno));
    exit(1);
  }
  switch (*p++) {
    CASE('q', OnQ());
    CASE('v', OnV());
    CASE('?', OnHelp());
    CASE('s', OnStep());
    CASE('n', OnNext());
    CASE('f', OnFinish());
    CASE('c', OnContinueTui());
    CASE('C', OnContinueExec());
    CASE('R', OnRestart());
    CASE('x', OnXmmDisp());
    CASE('t', OnXmmType());
    CASE('T', OnXmmSize());
    CASE('u', OnUp());
    CASE('d', OnDown());
    CASE('B', PopBreakpoint(&breakpoints));
    CASE('M', ToggleMouseTracking());
    CASE('\t', OnTab());
    CASE('\r', OnEnter());
    CASE('\n', OnEnter());
    CASE(Ctrl('C'), action |= INT);
    CASE(Ctrl('D'), action |= EXIT);
    CASE(Ctrl('\\'), raise(SIGQUIT));
    CASE(Ctrl('Z'), raise(SIGSTOP));
    CASE(Ctrl('L'), OnFeed());
    CASE(Ctrl('P'), OnUpArrow());
    CASE(Ctrl('N'), OnDownArrow());
    CASE(Ctrl('V'), OnPageDown());
    CASE(Ctrl('T'), OnTurbo());
    case 033:
      switch (*p++) {
        CASE('v', OnPageUp()); /* alt+v */
        CASE('t', OnSlowmo()); /* alt+t */
        case 'O':
          switch (*p++) {
            CASE('P', OnHelp()); /* \033OP is F1 */
            default:
              break;
          }
          break;
        case '[':
          switch (*p++) {
            CASE('<', OnMouse(p));
            CASE('A', OnUpArrow());   /* \e[A  is up */
            CASE('B', OnDownArrow()); /* \e[B  is down */
            CASE('F', OnEnd());       /* \e[F  is end */
            CASE('H', OnHome());      /* \e[H  is home */
            CASE('1', OnHome());      /* \e[1~ is home */
            CASE('4', OnEnd());       /* \e[1~ is end */
            CASE('5', OnPageUp());    /* \e[1~ is pgup */
            CASE('6', OnPageDown());  /* \e[1~ is pgdn */
            default:
              break;
          }
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}

static int64_t ParseHexValue(const char *s) {
  char *ep;
  int64_t x;
  x = strtoll(s, &ep, 16);
  if (*ep) {
    fputs("ERROR: bad hexadecimal: ", stderr);
    fputs(s, stderr);
    fputc('\r', stderr);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
  }
  return x;
}

static void HandleBreakpointFlag(const char *s) {
  struct Breakpoint b;
  memset(&b, 0, sizeof(b));
  if (isdigit(*s)) {
    b.addr = ParseHexValue(s);
  } else {
    b.symbol = optarg;
  }
  PushBreakpoint(&breakpoints, &b);
}

_Noreturn static void PrintUsage(int rc, FILE *f) {
  fprintf(f, "SYNOPSIS\r\n\r\n  %s%s", "blink", USAGE);
  exit(rc);
}

static void LogInstruction(void) {
  LOGF(
      "EXEC %8" PRIx64 " SP %012" PRIx64 " AX %016" PRIx64 " CX %016" PRIx64
      " DX %016" PRIx64 " BX %016" PRIx64 " BP %016" PRIx64 " SI %016" PRIx64
      " DI %016" PRIx64 " R8 %016" PRIx64 " R9 %016" PRIx64 " R10 %016" PRIx64
      " R11 %016" PRIx64 " R12 %016" PRIx64 " R13 %016" PRIx64
      " R14 %016" PRIx64 " R15 %016" PRIx64 " FS %012" PRIx64 " GS %012" PRIx64,
      m->ip, Read64(m->sp) & 0xffffffffffff, Read64(m->ax), Read64(m->cx),
      Read64(m->dx), Read64(m->bx), Read64(m->bp), Read64(m->si), Read64(m->di),
      Read64(m->r8), Read64(m->r9), Read64(m->r10), Read64(m->r11),
      Read64(m->r12), Read64(m->r13), Read64(m->r14), Read64(m->r15),
      Read64(m->fs) & 0xffffffffffff, Read64(m->gs) & 0xffffffffffff);
}

static void Exec(void) {
  int sig;
  ssize_t bp;
  int interrupt;
  ExecSetup();
  if (!(interrupt = setjmp(m->onhalt))) {
    if (!(action & CONTINUE) &&
        (bp = IsAtBreakpoint(&breakpoints, GetIp())) != -1) {
      LOGF("BREAK1 %012" PRIx64 "", breakpoints.p[bp].addr);
      tuimode = true;
      LoadInstruction(m);
      if (verbose) LogInstruction();
      ExecuteInstruction(m);
      if (signals.n && signals.i < signals.n) {
        if ((sig = ConsumeSignal(m, &signals)) && sig != SIGALRM) {
          TerminateSignal(m, sig);
        }
      }
      ++opcount;
      CheckFramePointer();
    } else {
      action &= ~CONTINUE;
      for (;;) {
        LoadInstruction(m);
        if ((bp = IsAtBreakpoint(&breakpoints, GetIp())) != -1) {
          LOGF("BREAK2 %012" PRIx64 "", breakpoints.p[bp].addr);
          action &= ~(FINISH | NEXT | CONTINUE);
          tuimode = true;
          break;
        }
        if (verbose) LogInstruction();
        ExecuteInstruction(m);
        if (signals.n && signals.i < signals.n) {
          if ((sig = ConsumeSignal(m, &signals)) && sig != SIGALRM) {
            TerminateSignal(m, sig);
          }
        }
        ++opcount;
      KeepGoing:
        CheckFramePointer();
        if (action & ALARM) {
          /* TODO(jart): Fix me */
          /* DrawDisplayOnly(&pan.display); */
          action &= ~ALARM;
        }
        if (action & EXIT) {
          LOGF("EXEC EXIT");
          break;
        }
        if (action & INT) {
          LOGF("EXEC INT");
          if (react) {
            LOGF("REACT");
            action &= ~(INT | STEP | FINISH | NEXT);
            tuimode = true;
          } else {
            action &= ~INT;
            EnqueueSignal(m, &signals, SIGINT, 0);
          }
        }
      }
    }
  } else {
    if (OnHalt(interrupt)) {
      goto KeepGoing;
    }
  }
}

static void Tui(void) {
  int sig;
  ssize_t bp;
  int interrupt;
  bool interactive;
  /* LOGF("TUI"); */
  TuiSetup();
  SetupDraw();
  ScrollOp(&pan.disassembly, GetDisIndex());
  if (!(interrupt = setjmp(m->onhalt))) {
    do {
      if (!(action & FAILURE)) {
        LoadInstruction(m);
        if ((action & (FINISH | NEXT | CONTINUE)) &&
            (bp = IsAtBreakpoint(&breakpoints, GetIp())) != -1) {
          action &= ~(FINISH | NEXT | CONTINUE);
          LOGF("BREAK %012" PRIx64 "", breakpoints.p[bp].addr);
        }
      } else {
        m->xedd = (struct XedDecodedInst *)m->opcache->icache[0];
        m->xedd->length = 1;
        m->xedd->bytes[0] = 0xCC;
        m->xedd->op.opcode = 0xCC;
      }
      if (action & WINCHED) {
        GetTtySize(ttyout);
        action &= ~WINCHED;
      }
      interactive = ++tick > speed;
      if (interactive && speed < 0) {
        Sleep(-speed);
      }
      if (action & ALARM) {
        HandleAlarm();
      }
      if (action & FAILURE) {
        ScrollMemoryViews();
      }
      if (!(action & CONTINUE) || interactive) {
        tick = 0;
        Redraw();
        CopyMachineState(&laststate);
      }
      if (dialog) {
        PrintMessageBox(ttyout, dialog, tyn, txn);
      }
      if (action & FAILURE) {
        LOGF("TUI FAILURE");
        PrintMessageBox(ttyout, systemfailure, tyn, txn);
        ReadKeyboard();
        if (action & INT) {
          LOGF("TUI INT");
          LeaveScreen();
          exit(1);
        }
      } else if (dialog || !IsExecuting() ||
                 (!(action & CONTINUE) && !(action & INT) &&
                  HasPendingKeyboard())) {
        ReadKeyboard();
      }
      if (action & INT) {
        LOGF("TUI INT");
        action &= ~INT;
        if (action & (CONTINUE | NEXT | FINISH)) {
          action &= ~(CONTINUE | NEXT | FINISH);
        } else {
          EnqueueSignal(m, &signals, SIGINT, 0);
          action |= STEP;
        }
      }
      if (action & EXIT) {
        LOGF("TUI EXIT");
        break;
      }
      if (action & RESTART) {
        LOGF("TUI RESTART");
        break;
      }
      if (IsExecuting()) {
        if (!(action & CONTINUE)) {
          action &= ~STEP;
          if (action & NEXT) {
            action &= ~NEXT;
            if (IsCall()) {
              BreakAtNextInstruction();
              break;
            }
          }
          if (action & FINISH) {
            if (IsCall()) {
              BreakAtNextInstruction();
              break;
            } else if (IsRet()) {
              action &= ~FINISH;
            }
          }
        }
        if (!IsDebugBreak()) {
          UpdateXmmType(m, &xmmtype);
          if (verbose) LogInstruction();
          ExecuteInstruction(m);
          if (signals.n && signals.i < signals.n) {
            if ((sig = ConsumeSignal(m, &signals)) && sig != SIGALRM) {
              TerminateSignal(m, sig);
            }
          }
          ++opcount;
          if (!(action & CONTINUE) || interactive) {
            if (!(action & CONTINUE)) ReactiveDraw();
            ScrollMemoryViews();
          }
        } else {
          m->ip += m->xedd->length;
          action &= ~NEXT;
          action &= ~FINISH;
          action &= ~CONTINUE;
        }
      KeepGoing:
        CheckFramePointer();
        if (!(action & CONTINUE)) {
          ScrollOp(&pan.disassembly, GetDisIndex());
          if ((action & FINISH) && IsRet()) action &= ~FINISH;
          if (((action & NEXT) && IsRet()) || (action & FINISH)) {
            action &= ~NEXT;
          }
        }
      }
    } while (tuimode);
  } else {
    if (OnHalt(interrupt)) {
      ReactiveDraw();
      ScrollMemoryViews();
      goto KeepGoing;
    }
    ReactiveDraw();
    ScrollOp(&pan.disassembly, GetDisIndex());
  }
  TuiCleanup();
}

static void GetOpts(int argc, char *argv[]) {
  int opt;
  const char *logpath = 0;
  while ((opt = getopt(argc, argv, "hvtrzRsb:HL:")) != -1) {
    switch (opt) {
      case 't':
        tuimode = false;
        break;
      case 'R':
        react = false;
        break;
      case 'r':
        m->mode = XED_MACHINE_MODE_REAL;
        g_disisprog_disable = true;
        break;
      case 's':
        printstats = true;
        break;
      case 'b':
        HandleBreakpointFlag(optarg);
        break;
      case 'H':
        memset(&g_high, 0, sizeof(g_high));
        break;
      case 'v':
        ++verbose;
        break;
      case 'L':
        logpath = optarg;
        break;
      case 'z':
        ++codeview.zoom;
        ++readview.zoom;
        ++writeview.zoom;
        ++stackview.zoom;
        break;
      case 'h':
        PrintUsage(0, stdout);
      default:
        PrintUsage(48, stderr);
    }
  }
  OpenLog(logpath);
}

static int OpenDevTty(void) {
  return open("/dev/tty", O_RDWR | O_NOCTTY);
}

static void AddHostFd(int fd) {
  int i = m->system->fds.i++;
  m->system->fds.p[i].fd = fd;
  m->system->fds.p[i].cb = &kMachineFdCbHost;
}

int Emulator(int argc, char *argv[]) {
  codepath = argv[optind++];
  m->system->fds.p = calloc((m->system->fds.n = 8), sizeof(struct MachineFd));
  do {
    action = 0;
    LoadProgram(m, codepath, argv + optind, environ, elf);
    ScrollMemoryViews();
    AddHostFd(0);
    AddHostFd(1);
    AddHostFd(2);
    if (tuimode) {
      ttyin = isatty(0) ? 0 : OpenDevTty();
      ttyout = isatty(1) ? 1 : OpenDevTty();
    } else {
      ttyin = -1;
      ttyout = -1;
    }
    if (ttyout != -1) {
      atexit(TtyRestore1);
      tyn = 24;
      txn = 80;
      GetTtySize(ttyout);
      if (isatty(0)) m->system->fds.p[0].cb = &kMachineFdCbPty;
      if (isatty(1)) m->system->fds.p[1].cb = &kMachineFdCbPty;
      if (isatty(2)) m->system->fds.p[2].cb = &kMachineFdCbPty;
    }
    do {
      if (!tuimode) {
        Exec();
      } else {
        Tui();
      }
    } while (!(action & (RESTART | EXIT)));
  } while (action & RESTART);
  munmap(elf->ehdr, elf->size);
  DisFree(dis);
  return exitcode;
}

int main(int argc, char *argv[]) {
  static struct sigaction sa;
  react = true;
  tuimode = true;
  pty = NewPty();
  m = NewMachine();
  m->mode = XED_MACHINE_MODE_LONG_64;
  m->system->redraw = Redraw;
  m->system->onbinbase = OnBinbase;
  m->system->onlongbranch = OnLongBranch;
  speed = 16;
  SetXmmSize(2);
  SetXmmDisp(kXmmHex);
  g_high.keyword = 155;
  g_high.reg = 215;
  g_high.literal = 182;
  g_high.label = 221;
  g_high.comment = 112;
  g_high.quote = 215;
  GetOpts(argc, argv);
  sigfillset(&sa.sa_mask);
  sa.sa_sigaction = OnSignal;
  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGHUP, &sa, 0);
  sigaction(SIGQUIT, &sa, 0);
  sigaction(SIGABRT, &sa, 0);
  sigaction(SIGUSR1, &sa, 0);
  sigaction(SIGUSR2, &sa, 0);
  sigaction(SIGPIPE, &sa, 0);
  sigaction(SIGTERM, &sa, 0);
  sigaction(SIGCHLD, &sa, 0);
  sigaction(SIGCONT, &sa, 0);
  sa.sa_sigaction = OnSigInt;
  sigaction(SIGINT, &sa, 0);
  sa.sa_sigaction = OnSigWinch;
  sigaction(SIGWINCH, &sa, 0);
  sa.sa_sigaction = OnSigAlrm;
  sigaction(SIGALRM, &sa, 0);
  if (optind == argc) PrintUsage(48, stderr);
  return Emulator(argc, argv);
}
