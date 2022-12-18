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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blink/address.h"
#include "blink/assert.h"
#include "blink/debug.h"
#include "blink/endian.h"
#include "blink/linux.h"
#include "blink/log.h"
#include "blink/path.h"
#include "blink/signal.h"

void RestoreIp(struct Machine *m) {
  if (m->oldip != -1) {
    m->ip = m->oldip;
    m->oldip = -1;
  }
}

void DeliverSignalToUser(struct Machine *m, int sig) {
  EnqueueSignal(m, sig);
  if (ConsumeSignal(m)) {
    TerminateSignal(m, sig);
  }
}

void HaltMachine(struct Machine *m, int code) {
  SIG_LOGF("HaltMachine(%d)", code);
  if (IsMakingPath(m)) {
    AbandonPath(m);
  }
  switch (code) {
    case kMachineDivideError:
    case kMachineFpuException:
    case kMachineSimdException:
      RestoreIp(m);
      DeliverSignalToUser(m, SIGFPE_LINUX);
      break;
    case kMachineHalt:
      // TODO(jart): We should do something for real mode.
    case kMachineDecodeError:
    case kMachineUndefinedInstruction:
      RestoreIp(m);
      DeliverSignalToUser(m, SIGILL_LINUX);
      break;
    case kMachineProtectionFault:
    case kMachineSegmentationFault:
      RestoreIp(m);
      DeliverSignalToUser(m, SIGSEGV_LINUX);
      break;
    case 1:
    case 3:
      DeliverSignalToUser(m, SIGTRAP_LINUX);
      break;
    default:
      if (code > 0) {
        break;
      } else {
        unassert(!"not possible");
      }
  }
  unassert(m->canhalt);
  longjmp(m->onhalt, code);
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
  LOGF("SEGMENTATION FAULT AT ADDRESS %" PRIx64 "\n\t%s", va, GetBacktrace(m));
  HaltMachine(m, kMachineSegmentationFault);
}

void OpUdImpl(struct Machine *m) {
  RestoreIp(m);
  LOGF("UNDEFINED INSTRUCTION\n\t%s", GetBacktrace(m));
  HaltMachine(m, kMachineUndefinedInstruction);
}

void OpUd(P) {
  OpUdImpl(m);
}

void OpHlt(P) {
  HaltMachine(m, kMachineHalt);
}
