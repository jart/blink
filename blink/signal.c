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
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/bitscan.h"
#include "blink/debug.h"
#include "blink/endian.h"
#include "blink/linux.h"
#include "blink/lock.h"
#include "blink/log.h"
#include "blink/macros.h"
#include "blink/signal.h"
#include "blink/syscall.h"
#include "blink/xlat.h"

void SigRestore(struct Machine *m) {
  union {
    struct fpstate_linux fp;
    struct ucontext_linux uc;
  } u;
  SIG_LOGF("restoring from signal");
  CopyFromUserRead(m, &u.uc, m->siguc, sizeof(u.uc));
  m->ip = Read64(u.uc.rip);
  m->flags = Read64(u.uc.eflags);
  m->sigmask = Read64(u.uc.uc_sigmask);
  memcpy(m->r8, u.uc.r8, 8);
  memcpy(m->r9, u.uc.r9, 8);
  memcpy(m->r10, u.uc.r10, 8);
  memcpy(m->r11, u.uc.r11, 8);
  memcpy(m->r12, u.uc.r12, 8);
  memcpy(m->r13, u.uc.r13, 8);
  memcpy(m->r14, u.uc.r14, 8);
  memcpy(m->r15, u.uc.r15, 8);
  memcpy(m->di, u.uc.rdi, 8);
  memcpy(m->si, u.uc.rsi, 8);
  memcpy(m->bp, u.uc.rbp, 8);
  memcpy(m->bx, u.uc.rbx, 8);
  memcpy(m->dx, u.uc.rdx, 8);
  memcpy(m->ax, u.uc.rax, 8);
  memcpy(m->cx, u.uc.rcx, 8);
  memcpy(m->sp, u.uc.rsp, 8);
  CopyFromUserRead(m, &u.fp, m->sigfp, sizeof(u.fp));
  m->fpu.cw = Read16(u.fp.cwd);
  m->fpu.sw = Read16(u.fp.swd);
  m->fpu.tw = Read16(u.fp.ftw);
  m->fpu.op = Read16(u.fp.fop);
  m->fpu.ip = Read64(u.fp.rip);
  m->fpu.dp = Read64(u.fp.rdp);
  memcpy(m->fpu.st, u.fp.st, 128);
  memcpy(m->xmm, u.fp.xmm, 256);
  m->sig = 0;
}

void DeliverSignal(struct Machine *m, int sig, int code) {
  u64 sp, siaddr;
  static struct siginfo_linux si;
  static struct fpstate_linux fp;
  static struct ucontext_linux uc;
  SIG_LOGF("delivering signal %s", DescribeSignal(sig));
  Write32(si.si_signo, sig);
  Write32(si.si_code, code);
  Write64(uc.uc_sigmask, m->sigmask);
  memcpy(uc.r8, m->r8, 8);
  memcpy(uc.r9, m->r9, 8);
  memcpy(uc.r10, m->r10, 8);
  memcpy(uc.r11, m->r11, 8);
  memcpy(uc.r12, m->r12, 8);
  memcpy(uc.r13, m->r13, 8);
  memcpy(uc.r14, m->r14, 8);
  memcpy(uc.r15, m->r15, 8);
  memcpy(uc.rdi, m->di, 8);
  memcpy(uc.rsi, m->si, 8);
  memcpy(uc.rbp, m->bp, 8);
  memcpy(uc.rbx, m->bx, 8);
  memcpy(uc.rdx, m->dx, 8);
  memcpy(uc.rax, m->ax, 8);
  memcpy(uc.rcx, m->cx, 8);
  memcpy(uc.rsp, m->sp, 8);
  Write64(uc.rip, m->ip);
  Write64(uc.eflags, m->flags);
  Write16(fp.cwd, m->fpu.cw);
  Write16(fp.swd, m->fpu.sw);
  Write16(fp.ftw, m->fpu.tw);
  Write16(fp.fop, m->fpu.op);
  Write64(fp.rip, m->fpu.ip);
  Write64(fp.rdp, m->fpu.dp);
  memcpy(fp.st, m->fpu.st, 128);
  memcpy(fp.xmm, m->xmm, 256);
  sp = Read64(m->sp);
  sp = ROUNDDOWN(sp - sizeof(si), 16);
  CopyToUserWrite(m, sp, &si, sizeof(si));
  siaddr = sp;
  sp = ROUNDDOWN(sp - sizeof(fp), 16);
  CopyToUserWrite(m, sp, &fp, sizeof(fp));
  m->sigfp = sp;
  Write64(uc.fpstate, sp);
  sp = ROUNDDOWN(sp - sizeof(uc), 16);
  CopyToUserWrite(m, sp, &uc, sizeof(uc));
  m->siguc = sp;
  m->sig = sig;
  sp -= 8;
  CopyToUserWrite(m, sp, m->system->hands[sig - 1].restorer, 8);
  Write64(m->sp, sp);
  Write64(m->di, sig);
  Write64(m->si, siaddr);
  Put64(m->dx, m->siguc);
  m->ip = Read64(m->system->hands[sig - 1].handler);
}

bool IsSignalIgnoredByDefault(int sig) {
  return sig == SIGURG_LINUX ||   //
         sig == SIGCONT_LINUX ||  //
         sig == SIGCHLD_LINUX ||  //
         sig == SIGWINCH_LINUX;
}

static int ConsumeSignalImpl(struct Machine *m) {
  int sig;
  i64 handler;
  u64 signals;
  // TODO(jart): We should have a stack of signal handlers so we can be
  //             smarter about avoiding re-entry than just using m->sig
  for (signals = m->signals; signals; signals &= ~(1ull << (sig - 1))) {
    sig = bsr(signals) + 1;
    // determine if signal should be deferred
    if (!(m->sigmask & (1ull << (sig - 1))) &&
        (!m->sig ||
         ((sig != m->sig ||
           (Read64(m->system->hands[m->sig - 1].flags) & SA_NODEFER_LINUX)) &&
          !(Read64(m->system->hands[m->sig - 1].mask) &
            (1ull << (sig - 1)))))) {
      // we're now handling the signal
      m->signals &= ~(1ull << (sig - 1));
      // determine how signal should be handled
      handler = Read64(m->system->hands[sig - 1].handler);
      if (handler == SIG_DFL_LINUX) {
        if (IsSignalIgnoredByDefault(sig)) {
          SIG_LOGF("default action is to ignore signal %s",
                   DescribeSignal(sig));
          return 0;
        } else {
          SIG_LOGF("default action is to terminate upon signal %s",
                   DescribeSignal(sig));
          return sig;
        }
      } else if (handler == SIG_IGN_LINUX) {
        SIG_LOGF("explicitly ignoring signal %s", DescribeSignal(sig));
        return 0;
      }
      DeliverSignal(m, sig, 0);
      return 0;
    } else if (sig == SIGFPE_LINUX ||   //
               sig == SIGILL_LINUX ||   //
               sig == SIGSEGV_LINUX ||  //
               sig == SIGTRAP_LINUX) {
      // signal is too dangerous to be deferred
      // TODO(jart): permit defer if sent by kill() or tkill()
      return sig;
    }
  }
  return 0;
}

int ConsumeSignal(struct Machine *m) {
  int rc;
  if (m->metal) return 0;
  LOCK(&m->system->sig_lock);
  rc = ConsumeSignalImpl(m);
  UNLOCK(&m->system->sig_lock);
  return rc;
}

void EnqueueSignal(struct Machine *m, int sig) {
  if (m && (1 <= sig && sig <= 64)) {
    m->signals |= 1ul << (sig - 1);
  }
}

_Noreturn void TerminateSignal(struct Machine *m, int sig) {
  int syssig;
  struct sigaction sa;
  LOGF("terminating due to signal %s", DescribeSignal(sig));
  if ((syssig = XlatSignal(sig)) == -1) syssig = SIGTERM;
  LOCK(&m->system->sig_lock);
  sa.sa_flags = 0;
  sa.sa_handler = SIG_DFL;
  sigemptyset(&sa.sa_mask);
  unassert(!sigaction(syssig, &sa, 0));
  unassert(!kill(getpid(), syssig));
  abort();
}
