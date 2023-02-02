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
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
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
#include <wctype.h>

#include "blink/assert.h"
#include "blink/bitscan.h"
#include "blink/breakpoint.h"
#include "blink/builtin.h"
#include "blink/case.h"
#include "blink/cga.h"
#include "blink/debug.h"
#include "blink/dis.h"
#include "blink/endian.h"
#include "blink/errno.h"
#include "blink/fds.h"
#include "blink/flags.h"
#include "blink/fpu.h"
#include "blink/high.h"
#include "blink/linux.h"
#include "blink/loader.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/map.h"
#include "blink/mda.h"
#include "blink/modrm.h"
#include "blink/mop.h"
#include "blink/panel.h"
#include "blink/pml4t.h"
#include "blink/pty.h"
#include "blink/signal.h"
#include "blink/sigwinch.h"
#include "blink/stats.h"
#include "blink/strwidth.h"
#include "blink/syscall.h"
#include "blink/thompike.h"
#include "blink/timespec.h"
#include "blink/tsan.h"
#include "blink/types.h"
#include "blink/util.h"
#include "blink/watch.h"
#include "blink/web.h"
#include "blink/xlat.h"
#include "blink/xmmtype.h"

#define USAGE \
  " [-?HhrRstv] [ROM] [ARGS...]\n\
\n\
DESCRIPTION\n\
\n\
  blinkenlights - x86_64-linux virtual machine tui\n\
\n\
FLAGS\n\
\n\
  -h        help\n\
  -z        zoom\n\
  -v        verbosity\n\
  -r        real mode\n\
  -s        statistics\n\
  -H        disable highlight\n\
  -t        disable tui mode\n\
  -R        disable reactive\n\
  -b ADDR   push a breakpoint\n\
  -w ADDR   push a watchpoint\n\
  -L PATH   log file location\n\
\n\
ARGUMENTS\n\
\n\
  ROM files may be ELF, Actually Portable Executable, or flat.\n\
  It should use x86_64 in accordance with the System Five ABI.\n\
  The SYSCALL ABI is defined as it is written in Linux Kernel.\n\
\n\
FEATURES\n\
\n\
  8086, 8087, i386, x86_64, SSE3, SSSE3, POPCNT, MDA, CGA, TTY\n\
  Type ? for keyboard shortcuts and CLI flags inside emulator.\n\
\n"

#define HELP \
  "\033[1mBlinkenlights v2.o\033[22m\
                https://github.com/jart/blink/\n\
\n\
KEYBOARD SHORTCUTS                CLI FLAGS\n\
\n\
ctrl-c  interrupt                 -t       no tui\n\
s       step                      -r       real mode\n\
n       next                      -s       statistics\n\
c       continue                  -b ADDR  push breakpoint\n\
C       continue harder           -w ADDR  push watchpoint\n\
q       quit                      -L PATH  log file location\n\
f       finish                    -R       deliver crash sigs\n\
R       restart                   -H       disable highlighting\n\
x       hex                       -v       increase verbosity\n\
?       help                      -j       enables jit\n\
t       sse type                  -m       disables memory safety\n\
w       sse width                 -N       natural scroll wheel\n\
B       pop breakpoint            -?       help\n\
p       profiling mode\n\
ctrl-t  turbo\n\
alt-t   slowmo"

#define FPS        60     // frames per second written to tty
#define TURBO      true   // to keep executing between frames
#define HISTORY    65536  // number of rewind renders to ring
#define WHEELDELTA 1      // how much impact scroll wheel has
#define MAXZOOM    16     // lg2 maximum memory panel scaling
#define DISPWIDTH  80     // size of the embedded tty display
#define DUMPWIDTH  64     // columns of bytes in memory panel
#define ASMWIDTH   40     // seed the width of assembly panel
#define ASMRAWMIN  (m->mode == XED_MODE_REAL ? 50 : 65)

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

#define m g_machine

#ifdef IUTF8
#define HR   L'─'
#define BULB "◎"
#else
#define HR   '-'
#define BULB "@"
#endif

struct Mouse {
  short y;
  short x;
  int e;
};

struct MemoryView {
  i64 start;
  int zoom;
};

struct Keystrokes {
  unsigned i;
  char p[4][32];
  struct timespec s[4];
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

struct Rendering {
  u64 cycle;
  void *data;
  unsigned compsize;
  unsigned origsize;
};

struct History {
  unsigned index;
  unsigned count;
  unsigned viewing;
  struct Rendering p[HISTORY];
};

struct ProfSym {
  int sym;  // dis->syms.p[sym]
  unsigned long hits;
};

struct ProfSyms {
  int i, n;
  unsigned long toto;
  struct ProfSym *p;
};

static const char kRipName[3][4] = {"IP", "EIP", "RIP"};

static const char kRegisterNames[3][16][4] = {
    {"AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI"},
    {"EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI"},
    {"RAX", "RCX", "RDX", "RBX", "RSP", "RBP", "RSI", "RDI",  //
     "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15"},
};

extern char **environ;

static bool belay;
static bool react;
static bool tuimode;
static bool alarmed;
static bool natural;
static bool mousemode;
static bool showhighsse;
static bool showprofile;
static bool readingteletype;

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
static u64 cycle;
static i64 oldlen;
static i64 opstart;
static int lastfds;
static u64 readaddr;
static u64 readsize;
static u64 writeaddr;
static u64 writesize;
static i64 mapsstart;
static u64 last_cycle;
static char *codepath;
static i64 framesstart;
static struct Pty *pty;
static jmp_buf *onbusted;
static const char *dialog;
static char *statusmessage;
static i64 breakpointsstart;
static unsigned long *ophits;
static struct ProfSyms profsyms;

static struct Panels pan;
static struct Keystrokes keystrokes;
static struct Breakpoints breakpoints;
static struct Watchpoints watchpoints;
static struct MemoryView codeview;
static struct MemoryView readview;
static struct MemoryView writeview;
static struct MemoryView stackview;
static struct MachineState laststate;
static struct MachineMemstat lastmemstat;
static struct XmmType xmmtype;
static struct Dis dis[1];

static struct timespec last_draw;
static struct timespec statusexpires;
static struct termios oldterm;
static char systemfailure[128];
static struct sigaction oldsig[4];
static char pathbuf[PATH_MAX];
struct History g_history;

static void Redraw(bool);
static void SetupDraw(void);
static void HandleKeyboard(const char *);

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

static char *FormatDouble(char buf[32], double x) {
  snprintf(buf, 32, "%g", x);
  return buf;
}

static char *FormatLongDouble(char buf[32], long double x) {
  snprintf(buf, 32, "%Lg", x);
  return buf;
}

static i64 SignExtend(u64 x, char b) {
  char k;
  unassert(1 <= b && b <= 64);
  k = 64 - b;
  return (i64)(x << k) >> k;
}

static void SetCarry(bool cf) {
  m->flags = SetFlag(m->flags, FLAGS_CF, cf);
}

static bool IsCall(void) {
  int dispatch = Mopcode(m->xedd->op.rde);
  return (dispatch == 0x0E8 ||
          (dispatch == 0x0FF && ModrmReg(m->xedd->op.rde) == 2));
}

static bool IsDebugBreak(void) {
  return Mopcode(m->xedd->op.rde) == 0x0CC;
}

static bool IsRet(void) {
  switch (Mopcode(m->xedd->op.rde)) {
    case 0x0C2:
    case 0x0C3:
    case 0x0CA:
    case 0x0CB:
    case 0x0CF:
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

static u8 CycleXmmType(u8 t) {
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

static u8 CycleXmmDisp(u8 t) {
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

static u8 CycleXmmSize(u8 w) {
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
  return 2 << m->mode;
}

static i64 GetSp(void) {
  switch (GetPointerWidth()) {
    default:
    case 8:
      return Read64(m->sp);
    case 4:
      return m->ss.base + Read32(m->sp);
    case 2:
      return m->ss.base + Read16(m->sp);
  }
}

static i64 ReadWord(u8 *p) {
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

static void AppendPanel(struct Panel *p, i64 line, const char *s) {
  if (0 <= line && line < p->bottom - p->top) {
    AppendStr(&p->lines[line], s);
  }
}

static int CompareProfSyms(const void *p, const void *q) {
  const struct ProfSym *a = (const struct ProfSym *)p;
  const struct ProfSym *b = (const struct ProfSym *)q;
  if (a->hits > b->hits) return -1;
  if (a->hits < b->hits) return +1;
  return 0;
}

static void SortProfSyms(void) {
  qsort(profsyms.p, profsyms.i, sizeof(*profsyms.p), CompareProfSyms);
}

static int AddProfSym(int sym, unsigned long hits) {
  if (!hits) return -1;
  if (profsyms.i == profsyms.n) {
    profsyms.p = (struct ProfSym *)realloc(profsyms.p,
                                           ++profsyms.n * sizeof(*profsyms.p));
  }
  profsyms.p[profsyms.i].sym = sym;
  profsyms.p[profsyms.i].hits = hits;
  return profsyms.i++;
}

static unsigned long TallyHits(i64 addr, int size) {
  i64 pc;
  unsigned long hits;
  for (hits = 0, pc = addr; pc < addr + size; ++pc) {
    hits += ophits[pc - m->system->codestart];
  }
  return hits;
}

static void GenerateProfile(void) {
  int sym;
  profsyms.i = 0;
  profsyms.toto = TallyHits(m->system->codestart, m->system->codesize);
  if (!ophits) return;
  for (sym = 0; sym < dis->syms.i; ++sym) {
    if (dis->syms.p[sym].addr >= m->system->codestart &&
        dis->syms.p[sym].addr + dis->syms.p[sym].size <
            m->system->codestart + m->system->codesize) {
      AddProfSym(sym, TallyHits(dis->syms.p[sym].addr, dis->syms.p[sym].size));
    }
  }
  SortProfSyms();
  profsyms.i = MIN(50, profsyms.i);
}

static void DrawProfile(struct Panel *p) {
  int i;
  char line[256];
  GenerateProfile();
  for (i = 0; i < profsyms.i; ++i) {
    snprintf(line, sizeof(line), "%7.3f%% %s",
             (double)profsyms.p[i].hits / profsyms.toto * 100,
             dis->syms.stab + dis->syms.p[profsyms.p[i].sym].name);
    AppendPanel(p, i - framesstart, line);
  }
}

static void CopyMachineState(struct MachineState *ms) {
  ms->ip = m->ip;
  ms->cs = m->cs;
  ms->ss = m->ss;
  ms->es = m->es;
  ms->ds = m->ds;
  ms->fs = m->fs;
  ms->gs = m->gs;
  memcpy(ms->weg, m->weg, sizeof(m->weg));
  memcpy(ms->xmm, m->xmm, sizeof(m->xmm));
  memcpy(&ms->fpu, &m->fpu, sizeof(m->fpu));
  memcpy(&ms->mxcsr, &m->mxcsr, sizeof(m->mxcsr));
}

/**
 * Handles file mapped page faults in valid page but past eof.
 */
static void OnSigBusted(int sig) {
  longjmp(*onbusted, 1);
}

/**
 * Returns true if 𝑣 is a shadow memory virtual address.
 */
static bool IsShadow(i64 v) {
  return 0x7fff8000 <= v && v < 0x100080000000;
}

/**
 * Returns glyph representing byte at virtual address 𝑣.
 */
static int VirtualBing(i64 v) {
  u8 *p;
  int rc;
  jmp_buf busted;
  onbusted = &busted;
  if ((p = (u8 *)LookupAddress(m, v))) {
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
static int VirtualShadow(i64 v) {
  int rc;
  char *p;
  jmp_buf busted;
  if (IsShadow(v)) return -2;
  onbusted = &busted;
  if ((p = (char *)LookupAddress(m, (v >> 3) + 0x7fff8000))) {
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

static void ScrollOp(struct Panel *p, i64 op) {
  i64 n;
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
  sprintf(buf, "\033[%d;%dH\033[S\n", tyn, txn);
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
  LOGF("TuiRejuvinate");
  GetTtySize(ttyout);
  HideCursor();
  memcpy(&term, &oldterm, sizeof(term));
  term.c_cc[VMIN] = 1;
  term.c_cc[VTIME] = 1;
  term.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  term.c_iflag &= ~(IXOFF | PARMRK | INLCR | IGNCR | IGNPAR);
  term.c_iflag |= IGNBRK;
  term.c_lflag &=
      ~(IEXTEN | ICANON | ECHO | ECHOE | ECHONL | NOFLSH | TOSTOP | ECHOK);
  term.c_lflag |= ISIG;
  term.c_cflag &= ~(CSIZE | PARENB);
  term.c_cflag |= CS8 | CREAD;
  term.c_oflag |= OPOST | ONLCR;
  term.c_lflag &= ~ECHOK;
#ifdef IMAXBEL
  term.c_iflag &= ~IMAXBEL;
#endif
#ifdef PENDIN
  term.c_lflag &= ~PENDIN;
#endif
#ifdef IUTF8
  term.c_iflag |= IUTF8;
#endif
  tcsetattr(ttyout, TCSANOW, &term);
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

static void OnSigSys(int sig) {
  // do nothing
}

static void OnSigWinch(int sig, siginfo_t *si, void *uc) {
  EnqueueSignal(m, SIGWINCH_LINUX);
  action |= WINCHED;
}

static void OnSigInt(int sig, siginfo_t *si, void *uc) {
  action |= INT;
}

static void OnSigAlrm(int sig, siginfo_t *si, void *uc) {
  EnqueueSignal(m, SIGALRM_LINUX);
  action |= ALARM;
}

static void OnSigCont(int sig, siginfo_t *si, void *uc) {
  if (tuimode) {
    TuiRejuvinate();
    Redraw(true);
  }
  EnqueueSignal(m, SIGCONT);
}

static void TtyRestore1(void) {
  LOGF("TtyRestore1");
  ShowCursor();
  TtyWriteString("\033[0m");
}

static void TtyRestore2(void) {
  LOGF("TtyRestore2");
  tcsetattr(ttyout, TCSANOW, &oldterm);
  DisableMouseTracking();
}

static void TuiCleanup(void) {
  LOGF("TuiCleanup");
  sigaction(SIGCONT, oldsig + 2, 0);
  TtyRestore1();
  DisableMouseTracking();
  tuimode = false;
  // LeaveScreen();
}

static void ResolveBreakpoints(void) {
  long i, sym;
  for (i = 0; i < breakpoints.i; ++i) {
    if (breakpoints.p[i].symbol && !breakpoints.p[i].addr) {
      if ((sym = DisFindSymByName(dis, breakpoints.p[i].symbol)) != -1) {
        breakpoints.p[i].addr = dis->syms.p[sym].addr;
      } else {
        fprintf(stderr,
                "error: breakpoint not found: %s (out of %d loaded symbols)\n",
                breakpoints.p[i].symbol, dis->syms.i);
        exit(1);
      }
    }
  }
}

static void ResolveWatchpoints(void) {
  long i, sym;
  for (i = 0; i < watchpoints.i; ++i) {
    if (watchpoints.p[i].symbol && !watchpoints.p[i].addr) {
      if ((sym = DisFindSymByName(dis, watchpoints.p[i].symbol)) != -1) {
        watchpoints.p[i].addr = dis->syms.p[sym].addr;
      } else {
        fprintf(stderr,
                "error: watchpoint not found: %s (out of %d loaded symbols)\n",
                watchpoints.p[i].symbol, dis->syms.i);
        exit(1);
      }
    }
  }
}

static void BreakAtNextInstruction(void) {
  struct Breakpoint b;
  memset(&b, 0, sizeof(b));
  b.addr = GetPc(m) + m->xedd->length;
  b.oneshot = true;
  PushBreakpoint(&breakpoints, &b);
}

static void LoadSyms(void) {
  LoadDebugSymbols(&m->system->elf);
  DisLoadElf(dis, &m->system->elf);
}

static int DrainInput(int fd) {
  char buf[32];
  struct pollfd fds[1];
  for (;;) {
    fds[0].fd = fd;
    fds[0].events = POLLIN;
#ifdef __COSMOPOLITAN__
    if (!IsWindows())
#endif
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
    if (tuimode || breakpoints.i || watchpoints.i) {
      LoadSyms();
      ResolveBreakpoints();
      ResolveWatchpoints();
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
    /* LOGF("loaded program %s\n%s", codepath, gc(FormatPml4t(m))); */
    CommonSetup();
    tcgetattr(ttyout, &oldterm);
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
  it.it_interval.tv_usec = 1. / FPS * 1e6;
  it.it_value.tv_sec = 0;
  it.it_value.tv_usec = 1. / FPS * 1e6;
  setitimer(ITIMER_REAL, &it, 0);
}

static void pcmpeqb(u8 x[16], const u8 y[16]) {
  for (int i = 0; i < 16; ++i) x[i] = -(x[i] == y[i]);
}

static unsigned pmovmskb(const u8 p[16]) {
  unsigned i, m;
  for (m = i = 0; i < 16; ++i) m |= !!(p[i] & 0x80) << i;
  return m;
}

static bool IsXmmNonZero(i64 start, i64 end) {
  i64 i;
  u8 v1[16], vz[16];
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
    if (GetSegmentBase(DISPATCH_NOTHING, i)) {
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

static int GetRegHexWidth(void) {
  switch (m->mode & 3) {
    case XED_MODE_LONG:
      return 16;
    case XED_MODE_LEGACY:
      return 8;
    case XED_MODE_REAL:
      if ((Read64(m->ax) >> 16) || (Read64(m->cx) >> 16) ||
          (Read64(m->dx) >> 16) || (Read64(m->bx) >> 16) ||
          (Read64(m->sp) >> 16) || (Read64(m->bp) >> 16) ||
          (Read64(m->si) >> 16) || (Read64(m->di) >> 16)) {
        return 8;
      } else {
        return 4;
      }
    default:
      __builtin_unreachable();
  }
}

static int GetAddrHexWidth(void) {
  switch (m->mode & 3) {
    case XED_MODE_LONG:
      return 12;
    case XED_MODE_LEGACY:
      return 8;
    case XED_MODE_REAL:
      if (m->fs.base >= 0x10fff0 || m->gs.base >= 0x10fff0) {
        return 8;
      } else {
        return 6;
      }
    default:
      __builtin_unreachable();
  }
}

void SetupDraw(void) {
  int i, j, n, a, b, yn, fit, cpuy, ssey, dx[2], c2y[3], c3y[5];

  cpuy = 9;
  if (IsSegNonZero()) cpuy += 2;
  ssey = PickNumberOfXmmRegistersToShow();
  if (ssey) ++ssey;

  int column[3] = {
      GetAddrHexWidth() + 1 + ASMWIDTH,
      DISPWIDTH,
      GetAddrHexWidth() + 1 + DUMPWIDTH,
  };
  bool growable[3] = {
      true,
      false,
      false,
  };
  bool shrinkable[3] = {
      true,
      true,
      false,
  };
  for (i = 0;; ++i) {
    for (fit = j = 0; j < ARRAYLEN(column); ++j) {
      fit += column[j];
    }
    if (fit > txn) {
      if (shrinkable[i % ARRAYLEN(column)]) {
        --column[i % ARRAYLEN(column)];
      }
    } else if (fit < txn) {
      if (growable[i % ARRAYLEN(column)]) {
        ++column[i % ARRAYLEN(column)];
      }
    } else {
      break;
    }
  }
  dis->noraw = column[0] < ASMRAWMIN;
  dx[0] = column[0];
  dx[1] = column[0] + column[1];

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
      pan.p[i].lines = (struct Buffer *)calloc(n, sizeof(struct Buffer));
      pan.p[i].n = n;
    }
  }

  PtyResize(pty, pan.display.bottom - pan.display.top,
            pan.display.right - pan.display.left);
}

static i64 Disassemble(void) {
  i64 lines;
  lines = pan.disassembly.bottom - pan.disassembly.top * 2;
  if (Dis(dis, m, GetPc(m), m->ip, lines) != -1) {
    return DisFind(dis, GetPc(m));
  } else {
    return -1;
  }
}

static i64 GetDisIndex(void) {
  i64 i;
  if ((i = DisFind(dis, GetPc(m) - m->oplen)) != -1 ||
      (i = Disassemble()) != -1) {
    while (i + 1 < dis->ops.i) {
      if (!dis->ops.p[i].size) {
        ++i;
      } else {
        break;
      }
    }
  }
  return i;
}

static void DrawDisassembly(struct Panel *p) {
  i64 i, j;
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
  i64 i, wp, ws, wl, wr;
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
  i64 i, wp, ws, wl;
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
  AppendFmt(&p->lines[0], "%sTELETYPEWRITER%s──%s──%s──%s──%s",
            readingteletype ? "\033[1m" : "", readingteletype ? "\033[22m" : "",
            (pty->conf & kPtyLed1) ? "\033[1;31m" BULB "\033[0m" : "○",
            (pty->conf & kPtyLed2) ? "\033[1;32m" BULB "\033[0m" : "○",
            (pty->conf & kPtyLed3) ? "\033[1;33m" BULB "\033[0m" : "○",
            (pty->conf & kPtyLed4) ? "\033[1;34m" BULB "\033[0m" : "○");
  for (i = 26 + wl; i < p->right - p->left; ++i) {
    AppendWide(&p->lines[0], HR);
  }
}

static void DrawTerminal(struct Panel *p) {
  i64 y, yn;
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
      DrawMda(p, (u8(*)[80][2])(m->system->real + 0xb0000));
      break;
    case 3:
      DrawHr(&pan.displayhr, "COLOR GRAPHICS ADAPTER");
      DrawCga(p, (u8(*)[80][2])(m->system->real + 0xb8000));
      break;
    default:
      DrawTerminalHr(&pan.displayhr);
      DrawTerminal(p);
      break;
  }
}

static void DrawFlag(struct Panel *p, i64 i, char name, bool value) {
  char str[3] = "  ";
  if (value) str[1] = name;
  AppendPanel(p, i, str);
}

static void DrawRegister(struct Panel *p, i64 i, i64 r) {
  char buf[32];
  u64 value, previous;
  value = Read64(m->weg[r]);
  previous = Read64(laststate.weg[r]);
  if (value != previous) AppendPanel(p, i, "\033[7m");
  snprintf(buf, sizeof(buf), "%-3s", kRegisterNames[m->mode][r]);
  AppendPanel(p, i, buf);
  AppendPanel(p, i, " ");
  snprintf(buf, sizeof(buf), "%0*" PRIx64, GetRegHexWidth(), value);
  AppendPanel(p, i, buf);
  if (value != previous) AppendPanel(p, i, "\033[27m");
  AppendPanel(p, i, "  ");
}

static void DrawSegment(struct Panel *p, i64 i, struct DescriptorCache value,
                        struct DescriptorCache previous, const char *name) {
  bool changed = value.sel != previous.sel || value.base != previous.base;
  char buf[32];
  if (changed) AppendPanel(p, i, "\033[7m");
  snprintf(buf, sizeof(buf), "%-3s", name);
  AppendPanel(p, i, buf);
  AppendPanel(p, i, " ");
  snprintf(buf, sizeof(buf), "%04" PRIx16 " (@%08" PRIx64 ")", value.sel,
           value.base);
  AppendPanel(p, i, buf);
  if (changed) AppendPanel(p, i, "\033[27m");
  AppendPanel(p, i, "  ");
}

static void DrawSt(struct Panel *p, i64 i, i64 r) {
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
  AppendPanel(p, i, FormatLongDouble(buf, value));
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
  snprintf(buf, sizeof(buf), "%-3s %0*" PRIx64 "  FLG", kRipName[m->mode],
           GetRegHexWidth(), m->ip);
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

static void DrawXmm(struct Panel *p, i64 i, i64 r) {
  float f;
  double d;
  wchar_t ival;
  char buf[32];
  bool changed;
  u64 itmp;
  u8 xmm[16];
  i64 j, k, n;
  int cells, left, cellwidth, panwidth;
  memcpy(xmm, m->xmm[r], sizeof(xmm));
  changed = memcmp(xmm, laststate.xmm[r], sizeof(xmm)) != 0;
  if (changed) AppendPanel(p, i, "\033[7m");
  left = sprintf(buf, "XMM%-2" PRId64 "", r);
  AppendPanel(p, i, buf);
  cells = GetXmmTypeCellCount(r);
  panwidth = p->right - p->left;
  cellwidth = MIN(MAX(0, (panwidth - left) / cells - 1), (int)sizeof(buf) - 1);
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
            sprintf(buf, "%lc", (wint_t)ival);
          } else {
            sprintf(buf, "%.*x", xmmtype.size[r] * 8 / 4, (unsigned)ival);
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
  i64 i, n;
  n = p->bottom - p->top;
  if (n > 0) {
    for (i = 0; i < MIN(16, n); ++i) {
      DrawXmm(p, i, i);
    }
  }
}

static void ScrollMemoryView(struct Panel *p, struct MemoryView *v, i64 a) {
  i64 i, n, w;
  w = DUMPWIDTH * (1ull << v->zoom);
  n = p->bottom - p->top;
  i = a / w;
  if (!(v->start <= i && i < v->start + n)) {
    v->start = i - n / 4;
  }
}

static void ZoomMemoryView(struct MemoryView *v, i64 y, i64 x, int dy) {
  i64 a, b, i, s;
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
  ScrollMemoryView(&pan.code, &codeview, GetPc(m));
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

static void DrawMemoryZoomed(struct Panel *p, struct MemoryView *view,
                             long histart, long hiend) {
  bool high, changed;
  u8 *canvas, *chunk, *invalid;
  i64 a, b, c, d, n, i, j, k, size;
  struct ContiguousMemoryRanges ranges;
  a = view->start * DUMPWIDTH * (1ull << view->zoom);
  b = (view->start + (p->bottom - p->top)) * DUMPWIDTH * (1ull << view->zoom);
  size = (p->bottom - p->top) * DUMPWIDTH;
  canvas = (u8 *)calloc(1, size);
  invalid = (u8 *)calloc(1, size);
  memset(&ranges, 0, sizeof(ranges));
  FindContiguousMemoryRanges(m, &ranges);
  for (k = i = 0; i < ranges.i; ++i) {
    if ((a >= ranges.p[i].a && a < ranges.p[i].b) ||
        (b >= ranges.p[i].a && b < ranges.p[i].b) ||
        (a < ranges.p[i].a && b >= ranges.p[i].b)) {
      c = MAX(a, ranges.p[i].a);
      d = MIN(b, ranges.p[i].b);
      n = ROUNDUP(d - c, 1ull << view->zoom);
      chunk = (u8 *)malloc(n);
      CopyFromUser(m, chunk, c, d - c);
      memset(chunk + (d - c), 0, n - (d - c));
      for (j = 0; j < view->zoom; ++j) {
        Magikarp(chunk, n);
        n >>= 1;
      }
      j = (c - a) / (1ull << view->zoom);
      memset(invalid + k, -1, j - k);
      memcpy(canvas + j, chunk, MIN(n, size - j));
      k = j + MIN(n, size - j);
      free(chunk);
    }
  }
  memset(invalid + k, -1, size - k);
  free(ranges.p);
  high = false;
  for (c = i = 0; i < p->bottom - p->top; ++i) {
    AppendFmt(&p->lines[i], "%0*" PRIx64 " ", GetAddrHexWidth(),
              ((view->start + i) * DUMPWIDTH * ((u64)1 << view->zoom)) &
                  0x0000ffffffffffff);
    for (j = 0; j < DUMPWIDTH; ++j, ++c) {
      a = ((view->start + i) * DUMPWIDTH + j + 0) * (1ull << view->zoom);
      b = ((view->start + i) * DUMPWIDTH + j + 1) * (1ull << view->zoom);
      changed = ((histart >= a && hiend < b) ||
                 (histart && hiend && histart >= a && hiend < b));
      if (changed && !high) {
        high = true;
        AppendStr(&p->lines[i], "\033[7m");
      } else if (!changed && high) {
        AppendStr(&p->lines[i], "\033[27m");
        high = false;
      }
      if (invalid[c]) {
        AppendWide(&p->lines[i], L'⋅');
      } else {
        AppendWide(&p->lines[i], kCp437[canvas[c]]);
      }
    }
    if (high) {
      AppendStr(&p->lines[i], "\033[27m");
      high = false;
    }
  }
  free(invalid);
  free(canvas);
}

static void DrawMemoryUnzoomed(struct Panel *p, struct MemoryView *view,
                               i64 histart, i64 hiend) {
  int c, s, x, sc;
  i64 i, j, k;
  bool high, changed;
  high = false;
  for (i = 0; i < p->bottom - p->top; ++i) {
    AppendFmt(&p->lines[i], "%0*" PRIx64 " ", GetAddrHexWidth(),
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
            Abort();
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

static void DrawMemory(struct Panel *p, struct MemoryView *view, i64 histart,
                       i64 hiend) {
  if (p->top == p->bottom) return;
  if (view->zoom) {
    DrawMemoryZoomed(p, view, histart, hiend);
  } else {
    DrawMemoryUnzoomed(p, view, histart, hiend);
  }
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
  i64 addr;
  const char *name;
  char *s, buf[256];
  i64 i, sym, line = 0;
  if (p->top == p->bottom) return;
  for (i = watchpoints.i; i--;) {
    if (watchpoints.p[i].disable) continue;
    if (line >= breakpointsstart) {
      addr = watchpoints.p[i].addr;
      sym = DisFindSym(dis, addr);
      name = sym != -1 ? dis->syms.stab + dis->syms.p[sym].name : "UNKNOWN";
      snprintf(buf, sizeof(buf), "%0*" PRIx64 " %s", GetAddrHexWidth(), addr,
               name);
      AppendPanel(p, line - breakpointsstart, buf);
      if (sym != -1 && addr != dis->syms.p[sym].addr) {
        snprintf(buf, sizeof(buf), "+%#" PRIx64, addr - dis->syms.p[sym].addr);
        AppendPanel(p, line - breakpointsstart, buf);
      }
      snprintf(buf, sizeof(buf), " [%#" PRIx64 "]", watchpoints.p[i].oldvalue);
      AppendPanel(p, line - breakpointsstart, buf);
    }
    ++line;
  }
  for (i = breakpoints.i; i--;) {
    if (breakpoints.p[i].disable) continue;
    if (line >= breakpointsstart) {
      addr = breakpoints.p[i].addr;
      sym = DisFindSym(dis, addr);
      name = sym != -1 ? dis->syms.stab + dis->syms.p[sym].name : "UNKNOWN";
      s = buf;
      s += sprintf(s, "%0*" PRIx64 " ", GetAddrHexWidth(), addr);
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
  switch (m->mode) {
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
  i64 sym;
  u8 *r;
  const char *name;
  char *s, line[256];
  i64 sp, bp, rp;
  if (p->top == p->bottom) return;
  rp = m->ip;
  bp = Read64(m->bp);
  sp = Read64(m->sp);
  for (i = 0; i < p->bottom - p->top;) {
    sym = DisFindSym(dis, rp);
    name = sym != -1 ? dis->syms.stab + dis->syms.p[sym].name : "UNKNOWN";
    s = line;
    s += sprintf(s, "%0*" PRIx64 " %0*" PRIx64 " ", GetAddrHexWidth(),
                 m->ss.base + bp, GetAddrHexWidth(), rp);
    s = Demangle(s, name, DIS_MAX_SYMBOL_LENGTH);
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
    if (((m->ss.base + bp) & 0xfff) > 0xff0) break;
    if (!(r = LookupAddress(m, m->ss.base + bp))) {
      AppendPanel(p, i - framesstart, "CORRUPT FRAME POINTER");
      break;
    }
    sp = bp;
    bp = ReadWord(r + 0);
    rp = ReadWord(r + 8);
  }
}

static void CheckFramePointerImpl(void) {
  u8 *r;
  i64 bp, rp;
  static i64 lastbp;
  bp = Read64(m->bp);
  if (bp && bp == lastbp) return;
  lastbp = bp;
  rp = m->ip;
  while (bp) {
    if (!(r = LookupAddress(m, m->ss.base + bp))) {
      LOGF("corrupt frame: %0*" PRIx64 "", GetAddrHexWidth(), bp);
      ThrowProtectionFault(m);
    }
    bp = Read64(r + 0) - 0;
    rp = Read64(r + 8) - 1;
    if (!bp && !(m->bofram[0] <= rp && rp <= m->bofram[1])) {
      LOGF("bad frame !(%0*" PRIx64 " <= %0*" PRIx64 " <= %0*" PRIx64 ")",
           GetAddrHexWidth(), m->bofram[0], GetAddrHexWidth(), rp,
           GetAddrHexWidth(), m->bofram[1]);
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

static int AppendStat(struct Buffer *b, int width, const char *name, i64 value,
                      bool changed) {
  char valbuf[27];
  AppendChar(b, ' ');
  if (changed) AppendStr(b, "\033[31m");
  FormatInt64Thousands(valbuf, value);
  width = AppendFmt(b, "%*s %s", width, valbuf, name);
  if (changed) AppendStr(b, "\033[39m");
  return 1 + width;
}

static const char *DescribeAction(void) {
  static char buf[128];
  char *p = buf;
  buf[0] = 0;
  if (action & RESTART) p = stpcpy(buf, "|RESTART");
  if (action & REDRAW) p = stpcpy(p, "|REDRAW");
  if (action & CONTINUE) p = stpcpy(p, "|CONTINUE");
  if (action & STEP) p = stpcpy(p, "|STEP");
  if (action & NEXT) p = stpcpy(p, "|NEXT");
  if (action & FINISH) p = stpcpy(p, "|FINISH");
  if (action & FAILURE) p = stpcpy(p, "|FAILURE");
  if (action & WINCHED) p = stpcpy(p, "|WINCHED");
  if (action & INT) p = stpcpy(p, "|INT");
  if (action & QUIT) p = stpcpy(p, "|QUIT");
  if (action & EXIT) p = stpcpy(p, "|EXIT");
  if (action & ALARM) p = stpcpy(p, "|ALARM");
  return buf + !!buf[0];
}

static char *GetStatus(int m) {
  bool first;
  unsigned i, n;
  struct timespec t;
  struct Buffer s = {0};
  if (statusmessage && CompareTime(GetTime(), statusexpires)) {
    AppendStr(&s, statusmessage);
  } else {
    AppendStr(&s, "das blinkenlights");
  }
  n = ARRAYLEN(keystrokes.p);
  for (first = true, t = GetTime(), i = 1; i <= n; --i) {
    if (!keystrokes.p[(keystrokes.i - i) % n][0]) continue;
    if (CompareTime(SubtractTime(t, keystrokes.s[(keystrokes.i - i) % n]),
                    FromSeconds(1)) > 0) {
      continue;
    }
    if (first) {
      first = false;
      AppendStr(&s, " (keystroke: ");
    } else {
      AppendChar(&s, ' ');
    }
    AppendStr(&s, keystrokes.p[(keystrokes.i - i) % n]);
  }
  if (!first) {
    AppendChar(&s, ')');
  }
  return s.p;
}

static void DrawStatus(struct Panel *p) {
#define MEMSTAT(f) m->system->memstat.f, m->system->memstat.f != lastmemstat.f
  long toto;
  char *status;
  struct Buffer *s;
  int yn, xn, rw, fds;
  rw = 0;
  yn = p->top - p->bottom;
  xn = p->right - p->left;
  if (!yn || !xn) return;
  fds = CountFds(&m->system->fds);
  s = (struct Buffer *)malloc(sizeof(*s));
  memset(s, 0, sizeof(*s));
  rw += AppendStr(s, DescribeAction());
  rw += AppendStat(s, 12, "ips", ips, false);
  toto = kRealSize + (long)m->system->memstat.allocated * 4096;
  rw += AppendStat(s, 10, "kb", toto / 1024, false);
  if (m->nolinear) rw += AppendStat(s, 8, "reserve", MEMSTAT(reserved));
  if (m->nolinear) rw += AppendStat(s, 8, "commit", MEMSTAT(committed));
  if (m->nolinear) rw += AppendStat(s, 5, "freed", MEMSTAT(freed));
  rw += AppendStat(s, 5, "tables", MEMSTAT(pagetables));
  rw += AppendStat(s, 3, "fds", fds, fds != lastfds);
  status = GetStatus(xn - rw);
  AppendFmt(&p->lines[0], "\033[7m%-*s%s\033[0m", xn - rw, status, s->p);
  free(status);
  free(s->p);
  free(s);
  lastmemstat = m->system->memstat;
  lastfds = fds;
#undef MEMSTAT
}

bool PreventBufferbloat(void) {
  bool should_write;
  struct timespec time, rate;
  static struct timespec last;
  time = GetTime();
  rate = FromMicroseconds(1. / FPS * 1e6);
  if (CompareTime(SubtractTime(time, last), rate) >= 0) {
    should_write = true;
    last = time;
  } else if (TURBO) {
    should_write = false;
  } else {
    SleepTime(SubtractTime(rate, SubtractTime(time, last)));
    should_write = true;
    last = time;
  }
  return should_write;
}

static void ClearHistory(void) {
  unsigned i;
  for (i = 0; i < HISTORY; ++i) {
    if (g_history.p[i].data) {
      free(g_history.p[i].data);
      g_history.p[i].data = 0;
    }
  }
  g_history.viewing = 0;
  g_history.count = 0;
}

static void AddHistory(const char *ansi, size_t size) {
  struct Rendering *r;
  unassert(g_history.count <= HISTORY);
  if (g_history.count &&
      g_history.p[(g_history.index - 1) % HISTORY].cycle == cycle) {
    return;  // execution hasn't advanced yet
  }
  if (g_history.count < HISTORY) {
    ++g_history.count;
  }
  r = g_history.p + g_history.index % HISTORY;
  free(r->data);
  r->cycle = cycle;
  r->origsize = size;
  r->data = Deflate(ansi, size, &r->compsize);
  ++g_history.index;
  STATISTIC(AVERAGE(redraw_compressed_bytes, r->compsize));
  STATISTIC(AVERAGE(redraw_uncompressed_bytes, r->origsize));
}

static void RewindHistory(int delta) {
  int count = g_history.count;
  int viewing = g_history.viewing;
  g_history.viewing = MAX(0, MIN(viewing + delta, count));
  // skip over the first history entry, since it doesn't feel right to
  // need to press up arrow twice to see some real history.
  if (g_history.viewing == 1) {
    if (delta > 0) {
      g_history.viewing = 2;
    } else if (delta < 0) {
      g_history.viewing = 0;
    }
  }
  g_history.viewing = MIN(g_history.viewing, g_history.count);
  // clear the crash dialog box if it exists.
  action &= ~FAILURE;
}

static void ShowHistory(void) {
  char *ansi;
  size_t len, size;
  char status[1024];
  struct Rendering *r;
  unassert(g_history.viewing > 0);
  unassert(g_history.viewing <= HISTORY);
  unassert(g_history.viewing <= g_history.count);
  r = g_history.p + (g_history.index - g_history.viewing) % HISTORY;
  unassert(r->data);
  len = snprintf(status, sizeof(status),
                 "\033[7;35;47m\033[%d;0H"
                 " [ HISTORY %d/%d CYCLE %" PRIu64 " ] "
                 "\033[0m\033[%d;%dH",
                 tyn, g_history.count - (g_history.viewing - 1),
                 g_history.count, r->cycle, tyn, txn);
  unassert(len < sizeof(status));
  size = r->origsize + len;
  unassert(ansi = (char *)malloc(size));
  Inflate(ansi, r->origsize, r->data, r->compsize);
  memcpy(ansi + r->origsize, status, len);
  if (PreventBufferbloat()) {
    unassert(UninterruptibleWrite(ttyout, ansi, size) != -1);
  }
  free(ansi);
}

static void Redraw(bool force) {
  int i, j;
  char *ansi;
  size_t size;
  double execsecs;
  struct timespec start_draw, end_draw;
  if (!tuimode) return;
  if (g_history.viewing) {
    ShowHistory();
    return;
  }
  start_draw = GetTime();
  execsecs = ToNanoseconds(SubtractTime(start_draw, last_draw)) * 1e-9;
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
  ips = last_cycle ? (cycle - last_cycle) / execsecs : 0;
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
  if (showprofile) {
    DrawHr(&pan.frameshr, "PROFILE");
  } else if (m->bofram[0]) {
    DrawHr(&pan.frameshr, "PROTECTED FRAMES");
  } else {
    DrawHr(&pan.frameshr, "FRAMES");
  }
  DrawHr(&pan.ssehr, "SSE");
  DrawHr(&pan.codehr, "CODE");
  DrawHr(&pan.readhr, "READ");
  DrawHr(&pan.writehr, "WRITE");
  DrawHr(&pan.stackhr, "STACK");
  DrawMaps(&pan.maps);
  if (showprofile) {
    DrawProfile(&pan.frames);
  } else {
    DrawFrames(&pan.frames);
  }
  DrawBreakpoints(&pan.breakpoints);
  DrawMemory(&pan.code, &codeview, GetPc(m), GetPc(m) + m->xedd->length);
  DrawMemory(&pan.readdata, &readview, readaddr, readaddr + readsize);
  DrawMemory(&pan.writedata, &writeview, writeaddr, writeaddr + writesize);
  DrawMemory(&pan.stack, &stackview, GetSp(), GetSp() + GetPointerWidth());
  DrawStatus(&pan.status);
  unassert(ansi = RenderPanels(ARRAYLEN(pan.p), pan.p, tyn, txn, &size));
  end_draw = GetTime();
  (void)end_draw;
  STATISTIC(AVERAGE(redraw_latency_us,
                    ToMicroseconds(SubtractTime(end_draw, start_draw))));
  if (force || PreventBufferbloat()) {
    unassert(UninterruptibleWrite(ttyout, ansi, size) != -1);
  }
  AddHistory(ansi, size);
  free(ansi);
  last_cycle = cycle;
  last_draw = GetTime();
}

static void ReactiveDraw(void) {
  if (tuimode) {
    // LOGF("%" PRIx64 " %s ReactiveDraw", GetPc(m), tuimode ? "TUI" : "EXEC");
    Redraw(true);
    tick = speed;
  }
}

static void DescribeKeystroke(char *b, const char *p) {
  int c;
  do {
    c = *p++ & 255;
    if (c == '\033') {
      b = stpcpy(b, "ALT-");
      c = *p++ & 255;
    }
    if (c <= 32) {
      b = stpcpy(b, "CTRL-");
      c = Ctrl(c);
    }
    *b++ = c;
    *b = 0;
  } while (*p);
}

static void RecordKeystroke(const char *k) {
  if (!strchr(k, '[')) {
    keystrokes.s[keystrokes.i] = GetTime();
    DescribeKeystroke(keystrokes.p[keystrokes.i], k);
    keystrokes.i = (keystrokes.i + 1) % ARRAYLEN(keystrokes.p);
  }
}

static void HandleAlarm(void) {
  alarmed = false;
  action &= ~ALARM;
  pty->conf &= ~kPtyBell;
  free(statusmessage);
  statusmessage = NULL;
}

static void HandleTerminalResize(void) {
  GetTtySize(ttyout);
  ClearHistory();
}

static void HandleAppReadInterrupt(void) {
  LOGF("HandleAppReadInterrupt");
  if (action & ALARM) {
    HandleAlarm();
  }
  if (action & WINCHED) {
    HandleTerminalResize();
    action &= ~WINCHED;
  }
  if (action & INT) {
    action &= ~INT;
    RecordKeystroke("\3");
    ReactiveDraw();
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
#ifdef __COSMOPOLITAN__
  if (!IsWindows())
#endif
    poll(fds, ARRAYLEN(fds), 0);
  return fds[0].revents & (POLLIN | POLLERR);
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

static struct Mouse ParseMouse(char *p) {
  int e, x, y;
  struct Mouse m;
  e = strtol(p, &p, 10);
  if (*p == ';') ++p;
  x = strtol(p, &p, 10);
  x = MIN(txn, MAX(1, x)) - 1;
  if (*p == ';') ++p;
  y = strtol(p, &p, 10);
  y = MIN(tyn, MAX(1, y)) - 1;
  e |= (*p == 'm') << 2;
  m.y = y;
  m.x = x;
  m.e = e;
  return m;
}

static ssize_t ReadAnsi(int fd, char *p, size_t n) {
  ssize_t rc;
  struct Mouse mo;
  for (;;) {
    LOGF("%" PRIx64 " %s ReadAnsi", GetPc(m), tuimode ? "TUI" : "EXEC");
    readingteletype = true;
    ReactiveDraw();
    rc = readansi(fd, p, n);
    readingteletype = false;
    if (rc != -1) {
      if (tuimode && rc > 3 && p[0] == '\033' && p[1] == '[') {
        if (p[2] == '2' && p[3] == '0' && p[4] == '0' && p[5] == '~') {
          belay = true;
          continue;
        }
        if (p[2] == '2' && p[3] == '0' && p[4] == '1' && p[5] == '~') {
          belay = false;
          continue;
        }
        if (p[2] == '<') {
          mo = ParseMouse(p + 3);
          if (LocatePanel(mo.y, mo.x) != &pan.display) {
            HandleKeyboard(p);
            continue;
          }
        }
      }
      return rc;
    } else {
      unassert(errno == EINTR);
      HandleAppReadInterrupt();
      return eintr();
    }
  }
}

static ssize_t ReadPtyFdDirect(int fd) {
  ssize_t rc;
  char buf[32];
  LOGF("ReadPtyFdDirect");
  pty->conf |= kPtyBlinkcursor;
  do {
    rc = ReadAnsi(fd, buf, sizeof(buf));
  } while (rc == -1 && errno == EINTR);
  pty->conf &= ~kPtyBlinkcursor;
  if (rc > 0) {
    PtyWriteInput(pty, buf, rc);
    ReactiveDraw();
    rc = 0;
  }
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
    for (;;) {
      if ((rc = PtyRead(pty, data, size))) {
        return rc;
      }
      if (ReadPtyFdDirect(fd) == -1) {
        return -1;
      }
    }
  } else {
    return 0;
  }
}

static int OnPtyFdPoll(struct pollfd *fds, nfds_t nfds, int ms) {
  bool once;
  int i, t, re;
  struct pollfd p2;
  ms &= INT_MAX;
  for (once = false, t = i = 0; i < nfds; ++i) {
    re = 0;
    if (fds[i].fd >= 0) {
      if (pty->input.i) {
        re = POLLIN | POLLOUT;
        ++t;
      } else {
        if (!once) {
          ReactiveDraw();
          once = true;
        }
        p2.fd = fds[i].fd;
        p2.events = fds[i].events;
        switch (poll(&p2, 1, ms)) {
          case -1:
            re = POLLERR;
            ++t;
            break;
          case 0:
            break;
          case 1:
            re = p2.revents;
            ++t;
            break;
          default:
            __builtin_unreachable();
        }
      }
    }
    fds[i].revents = re;
  }
  return t;
}

static void DrawDisplayOnly(struct Panel *p) {
  struct Buffer b;
  int i, y, yn, xn, tly, tlx;
  yn = MIN(tyn, p->bottom - p->top);
  xn = MIN(txn, p->right - p->left);
  for (i = 0; i < yn; ++i) {
    p->lines[i].i = 0;
  }
  DrawDisplay(p);
  memset(&b, 0, sizeof(b));
  tly = tyn / 2 - yn / 2;
  tlx = txn / 2 - xn / 2;
  AppendStr(&b, "\033[0m\033[H");
  for (y = 0; y < tyn; ++y) {
    if (y) AppendStr(&b, "\r\n");
    if (tly <= y && y <= tly + yn) {
      for (i = 0; i < tlx; ++i) {
        AppendChar(&b, ' ');
      }
      AppendData(&b, p->lines[y - tly].p, p->lines[y - tly].i);
    }
    AppendStr(&b, "\033[0m\033[K");
  }
  write(ttyout, b.p, b.i);
  free(b.p);
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

static int OnPtyFdTiocswinsz(int fd, const struct winsize *ws) {
  return 0;
}

static int OnPtyFdTcsets(int fd, u64 request, struct termios *c) {
  return 0;
}

static int OnPtyTcgetattr(int fd, struct termios *c) {
  // TODO(jart): We should just use the Linux ABI for these.
  memset(c, 0, sizeof(*c));
  c->c_iflag = ICRNL | IXON
#ifdef IUTF8
               | IUTF8
#endif
      ;
  c->c_oflag = 0
#ifdef ONLCR
               | ONLCR
#endif
      ;
  c->c_cflag = CREAD | CS8;
  c->c_lflag = ISIG | ECHOE | IEXTEN | ECHOK
#ifdef ECHOCTL
               | ECHOCTL
#endif
#ifdef ECHOKE
               | ECHOKE
#endif
      ;
  c->c_cc[VMIN] = 1;
  c->c_cc[VTIME] = 0;
  c->c_cc[VINTR] = Ctrl('C');
  c->c_cc[VQUIT] = Ctrl('\\');
  c->c_cc[VERASE] = Ctrl('?');
  c->c_cc[VKILL] = Ctrl('U');
  c->c_cc[VEOF] = Ctrl('D');
  c->c_cc[VSTART] = Ctrl('Q');
  c->c_cc[VSTOP] = Ctrl('S');
  c->c_cc[VSUSP] = Ctrl('Z');
  c->c_cc[VEOL] = Ctrl('@');
#ifdef VSWTC
  c->c_cc[VSWTC] = Ctrl('@');
#endif
#ifdef VREPRINT
  c->c_cc[VREPRINT] = Ctrl('R');
#endif
#ifdef VDISCARD
  c->c_cc[VDISCARD] = Ctrl('O');
#endif
#ifdef VWERASE
  c->c_cc[VWERASE] = Ctrl('W');
#endif
#ifdef VLNEXT
  c->c_cc[VLNEXT] = Ctrl('V');
#endif
#ifdef VEOL2
  c->c_cc[VEOL2] = Ctrl('@');
#endif
  if (!(pty->conf & kPtyNocanon)) c->c_iflag |= ICANON;
  if (!(pty->conf & kPtyNoecho)) c->c_iflag |= ECHO;
  if (!(pty->conf & kPtyNoopost)) c->c_oflag |= OPOST;
  return 0;
}

static int OnPtyTcsetattr(int fd, int cmd, const struct termios *c) {
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

static const struct FdCb kFdCbPty = {
    .close = OnPtyFdClose,
    .readv = OnPtyFdReadv,
    .writev = OnPtyFdWritev,
    .poll = OnPtyFdPoll,
    .tcgetattr = OnPtyTcgetattr,
    .tcsetattr = OnPtyTcsetattr,
    .tcgetwinsize = OnPtyFdTiocgwinsz,
    .tcsetwinsize = OnPtyFdTiocswinsz,
};

static void LaunchDebuggerReactively(void) {
  LOGF("LaunchDebuggerReactively");
  LOGF("%s", systemfailure);
  if (tuimode) {
    action |= FAILURE;
  } else {
    if (react) {
      tuimode = true;
      action |= FAILURE;
    } else {
      fprintf(stderr, "ERROR: %s\n", systemfailure);
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
           "SEGMENTATION FAULT %0*" PRIx64, GetAddrHexWidth(), m->faultaddr);
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
  stpcpy(systemfailure, "INSTRUCTION DECODE ERROR");
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
  m->ah = 0x00;
  SetCarry(false);
}

static void OnDiskServiceBadCommand(void) {
  m->ah = 0x01;
  SetCarry(true);
}

static void OnDiskServiceGetParams(void) {
  size_t lastsector, lastcylinder, lasthead;
  lastcylinder = GetLastIndex(m->system->elf.mapsize, 512 * 63 * 255, 0, 1023);
  lasthead = GetLastIndex(m->system->elf.mapsize, 512 * 63, 0, 255);
  lastsector = GetLastIndex(m->system->elf.mapsize, 512, 1, 63);
  m->dl = 1;
  m->dh = lasthead;
  m->cl = lastcylinder >> 8 << 6 | lastsector;
  m->ch = lastcylinder;
  m->ah = 0;
  m->es.sel = m->es.base = 0;
  Put16(m->di, 0);
  SetCarry(false);
}

static void OnDiskServiceReadSectors(void) {
  i64 addr, size;
  i64 sectors, drive, head, cylinder, sector, offset;
  sectors = m->al;
  drive = m->dl;
  head = m->dh;
  cylinder = (m->cl & 192) << 2 | m->ch;
  sector = (m->cl & 63) - 1;
  size = sectors * 512;
  offset = sector * 512 + head * 512 * 63 + cylinder * 512 * 63 * 255;
  (void)drive;
  LOGF("bios read sectors %" PRId64 " "
       "@ sector %" PRId64 " cylinder %" PRId64 " head %" PRId64
       " drive %" PRId64 " offset %#" PRIx64 "",
       sectors, sector, cylinder, head, drive, offset);
  if (0 <= sector && offset + size <= m->system->elf.mapsize) {
    addr = m->es.base + Get16(m->bx);
    if (addr + size <= kRealSize) {
      SetWriteAddr(m, addr, size);
      memcpy(m->system->real + addr, m->system->elf.map + offset, size);
      m->ah = 0x00;
      SetCarry(false);
    } else {
      m->al = 0x00;
      m->ah = 0x02;
      SetCarry(true);
    }
  } else {
    LOGF("bios read sector failed 0 <= %" PRId64 " && %" PRIx64 " + %" PRIx64
         " <= %lx",
         sector, offset, size, m->system->elf.mapsize);
    m->al = 0x00;
    m->ah = 0x0d;
    SetCarry(true);
  }
}

static void OnDiskServiceProbeExtended(void) {
  u8 drive = m->dl;
  u16 magic = Get16(m->bx);
  (void)drive;
  if (magic == 0x55AA) {
    Put16(m->bx, 0xAA55);
    m->ah = 0x30;
    SetCarry(false);
  } else {
    m->ah = 0x01;
    SetCarry(true);
  }
}

static void OnDiskServiceReadSectorsExtended(void) {
  u8 drive = m->dl;
  i64 pkt_addr = m->ds.base + Get16(m->si), addr, sectors, size, lba, offset;
  u8 pkt_size, *pkt;
  (void)drive;
  SetReadAddr(m, pkt_addr, 1);
  pkt = m->system->real + pkt_addr;
  pkt_size = Get8(pkt);
  if ((pkt_size != 0x10 && pkt_size != 0x18) || Get8(pkt + 1) != 0) {
    m->ah = 0x01;
    SetCarry(true);
  } else {
    SetReadAddr(m, pkt_addr, pkt_size);
    addr = Read32(pkt + 4);
    if (addr == 0xFFFFFFFF && pkt_size == 0x18) {
      addr = Read64(pkt + 0x10);
    } else {
      addr = (addr >> 16 << 4) + (addr & 0xFFFF);
    }
    sectors = Read16(pkt + 2);
    size = sectors * 512;
    lba = Read32(pkt + 8);
    offset = lba * 512;
    LOGF("bios read sector ext 0 <= %" PRId64 " && %" PRIx64 " + %" PRIx64
         " <= %lx",
         lba, offset, size, m->system->elf.mapsize);
    if (offset >= m->system->elf.mapsize ||
        offset + size > m->system->elf.mapsize) {
      LOGF("bios read sector failed 0 <= %" PRId64 " && %" PRIx64 " <= %lx",
           lba, offset, m->system->elf.mapsize);
      SetWriteAddr(m, pkt_addr + 2, 2);
      Write16(pkt + 2, 0);
      m->ah = 0x0d;
    } else if (addr >= kRealSize || addr + size > kRealSize) {
      SetWriteAddr(m, pkt_addr + 2, 2);
      Write16(pkt + 2, 0);
      m->ah = 0x02;
      SetCarry(true);
    } else {
      SetWriteAddr(m, addr, size);
      memcpy(m->system->real + addr, m->system->elf.map + offset, size);
      m->ah = 0x00;
      SetCarry(false);
    }
  }
}

static void OnDiskService(void) {
  switch (m->ah) {
    case 0x00:
      OnDiskServiceReset();
      break;
    case 0x02:
      OnDiskServiceReadSectors();
      break;
    case 0x08:
      OnDiskServiceGetParams();
      break;
    case 0x41:
      OnDiskServiceProbeExtended();
      break;
    case 0x42:
      OnDiskServiceReadSectorsExtended();
      break;
    default:
      OnDiskServiceBadCommand();
      break;
  }
}

static void OnVidyaServiceSetMode(void) {
  if (LookupAddress(m, 0xB0000)) {
    vidya = m->al;
  } else {
    LOGF("maybe you forgot -r flag");
  }
}

static void OnVidyaServiceGetMode(void) {
  m->al = vidya;
  m->ah = 80;  // columns
  m->bh = 0;   // page
}

static void OnVidyaServiceSetCursorPosition(void) {
  PtySetY(pty, m->dh);
  PtySetX(pty, m->dl);
}

static void OnVidyaServiceGetCursorPosition(void) {
  m->dh = pty->y;
  m->dl = pty->x;
  m->ch = 5;  // cursor ▂ scan lines 5..7 of 0..7
  m->cl = 7 | !!(pty->conf & kPtyNocursor) << 5;
}

static int GetVidyaByte(unsigned char b) {
  if (0x20 <= b && b <= 0x7F) return b;
#if 0
  /*
   * The original hardware displayed 0x00, 0x20, and 0xff as space. It
   * made sense for viewing sparse binary data that 0x00 be blank. But
   * it doesn't make sense for dense data too, and we don't need three
   * space characters. So we diverge in our implementation and display
   * 0xff as lambda.
   */
  if (b == 0xFF) b = 0x00;
#endif
  return kCp437[b];
}

static void OnVidyaServiceWriteCharacter(void) {
  int i;
  u64 w;
  char *p, buf[32];
  p = buf;
  p += FormatCga(m->bl, p);
  p = stpcpy(p, "\0337");
  w = tpenc(GetVidyaByte(m->al));
  do {
    *p++ = w;
  } while ((w >>= 8));
  p = stpcpy(p, "\0338");
  for (i = Get16(m->cx); i--;) {
    PtyWrite(pty, buf, p - buf);
  }
}

static wint_t VidyaServiceXlatTeletype(u8 c) {
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
  u64 w;
  char buf[12];
  n = 0 /* FormatCga(m->bl, buf) */;
  w = tpenc(VidyaServiceXlatTeletype(m->al));
  do {
    buf[n++] = w;
  } while ((w >>= 8));
  PtyWrite(pty, buf, n);
}

static void OnVidyaService(void) {
  switch (m->ah) {
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
  static char buf[32];
  static size_t pending;
  LOGF("OnKeyboardServiceReadKeyPress");
  if (!tuimode) {
    tuimode = true;
    action |= CONTINUE;
  }
  pty->conf |= kPtyBlinkcursor;
  if (!pending) {
    rc = ReadAnsi(ttyin, buf, sizeof(buf));
    if (rc > 0) {
      pending = rc;
    } else if (rc == -1 && errno == EINTR) {
      return;
    } else {
      exitcode = 0;
      action |= EXIT;
      return;
    }
  }
  b = buf[0];
  if (pending > 1) {
    memmove(buf, buf + 1, pending - 1);
  }
  --pending;
  pty->conf &= ~kPtyBlinkcursor;
  ReactiveDraw();
  if (b == 0177) b = '\b';
  m->ax[0] = b;
  m->ax[1] = 0;
}

static void OnKeyboardService(void) {
  switch (m->ah) {
    case 0x00:
      OnKeyboardServiceReadKeyPress();
      break;
    default:
      break;
  }
}

static void OnApmService(void) {
  if (Get16(m->ax) == 0x5300 && Get16(m->bx) == 0x0000) {
    Put16(m->bx, 'P' << 8 | 'M');
    SetCarry(false);
  } else if (Get16(m->ax) == 0x5301 && Get16(m->bx) == 0x0000) {
    SetCarry(false);
  } else if (Get16(m->ax) == 0x5307 && m->bl == 1 && m->cl == 3) {
    LOGF("APM SHUTDOWN");
    exit(0);
  } else {
    SetCarry(true);
  }
}

static void OnE820(void) {
  i64 addr;
  u8 p[20];
  addr = m->es.base + Get16(m->di);
  if (Get32(m->dx) == 0x534D4150 && Get32(m->cx) == 24 &&
      addr + (int)sizeof(p) <= kRealSize) {
    if (!Get32(m->bx)) {
      Store64(p + 0, 0);
      Store64(p + 8, kRealSize);
      Store32(p + 16, 1);
      memcpy(m->system->real + addr, p, sizeof(p));
      SetWriteAddr(m, addr, sizeof(p));
      Put32(m->cx, sizeof(p));
      Put32(m->bx, 1);
    } else {
      Put32(m->bx, 0);
      Put32(m->cx, 0);
    }
    Put32(m->ax, 0x534D4150);
    SetCarry(false);
  } else {
    SetCarry(true);
  }
}

static void OnInt15h(void) {
  if (Get32(m->ax) == 0xE820) {
    OnE820();
  } else if (m->ah == 0x53) {
    OnApmService();
  } else {
    SetCarry(true);
  }
}

static bool OnHalt(int interrupt) {
  LOGF("%" PRIx64 " %s OnHalt(%#x)", GetPc(m), tuimode ? "TUI" : "EXEC",
       interrupt);
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
    case kMachineEscape:
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
    case kMachineHalt:
    default:
      OnExit(interrupt & 255);
      return false;
  }
}

static void OnBinbase(struct Machine *m) {
  int i;
  i64 skew;
  skew = m->xedd->op.disp * 512;
  LOGF("skew binbase %" PRId64 " @ %0*" PRIx64 "", skew, GetAddrHexWidth(),
       GetPc(m));
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

static void SetStatus(const char *fmt, ...) {
  char *s;
  va_list va;
  struct itimerval it;
  va_start(va, fmt);
  unassert(vasprintf_(&s, fmt, va) >= 0);
  va_end(va);
  free(statusmessage);
  statusmessage = s;
  statusexpires = AddTime(GetTime(), FromSeconds(1));
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
  RewindHistory(+1);
}

static void OnDownArrow(void) {
  RewindHistory(-1);
}

static void OnPageUp(void) {
  RewindHistory(+100);
}

static void OnPageDown(void) {
  RewindHistory(-100);
}

static void OnHome(void) {
  RewindHistory(+g_history.count);
}

static void OnEnd(void) {
  RewindHistory(-g_history.count);
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

static void OnInt(void) {
  action |= INT;
  ReactiveDraw();
}

static void OnRestart(void) {
  action |= RESTART;
}

static void OnXmmType(void) {
  u8 t;
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
#ifdef __COSMOPOLITAN__
  if (!IsWindows())
#endif
    poll((struct pollfd[]){{ttyin, POLLIN}}, 1, ms);
}

static void OnMouseWheelUp(struct Panel *p, int y, int x) {
  if (p == &pan.disassembly) {
    RewindHistory(+WHEELDELTA);
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
    RewindHistory(-WHEELDELTA);
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

static void OnMouse(const char *p) {
  int e, x, y;
  struct Panel *ep;
  e = strtol(p, (char **)&p, 10);
  if (*p == ';') ++p;
  x = strtol(p, (char **)&p, 10);
  x = MIN(txn, MAX(1, x)) - 1;
  if (*p == ';') ++p;
  y = strtol(p, (char **)&p, 10);
  y = MIN(tyn, MAX(1, y)) - 1;
  e |= (*p == 'm') << 2;
  if ((ep = LocatePanel(y, x))) {
    y -= ep->top;
    x -= ep->left;
    switch (e) {
      case kMouseWheelUp:
        if (!natural) {
          OnMouseWheelUp(ep, y, x);
        } else {
          OnMouseWheelDown(ep, y, x);
        }
        break;
      case kMouseWheelDown:
        if (!natural) {
          OnMouseWheelDown(ep, y, x);
        } else {
          OnMouseWheelUp(ep, y, x);
        }
        break;
      case kMouseCtrlWheelUp:
        if (!natural) {
          OnMouseCtrlWheelUp(ep, y, x);
        } else {
          OnMouseCtrlWheelDown(ep, y, x);
        }
        break;
      case kMouseCtrlWheelDown:
        if (!natural) {
          OnMouseCtrlWheelDown(ep, y, x);
        } else {
          OnMouseCtrlWheelUp(ep, y, x);
        }
        break;
      default:
        break;
    }
  }
}

static void OnHelp(void) {
  dialog = HELP;
}

static void HandleKeyboard(const char *k) {
  const char *p = k;
  // LOGF("HandleKeyboard(%#x [%c])", *k, isprint(*k) ? *k : '.');
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
    CASE('V', ++verbose);
    CASE('p', showprofile = !showprofile);
    CASE('B', PopBreakpoint(&breakpoints));
    CASE('M', ToggleMouseTracking());
    CASE('\t', OnTab());
    CASE('\r', OnEnter());
    CASE('\n', OnEnter());
    CASE(Ctrl('C'), OnInt());
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
  RecordKeystroke(k);
}

static void ReadKeyboard(void) {
  char buf[64];
  // LOGF("ReadKeyboard");
  memset(buf, 0, sizeof(buf));
  dialog = NULL;
  if (readansi(ttyin, buf, sizeof(buf)) == -1) {
    if (errno == EINTR) {
      LOGF("ReadKeyboard interrupted");
      return;
    }
    fprintf(stderr, "ReadKeyboard failed: %s\n", strerror(errno));
    exit(1);
  }
  HandleKeyboard(buf);
}

static i64 ParseHexValue(const char *s) {
  char *ep;
  i64 x;
  x = strtoll(s, &ep, 16);
  if (*ep) {
    fputs("ERROR: bad hexadecimal: ", stderr);
    fputs(s, stderr);
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
    b.symbol = optarg_;
  }
  PushBreakpoint(&breakpoints, &b);
}

static void HandleWatchpointFlag(const char *s) {
  struct Watchpoint b;
  memset(&b, 0, sizeof(b));
  if (isdigit(*s)) {
    b.addr = ParseHexValue(s);
  } else {
    b.symbol = optarg_;
  }
  PushWatchpoint(&watchpoints, &b);
}

_Noreturn static void PrintUsage(int rc, FILE *f) {
  fprintf(f, "SYNOPSIS\n\n  %s%s", "blink", USAGE);
  exit(rc);
}

static void LogInstruction(void) {
  LOGF("EXEC %8" PRIx64 " SP %012" PRIx64 " AX %016" PRIx64 " CX %016" PRIx64
       " DX %016" PRIx64 " BX %016" PRIx64 " BP %016" PRIx64 " SI %016" PRIx64
       " DI %016" PRIx64 " R8 %016" PRIx64 " R9 %016" PRIx64 " R10 %016" PRIx64
       " R11 %016" PRIx64 " R12 %016" PRIx64 " R13 %016" PRIx64
       " R14 %016" PRIx64 " R15 %016" PRIx64 " FS %012" PRIx64
       " GS %012" PRIx64,
       m->ip, Get64(m->sp) & 0xffffffffffff, Get64(m->ax), Get64(m->cx),
       Get64(m->dx), Get64(m->bx), Get64(m->bp), Get64(m->si), Get64(m->di),
       Get64(m->r8), Get64(m->r9), Get64(m->r10), Get64(m->r11), Get64(m->r12),
       Get64(m->r13), Get64(m->r14), Get64(m->r15), m->fs.base & 0xffffffffffff,
       m->gs.base & 0xffffffffffff);
}

static void EnterWatchpoint(long bp) {
  LOGF("WATCHPOINT %0*" PRIx64 " %s", GetAddrHexWidth(), watchpoints.p[bp].addr,
       watchpoints.p[bp].symbol);
  snprintf(systemfailure, sizeof(systemfailure),
           "watchpoint %" PRIx64 " triggered%s%s", watchpoints.p[bp].addr,
           watchpoints.p[bp].symbol ? "\n" : "",
           watchpoints.p[bp].symbol ? watchpoints.p[bp].symbol : "");
  dialog = systemfailure;
  action &= ~(FINISH | NEXT | CONTINUE);
  action |= FAILURE;
  tuimode = true;
}

static void ProfileOp(struct Machine *m, u64 pc) {
  if (ophits &&                      //
      pc >= m->system->codestart &&  //
      pc < m->system->codestart + m->system->codesize) {
    ++ophits[pc - m->system->codestart];
  }
}

static void StartOp_Tui(P) {
  ++cycle;
  ProfileOp(m, m->ip - m->oplen);
}

static void Execute(void) {
  u64 c;
  if (g_history.viewing) {
    g_history.viewing = 0;
  }
  c = cycle;
  ExecuteInstruction(m);
  if (c == cycle) {
    ++cycle;
    ProfileOp(m, GetPc(m) - m->oplen);
  }
}

static void Exec(void) {
  int sig;
  ssize_t bp;
  int interrupt;
  LOGF("Exec");
  ExecSetup();
  if (!(interrupt = sigsetjmp(m->onhalt, 1))) {
    m->canhalt = true;
    if (!(action & CONTINUE) &&
        (bp = IsAtBreakpoint(&breakpoints, m->ip)) != -1) {
      LOGF("BREAK1 %0*" PRIx64 "", GetAddrHexWidth(), breakpoints.p[bp].addr);
    ReactToPoint:
      tuimode = true;
      LoadInstruction(m, GetPc(m));
      if (verbose) LogInstruction();
      Execute();
      if (m->signals) {
        if ((sig = ConsumeSignal(m)) && sig != SIGALRM_LINUX) {
          exit(128 + sig);
        }
      }
      CheckFramePointer();
    } else if (!(action & CONTINUE) &&
               (bp = IsAtWatchpoint(&watchpoints, m)) != -1) {
      LOGF("WATCH1 %0*" PRIx64 " %s", GetAddrHexWidth(), watchpoints.p[bp].addr,
           watchpoints.p[bp].symbol);
      goto ReactToPoint;
    } else {
      action &= ~CONTINUE;
      for (;;) {
        LoadInstruction(m, GetPc(m));
        if ((bp = IsAtBreakpoint(&breakpoints, m->ip)) != -1) {
          LOGF("BREAK2 %0*" PRIx64 "", GetAddrHexWidth(),
               breakpoints.p[bp].addr);
          action &= ~(FINISH | NEXT | CONTINUE);
          tuimode = true;
          break;
        }
        if ((bp = IsAtWatchpoint(&watchpoints, m)) != -1) {
          EnterWatchpoint(bp);
          break;
        }
        if (verbose) LogInstruction();
        Execute();
        if (m->signals) {
          if ((sig = ConsumeSignal(m)) && sig != SIGALRM_LINUX) {
            exit(128 + sig);
          }
        }
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
            EnqueueSignal(m, SIGINT_LINUX);
          }
        }
      }
    }
  } else {
    if (IsMakingPath(m)) {
      AbandonPath(m);
    }
    if (OnHalt(interrupt)) {
      if (!tuimode) {
        goto KeepGoing;
      }
    }
  }
  m->canhalt = false;
}

static void Tui(void) {
  int sig;
  ssize_t bp;
  int interrupt;
  bool interactive;
  LOGF("Tui");
  TuiSetup();
  SetupDraw();
  ScrollOp(&pan.disassembly, GetDisIndex());
  if (!(interrupt = sigsetjmp(m->onhalt, 1))) {
    m->canhalt = true;
    do {
      if (!(action & FAILURE)) {
        LoadInstruction(m, GetPc(m));
        if ((action & (FINISH | NEXT | CONTINUE)) &&
            (bp = IsAtBreakpoint(&breakpoints, m->ip)) != -1) {
          action &= ~(FINISH | NEXT | CONTINUE);
          LOGF("BREAK %0*" PRIx64 "", GetAddrHexWidth(),
               breakpoints.p[bp].addr);
        } else if ((action & (FINISH | NEXT | CONTINUE)) &&
                   (bp = IsAtWatchpoint(&watchpoints, m)) != -1) {
          action &= ~(FINISH | NEXT | CONTINUE);
          LOGF("WATCH %0*" PRIx64 " AT PC %" PRIx64, GetAddrHexWidth(),
               watchpoints.p[bp].addr, GetPc(m));
        }
      } else {
        m->xedd = (struct XedDecodedInst *)m->opcache->icache[0];
        m->xedd->length = 1;
        m->xedd->bytes[0] = 0xCC;
        m->xedd->op.rde &= ~00000077760000000000000;
        m->xedd->op.rde &= 0xCCull << 40;  // sets mopcode to int3
      }
      if (action & WINCHED) {
        HandleTerminalResize();
        action &= ~WINCHED;
      }
      interactive = ++tick >= speed;
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
        Redraw(false);
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
        RecordKeystroke("\3");
        if (action & (CONTINUE | NEXT | FINISH)) {
          action &= ~(CONTINUE | NEXT | FINISH);
        } else {
          EnqueueSignal(m, SIGINT_LINUX);
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
        if (!IsDebugBreak() && IsAtWatchpoint(&watchpoints, m) == -1) {
          UpdateXmmType(m->xedd->op.rde, &xmmtype);
          if (verbose) LogInstruction();
          Execute();
          if (m->signals) {
            if ((sig = ConsumeSignal(m)) && sig != SIGALRM_LINUX) {
              exit(128 + sig);
            }
          }
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
    if (IsMakingPath(m)) {
      AbandonPath(m);
    }
    if (OnHalt(interrupt)) {
      ReactiveDraw();
      ScrollMemoryViews();
      goto KeepGoing;
    }
    ReactiveDraw();
    ScrollOp(&pan.disassembly, GetDisIndex());
  }
  m->canhalt = false;
  TuiCleanup();
}

static void GetOpts(int argc, char *argv[]) {
  int opt;
  bool wantjit = false;
  bool wantunsafe = false;
  const char *logpath = 0;
  while ((opt = GetOpt(argc, argv, "hjmCvtrzRNsb:Hw:L:")) != -1) {
    switch (opt) {
      case 'j':
        wantjit = true;
        break;
      case 't':
        tuimode = false;
        break;
      case 'C':
        FLAG_noconnect = true;
        break;
      case 'm':
        wantunsafe = true;
        if (!CanHaveLinearMemory()) {
          fprintf(stderr,
                  "linearization not possible on this system"
                  " (word size is %d bits and page size is %ld)\n",
                  bsr(UINTPTR_MAX) + 1, sysconf(_SC_PAGESIZE));
          exit(1);
        }
        break;
      case 'R':
        react = false;
        break;
      case 'N':
        natural = true;
        break;
      case 'r':
        m->metal = true;
        SetMachineMode(m, XED_MODE_REAL);
        g_disisprog_disable = true;
        break;
      case 's':
        FLAG_statistics = true;
        break;
      case 'b':
        HandleBreakpointFlag(optarg_);
        break;
      case 'w':
        HandleWatchpointFlag(optarg_);
        break;
      case 'H':
        memset(&g_high, 0, sizeof(g_high));
        break;
      case 'v':
        ++verbose;
        break;
      case 'L':
        logpath = optarg_;
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
  LogInit(logpath);
  if (!wantjit) {
    DisableJit(&m->system->jit);
  }
  m->nolinear = !wantunsafe;
  m->system->nolinear = !wantunsafe;
}

static int OpenDevTty(void) {
  return open("/dev/tty", O_RDWR | O_NOCTTY);
}

static void AddPath_StartOp_Tui(P) {
  Jitter(m, rde, 0, 0, "qc", StartOp_Tui);
}

int VirtualMachine(int argc, char *argv[]) {
  struct Dll *e;
  if (!Commandv(argv[optind_], pathbuf, sizeof(pathbuf))) {
    fprintf(stderr, "%s: command not found: %s\n", argv[0], argv[optind_]);
    exit(127);
  }
  codepath = pathbuf;
  optind_++;
  do {
    action = 0;
    LoadProgram(m, codepath, argv + optind_ - 1, environ);
    if (m->system->codesize) {
      ophits = (unsigned long *)AllocateBig(
          m->system->codesize * sizeof(unsigned long), PROT_READ | PROT_WRITE,
          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    }
    ScrollMemoryViews();
    AddStdFd(&m->system->fds, 0);
    AddStdFd(&m->system->fds, 1);
    AddStdFd(&m->system->fds, 2);
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
      for (e = dll_first(m->system->fds.list); e;
           e = dll_next(m->system->fds.list, e)) {
        if (isatty(FD_CONTAINER(e)->fildes)) {
          FD_CONTAINER(e)->cb = &kFdCbPty;
        }
      }
    }
    do {
      if (!tuimode) {
        Exec();
      } else {
        Tui();
      }
    } while (!(action & (RESTART | EXIT)));
  } while (action & RESTART);
  if (m->system->elf.ehdr) {
    unassert(!Munmap(m->system->elf.ehdr, m->system->elf.size));
  }
  DisFree(dis);
  return exitcode;
}

void FreePanels(void) {
  int i, j;
  for (i = 0; i < ARRAYLEN(pan.p); ++i) {
    for (j = 0; j < pan.p[i].n; ++j) {
      free(pan.p[i].lines[j].p);
    }
    free(pan.p[i].lines);
  }
}

void TerminateSignal(struct Machine *m, int sig) {
  if (!react) {
    LOGF("terminating due to signal %s", DescribeSignal(sig));
    WriteErrorString("\r\033[K\033[J"
                     "terminating due to signal; see log\n");
    exit(128 + sig);
  }
}

static void OnSigSegv(int sig, siginfo_t *si, void *uc) {
  LOGF("OnSigSegv(%p)", si->si_addr);
  RestoreIp(g_machine);
  // TODO(jart): Fix address translation in non-linear mode.
  g_machine->faultaddr = ToGuest(si->si_addr);
  LOGF("SIGSEGV AT ADDRESS %" PRIx64 " (OR %p)\n\t%s", g_machine->faultaddr,
       si->si_addr, GetBacktrace(g_machine));
  if (!react) DeliverSignalToUser(g_machine, SIGSEGV_LINUX);
  siglongjmp(g_machine->onhalt, kMachineSegmentationFault);
}

int main(int argc, char *argv[]) {
  int rc;
  struct System *s;
  static struct sigaction sa;
  g_exitdontabort = true;
  SetupWeb();
  g_blink_path = argc > 0 ? argv[0] : 0;
  react = true;
  tuimode = true;
  WriteErrorInit();
  AddPath_StartOp_Hook = AddPath_StartOp_Tui;
  unassert((pty = NewPty()));
  unassert((s = NewSystem(XED_MODE_REAL)));
  unassert((m = NewMachine(s, 0)));
  SetMachineMode(m, XED_MODE_LONG);
  m->system->redraw = Redraw;
  m->system->onbinbase = OnBinbase;
  m->system->onlongbranch = OnLongBranch;
  speed = 1;
  SetXmmSize(2);
  SetXmmDisp(kXmmHex);
  GetOpts(argc, argv);
  sigfillset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = OnSigSys;
  unassert(!sigaction(SIGSYS, &sa, 0));
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = OnSigInt;
  unassert(!sigaction(SIGINT, &sa, 0));
  sa.sa_sigaction = OnSigWinch;
  unassert(!sigaction(SIGWINCH, &sa, 0));
  sa.sa_sigaction = OnSigAlrm;
  unassert(!sigaction(SIGALRM, &sa, 0));
#ifndef __SANITIZE_THREAD__
  sa.sa_sigaction = OnSigSegv;
  unassert(!sigaction(SIGSEGV, &sa, 0));
#endif
  m->system->blinksigs |= 1ull << (SIGINT_LINUX - 1) |   //
                          1ull << (SIGALRM_LINUX - 1) |  //
                          1ull << (SIGWINCH_LINUX - 1);  //
  if (optind_ == argc) PrintUsage(48, stderr);
  rc = VirtualMachine(argc, argv);
  FreeBig(ophits, m->system->codesize * sizeof(unsigned long));
  FreeMachine(m);
  ClearHistory();
  FreePanels();
  free(profsyms.p);
  if (FLAG_statistics) {
    PrintStats();
  }
  return rc;
}
