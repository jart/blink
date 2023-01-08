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
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/builtin.h"
#include "blink/debug.h"
#include "blink/dis.h"
#include "blink/high.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/modrm.h"
#include "blink/stats.h"

#define APPEND(...) o += snprintf(b + o, n - o, __VA_ARGS__)

void (*AddPath_StartOp_Hook)(P);

#ifdef CLOG
static int g_clog;
static struct Dis g_dis;
#endif

static void StartPath(struct Machine *m) {
  JIX_LOGF("%" PRIx64 " <path>", m->ip);
}

static void DebugOp(struct Machine *m, i64 expected_ip) {
  if (m->ip != expected_ip) {
    LOGF("IP was %" PRIx64 " but it should have been %" PRIx64, m->ip,
         expected_ip);
  }
  unassert(m->ip == expected_ip);
}

static void StartOp(struct Machine *m) {
  JIX_LOGF("%" PRIx64 "   <op>", GetPc(m));
  JIX_LOGF("%" PRIx64 "     %s", GetPc(m), DescribeOp(m, GetPc(m)));
  unassert(!IsMakingPath(m));
}

static void EndOp(struct Machine *m) {
  JIX_LOGF("%" PRIx64 "   </op>", GetPc(m));
  m->oplen = 0;
  if (m->stashaddr) {
    CommitStash(m);
  }
}

static void EndPath(struct Machine *m) {
  JIX_LOGF("%" PRIx64 "   %s", GetPc(m), DescribeOp(m, GetPc(m)));
  JIX_LOGF("%" PRIx64 " </path>", GetPc(m));
}

#ifdef HAVE_JIT
#if defined(__x86_64__)
static const u8 kEnter[] = {
    0x55,                    // push %rbp
    0x48, 0x89, 0345,        // mov  %rsp,%rbp
    0x48, 0x83, 0354, 0x30,  // sub  $0x30,%rsp
    0x48, 0x89, 0135, 0xd8,  // mov  %rbx,-0x28(%rbp)
    0x4c, 0x89, 0145, 0xe0,  // mov  %r12,-0x20(%rbp)
    0x4c, 0x89, 0155, 0xe8,  // mov  %r13,-0x18(%rbp)
    0x4c, 0x89, 0165, 0xf0,  // mov  %r14,-0x10(%rbp)
    0x4c, 0x89, 0175, 0xf8,  // mov  %r15,-0x08(%rbp)
    0x48, 0x89, 0xfb,        // mov  %rdi,%rbx
};
static const u8 kLeave[] = {
    0x4c, 0x8b, 0175, 0xf8,  // mov -0x08(%rbp),%r15
    0x4c, 0x8b, 0165, 0xf0,  // mov -0x10(%rbp),%r14
    0x4c, 0x8b, 0155, 0xe8,  // mov -0x18(%rbp),%r13
    0x4c, 0x8b, 0145, 0xe0,  // mov -0x20(%rbp),%r12
    0x48, 0x8b, 0135, 0xd8,  // mov -0x28(%rbp),%rbx
    0x48, 0x83, 0304, 0x30,  // add $0x30,%rsp
    0x5d,                    // pop %rbp
};
#elif defined(__aarch64__)
static const u32 kEnter[] = {
    0xa9bc7bfd,  // stp x29, x30, [sp, #-64]!
    0x910003fd,  // mov x29, sp
    0xa90153f3,  // stp x19, x20, [sp, #16]
    0xa9025bf5,  // stp x21, x22, [sp, #32]
    0xa90363f7,  // stp x23, x24, [sp, #48]
    0xaa0003f3,  // mov x19, x0
};
static const u32 kLeave[] = {
    0xa94153f3,  // ldp x19, x20, [sp, #16]
    0xa9425bf5,  // ldp x21, x22, [sp, #32]
    0xa94363f7,  // ldp x23, x24, [sp, #48]
    0xa8c47bfd,  // ldp x29, x30, [sp], #64
};
#endif
#endif

long GetPrologueSize(void) {
#ifdef HAVE_JIT
  return sizeof(kEnter);
#else
  return 0;
#endif
}

void SetupClog(struct Machine *m) {
#ifdef CLOG
  LoadDebugSymbols(&m->system->elf);
  DisLoadElf(&g_dis, &m->system->elf);
  g_clog = open("/tmp/blink.s", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  g_clog = fcntl(g_clog, F_DUPFD_CLOEXEC, kMinBlinkFd);
#endif
}

static void WriteClog(const char *fmt, ...) {
#ifdef CLOG
  int n;
  va_list va;
  char buf[256];
  if (!g_clog) return;
  va_start(va, fmt);
  n = vsnprintf(buf, sizeof(buf), fmt, va);
  va_end(va);
  write(g_clog, buf, MIN(n, sizeof(buf)));
#endif
}

static void BeginClog(struct Machine *m, i64 pc) {
#ifdef CLOG
  char b[256];
  char spec[64];
  int i, o = 0, n = sizeof(b);
  if (!g_clog) return;
  DISABLE_HIGHLIGHT_BEGIN;
  APPEND("/\t");
  unassert(!GetInstruction(m, pc, g_dis.xedd));
  DisInst(&g_dis, b + o, DisSpec(g_dis.xedd, spec));
  o = strlen(b);
  APPEND(" #");
  for (i = 0; i < g_dis.xedd->length; ++i) {
    APPEND(" %02x", g_dis.xedd->bytes[i]);
  }
  APPEND("\n");
  write(g_clog, b, MIN(o, n));
  DISABLE_HIGHLIGHT_END;
#endif
}

static void FlushClog(struct JitBlock *jb) {
#ifdef CLOG
  char b[256];
  char spec[64];
  if (!g_clog) return;
  if (jb->index == jb->blocksize + 1) {
    WriteClog("/\tOOM!\n");
    jb->clog = jb->index;
    return;
  }
  DISABLE_HIGHLIGHT_BEGIN;
  for (; jb->clog < jb->index; jb->clog += g_dis.xedd->length) {
    unassert(!DecodeInstruction(g_dis.xedd, jb->addr + jb->clog,
                                jb->index - jb->clog, XED_MODE_LONG));
    g_dis.addr = (intptr_t)jb->addr + jb->clog;
    DisInst(&g_dis, b, DisSpec(g_dis.xedd, spec));
    WriteClog("\t%s\n", b);
  }
  DISABLE_HIGHLIGHT_END;
#endif
}

static void InitPaths(struct System *s) {
#ifdef HAVE_JIT
  struct JitBlock *jb;
  if (!s->ender) {
    unassert((jb = StartJit(&s->jit)));
    WriteClog("\nJit_%" PRIx64 ":\n", jb->addr + jb->index);
    s->ender = GetJitPc(jb);
#if LOG_JIX
    AppendJitMovReg(jb, kJitArg0, kJitSav0);
    AppendJitCall(jb, (void *)EndPath);
#endif
    AppendJit(jb, kLeave, sizeof(kLeave));
    AppendJitRet(jb);
    FlushClog(jb);
    unassert(FinishJit(&s->jit, jb, 0));
  }
#endif
}

bool CreatePath(P) {
#ifdef HAVE_JIT
  bool res;
  i64 pc, jpc;
  unassert(!IsMakingPath(m));
  InitPaths(m->system);
  if ((pc = GetPc(m))) {
    if ((m->path.jb = StartJit(&m->system->jit))) {
      JIT_LOGF("starting new path jit_pc:%" PRIxPTR " at pc:%" PRIx64,
               GetJitPc(m->path.jb), pc);
      FlushClog(m->path.jb);
      jpc = (intptr_t)m->path.jb->addr + m->path.jb->index;
      AppendJit(m->path.jb, kEnter, sizeof(kEnter));
#if LOG_JIX
      Jitter(A,
             "q"   // arg0 = machine
             "c"   // call function (StartPath)
             "q",  // arg0 = machine
             StartPath);
#endif
      WriteClog("\nJit_%" PRIx64 "_%" PRIx64 ":\n", pc, jpc);
      FlushClog(m->path.jb);
      m->path.start = pc;
      m->path.elements = 0;
      SetHook(m, pc, JitlessDispatch);
      res = true;
    } else {
      LOGF("jit failed: %s", strerror(errno));
      res = false;
    }
  } else {
    res = false;
  }
  return res;
#else
  return false;
#endif
}

void CompletePath(P) {
  unassert(IsMakingPath(m));
  AppendJitJump(m->path.jb, (void *)m->system->ender);
  FinishPath(m);
}

void FinishPath(struct Machine *m) {
  unassert(IsMakingPath(m));
  FlushClog(m->path.jb);
  STATISTIC(path_longest_bytes =
                MAX(path_longest_bytes, m->path.jb->index - m->path.jb->start));
  STATISTIC(path_longest = MAX(path_longest, m->path.elements));
  STATISTIC(AVERAGE(path_average_elements, m->path.elements));
  STATISTIC(AVERAGE(path_average_bytes, m->path.jb->index - m->path.jb->start));
  if (FinishJit(&m->system->jit, m->path.jb, m->fun + m->path.start)) {
    STATISTIC(++path_count);
    JIT_LOGF("staged path to %" PRIx64, m->path.start);
  } else {
    STATISTIC(++path_ooms);
    JIT_LOGF("path starting at %" PRIx64 " ran out of space", m->path.start);
    SetHook(m, m->path.start, 0);
  }
  m->path.jb = 0;
}

void AbandonPath(struct Machine *m) {
  WriteClog("/\tABANDONED\n");
  unassert(IsMakingPath(m));
  STATISTIC(++path_abandoned);
  JIT_LOGF("abandoning path jit_pc:%" PRIxPTR " which started at pc:%" PRIx64,
           GetJitPc(m->path.jb), m->path.start);
  AbandonJit(&m->system->jit, m->path.jb);
  SetHook(m, m->path.start, 0);
  m->path.jb = 0;
}

void AddPath_StartOp(P) {
  BeginClog(m, GetPc(m));
#ifndef NDEBUG
  if (FLAG_statistics) {
    Jitter(A,
           "a0i"  // arg0 = &instructions_jitted
           "m",   // call micro-op (CountOp)
           &instructions_jitted, CountOp);
  }
#endif
  if (AddPath_StartOp_Hook) {
    AddPath_StartOp_Hook(A);
  }
#if LOG_JIX || defined(DEBUG)
  Jitter(A,
         "a1i"  // arg1 = m->ip
         "q"    // arg0 = machine
         "c",   // call function (DebugOp)
         m->ip, DebugOp);
#endif
#if LOG_JIX
  Jitter(A,
         "a1i"  // arg1 = Oplength(rde)
         "q"    // arg0 = machine
         "c",   // call function (StartOp)
         Oplength(rde), StartOp);
#endif
  Jitter(A,
         "a1i"  // arg1 = Oplength(rde)
         "q"    // arg0 = machine
         "m"    // call micro-op (AddIp)
         "q",   // arg0 = machine
         Oplength(rde), AddIp);
  m->reserving = false;
}

void AddPath_EndOp(P) {
  _Static_assert(offsetof(struct Machine, stashaddr) < 128, "");
#if !LOG_JIX && defined(__x86_64__)
  if (m->reserving) {
    AppendJitMovReg(m->path.jb, kJitArg0, kJitSav0);
    u8 sa = offsetof(struct Machine, stashaddr);
    u8 code[] = {
        0x48, 0x83, 0177, sa, 0x00,  // cmpq $0x0,0x18(%rdi)
        0x74, 0x05,                  // jz +5
    };
    AppendJit(m->path.jb, code, sizeof(code));
    AppendJitCall(m->path.jb, (void *)CommitStash);
  }
#elif !LOG_JIX && defined(__aarch64__)
  if (m->reserving) {
    AppendJitMovReg(m->path.jb, kJitArg0, kJitSav0);
    u32 sa = offsetof(struct Machine, stashaddr);
    u32 code[] = {
        0xf9400001 | (sa / 8) << 10,  // ldr x1, [x0, #stashaddr]
        0xb4000001 | 2 << 5,          // cbz x1, +2
    };
    AppendJit(m->path.jb, code, sizeof(code));
    AppendJitCall(m->path.jb, (void *)CommitStash);
  }
#else
  Jitter(A,
         "q"   // arg0 = machine
         "c",  // call function (EndOp)
         EndOp);
#endif
  FlushClog(m->path.jb);
}

bool AddPath(P) {
  unassert(IsMakingPath(m));
  AppendJitSetReg(m->path.jb, kJitArg[kArgRde], rde);
  AppendJitSetReg(m->path.jb, kJitArg[kArgDisp], disp);
  AppendJitSetReg(m->path.jb, kJitArg[kArgUimm0], uimm0);
  AppendJitCall(m->path.jb, (void *)GetOp(Mopcode(rde)));
  return true;
}
