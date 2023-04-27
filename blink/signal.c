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
    if ((m->signals |= 1ul << (sig - 1)) & ~m->sigmask) {
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
