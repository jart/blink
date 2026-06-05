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
#include "blink/signal.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/atomic.h"
#include "blink/bitscan.h"
#include "blink/endian.h"
#include "blink/ldbl.h"
#include "blink/linux.h"
#include "blink/log.h"
#include "blink/macros.h"
#include "blink/syscall.h"
#include "blink/thread.h"
#include "blink/util.h"
#include "blink/xlat.h"

struct SignalFrame {
  u8 ret[8];
  struct siginfo_linux si;
  struct ucontext_linux uc;
  struct fpstate_linux fp;
};

// FreeBSD signal frame layout (amd64). The signal handler is called with
// (sig, &siginfo, &ucontext). FreeBSD's libthr inspects the ucontext on
// handler entry and clobbers it on handler exit, so the layout MUST match
// FreeBSD's <sys/_ucontext.h>, <machine/ucontext.h>, and <sys/signal.h>.
// Linux-format frames here cause libthr to overwrite the saved RIP with
// garbage and crash via 0x7fff0000 → SigRestore → ip=0.
//
// mcontext_t offsets (amd64):
#define FBSD_MC_ONSTACK    0
#define FBSD_MC_RDI        8
#define FBSD_MC_RSI       16
#define FBSD_MC_RDX       24
#define FBSD_MC_RCX       32
#define FBSD_MC_R8        40
#define FBSD_MC_R9        48
#define FBSD_MC_RAX       56
#define FBSD_MC_RBX       64
#define FBSD_MC_RBP       72
#define FBSD_MC_R10       80
#define FBSD_MC_R11       88
#define FBSD_MC_R12       96
#define FBSD_MC_R13      104
#define FBSD_MC_R14      112
#define FBSD_MC_R15      120
#define FBSD_MC_TRAPNO   128  /* u32 */
#define FBSD_MC_FS       132  /* u16 */
#define FBSD_MC_GS       134  /* u16 */
#define FBSD_MC_ADDR     136
#define FBSD_MC_FLAGS    144  /* u32 */
#define FBSD_MC_ES       148  /* u16 */
#define FBSD_MC_DS       150  /* u16 */
#define FBSD_MC_ERR      152
#define FBSD_MC_RIP      160
#define FBSD_MC_CS       168
#define FBSD_MC_RFLAGS   176
#define FBSD_MC_RSP      184
#define FBSD_MC_SS       192
#define FBSD_MC_LEN      200
#define FBSD_MC_FPFORMAT 208
#define FBSD_MC_OWNEDFP  216
#define FBSD_MC_FPSTATE  224  /* 512 bytes, 16-byte aligned (FXSAVE area) */
#define FBSD_MC_FSBASE   736
#define FBSD_MC_GSBASE   744
#define FBSD_MCONTEXT_SIZE 800

// ucontext_t offsets:
#define FBSD_UC_SIGMASK    0   /* 16 bytes (4 x uint32) */
#define FBSD_UC_MCONTEXT  16
#define FBSD_UC_LINK     816
#define FBSD_UC_STACK    824   /* 24 bytes */
#define FBSD_UC_FLAGS    848
#define FBSD_UCONTEXT_SIZE 880

// siginfo_t offsets:
#define FBSD_SI_SIGNO    0
#define FBSD_SI_ERRNO    4
#define FBSD_SI_CODE     8
#define FBSD_SI_PID     12
#define FBSD_SI_UID     16
#define FBSD_SI_STATUS  20
#define FBSD_SI_ADDR    24
#define FBSD_SI_VALUE   32
#define FBSD_SI_TRAPNO  40   /* _reason._fault._trapno */
#define FBSD_SIGINFO_SIZE 80

#define FBSD_FRAME_SIZE (8 + FBSD_SIGINFO_SIZE + FBSD_UCONTEXT_SIZE)

#define FBSD_MC_FPFMT_XMM    0x10002
#define FBSD_MC_FPOWNED_FPU  0x20001
#define FBSD_MC_HASFPXSTATE  0x4

// Linux → FreeBSD signal number. Inverse of XlatFreeBSDSignal() in xlat.c.
static int LinuxToFreeBSDSignal(int x) {
  switch (x) {
    case SIGBUS_LINUX:    return 10;  // FreeBSD SIGBUS
    case SIGUSR1_LINUX:   return 30;  // FreeBSD SIGUSR1
    case SIGUSR2_LINUX:   return 31;  // FreeBSD SIGUSR2
    case SIGCHLD_LINUX:   return 20;  // FreeBSD SIGCHLD
    case SIGCONT_LINUX:   return 19;
    case SIGSTOP_LINUX:   return 17;
    case SIGTSTP_LINUX:   return 18;
    case SIGURG_LINUX:    return 16;
    case SIGIO_LINUX:     return 23;
    case SIGSYS_LINUX:    return 12;
    case SIGINFO_LINUX:   return 29;
    case SIGEMT_LINUX:    return 7;
    default:              return x;   // 1-6, 8-9, 11, 13-15, 21-22, 24-28
  }
}

static void DeliverFreebsdSignal(struct Machine *m, int sig, int code) {
  u8 buf[FBSD_FRAME_SIZE];
  u8 *si, *uc, *mc;
  u64 sp;
  int fbsd_sig;
  memset(buf, 0, sizeof(buf));
  fbsd_sig = LinuxToFreeBSDSignal(sig);
  si = buf + 8;
  uc = buf + 8 + FBSD_SIGINFO_SIZE;
  mc = uc + FBSD_UC_MCONTEXT;
  // siginfo
  Write32(si + FBSD_SI_SIGNO, fbsd_sig);
  Write32(si + FBSD_SI_CODE, code);
  if (sig == SIGILL_LINUX || sig == SIGFPE_LINUX || sig == SIGSEGV_LINUX ||
      sig == SIGBUS_LINUX || sig == SIGTRAP_LINUX) {
    Write64(si + FBSD_SI_ADDR, m->faultaddr);
    SYS_LOGF("delivering %s [fbsd %d] {.si_code = %d, .si_addr = %#" PRIx64 "}",
             DescribeSignal(sig), fbsd_sig, code, m->faultaddr);
  } else {
    SYS_LOGF("delivering %s [fbsd %d] {.si_code = %d}",
             DescribeSignal(sig), fbsd_sig, code);
  }
  // ucontext: sigmask is FreeBSD's 4x uint32. m->sigmask is 64 bits; stash it
  // in the low half so SigRestoreFreebsd can read it back. The high half is
  // zero, which matches libthr's expectation that signals 33-128 aren't set.
  Write64(uc + FBSD_UC_SIGMASK + 0, m->sigmask);
  // mcontext: integer registers (offsets must match FreeBSD __mcontext)
  memcpy(mc + FBSD_MC_RDI, m->di, 8);
  memcpy(mc + FBSD_MC_RSI, m->si, 8);
  memcpy(mc + FBSD_MC_RDX, m->dx, 8);
  memcpy(mc + FBSD_MC_RCX, m->cx, 8);
  memcpy(mc + FBSD_MC_R8,  m->r8, 8);
  memcpy(mc + FBSD_MC_R9,  m->r9, 8);
  memcpy(mc + FBSD_MC_RAX, m->ax, 8);
  memcpy(mc + FBSD_MC_RBX, m->bx, 8);
  memcpy(mc + FBSD_MC_RBP, m->bp, 8);
  memcpy(mc + FBSD_MC_R10, m->r10, 8);
  memcpy(mc + FBSD_MC_R11, m->r11, 8);
  memcpy(mc + FBSD_MC_R12, m->r12, 8);
  memcpy(mc + FBSD_MC_R13, m->r13, 8);
  memcpy(mc + FBSD_MC_R14, m->r14, 8);
  memcpy(mc + FBSD_MC_R15, m->r15, 8);
  if (sig == SIGTRAP_LINUX) {
    Write32(mc + FBSD_MC_TRAPNO, m->trapno);
  }
  Write64(mc + FBSD_MC_ADDR, m->faultaddr);
  Write32(mc + FBSD_MC_FLAGS, FBSD_MC_HASFPXSTATE);
  Write64(mc + FBSD_MC_RIP, m->ip);
  Write64(mc + FBSD_MC_RFLAGS, m->flags);
  memcpy(mc + FBSD_MC_RSP, m->sp, 8);
  Write64(mc + FBSD_MC_LEN, FBSD_MCONTEXT_SIZE);
  Write64(mc + FBSD_MC_FPFORMAT, FBSD_MC_FPFMT_XMM);
  Write64(mc + FBSD_MC_OWNEDFP, FBSD_MC_FPOWNED_FPU);
  // FXSAVE area: XMM regs live at offset 160 within the 512-byte area.
  // Most handlers don't read these but libthr's TLS save/restore touches
  // fxsave headers, so zero is fine for everything except the XMM block.
  memcpy(mc + FBSD_MC_FPSTATE + 160, m->xmm, 16 * 16);
  Write64(mc + FBSD_MC_FSBASE, m->fs.base);
  Write64(mc + FBSD_MC_GSBASE, m->gs.base);
  // update signal mask (same as Linux path)
  m->sigmask |= Read64(m->system->hands[sig - 1].mask);
  if (~Read64(m->system->hands[sig - 1].flags) & SA_NODEFER_LINUX) {
    m->sigmask |= (u64)1 << (sig - 1);
  }
  SIG_LOGF("sigmask deliver %" PRIx64, m->sigmask);
  if ((Read64(m->system->hands[sig - 1].flags) & SA_ONSTACK_LINUX) &&
      !(Read32(m->sigaltstack.flags) & SS_DISABLE_LINUX)) {
    sp = Read64(m->sigaltstack.sp) + Read64(m->sigaltstack.size);
    if (Read32(m->sigaltstack.flags) & SS_AUTODISARM_LINUX) {
      Write32(m->sigaltstack.flags,
              Read32(m->sigaltstack.flags) & ~SS_AUTODISARM_LINUX);
    }
  } else {
    sp = Read64(m->sp);
    sp -= kRedzoneSize;
  }
  sp = ROUNDDOWN(sp, 16);
  sp -= FBSD_FRAME_SIZE;
  // The trampoline address — handler returns here, ExecuteInstruction
  // intercepts and dispatches to SigRestore.
  Write64(buf, m->system->sigtramp);
  if (CopyToUserWrite(m, sp, buf, FBSD_FRAME_SIZE) == -1) {
    LOGF("stack overflow delivering signal");
    TerminateSignal(m, SIGSEGV_LINUX, m->segvcode);
    return;
  }
  Put64(m->sp, sp);
  Put64(m->di, fbsd_sig);                          // arg1: signo (FBSD #)
  Put64(m->si, sp + 8);                            // arg2: siginfo *
  Put64(m->dx, sp + 8 + FBSD_SIGINFO_SIZE);        // arg3: ucontext *
  SIG_LOGF("handler is %" PRIx64, Read64(m->system->hands[sig - 1].handler));
  m->ip = Read64(m->system->hands[sig - 1].handler);
}

static void SigRestoreFreebsd(struct Machine *m) {
  u8 ucbuf[FBSD_UCONTEXT_SIZE];
  u8 *uc, *mc;
  u64 uc_addr;
  // Two entry points hand off here:
  //  - sigtramp intercept (machine.c:2194): the handler did `ret` to the
  //    magic 0x7fff0000, so m->sp now points 8 bytes past the start of the
  //    SignalFrame we built in DeliverFreebsdSignal. The ucontext lives at
  //    sp + 80 (sigsize) within that frame.
  //  - sigreturn(ucp) syscall (FBSD 417 → Linux 0xF): libthr/libc passes the
  //    ucontext pointer explicitly in %rdi, possibly after modifying its
  //    contents. SP at this point is wherever libthr left it — usually deep
  //    inside its wrapper — so we MUST honor %rdi instead of guessing from
  //    SP, otherwise we read garbage and restore a bogus RIP.
  if (m->ip == m->system->sigtramp) {
    uc_addr = Read64(m->sp) + FBSD_SIGINFO_SIZE;  // sp + 80
  } else {
    uc_addr = Read64(m->di);
  }
  SYS_LOGF("fbsd_sigreturn(uc=%#" PRIx64 ")", uc_addr);
  if (CopyFromUserRead(m, ucbuf, uc_addr, FBSD_UCONTEXT_SIZE) == -1) {
    return;
  }
  uc = ucbuf;
  mc = uc + FBSD_UC_MCONTEXT;
  memcpy(m->di, mc + FBSD_MC_RDI, 8);
  memcpy(m->si, mc + FBSD_MC_RSI, 8);
  memcpy(m->dx, mc + FBSD_MC_RDX, 8);
  memcpy(m->cx, mc + FBSD_MC_RCX, 8);
  memcpy(m->r8,  mc + FBSD_MC_R8,  8);
  memcpy(m->r9,  mc + FBSD_MC_R9,  8);
  memcpy(m->ax, mc + FBSD_MC_RAX, 8);
  memcpy(m->bx, mc + FBSD_MC_RBX, 8);
  memcpy(m->bp, mc + FBSD_MC_RBP, 8);
  memcpy(m->r10, mc + FBSD_MC_R10, 8);
  memcpy(m->r11, mc + FBSD_MC_R11, 8);
  memcpy(m->r12, mc + FBSD_MC_R12, 8);
  memcpy(m->r13, mc + FBSD_MC_R13, 8);
  memcpy(m->r14, mc + FBSD_MC_R14, 8);
  memcpy(m->r15, mc + FBSD_MC_R15, 8);
  m->ip = Read64(mc + FBSD_MC_RIP);
  m->flags = Read64(mc + FBSD_MC_RFLAGS);
  memcpy(m->sp, mc + FBSD_MC_RSP, 8);
  m->sigmask = Read64(uc + FBSD_UC_SIGMASK);
  SIG_LOGF("sigmask restore %" PRIx64, m->sigmask);
  memcpy(m->xmm, mc + FBSD_MC_FPSTATE + 160, 16 * 16);
  m->restored = true;
  atomic_store_explicit(&m->attention, true, memory_order_release);
}

bool IsSignalIgnoredByDefault(int sig) {
  return sig == SIGURG_LINUX ||   //
         sig == SIGCONT_LINUX ||  //
         sig == SIGCHLD_LINUX ||  //
         sig == SIGWINCH_LINUX;
}

bool IsSignalSerious(int sig) {
  return sig == SIGFPE_LINUX ||   //
         sig == SIGILL_LINUX ||   //
         sig == SIGBUS_LINUX ||   //
         sig == SIGQUIT_LINUX ||  //
         sig == SIGTRAP_LINUX ||  //
         sig == SIGSEGV_LINUX ||  //
         sig == SIGSTOP_LINUX ||  //
         sig == SIGKILL_LINUX;
}

void DeliverSignal(struct Machine *m, int sig, int code) {
  u64 sp;
  struct SignalFrame sf;
  if (IsMakingPath(g_machine)) AbandonPath(g_machine);
  if (m->system->isfreebsd) {
    DeliverFreebsdSignal(m, sig, code);
    return;
  }
  memset(&sf, 0, sizeof(sf));
  // capture the current state of the machine
  Write32(sf.si.signo, sig);
  Write32(sf.si.code, code);
  if (sig == SIGILL_LINUX ||   //
      sig == SIGFPE_LINUX ||   //
      sig == SIGSEGV_LINUX ||  //
      sig == SIGBUS_LINUX ||   //
      sig == SIGTRAP_LINUX) {
    Write64(sf.si.addr, m->faultaddr);
    SYS_LOGF("delivering %s {.si_code = %d, .si_addr = %#" PRIx64 "}",
             DescribeSignal(sig), code, m->faultaddr);
  } else {
    SYS_LOGF("delivering %s, {.si_code = %d}", DescribeSignal(sig), code);
    if (sig == SIGTRAP_LINUX) {
      Write64(sf.uc.trapno, m->trapno);
    }
  }
  Write64(sf.uc.sigmask, m->sigmask);
  memcpy(sf.uc.r8, m->r8, 8);
  memcpy(sf.uc.r9, m->r9, 8);
  memcpy(sf.uc.r10, m->r10, 8);
  memcpy(sf.uc.r11, m->r11, 8);
  memcpy(sf.uc.r12, m->r12, 8);
  memcpy(sf.uc.r13, m->r13, 8);
  memcpy(sf.uc.r14, m->r14, 8);
  memcpy(sf.uc.r15, m->r15, 8);
  memcpy(sf.uc.rdi, m->di, 8);
  memcpy(sf.uc.rsi, m->si, 8);
  memcpy(sf.uc.rbp, m->bp, 8);
  memcpy(sf.uc.rbx, m->bx, 8);
  memcpy(sf.uc.rdx, m->dx, 8);
  memcpy(sf.uc.rax, m->ax, 8);
  memcpy(sf.uc.rcx, m->cx, 8);
  memcpy(sf.uc.rsp, m->sp, 8);
  Write64(sf.uc.rip, m->ip);
  Write64(sf.uc.eflags, m->flags);
  Write16(sf.fp.cwd, m->fpu.cw);
#ifndef DISABLE_X87
  Write16(sf.fp.swd, m->fpu.sw);
  Write16(sf.fp.ftw, m->fpu.tw);
  Write16(sf.fp.fop, m->fpu.op);
  Write64(sf.fp.rip, m->fpu.ip);
  Write64(sf.fp.rdp, m->fpu.dp);
  {
    int i;
    for (i = 0; i < 8; ++i) {
      SerializeLdbl(sf.fp.st[i], m->fpu.st[i]);
    }
  }
#endif
  memcpy(sf.fp.xmm, m->xmm, sizeof(sf.fp.xmm));
  // set the thread signal mask to the one specified by the signal
  // handler. by default, the signal being delivered will be added
  // within the mask unless the guest program specifies SA_NODEFER
  m->sigmask |= Read64(m->system->hands[sig - 1].mask);
  if (~Read64(m->system->hands[sig - 1].flags) & SA_NODEFER_LINUX) {
    m->sigmask |= (u64)1 << (sig - 1);
  }
  SIG_LOGF("sigmask deliver %" PRIx64, m->sigmask);
  // if the guest setup a sigaltstack() and the signal handler used
  // SA_ONSTACK then use that alternative stack for signal handling
  // otherwise use the current stack, and do not touch the red zone
  // because gcc assumes that it owns the 128 bytes underneath rsp.
  if ((Read64(m->system->hands[sig - 1].flags) & SA_ONSTACK_LINUX) &&
      !(Read32(m->sigaltstack.flags) & SS_DISABLE_LINUX)) {
    sp = Read64(m->sigaltstack.sp) + Read64(m->sigaltstack.size);
    if (Read32(m->sigaltstack.flags) & SS_AUTODISARM_LINUX) {
      Write32(m->sigaltstack.flags,
              Read32(m->sigaltstack.flags) & ~SS_AUTODISARM_LINUX);
    }
  } else {
    sp = Read64(m->sp);
    sp -= kRedzoneSize;
  }
  // put signal and machine state on the stack. the guest may change
  // these values to edit the program's non-signal handler cpu state
  _Static_assert(!(sizeof(struct siginfo_linux) & 15), "");
  _Static_assert(!(sizeof(struct fpstate_linux) & 15), "");
  _Static_assert(!(sizeof(struct ucontext_linux) & 15), "");
  _Static_assert((sizeof(struct SignalFrame) & 15) == 8, "");
  sp = ROUNDDOWN(sp, 16);
  sp -= sizeof(sf);
  unassert((sp & 15) == 8);
  SIG_LOGF("restorer is %" PRIx64, Read64(m->system->hands[sig - 1].restorer));
  memcpy(sf.ret, m->system->hands[sig - 1].restorer, 8);
  Write64(sf.uc.fpstate, sp + offsetof(struct SignalFrame, fp));
  SIG_LOGF("delivering signal @ %" PRIx64, sp);
  if (CopyToUserWrite(m, sp, &sf, sizeof(sf)) == -1) {
    LOGF("stack overflow delivering signal");
    TerminateSignal(m, SIGSEGV_LINUX, m->segvcode);
  }
  // finally, call the signal handler using the sigaction arguments
  Put64(m->sp, sp);
  Put64(m->di, sig);
  Put64(m->si, sp + offsetof(struct SignalFrame, si));
  Put64(m->dx, sp + offsetof(struct SignalFrame, uc));
  SIG_LOGF("handler is %" PRIx64, Read64(m->system->hands[sig - 1].handler));
  m->ip = Read64(m->system->hands[sig - 1].handler);
}

void SigRestore(struct Machine *m) {
  struct SignalFrame sf;
  if (m->system->isfreebsd) {
    SigRestoreFreebsd(m);
    return;
  }
  // when the guest returns from the signal handler, it'll call a
  // pointer to the sa_restorer trampoline which is assumed to be
  //
  //   __restore_rt:
  //     mov $15,%rax
  //     syscall
  //
  // which doesn't change SP, thus we can restore the SignalFrame
  // and load any change that the guest made to the machine state
  SYS_LOGF("rt_sigreturn(%#" PRIx64 ")", Read64(m->sp) - 8);
  unassert(!CopyFromUserRead(m, &sf, Read64(m->sp) - 8, sizeof(sf)));
  m->ip = Read64(sf.uc.rip);
  m->flags = Read64(sf.uc.eflags);
  m->sigmask = Read64(sf.uc.sigmask);
  SIG_LOGF("sigmask restore %" PRIx64, m->sigmask);
  memcpy(m->r8, sf.uc.r8, 8);
  memcpy(m->r9, sf.uc.r9, 8);
  memcpy(m->r10, sf.uc.r10, 8);
  memcpy(m->r11, sf.uc.r11, 8);
  memcpy(m->r12, sf.uc.r12, 8);
  memcpy(m->r13, sf.uc.r13, 8);
  memcpy(m->r14, sf.uc.r14, 8);
  memcpy(m->r15, sf.uc.r15, 8);
  memcpy(m->di, sf.uc.rdi, 8);
  memcpy(m->si, sf.uc.rsi, 8);
  memcpy(m->bp, sf.uc.rbp, 8);
  memcpy(m->bx, sf.uc.rbx, 8);
  memcpy(m->dx, sf.uc.rdx, 8);
  memcpy(m->ax, sf.uc.rax, 8);
  memcpy(m->cx, sf.uc.rcx, 8);
  m->fpu.cw = Read16(sf.fp.cwd);
  memcpy(m->sp, sf.uc.rsp, 8);
#ifndef DISABLE_X87
  m->fpu.sw = Read16(sf.fp.swd);
  m->fpu.tw = Read16(sf.fp.ftw);
  m->fpu.op = Read16(sf.fp.fop);
  m->fpu.ip = Read64(sf.fp.rip);
  m->fpu.dp = Read64(sf.fp.rdp);
  {
    int i;
    for (i = 0; i < 8; ++i) {
      m->fpu.st[i] = DeserializeLdbl(sf.fp.st[i]);
    }
  }
#endif
  memcpy(m->xmm, sf.fp.xmm, sizeof(sf.fp.xmm));
  m->restored = true;
  atomic_store_explicit(&m->attention, true, memory_order_release);
}

static int ConsumeSignalImpl(struct Machine *m, int *delivered, bool *restart) {
  int sig;
  i64 handler;
  u64 signals;
  if (delivered) *delivered = 0;
  if (restart) *restart = true;
  // look for a pending signal that isn't currently masked
  while ((signals = m->signals & ~m->sigmask)) {
    sig = bsr(signals) + 1;
    m->signals &= ~((u64)1 << (sig - 1));
    handler = Read64(m->system->hands[sig - 1].handler);
    if (handler == SIG_DFL_LINUX) {
      if (IsSignalIgnoredByDefault(sig)) {
        SYS_LOGF("ignoring %s", DescribeSignal(sig));
        return 0;
      } else {
        SIG_LOGF("default action is to terminate upon signal %s",
                 DescribeSignal(sig));
        return sig;
      }
    } else if (handler == SIG_IGN_LINUX) {
      SYS_LOGF("explicitly ignoring %s", DescribeSignal(sig));
      return 0;
    }
    if (delivered) {
      *delivered = sig;
    }
    if (restart) {
      *restart = !!(Read64(m->system->hands[sig - 1].flags) & SA_RESTART_LINUX);
    }
    DeliverSignal(m, sig, SI_KERNEL_LINUX);
    return 0;
  }
  return 0;
}

int ConsumeSignal(struct Machine *m, int *delivered, bool *restart) {
  int rc;
  if (m->metal) return 0;
  LOCK(&m->system->sig_lock);
  rc = ConsumeSignalImpl(m, delivered, restart);
  UNLOCK(&m->system->sig_lock);
  return rc;
}

void EnqueueSignal(struct Machine *m, int sig) {
  if (m && (1 <= sig && sig <= 64)) {
    m->signals |= 1ul << (sig - 1);
    if ((m->signals & ~m->sigmask)) {
      atomic_store_explicit(&m->attention, true, memory_order_release);
    }
  }
}

void CheckForSignals(struct Machine *m) {
  int sig;
  if (atomic_load_explicit(&m->killed, memory_order_acquire)) {
    SysExit(m, 0);
#ifndef DISABLE_JIT
  } else if (m->selfmodifying) {
    FlushSmcQueue(m);
    m->selfmodifying = false;
#endif
  } else if (m->signals & ~m->sigmask) {
    if ((sig = ConsumeSignal(m, 0, 0))) {
      TerminateSignal(m, sig, 0);
    }
  } else {
    atomic_store_explicit(&m->attention, false, memory_order_relaxed);
  }
}
