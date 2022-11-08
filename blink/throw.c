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
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blink/address.h"
#include "blink/endian.h"
#include "blink/log.h"
#include "blink/throw.h"

static bool IsHaltingInitialized(struct Machine *m) {
  jmp_buf zb;
  memset(zb, 0, sizeof(zb));
  return memcmp(m->onhalt, zb, sizeof(m->onhalt)) != 0;
}

void HaltMachine(struct Machine *m, int code) {
  if (!IsHaltingInitialized(m)) abort();
  longjmp(m->onhalt, code);
}

void ThrowDivideError(struct Machine *m) {
  HaltMachine(m, kMachineDivideError);
}

void ThrowSegmentationFault(struct Machine *m, int64_t va) {
  m->faultaddr = va;
  if (m->xedd) m->ip -= m->xedd->length;
  LOGF("SEGMENTATION FAULT ADDR %012" PRIx64 " IP %012" PRIx64 " AX %" PRIx64
       " CX %" PRIx64 " DX %" PRIx64 " BX %" PRIx64 " SP %" PRIx64 " "
       "BP %" PRIx64 " SI %" PRIx64 " DI %" PRIx64 " R8 %" PRIx64 " R9 %" PRIx64
       " R10 %" PRIx64 " R11 %" PRIx64 " R12 %" PRIx64 " "
       "R13 %" PRIx64 " R14 %" PRIx64 " R15 %" PRIx64,
       va, m->ip, Read64(m->ax), Read64(m->cx), Read64(m->dx), Read64(m->bx),
       Read64(m->sp), Read64(m->bp), Read64(m->si), Read64(m->di),
       Read64(m->r8), Read64(m->r9), Read64(m->r10), Read64(m->r11),
       Read64(m->r12), Read64(m->r13), Read64(m->r14), Read64(m->r15));
  HaltMachine(m, kMachineSegmentationFault);
}

void ThrowProtectionFault(struct Machine *m) {
  HaltMachine(m, kMachineProtectionFault);
}

void OpUd(struct Machine *m, uint32_t rde) {
  if (m->xedd) m->ip -= m->xedd->length;
  HaltMachine(m, kMachineUndefinedInstruction);
}

void OpHlt(struct Machine *m, uint32_t rde) {
  HaltMachine(m, kMachineHalt);
}
