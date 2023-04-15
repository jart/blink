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
#include <inttypes.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blink/assert.h"
#include "blink/debug.h"
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/linux.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/signal.h"

void RestoreIp(struct Machine *m) {
  if (m) {
    m->ip -= m->oplen;
    m->oplen = 0;
  }
}

void DeliverSignalToUser(struct Machine *m, int sig, int code) {
  if (m->sigmask & ((u64)1 << (sig - 1))) {
    TerminateSignal(m, sig, code);
  }
  LOCK(&m->system->sig_lock);
  switch (Read64(m->system->hands[sig - 1].handler)) {
    case SIG_IGN_LINUX:
    case SIG_DFL_LINUX:
      UNLOCK(&m->system->sig_lock);
      TerminateSignal(m, sig, code);
      return;
    default:
      DeliverSignal(m, sig, code);
      break;
  }
  UNLOCK(&m->system->sig_lock);
  if ((sig = ConsumeSignal(m, 0, 0))) {
    TerminateSignal(m, sig, code);
  }
}

void HaltMachine(struct Machine *m, int code) {
  SIG_LOGF("HaltMachine(%d) at %#" PRIx64, code, m->ip);
  switch ((m->trapno = code)) {
    case kMachineDivideError:
      RestoreIp(m);
      m->faultaddr = m->ip;
      DeliverSignalToUser(m, SIGFPE_LINUX, FPE_INTDIV_LINUX);
      break;
    case kMachineFpuException:
    case kMachineSimdException:
      RestoreIp(m);
      m->faultaddr = m->ip;
      DeliverSignalToUser(m, SIGFPE_LINUX, FPE_FLTINV_LINUX);
      break;
    case kMachineHalt:
    case kMachineDecodeError:
    case kMachineUndefinedInstruction:
      RestoreIp(m);
      m->faultaddr = m->ip;
      DeliverSignalToUser(m, SIGILL_LINUX, ILL_ILLOPC_LINUX);
      break;
    case kMachineProtectionFault:
      RestoreIp(m);
      m->faultaddr = m->ip;
      DeliverSignalToUser(m, SIGILL_LINUX, ILL_PRVOPC_LINUX);
      break;
    case kMachineSegmentationFault:
      RestoreIp(m);
      DeliverSignalToUser(m, SIGSEGV_LINUX,
                          m->segvcode ? m->segvcode : SEGV_MAPERR_LINUX);
      break;
    case 1:
    case 3:
      m->faultaddr = m->ip - m->oplen;
      DeliverSignalToUser(m, SIGTRAP_LINUX, SI_KERNEL_LINUX);
      break;
    case 4:
      m->faultaddr = 0;
      DeliverSignalToUser(m, SIGSEGV_LINUX, SI_KERNEL_LINUX);
      break;
    case kMachineExitTrap:
      RestoreIp(m);
      break;
    default:
      if (code >= 0) {
        if (!m->metal) {
          RestoreIp(m);
          m->faultaddr = 0;
          DeliverSignalToUser(m, SIGSEGV_LINUX, SI_KERNEL_LINUX);
        }
      } else {
        unassert(!"not possible");
      }
  }
  unassert(m->canhalt);
  siglongjmp(m->onhalt, code);
}

void RaiseDivideError(struct Machine *m) {
  HaltMachine(m, kMachineDivideError);
}

void ThrowProtectionFault(struct Machine *m) {
  HaltMachine(m, kMachineProtectionFault);
}

void ThrowSegmentationFault(struct Machine *m, i64 va) {
  RestoreIp(m);
  m->faultaddr = va;
  HaltMachine(m, kMachineSegmentationFault);
}

void OpUdImpl(struct Machine *m) {
  RestoreIp(m);
  HaltMachine(m, kMachineUndefinedInstruction);
}

void OpUd(P) {
  OpUdImpl(m);
}

void OpHlt(P) {
  if (Cpl(m) == 0 && GetFlag(m->flags, FLAGS_IF)) {
    sched_yield();
  } else {
    HaltMachine(m, kMachineHalt);
  }
}
