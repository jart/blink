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
#include <string.h>

#include "blink/address.h"
#include "blink/assert.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/memory.h"
#include "blink/modrm.h"
#include "blink/path.h"
#include "blink/stats.h"

static void StartPath(struct Machine *m) {
  JIT_LOGF("%" PRIx64 " <path>", m->ip);
}

static void StartOp(P) {
  JIT_LOGF("%" PRIx64 "   <op>", m->ip);
  JIT_LOGF("%" PRIx64 "     %s", m->ip, DescribeOp(m));
  STATISTIC(++instructions_jitted);
  unassert(!m->path.jp);
  m->oldip = m->ip;
  m->ip += disp;
}

static void EndOp(struct Machine *m) {
  JIT_LOGF("%" PRIx64 "   </op>", m->ip);
  m->oldip = -1;
  if (m->stashaddr) {
    CommitStash(m);
  }
}

static void EndPath(struct Machine *m) {
  JIT_LOGF("%" PRIx64 "   %s", m->ip, DescribeOp(m));
  JIT_LOGF("%" PRIx64 " </path>", m->ip);
}

bool CreatePath(struct Machine *m) {
  i64 pc;
  bool res;
  unassert(!m->path.jp);
  if ((pc = GetPc(m))) {
    if ((m->path.jp = StartJit(&m->system->jit))) {
      JIT_LOGF("starting new path at %" PRIx64, pc);
      m->path.start = pc;
      m->path.elements = 0;
#if LOG_JIT
      AppendJitCall(m->path.jp, StartPath);
#endif
      res = true;
    } else {
      LOGF("jit failed: %s", strerror(errno));
      res = false;
    }
  } else {
    res = false;
  }
  return res;
}

void CommitPath(struct Machine *m, intptr_t splice) {
  unassert(m->path.jp);
#if LOG_JIT
  AppendJitCall(m->path.jp, EndPath);
#endif
  STATISTIC(path_longest_bytes =
                MAX(path_longest_bytes, m->path.jp->index - m->path.jp->start));
  STATISTIC(path_longest = MAX(path_longest, m->path.elements));
  STATISTIC(AVERAGE(path_average_elements, m->path.elements));
  STATISTIC(AVERAGE(path_average_bytes, m->path.jp->index - m->path.jp->start));
  if (SpliceJit(&m->system->jit, m->path.jp, (hook_t *)(m->fun + m->path.start),
                (intptr_t)JitlessDispatch, splice)) {
    STATISTIC(++path_count);
    JIT_LOGF("staged path to %" PRIx64, m->path.start);
  } else {
    STATISTIC(++path_ooms);
    JIT_LOGF("path starting at %" PRIx64 " ran out of space", m->path.start);
  }
  m->path.jp = 0;
}

void AbandonPath(struct Machine *m) {
  unassert(m->path.jp);
  STATISTIC(++path_abandoned);
  AbandonJit(&m->system->jit, m->path.jp);
  m->path.jp = 0;
}

void AddPath_StartOp(struct Machine *m, u64 rde) {
#if !LOG_JIT && defined(__x86_64__)
  _Static_assert(offsetof(struct Machine, ip) < 128, "");
  _Static_assert(offsetof(struct Machine, oldip) < 128, "");
  AppendJitMovReg(m->path.jp, kAmdDi, kAmdBx);
  i8 ip = offsetof(struct Machine, ip);
  i8 oldip = offsetof(struct Machine, oldip);
  i8 delta = Oplength(rde);
  u8 code[] = {
      0x48, 0x8b, 0107, (u8)ip,     // mov 8(%rdi),%rax
      0x48, 0x89, 0107, (u8)oldip,  // mov %rax,16(%rdi)
      0x48, 0x83, 0300, (u8)delta,  // add $delta,%rax
      0x48, 0x89, 0107, (u8)ip,     // mov %rax,8(%rdi)
  };
  AppendJit(m->path.jp, code, sizeof(code));
#else
  AppendJitSetArg(m->path.jp, kParamDisp, Oplength(rde));
  AppendJitMovReg(m->path.jp, 0, 19);
  AppendJitCall(m->path.jp, (void *)StartOp);
#endif
}

void AddPath_EndOp(struct Machine *m) {
#if !LOG_JIT && defined(__x86_64__)
  _Static_assert(offsetof(struct Machine, stashaddr) < 128, "");
  AppendJitMovReg(m->path.jp, kAmdDi, kAmdBx);
  i8 stashaddr = offsetof(struct Machine, stashaddr);
  u8 code2[] = {
      0x48, 0x83, 0177, (u8)stashaddr, 0x00,  // cmpq $0x0,0x18(%rdi)
      0x74, 0x05,                             // jnz +5
  };
  AppendJit(m->path.jp, code2, sizeof(code2));
  AppendJitCall(m->path.jp, (void *)CommitStash);
#elif defined(__aarch64__)
  AppendJitMovReg(m->path.jp, 0, 19);
  AppendJitCall(m->path.jp, (void *)EndOp);
#endif
}

bool AddPath(P) {
  unassert(m->path.jp);
  JIT_LOGF("adding [%s] from address %" PRIx64 " to path starting at %" PRIx64,
           DescribeOp(m), GetPc(m), m->path.start);
  AppendJitSetArg(m->path.jp, kParamRde, rde);
  AppendJitSetArg(m->path.jp, kParamDisp, disp);
  AppendJitSetArg(m->path.jp, kParamUimm0, uimm0);
  AppendJitCall(m->path.jp, (void *)GetOp(Mopcode(rde)));
  return true;
}
