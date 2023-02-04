/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2023 Justine Alexandra Roberts Tunney                              │
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
#include "blink/assert.h"
#include "blink/debug.h"
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/machine.h"
#include "blink/rde.h"
#include "blink/stats.h"

/**
 * @fileoverview Branch Micro-Op Fusion.
 */

bool FuseBranchTest(P) {
#ifdef HAVE_JIT
  i64 bdisp;
  u8 *p, jcc, jlen;
  if (RegLog2(rde) < 2) {
    LogCodOp(m, "can't fuse test: byte/word fuse unimplemented");
    return false;
  }
  if (4096 - (m->ip & 4095) < 6) {
    LogCodOp(m, "can't fuse test: too close to the edge");
    return false;
  }
  if (RexrReg(rde) != RexbRm(rde)) {
    LogCodOp(m, "can't fuse test: different operands unimplemented");
    return false;
  }
  if (!(p = GetAddress(m, m->ip))) {
    LogCodOp(m, "can't fuse test: null address");
    return false;
  }
  if ((p[0] & 0xf0) == 0x70) {  // Jcc Jbs
    jlen = 2;
    jcc = p[0] & 0x0f;
    bdisp = (i8)Read8(p + 1);
  } else if (p[0] == 0x0f && (p[1] & 0xf0) == 0x80) {  // Jcc Jvds
    jlen = 6;
    jcc = p[1] & 0x0f;
    bdisp = (i32)Read32(p + 2);
  } else {
    LogCodOp(m, "can't fuse test: not followed by jump");
    return false;
  }
#ifndef __x86_64__
  switch (jcc) {
    case 0x04:  // jz
      break;
    case 0x05:  // jnz
      break;
    default:
      LogCodOp(m, "can't fuse test: unsupported jump operation");
      return false;
  }
#endif
  if (GetNeededFlags(m, m->ip + jlen + bdisp, CF | ZF | SF | OF | AF | PF)) {
    LogCodOp(m, "can't fuse test: loop carries");
    return false;
  }
  if (GetNeededFlags(m, m->ip + jlen, CF | ZF | SF | OF | AF | PF)) {
    LogCodOp(m, "can't fuse test: loop exit carries");
    return false;
  }
#if LOG_CPU
  LogCpu(m);
#endif
  FlushCod(m->path.jb);
  WriteCod("/\tfusing branch test+jcc\n");
  BeginCod(m, m->ip);
#if LOG_JIX
  Jitter(A,
         "a1i"  // arg1 = ip
         "c"    // call function
         "q",   // arg0 = machine
         m->ip, FuseOp);
#endif
  Jitter(A,
         "a1i"  // arg1 = skew + jlen
         "m",   // call micro-op
         m->path.skew + jlen, AdvanceIp);
  m->path.skew = 0;
#ifdef __x86_64__
  Jitter(A, "A"    // res0 = GetReg(RexrReg)
            "q");  // arg0 = machine
  if (!Rexw(rde)) {
    AlignJit(m->path.jb, 8, 4);
  } else {
    AlignJit(m->path.jb, 8, 3);
    AppendJit(m->path.jb, (u8[]){0x48}, 1);  // rex.w
  }
  u8 code[] = {
      0x85, 0300 | kJitRes0 << 3 | kJitRes0,  // test %eax,%eax
      0x70 | jcc, 5,                          // jz/jnz +5
  };
#elif defined(__aarch64__)
  Jitter(A, "A"      // res0 = GetReg(RexrReg)
            "r0a1="  // arg1 = res0
            "q");    // arg0 = machine
  u32 code[] = {
      // b4000042 cbz  x2, #8
      // 34000042 cbz  w2, #8
      // b5000042 cbnz x2, #8
      // 35000042 cbnz w2, #8
      Rexw(rde) << 31 | 0x30000000 | jcc << 24 | (8 / 4) << 5 | kJitArg1,
  };
#else
#error "architecture not implemented"
#endif
  AppendJit(m->path.jb, code, sizeof(code));
  Connect(A, m->ip + jlen, false);
  Jitter(A,
         "a1i"  // arg1 = disp
         "m"    // call micro-op
         "q",   // arg0 = machine
         bdisp, AdvanceIp);
  AlignJit(m->path.jb, 8, 0);
  Connect(A, m->ip + jlen + bdisp, false);
  FinishPath(m);
  m->path.skip = 1;
  STATISTIC(++fused_branches);
  return true;
#else
  return false;
#endif
}

bool FuseBranchCmp(P, bool imm) {
#ifdef HAVE_JIT
  i64 bdisp;
  u8 *p, jcc, jlen;
  if (RegLog2(rde) < 2) {
    LogCodOp(m, "can't fuse cmp: byte/word fuse unimplemented");
    return false;
  }
  if (4096 - (m->ip & 4095) < 6) {
    LogCodOp(m, "can't fuse cmp: too close to the edge");
    return false;
  }
  if (!(p = GetAddress(m, m->ip))) {
    LogCodOp(m, "can't fuse cmp: null address");
    return false;
  }
  if ((p[0] & 0xf0) == 0x70) {  // Jcc Jbs
    jlen = 2;
    jcc = p[0] & 0x0f;
    bdisp = (i8)Read8(p + 1);
  } else if (p[0] == 0x0f && (p[1] & 0xf0) == 0x80) {  // Jcc Jvds
    jlen = 6;
    jcc = p[1] & 0x0f;
    bdisp = (i32)Read32(p + 2);
  } else {
    LogCodOp(m, "can't fuse cmp: not followed by jump");
    return false;
  }
#ifndef __x86_64__
  switch (jcc) {
    case 0x0:  // jo
      break;
    case 0x1:  // jno
      break;
    case 0x2:  // jb
      break;
    case 0x3:  // jae
      break;
    case 0x4:  // je
      break;
    case 0x5:  // jne
      break;
    case 0x6:  // jbe
      break;
    case 0x7:  // ja
      break;
    case 0xC:  // jl
      break;
    case 0xD:  // jge
      break;
    case 0xE:  // jle
      break;
    case 0xF:  // jg
      break;
    default:
      LogCodOp(m, "can't fuse cmp: unsupported jump operation");
      return false;
  }
#endif
  if (GetNeededFlags(m, m->ip + jlen + bdisp, CF | ZF | SF | OF | AF | PF)) {
    LogCodOp(m, "can't fuse cmp: loop carries");
    return false;
  }
  if (GetNeededFlags(m, m->ip + jlen, CF | ZF | SF | OF | AF | PF)) {
    LogCodOp(m, "can't fuse cmp: loop exit carries");
    return false;
  }
#if LOG_CPU
  LogCpu(m);
#endif
  FlushCod(m->path.jb);
  WriteCod("/\tfusing branch cmp+jcc\n");
  BeginCod(m, m->ip);
#if LOG_JIX
  Jitter(A,
         "a1i"  // arg1 = ip
         "c"    // call function
         "q",   // arg0 = machine
         m->ip, FuseOp);
#endif
  if (IsModrmRegister(rde)) {
    Jitter(A,
           "a1i"  // arg1 = skew + jlen
           "m",   // call micro-op
           m->path.skew + jlen, AdvanceIp);
  } else {
    Jitter(A,
           "a2i"  // arg2 = delta
           "a1i"  // arg1 = oplen
           "m",   // call micro-op
           m->path.skew + jlen, Oplength(rde) + jlen, SkewIp);
  }
  m->path.skew = 0;
  if (imm) {
    Jitter(A, "s1i", uimm0);
  } else {
    Jitter(A, "A"        // res0 = GetReg(RexrReg)
              "r0s1=");  // sav1 = res0
  }
#ifdef __x86_64__
  Jitter(A, "B"    // res0 = GetRegOrMem(RexbRm)
            "q");  // arg0 = machine
  AlignJit(m->path.jb, 8, 3);
  u8 code[] = {
      // cmp %r12,%rax
      (Rexw(rde) ? kAmdRexw : 0) | (kJitSav1 > 7 ? kAmdRexr : 0),
      0x39,
      0300 | (kJitSav1 & 7) << 3 | kJitRes0,
      // jz/jnz +5
      0x70 | jcc,
      5,
  };
#elif defined(__aarch64__)
  Jitter(A, "B"      // res0 = GetRegOrMem(RexbRm)
            "r0a1="  // arg1 = res0
            "q");    // arg0 = machine
  // 54000000 b.eq #0 equal
  // 54000001 b.ne #0 not equal
  // 54000002 b.cs #0 carry set
  // 54000003 b.cc #0 carry clear
  // 54000004 b.mi #0 less than
  // 54000005 b.pl #0 positive or zero
  // 54000006 b.vs #0 signed overflow
  // 54000007 b.vc #0 no signed overflow
  // 54000008 b.hi #0 greater than (unsigned)
  // 54000009 b.ls #0 less than or equal to (unsigned)
  // 5400000a b.ge #0 greater than or equal to (signed)
  // 5400000b b.lt #0 less than (signed)
  // 5400000c b.gt #0 greater than (signed)
  // 5400000d b.le #0 less than or equal to (signed)
  switch (jcc) {
    case 0x0:  // jo → b.vs
      jcc = 0x6;
      break;
    case 0x1:  // jno → b.vc
      jcc = 0x7;
      break;
    case 0x2:  // jb → b.cc
      jcc = 0x3;
      break;
    case 0x3:  // jae → b.cs
      jcc = 0x2;
      break;
    case 0x4:  // je → b.eq
      jcc = 0x0;
      break;
    case 0x5:  // jne → b.ne
      jcc = 0x1;
      break;
    case 0x6:  // jbe → b.ls
      jcc = 0x9;
      break;
    case 0x7:  // ja → b.hi
      jcc = 0x8;
      break;
    case 0xC:  // jl → b.lt
      jcc = 0xB;
      break;
    case 0xD:  // jge → b.ge
      jcc = 0xA;
      break;
    case 0xE:  // jle → b.le
      jcc = 0xD;
      break;
    case 0xF:  // jg → b.gt
      jcc = 0xC;
      break;
    default:
      __builtin_unreachable();
  }
  u32 code[] = {
      // 6b07007f cmp w3, w7
      // eb07007f cmp x3, x7
      Rexw(rde) << 31 | 0x6b00001f | kJitSav1 << 16 | kJitArg1 << 5,
      // 54000000 b.xx
      0x54000000 | (8 / 4) << 5 | jcc,
  };
#else
#error "architecture not implemented"
#endif
  AppendJit(m->path.jb, code, sizeof(code));
  Connect(A, m->ip + jlen, false);
  Jitter(A,
         "a1i"  // arg1 = disp
         "m"    // call micro-op
         "q",   // arg0 = machine
         bdisp, AdvanceIp);
  AlignJit(m->path.jb, 8, 0);
  Connect(A, m->ip + jlen + bdisp, false);
  FinishPath(m);
  m->path.skip = 1;
  STATISTIC(++fused_branches);
  return true;
#else
  return false;
#endif
}
