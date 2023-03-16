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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blink/alu.h"
#include "blink/assert.h"
#include "blink/atomic.h"
#include "blink/bitscan.h"
#include "blink/builtin.h"
#include "blink/bus.h"
#include "blink/case.h"
#include "blink/debug.h"
#include "blink/endian.h"
#include "blink/flag.h"
#include "blink/flags.h"
#include "blink/fpu.h"
#include "blink/jit.h"
#include "blink/likely.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/map.h"
#include "blink/random.h"
#include "blink/signal.h"
#include "blink/sse.h"
#include "blink/stats.h"
#include "blink/string.h"
#include "blink/swap.h"
#include "blink/syscall.h"
#include "blink/thread.h"
#include "blink/time.h"
#include "blink/util.h"
#include "blink/x86.h"
#include "blink/xlat.h"

_Thread_local siginfo_t g_siginfo;
_Thread_local struct Machine *g_machine;

static void OpHintNopEv(P) {
}

static void OpCmc(P) {
  m->flags ^= CF;
}

static void OpClc(P) {
  m->flags = SetFlag(m->flags, FLAGS_CF, false);
}

static void OpStc(P) {
  m->flags = SetFlag(m->flags, FLAGS_CF, true);
}

static void OpCli(P) {
  m->flags = SetFlag(m->flags, FLAGS_IF, false);
}

static void OpSti(P) {
  m->flags = SetFlag(m->flags, FLAGS_IF, true);
}

static void OpCld(P) {
  m->flags = SetFlag(m->flags, FLAGS_DF, false);
}

static void OpStd(P) {
  m->flags = SetFlag(m->flags, FLAGS_DF, true);
}

static void OpPushf(P) {
  Push(A, ExportFlags(m->flags) & 0xFCFFFF);
}

static void OpPopf(P) {
  if (!Osz(rde)) {
    ImportFlags(m, Pop(A, 0));
  } else {
    ImportFlags(m, (m->flags & ~0xffff) | Pop(A, 0));
  }
}

static void OpLahf(P) {
  m->ah = ExportFlags(m->flags);
}

static void OpSahf(P) {
  ImportFlags(m, (m->flags & ~0xff) | m->ah);
}

static void OpLeaGvqpM(P) {
  WriteRegister(rde, RegRexrReg(m, rde), LoadEffectiveAddress(A).addr);
  if (IsMakingPath(m)) {
    Jitter(A, "L"      // res0 = LoadEffectiveAddress()
              "r0C");  // PutReg(RexrReg, res0)
  }
}

static void OpMovEvqpGvqp(P) {
  WriteRegisterOrMemory(rde, GetModrmRegisterWordPointerWriteOszRexw(A),
                        ReadRegister(rde, RegRexrReg(m, rde)));
  if (IsMakingPath(m)) {
    Jitter(A, "A"      // res0 = GetReg(RexrReg)
              "r0D");  // PutRegOrMem(RexbRm, res0)
  }
}

static void OpMovGvqpEvqp(P) {
  WriteRegister(rde, RegRexrReg(m, rde),
                ReadMemory(rde, GetModrmRegisterWordPointerReadOszRexw(A)));
  if (IsMakingPath(m)) {
    Jitter(A, "B"      // res0 = GetRegOrMem(RexbRm)
              "r0C");  // PutReg(RexrReg, res0)
  }
}

static void OpMovzbGvqpEb(P) {
  WriteRegister(rde, RegRexrReg(m, rde),
                Load8(GetModrmRegisterBytePointerRead1(A)));
  if (IsMakingPath(m)) {
    Jitter(A, "B"       // res0 = GetRegOrMem(RexbRm)
              "r0wC");  // PutReg[force16/32/64bit](RexrReg, res0)
  }
}

static void OpMovzwGvqpEw(P) {
  WriteRegister(rde, RegRexrReg(m, rde),
                Load16(GetModrmRegisterWordPointerRead2(A)));
  if (IsMakingPath(m)) {
    Jitter(A, "z1B"    // res0 = GetRegOrMem[force16bit](RexbRm)
              "r0C");  // PutReg(RexrReg, res0)
  }
}

static void OpMovsbGvqpEb(P) {
  WriteRegister(rde, RegRexrReg(m, rde),
                (i8)Load8(GetModrmRegisterBytePointerRead1(A)));
  if (IsMakingPath(m)) {
    Jitter(A, "B"       // res0 = GetRegOrMem(RexbRm)
              "r0x"     // res0 = SignExtend(res0)
              "r0wC");  // PutReg[force16/32/64bit](RexrReg, res0)
  }
}

static void OpMovswGvqpEw(P) {
  WriteRegister(rde, RegRexrReg(m, rde),
                (i16)Load16(GetModrmRegisterWordPointerRead2(A)));
  if (IsMakingPath(m)) {
    Jitter(A, "z1B"    // res0 = GetRegOrMem[force16bit](RexbRm)
              "r0z1x"  // res0 = SignExtend[force16bit](res0)
              "r0C");  // PutReg(RexrReg, res0)
  }
}

static void OpMovslGdqpEd(P) {
  WriteRegister(rde, RegRexrReg(m, rde),
                (i32)Load32(GetModrmRegisterWordPointerRead4(A)));
  if (IsMakingPath(m)) {
    Jitter(A, "z2B"    // res0 = GetRegOrMem[force32bit](RexbRm)
              "r0z2x"  // res0 = SignExtend[force32bit](res0)
              "r0C");  // PutReg(RexrReg, res0)
  }
}

static relegated u64 GetDescriptorLimit(u64 d) {
  u64 lim = (d & 0x000f000000000000) >> 32 | (d & 0xffff);
  if ((d & 0x0080000000000000) != 0) lim = (lim << 12) | 0xfff;
  return lim;
}

relegated int GetDescriptor(struct Machine *m, int selector,
                            u64 *out_descriptor) {
  u64 base = m->system->gdt_base, daddr;
  selector &= -8;
  if (8 <= selector && selector + 7 <= m->system->gdt_limit) {
    daddr = base + selector;
    if (daddr >= kRealSize || daddr + 8 > kRealSize) return -1;
    SetReadAddr(m, daddr, 8);
    *out_descriptor = Load64(m->system->real + daddr);
    return 0;
  } else {
    return -1;
  }
}

static relegated void OpLsl(P) {
  u64 descriptor;
  if (GetDescriptor(m, Load16(GetModrmRegisterWordPointerRead2(A)),
                    &descriptor) != -1) {
    WriteRegister(rde, RegRexrReg(m, rde), GetDescriptorLimit(descriptor));
    m->flags = SetFlag(m->flags, FLAGS_ZF, true);
  } else {
    m->flags = SetFlag(m->flags, FLAGS_ZF, false);
  }
}

void SetMachineMode(struct Machine *m, int mode) {
  m->mode = mode;
  m->system->mode = mode;
}

static relegated void OpXlatAlBbb(P) {
  i64 v;
  v = MaskAddress(Eamode(rde), Get64(m->bx) + Get8(m->ax));
  v = DataSegment(A, v);
  SetReadAddr(m, v, 1);
  m->al = Load8(ResolveAddress(m, v));
}

static void OpXchgZvqp(P) {
  u64 x, y;
  x = Get64(m->ax);
  y = Get64(RegRexbSrm(m, rde));
  WriteRegister(rde, m->ax, y);
  WriteRegister(rde, RegRexbSrm(m, rde), x);
}

static void Op1c7(P) {
  bool ismem;
  ismem = !IsModrmRegister(rde);
  switch (ModrmReg(rde)) {
    case 6:
      if (!ismem) {
        OpRdrand(A);
      } else {
        OpUdImpl(m);
      }
      break;
    case 7:
      if (!ismem) {
        if (Rep(rde) == 3) {
          OpRdpid(A);
        } else {
          OpRdseed(A);
        }
      } else {
        OpUdImpl(m);
      }
      break;
    default:
      OpUdImpl(m);
  }
}

static void TripleOp(P, const nexgen32e_f ops[3]) {
  nexgen32e_f op;
  op = ops[WordLog2(rde) - 1];
  op(A);
  if (IsMakingPath(m)) {
    Jitter(A, "m", op);  // call micro-op
  }
}

static void OpSax(P) {
  TripleOp(A, kSax);
}

static void OpConvert(P) {
  TripleOp(A, kConvert);
}

static void OpBswapZvqp(P) {
  u64 x = Get64(RegRexbSrm(m, rde));
  if (Rexw(rde)) {
    Put64(RegRexbSrm(m, rde), SWAP64(x));
  } else if (!Osz(rde)) {
    Put64(RegRexbSrm(m, rde), SWAP32(x));
  } else {
    Put16(RegRexbSrm(m, rde), SWAP16(x));
  }
}

static void OpMovAlOb(P) {
  i64 addr = AddressOb(A);
  SetWriteAddr(m, addr, 1);
  Put8(m->ax, Load8(ResolveAddress(m, addr)));
}

static void OpMovObAl(P) {
  i64 addr = AddressOb(A);
  SetReadAddr(m, addr, 1);
  Store8(ResolveAddress(m, addr), Get8(m->ax));
}

static void OpMovRaxOvqp(P) {
  i64 v = DataSegment(A, disp);
  SetReadAddr(m, v, 1 << RegLog2(rde));
  WriteRegister(rde, m->ax, ReadMemory(rde, ResolveAddress(m, v)));
}

static void OpMovOvqpRax(P) {
  i64 v = DataSegment(A, disp);
  SetWriteAddr(m, v, 1 << RegLog2(rde));
  WriteMemory(rde, ResolveAddress(m, v), Get64(m->ax));
}

static void OpMovEbGb(P) {
  Store8(GetModrmRegisterBytePointerWrite1(A), Get8(ByteRexrReg(m, rde)));
  if (IsMakingPath(m)) {
    Jitter(A, "A"      // res0 = GetReg(RexrReg)
              "r0D");  // PutRegOrMem(RexbRm, res0)
  }
}

static void OpMovGbEb(P) {
  Put8(ByteRexrReg(m, rde), Load8(GetModrmRegisterBytePointerRead1(A)));
  unassert(!RegLog2(rde));
  if (IsMakingPath(m)) {
    Jitter(A, "B"      // res0 = GetRegOrMem(RexbRm)
              "r0C");  // PutReg(RexrReg, res0)
  }
}

static void OpMovZbIb(P) {
  Put8(ByteRexbSrm(m, rde), uimm0);
  if (IsMakingPath(m)) {
    Jitter(A,
           "a2"    // push arg2
           "i"     // <pop> = uimm0
           "u"     // unpop
           "z0F",  // PutReg[force8bit](RexbSrm, arg2)
           uimm0);
  }
}

static void OpMovZvqpIvqp(P) {
  WriteRegister(rde, RegRexbSrm(m, rde), uimm0);
  if (IsMakingPath(m)) {
    if (!Rexw(rde) && !Osz(rde)) {
      unassert(uimm0 == (u32)uimm0);
      Jitter(A,
             "a0"    // push arg2
             "i"     // <pop> = uimm0
             "u"     // unpop
             "z3F",  // PutReg[kludge64bit](RexbSrm, arg2)
             uimm0);
    } else {
      Jitter(A,
             "a0"   // push arg2
             "i"    // <pop> = uimm0
             "u"    // unpop
             "wF",  // PutReg[force16+bit](RexbSrm, arg2)
             uimm0);
    }
  }
}

static void OpMovImm(P) {
  WriteRegisterOrMemoryBW(rde, GetModrmWriteBW(A), uimm0);
  if (IsMakingPath(m)) {
    Jitter(A,
           "a3"  // push arg3
           "i"   // <pop> = uimm0
           "u"   // unpop
           "D",  // PutRegOrMem(RexbRm, arg3)
           uimm0);
  }
}

// we want to have independent jit paths jump directly into one another
// to avoid having control flow drop back to the main interpreter loop.
void Connect(P, u64 pc, bool avoid_cycles) {
#ifdef HAVE_JIT
  void *jump;
  uintptr_t f;
  STATISTIC(++path_connected_total);
  // 1. cyclic paths can block asynchronous sigs & deadlock exit
  // 2. we don't want to stitch together paths on separate pages
  if ((!avoid_cycles && m->path.start == pc) ||
      RecordJitEdge(&m->system->jit, m->path.start, pc)) {
    // is a preexisting jit path installed at destination?
    if ((f = GetJitHook(&m->system->jit, pc)) &&
        f != (uintptr_t)JitlessDispatch) {
      // tail call into the other generated jit path function
      jump = (u8 *)f + GetPrologueSize();
      STATISTIC(++path_connected_directly);
    } else {
      STATISTIC(++path_connected_lazily);
      // generate assembly to drop back into main interpreter
      // then apply an smc fixup later on, if dest is created
      if (!FLAG_noconnect) {
        RecordJitJump(m->path.jb, pc, GetPrologueSize());
      }
      jump = (void *)m->system->ender;
    }
  } else {
    // generate assembly to drop back into main interpreter
    STATISTIC(++path_connected_interpreter);
    jump = (void *)m->system->ender;
  }
  AppendJitJump(m->path.jb, jump);
#endif
}

static void AluRo(P, const aluop_f ops[4], const aluop_f fops[4]) {
  ops[RegLog2(rde)](m, ReadRegisterOrMemoryBW(rde, GetModrmReadBW(A)),
                    ReadRegisterBW(rde, RegLog2(rde) ? RegRexrReg(m, rde)
                                                     : ByteRexrReg(m, rde)));
  if (IsMakingPath(m)) {
    STATISTIC(++alu_ops);
    LoadAluArgs(A);
    switch (GetNeededFlags(m, m->ip, CF | ZF | SF | OF | AF | PF)) {
      case 0:
      case CF:
      case ZF:
      case CF | ZF:
        STATISTIC(++alu_simplified);
        Jitter(A,
               "q"   // arg0 = sav0 (machine)
               "m",  // call micro-op
               fops[RegLog2(rde)]);
        break;
      default:
        Jitter(A,
               "q"   // arg0 = sav0 (machine)
               "c",  // call function
               ops[RegLog2(rde)]);
        break;
    }
  }
}

static void OpAluTest(P) {
  if (IsMakingPath(m) && FuseBranchTest(A)) {
    kAlu[ALU_AND][RegLog2(rde)](
        m, ReadRegisterOrMemoryBW(rde, GetModrmReadBW(A)),
        ReadRegisterBW(
            rde, RegLog2(rde) ? RegRexrReg(m, rde) : ByteRexrReg(m, rde)));
    return;
  }
  AluRo(A, kAlu[ALU_AND], kAluFast[ALU_AND]);
}

static void OpAluCmp(P) {
  if (IsMakingPath(m) && FuseBranchCmp(A, false)) {
    kAlu[ALU_SUB][RegLog2(rde)](
        m, ReadRegisterOrMemoryBW(rde, GetModrmReadBW(A)),
        ReadRegisterBW(
            rde, RegLog2(rde) ? RegRexrReg(m, rde) : ByteRexrReg(m, rde)));
    return;
  }
  AluRo(A, kAlu[ALU_SUB], kAluFast[ALU_SUB]);
}

static void OpAluFlip(P) {
  aluop_f op = kAlu[(Opcode(rde) & 070) >> 3][RegLog2(rde)];
  u8 *q = RegLog2(rde) ? RegRexrReg(m, rde) : ByteRexrReg(m, rde);
  WriteRegisterBW(rde, q,
                  op(m, ReadRegisterBW(rde, q),
                     ReadRegisterOrMemoryBW(rde, GetModrmReadBW(A))));
  if (IsMakingPath(m)) {
    STATISTIC(++alu_ops);
    LoadAluFlipArgs(A);
    switch (GetNeededFlags(m, m->ip, CF | ZF | SF | OF | AF | PF)) {
      case 0:
        STATISTIC(++alu_unflagged);
        if (GetFlagDeps(rde)) Jitter(A, "q");  // arg0 = sav0 (machine)
        Jitter(A,
               "m"     // call micro-op
               "r0C",  // PutReg(RexrReg, res0)
               kJustAlu[(Opcode(rde) & 070) >> 3]);
        break;
      case CF:
      case ZF:
      case CF | ZF:
        STATISTIC(++alu_simplified);
        Jitter(A,
               "q"     // arg0 = sav0 (machine)
               "m"     // call micro-op
               "r0C",  // PutReg(RexrReg, res0)
               kAluFast[(Opcode(rde) & 070) >> 3][RegLog2(rde)]);
        break;
      default:
        Jitter(A,
               "q"     // arg0 = sav0 (machine)
               "c"     // call function
               "r0C",  // PutReg(RexrReg, res0)
               op);
        break;
    }
  }
}

static void OpAluFlipCmp(P) {
  aluop_f op = kAlu[ALU_SUB][RegLog2(rde)];
  u8 *q = RegLog2(rde) ? RegRexrReg(m, rde) : ByteRexrReg(m, rde);
  op(m, ReadRegisterBW(rde, q), ReadRegisterOrMemoryBW(rde, GetModrmReadBW(A)));
  if (IsMakingPath(m)) {
    STATISTIC(++alu_ops);
    LoadAluFlipArgs(A);
    switch (GetNeededFlags(m, m->ip, CF | ZF | SF | OF | AF | PF)) {
      case 0:
      case CF:
      case ZF:
      case CF | ZF:
        STATISTIC(++alu_simplified);
        Jitter(A,
               "q"   // arg0 = sav0 (machine)
               "m",  // call micro-op
               kAluFast[ALU_SUB][RegLog2(rde)]);
        break;
      default:
        Jitter(A,
               "q"   // arg0 = sav0 (machine)
               "c",  // call function
               op);
        break;
    }
  }
}

static void OpAluAxImm(P) {
  aluop_f op;
  op = kAlu[(Opcode(rde) & 070) >> 3][RegLog2(rde)];
  WriteRegisterBW(rde, m->ax, op(m, ReadRegisterBW(rde, m->ax), uimm0));
  if (IsMakingPath(m)) {
    switch (GetNeededFlags(m, m->ip, CF | ZF | SF | OF | AF | PF)) {
      case 0:
      case CF:
      case ZF:
      case CF | ZF:
        STATISTIC(++alu_simplified);
        Jitter(A,
               "G"      // res0 = %ax
               "r0a1="  // arg1 = res0
               "a2i"    //
               "q"      // arg0 = machine
               "m"      // call op
               "r0H",   // %ax = res0
               uimm0, kAluFast[(Opcode(rde) & 070) >> 3][RegLog2(rde)]);
        break;
      default:
        Jitter(A,
               "G"      // res0 = %ax
               "r0a1="  // arg1 = res0
               "a2i"    //
               "q"      // arg0 = machine
               "c"      // call op
               "r0H",   // %ax = res0
               uimm0, op);
        break;
    }
  }
}

static void OpRoAxImm(P, const aluop_f ops[4], const aluop_f fops[4]) {
  ops[RegLog2(rde)](m, ReadRegisterBW(rde, m->ax), uimm0);
  if (IsMakingPath(m)) {
    STATISTIC(++alu_ops);
    switch (GetNeededFlags(m, m->ip, CF | ZF | SF | OF | AF | PF)) {
      case 0:
      case CF:
      case ZF:
      case CF | ZF:
        STATISTIC(++alu_simplified);
        Jitter(A,
               "G"      // r0 = GetReg(AX)
               "a2i"    // arg2 = uimm0
               "r0a1="  // arg1 = res0
               "q"      // arg0 = sav0 (machine)
               "m",     // call micro-op
               uimm0, fops[RegLog2(rde)]);
        break;
      default:
        Jitter(A,
               "G"      // r0 = GetReg(AX)
               "a2i"    // arg2 = uimm0
               "r0a1="  // arg1 = res0
               "q"      // arg0 = sav0 (machine)
               "c",     // call function
               uimm0, ops[RegLog2(rde)]);
        break;
    }
  }
}

static void OpCmpAxImm(P) {
  OpRoAxImm(A, kAlu[ALU_SUB], kAluFast[ALU_SUB]);
}

static void OpTestAxImm(P) {
  OpRoAxImm(A, kAlu[ALU_AND], kAluFast[ALU_AND]);
}

static void OpBsuwiCl(P) {
  aluop_f op = kBsu[ModrmReg(rde)][RegLog2(rde)];
  u8 *p = GetModrmRegisterWordPointerWriteOszRexw(A);
  WriteRegisterOrMemory(rde, p, op(m, ReadMemory(rde, p), m->cl));
  if (IsMakingPath(m)) {
    switch (ModrmReg(rde)) {
      case BSU_ROL:
      case BSU_ROR:
      case BSU_SHL:
      case BSU_SHR:
      case BSU_SAL:
      case BSU_SAR:
        if (!GetNeededFlags(m, m->ip, GetFlagClobbers(rde))) {
          if (Rexw(rde)) {
            Jitter(A,
                   "B"      // res0 = GetRegOrMem(RexbRm)
                   "s0a1="  // arg1 = machine
                   "t"      // arg0 = res0
                   "m"      // call function
                   "r0D",   // PutRegOrMem(RexbRm, res0)
                   kJustBsuCl64[ModrmReg(rde)]);
          } else if (!Osz(rde)) {
            Jitter(A,
                   "B"      // res0 = GetRegOrMem(RexbRm)
                   "s0a1="  // arg1 = machine
                   "t"      // arg0 = res0
                   "m"      // call function
                   "r0D",   // PutRegOrMem(RexbRm, res0)
                   kJustBsuCl32[ModrmReg(rde)]);
          }
        }
        break;
      default:
        break;
    }
  }
}

static void BsuwiConstant(P, u64 y) {
  aluop_f op = kBsu[ModrmReg(rde)][RegLog2(rde)];
  u8 *p = GetModrmRegisterWordPointerWriteOszRexw(A);
  WriteRegisterOrMemory(rde, p, op(m, ReadMemory(rde, p), y));
  if (IsMakingPath(m)) {
    switch (ModrmReg(rde)) {
      case BSU_ROL:
      case BSU_ROR:
      case BSU_SHL:
      case BSU_SHR:
      case BSU_SAL:
      case BSU_SAR:
        STATISTIC(++alu_ops);
        if (!GetNeededFlags(m, m->ip, GetFlagClobbers(rde))) {
          if (Rexw(rde) && (y &= 63)) {
            STATISTIC(++alu_unflagged);
            Jitter(A,
                   "B"     // res0 = GetRegOrMem(RexbRm)
                   "a3i"   // arg3 = shift amount
                   ""      // arg2 = undefined
                   ""      // arg1 = undefined
                   "t"     // arg0 = res0
                   "m"     // call micro-op
                   "r0D",  // PutRegOrMem(RexbRm, res0)
                   y, kJustBsu[ModrmReg(rde)]);
            return;
          } else if (!Osz(rde) && (y &= 31)) {
            STATISTIC(++alu_unflagged);
            Jitter(A,
                   "B"     // res0 = GetRegOrMem(RexbRm)
                   "a3i"   // arg3 = shift amount
                   ""      // arg2 = undefined
                   ""      // arg1 = undefined
                   "t"     // arg0 = res0
                   "m"     // call micro-op
                   "r0D",  // PutRegOrMem(RexbRm, res0)
                   y, kJustBsu32[ModrmReg(rde)]);
            return;
          }
        }
        break;
      default:
        break;
    }
    Jitter(A,
           "B"      // res0 = GetRegOrMem(RexbRm)
           "a2i"    // arg2 = shift amount
           "r0a1="  // arg1 = res0
           "q"      // arg0 = sav0 (machine)
           "c"      // call function
           "r0D",   // PutRegOrMem(RexbRm, res0)
           y, op);
  }
}

static void OpBsuwi1(P) {
  BsuwiConstant(A, 1);
}

static void OpBsuwiImm(P) {
  BsuwiConstant(A, uimm0);
}

static aluop_f Bsubi(P, u64 y) {
  aluop_f op = kBsu[ModrmReg(rde)][RegLog2(rde)];
  u8 *a = GetModrmRegisterBytePointerWrite1(A);
  Store8(a, op(m, Load8(a), y));
  return op;
}

static void OpBsubiCl(P) {
  aluop_f op;
  op = Bsubi(A, m->cl);
  if (IsMakingPath(m)) {
    Jitter(A,
           "B"      // res0 = GetRegOrMem(RexbRm)
           "r0s1="  // sav1 = res0
           "%cl"    //
           "r0a2="  // arg2 = res0
           "s1a1="  // arg1 = sav1
           "q"      // arg0 = sav0 (machine)
           "c"      // call function
           "r0D",
           op);
  }
}

static void BsubiConstant(P, u64 y) {
  aluop_f op;
  op = Bsubi(A, y);
  if (IsMakingPath(m)) {
    Jitter(A,
           "B"      // res0 = GetRegOrMem(RexbRm)
           "r0a1="  // arg1 = res0
           "q"      // arg0 = sav0 (machine)
           "a2i"    //
           "c"      // call function
           "r0D",   //
           y, op);
  }
}

static void OpBsubi1(P) {
  BsubiConstant(A, 1);
}

static void OpBsubiImm(P) {
  BsubiConstant(A, uimm0);
}

static void OpPushImm(P) {
  Push(A, uimm0);
}

static void OpInterruptImm(P) {
  HaltMachine(m, uimm0);
}

static void OpInterrupt1(P) {
  HaltMachine(m, 1);
}

static void OpInterrupt3(P) {
  HaltMachine(m, 3);
}

void Terminate(P, void uop(struct Machine *, u64)) {
  if (IsMakingPath(m)) {
    Jitter(A,
           "a1i"  // arg1 = disp
           "m"    // call micro-op
           "q",   // arg0 = sav0 (machine)
           disp, uop);
    AlignJit(m->path.jb, 8, 0);
    Connect(A, m->ip, true);
    FinishPath(m);
  }
}

static void OpJmp(P) {
  m->ip += disp;
  Terminate(A, FastJmp);
}

static cc_f GetCc(P) {
  int code;
  code = Opcode(rde) & 15;
  unassert(code != 0xA);  // JP
  unassert(code != 0xB);  // JNP
  return kConditionCode[code];
}

static void OpJcc(P) {
  cc_f cc;
  cc = GetCc(A);
  if (IsMakingPath(m)) {
    FlushSkew(A);
#ifdef __x86_64__
    Jitter(A, "mq", cc);
    AlignJit(m->path.jb, 8, 4);
    u8 code[] = {
        0x85, 0300 | kJitRes0 << 3 | kJitRes0,  // test %eax,%eax
        0x75, 5,                                // jnz  +5
    };
#else
    Jitter(A,
           "m"      // res0 = condition code
           "r0a2="  // arg2 = res0
           "q",     // arg0 = machine
           cc);
    u32 code[] = {
        0xb5000000 | (8 / 4) << 5 | kJitArg2,  // cbnz x2,#8
    };
#endif
    AppendJit(m->path.jb, code, sizeof(code));
    Connect(A, m->ip, true);
    Jitter(A,
           "a1i"  // arg1 = disp
           "m"    // call micro-op
           "q",   // arg0 = machine
           disp, FastJmp);
    AlignJit(m->path.jb, 8, 0);
    Connect(A, m->ip + disp, false);
    FinishPath(m);
  }
  if (cc(m)) {
    m->ip += disp;
  }
}

static void OpJp(P) {
  if (IsParity(m)) {
    m->ip += disp;
  }
}

static void OpJnp(P) {
  if (!IsParity(m)) {
    m->ip += disp;
  }
}

static void SetEb(P, bool x) {
  Store8(GetModrmRegisterBytePointerWrite1(A), x);
}

static void OpSetcc(P) {
  cc_f cc;
  cc = GetCc(A);
  SetEb(A, cc(m));
  if (IsMakingPath(m)) {
    Jitter(A,
           "m"       // call micro-op
           "r0z0D",  // PutRegOrMem[force8](RexbRm, res0)
           cc);
  }
}

static void OpSetp(P) {
  SetEb(A, IsParity(m));
}

static void OpSetnp(P) {
  SetEb(A, !IsParity(m));
}

static void OpCmovImpl(P, bool cond) {
  u64 x;
  if (cond) {
    x = ReadMemory(rde, GetModrmRegisterWordPointerReadOszRexw(A));
  } else {
    x = Get64(RegRexrReg(m, rde));
  }
  WriteRegister(rde, RegRexrReg(m, rde), x);
}

static void OpCmov(P) {
  cc_f cc;
  cc = GetCc(A);
  OpCmovImpl(A, cc(m));
  if (IsMakingPath(m)) {
    Jitter(A,
           "wB"     // res0 = GetRegOrMem[force16+bit](RexbRm)
           "r0s1="  // sav1 = res0
           "wA"     // res0 = GetReg[force16+bit](RexrReg)
           "r0s2="  // sav2 = res0
           "q"      // arg0 = sav0 (machine)
           "m"      // call micro-op (cc)
           "s2a2="  // arg2 = sav2
           "s1a1="  // arg1 = sav1
           "t"      // arg0 = res0
           "m"      // call micro-op (Pick)
           "r0wC",  // PutReg[force16+bit](RexrReg, res0)
           cc, Pick);
  }
}

static void OpCmovp(P) {
  OpCmovImpl(A, IsParity(m));
}

static void OpCmovnp(P) {
  OpCmovImpl(A, !IsParity(m));
}

static void OpJcxz(P) {
  if (!MaskAddress(Eamode(rde), Get64(m->cx))) {
    m->ip += disp;
  }
}

static u64 AluPopcnt(u64 x, struct Machine *m) {
  m->flags = SetFlag(m->flags, FLAGS_ZF, !x);
  m->flags = SetFlag(m->flags, FLAGS_CF, false);
  m->flags = SetFlag(m->flags, FLAGS_SF, false);
  m->flags = SetFlag(m->flags, FLAGS_OF, false);
  m->flags = SetFlag(m->flags, FLAGS_PF, false);
  return popcount(x);
}

static u64 AluLzcnt(u64 x, struct Machine *m, int bits) {
  u64 r = x ? bsf(x) : bits;
  m->flags = SetFlag(m->flags, FLAGS_CF, !x);
  m->flags = SetFlag(m->flags, FLAGS_ZF, !r);
  return r;
}
static u64 AluLzcnt64(u64 x, struct Machine *m) {
  return AluLzcnt(x, m, 64);
}
static u64 AluLzcnt32(u64 x, struct Machine *m) {
  return AluLzcnt(x, m, 32);
}
static u64 AluLzcnt16(u64 x, struct Machine *m) {
  return AluLzcnt(x, m, 16);
}
static u64 AluBsf(u64 x, struct Machine *m) {
  m->flags = SetFlag(m->flags, FLAGS_ZF, !x);
  return x ? bsf(x) : 0;
}

static u64 AluTzcnt(u64 x, struct Machine *m, int bits) {
  u64 r = x ? bsr(x) : bits;
  m->flags = SetFlag(m->flags, FLAGS_CF, !x);
  m->flags = SetFlag(m->flags, FLAGS_ZF, !r);
  return r;
}
static u64 AluTzcnt64(u64 x, struct Machine *m) {
  return AluTzcnt(x, m, 64);
}
static u64 AluTzcnt32(u64 x, struct Machine *m) {
  return AluTzcnt(x, m, 32);
}
static u64 AluTzcnt16(u64 x, struct Machine *m) {
  return AluTzcnt(x, m, 16);
}
static u64 AluBsr(u64 x, struct Machine *m) {
  m->flags = SetFlag(m->flags, FLAGS_ZF, !x);
  return x ? bsr(x) : 0;
}

static void Bitscan(P, u64 op(u64, struct Machine *)) {
  WriteRegister(
      rde, RegRexrReg(m, rde),
      op(ReadMemory(rde, GetModrmRegisterWordPointerReadOszRexw(A)), m));
  if (IsMakingPath(m)) {
    Jitter(A,
           "wB"     // res0 = GetRegOrMem[force16+bit](RexbRm)
           "s0a1="  // arg1 = sav0
           "t"      // arg0 = res0
           "c"      // call function (op)
           "r0wC",  // PutReg[force16+bit](RexrReg, res0)
           op);
  }
}

static void OpBsf(P) {
  u64 (*op)(u64, struct Machine *);
  if (Rep(rde) == 3) {
    if (Rexw(rde)) {
      op = AluLzcnt64;
    } else if (!Osz(rde)) {
      op = AluLzcnt32;
    } else {
      op = AluLzcnt16;
    }
  } else {
    op = AluBsf;
  }
  Bitscan(A, op);
}

static void OpBsr(P) {
  u64 (*op)(u64, struct Machine *);
  if (Rep(rde) == 3) {
    if (Rexw(rde)) {
      op = AluTzcnt64;
    } else if (!Osz(rde)) {
      op = AluTzcnt32;
    } else {
      op = AluTzcnt16;
    }
  } else {
    op = AluBsr;
  }
  Bitscan(A, op);
}

static void Op1b8(P) {
  if (Rep(rde) == 3) {
    Bitscan(A, AluPopcnt);
  } else {
    OpUdImpl(m);
  }
}

static relegated void Loop(P, bool cond) {
  u64 cx;
  cx = Get64(m->cx) - 1;
  if (Eamode(rde) != XED_MODE_REAL) {
    if (Eamode(rde) == XED_MODE_LEGACY) {
      cx &= 0xffffffff;
    }
    Put64(m->cx, cx);
  } else {
    cx &= 0xffff;
    Put16(m->cx, cx);
  }
  if (cx && cond) {
    m->ip += disp;
  }
}

static relegated void OpLoope(P) {
  Loop(A, GetFlag(m->flags, FLAGS_ZF));
}

static relegated void OpLoopne(P) {
  Loop(A, !GetFlag(m->flags, FLAGS_ZF));
}

static relegated void OpLoop1(P) {
  Loop(A, true);
}

static const nexgen32e_f kOp0f6[] = {
    OpTest,
    OpTest,
    OpNotEb,
    OpNegEb,
    OpMulAxAlEbUnsigned,
    OpMulAxAlEbSigned,
    OpDivAlAhAxEbUnsigned,
    OpDivAlAhAxEbSigned,
};

static void Op0f6(P) {
  kOp0f6[ModrmReg(rde)](A);
}

static const nexgen32e_f kOp0f7[] = {
    OpTest,
    OpTest,
    OpNotEvqp,
    OpNegEvqp,
    OpMulRdxRaxEvqpUnsigned,
    OpMulRdxRaxEvqpSigned,
    OpDivRdxRaxEvqpUnsigned,
    OpDivRdxRaxEvqpSigned,
};

static void Op0f7(P) {
  kOp0f7[ModrmReg(rde)](A);
}

#ifdef DISABLE_METAL
#define OpCallfEq   OpUd
#define OpJmpfEq    OpUd
#endif

static const nexgen32e_f kOp0ff[] = {
    OpIncEvqp,  //
    OpDecEvqp,  //
    OpCallEq,   //
    OpCallfEq,  //
    OpJmpEq,    //
    OpJmpfEq,   //
    OpPushEvq,  //
    OpUd,       //
};

static void Op0ff(P) {
  kOp0ff[ModrmReg(rde)](A);
}

static void OpDoubleShift(P) {
  u8 *p;
  u8 W[2][2] = {{2, 3}, {1, 3}};
  p = GetModrmRegisterWordPointerWriteOszRexw(A);
  WriteRegisterOrMemory(
      rde, p,
      BsuDoubleShift(m, W[Osz(rde)][Rexw(rde)], ReadMemory(rde, p),
                     ReadRegister(rde, RegRexrReg(m, rde)),
                     Opcode(rde) & 1 ? m->cl : uimm0, Opcode(rde) & 8));
}

static void OpFxsave(P) {
  i64 v;
  u8 buf[32];
  memset(buf, 0, 32);
  Write16(buf + 0, m->fpu.cw);
#ifndef DISABLE_X87
  Write16(buf + 2, m->fpu.sw);
  Write8(buf + 4, m->fpu.tw);
  Write16(buf + 6, m->fpu.op);
  Write32(buf + 8, m->fpu.ip);
#endif
  Write32(buf + 24, m->mxcsr);
  v = ComputeAddress(A);
  CopyToUser(m, v + 0, buf, 32);
#ifndef DISABLE_X87
  CopyToUser(m, v + 32, m->fpu.st, 128);
#endif
  CopyToUser(m, v + 160, m->xmm, 256);
  SetWriteAddr(m, v, 416);
}

static void OpFxrstor(P) {
  i64 v;
  u8 buf[32];
  v = ComputeAddress(A);
  SetReadAddr(m, v, 416);
  CopyFromUser(m, buf, v + 0, 32);
#ifndef DISABLE_X87
  CopyFromUser(m, m->fpu.st, v + 32, 128);
#endif
  CopyFromUser(m, m->xmm, v + 160, 256);
  m->fpu.cw = Load16(buf + 0);
#ifndef DISABLE_X87
  m->fpu.sw = Load16(buf + 2);
  m->fpu.tw = Load8(buf + 4);
  m->fpu.op = Load16(buf + 6);
  m->fpu.ip = Load32(buf + 8);
#endif
  m->mxcsr = Load32(buf + 24);
}

static void OpXsave(P) {
}

static void OpLdmxcsr(P) {
  m->mxcsr = Load32(ComputeReserveAddressRead4(A));
}

static void OpStmxcsr(P) {
  Store32(ComputeReserveAddressWrite4(A), m->mxcsr);
}

static void OpRdfsbase(P) {
  WriteRegister(rde, RegRexbRm(m, rde), m->fs.base);
}

static void OpRdgsbase(P) {
  WriteRegister(rde, RegRexbRm(m, rde), m->gs.base);
}

static void OpWrfsbase(P) {
  m->fs.base = ReadRegister(rde, RegRexbRm(m, rde));
}

static void OpWrgsbase(P) {
  m->gs.base = ReadRegister(rde, RegRexbRm(m, rde));
}

static void OpMfence(P) {
  atomic_thread_fence(memory_order_seq_cst);
}

static void OpLfence(P) {
  OpMfence(A);
}

static void OpSfence(P) {
  OpMfence(A);
}

static void OpWbinvd(P) {
  OpMfence(A);
}

static void OpClflush(P) {
  OpMfence(A);
}

static void Op1ae(P) {
  bool ismem;
  ismem = !IsModrmRegister(rde);
  switch (ModrmReg(rde)) {
    case 0:
      if (ismem) {
        OpFxsave(A);
      } else {
        OpRdfsbase(A);
      }
      break;
    case 1:
      if (ismem) {
        OpFxrstor(A);
      } else {
        OpRdgsbase(A);
      }
      break;
    case 2:
      if (ismem) {
        OpLdmxcsr(A);
      } else {
        OpWrfsbase(A);
      }
      break;
    case 3:
      if (ismem) {
        OpStmxcsr(A);
      } else {
        OpWrgsbase(A);
      }
      break;
    case 4:
      if (ismem) {
        OpXsave(A);
      } else {
        OpUdImpl(m);
      }
      break;
    case 5:
      OpLfence(A);
      break;
    case 6:
      OpMfence(A);
      break;
    case 7:
      if (ismem) {
        OpClflush(A);
      } else {
        OpSfence(A);
      }
      break;
    default:
      OpUdImpl(m);
  }
}

static relegated void OpSalc(P) {
  if (GetFlag(m->flags, FLAGS_CF)) {
    m->al = 255;
  } else {
    m->al = 0;
  }
}

static relegated void OpBofram(P) {
  if (disp) {
    m->bofram[0] = m->ip;
    m->bofram[1] = m->ip + (disp & 0xff);
  } else {
    m->bofram[0] = 0;
    m->bofram[1] = 0;
  }
}

static relegated void OpBinbase(P) {
  if (m->system->onbinbase) {
    m->system->onbinbase(m);
  }
}

static void OpNoop(P) {
  if (IsMakingPath(m)) {
    AppendJitNop(m->path.jb);
  }
}

static void OpNopEv(P) {
  switch (ModrmMod(rde) << 6 | ModrmReg(rde) << 3 | ModrmRm(rde)) {
    case 0105:
      OpBofram(A);
      break;
    case 0007:
    case 0107:
    case 0207:
      OpBinbase(A);
      break;
    default:
      OpNoop(A);
  }
}

static void OpNop(P) {
  if (Rexb(rde)) {
    OpXchgZvqp(A);
  } else if (Rep(rde) == 3) {
    OpPause(A);
  } else {
    OpNoop(A);
  }
}

static void OpEmms(P) {
#ifndef DISABLE_X87
  m->fpu.tw = -1;
#endif
}

#ifdef DISABLE_METAL
#define OpCallf     OpUd
#define OpDecZv     OpUd
#define OpInAlDx    OpUd
#define OpInAlImm   OpUd
#define OpInAxDx    OpUd
#define OpInAxImm   OpUd
#define OpIncZv     OpUd
#define OpJmpf      OpUd
#define OpLds       OpUd
#define OpLes       OpUd
#define OpMovCqRq   OpUd
#define OpMovEvqpSw OpUd
#define OpMovRqCq   OpUd
#define OpMovSwEvqp OpUd
#define OpOutDxAl   OpUd
#define OpOutDxAx   OpUd
#define OpOutImmAl  OpUd
#define OpOutImmAx  OpUd
#define OpPopSeg    OpUd
#define OpPopa      OpUd
#define OpPushSeg   OpUd
#define OpPusha     OpUd
#define OpRdmsr     OpUd
#define OpRetf      OpUd
#define OpWrmsr     OpUd
#endif

#ifdef DISABLE_X87
#define OpFwait OpUd
#endif

#ifdef DISABLE_BMI2
#define Op2f5  OpUd
#define Op2f6  OpUd
#define OpShx  OpUd
#define OpRorx OpUd
#endif

#ifdef DISABLE_BCD
#define OpDas OpUd
#define OpAaa OpUd
#define OpAas OpUd
#define OpAam OpUd
#define OpAad OpUd
#define OpDaa OpUd
#endif

static const nexgen32e_f kNexgen32e[] = {
    /*000*/ OpAlub,                  //
    /*001*/ OpAluw,                  // #8    (5.653689%)
    /*002*/ OpAluFlip,               // #180  (0.000087%)
    /*003*/ OpAluFlip,               // #7    (5.840835%)
    /*004*/ OpAluAxImm,              //
    /*005*/ OpAluAxImm,              // #166  (0.000114%)
    /*006*/ OpPushSeg,               //
    /*007*/ OpPopSeg,                //
    /*008*/ OpAlub,                  // #154  (0.000207%)
    /*009*/ OpAluw,                  // #21   (0.520082%)
    /*00A*/ OpAluFlip,               // #120  (0.001072%)
    /*00B*/ OpAluFlip,               // #114  (0.001252%)
    /*00C*/ OpAluAxImm,              //
    /*00D*/ OpAluAxImm,              // #282  (0.000001%)
    /*00E*/ OpPushSeg,               //
    /*00F*/ OpPopSeg,                //
    /*010*/ OpAlub,                  //
    /*011*/ OpAluw,                  // #11   (5.307809%)
    /*012*/ OpAluFlip,               //
    /*013*/ OpAluFlip,               // #108  (0.001526%)
    /*014*/ OpAluAxImm,              // #97   (0.002566%)
    /*015*/ OpAluAxImm,              //
    /*016*/ OpPushSeg,               //
    /*017*/ OpPopSeg,                //
    /*018*/ OpAlub,                  //
    /*019*/ OpAluw,                  // #65   (0.015300%)
    /*01A*/ OpAluFlip,               //
    /*01B*/ OpAluFlip,               // #44   (0.241806%)
    /*01C*/ OpAluAxImm,              // #96   (0.002566%)
    /*01D*/ OpAluAxImm,              //
    /*01E*/ OpPushSeg,               //
    /*01F*/ OpPopSeg,                //
    /*020*/ OpAlub,                  // #165  (0.000130%)
    /*021*/ OpAluw,                  // #59   (0.019691%)
    /*022*/ OpAluFlip,               //
    /*023*/ OpAluFlip,               // #41   (0.279852%)
    /*024*/ OpAluAxImm,              // #279  (0.000001%)
    /*025*/ OpAluAxImm,              // #43   (0.275823%)
    /*026*/ OpUd,                    //
    /*027*/ OpDaa,                   //
    /*028*/ OpAlub,                  //
    /*029*/ OpAluw,                  // #29   (0.334693%)
    /*02A*/ OpAluFlip,               // #179  (0.000087%)
    /*02B*/ OpAluFlip,               // #71   (0.012465%)
    /*02C*/ OpAluAxImm,              //
    /*02D*/ OpAluAxImm,              // #112  (0.001317%)
    /*02E*/ OpUd,                    //
    /*02F*/ OpDas,                   //
    /*030*/ OpAlub,                  // #140  (0.000397%)
    /*031*/ OpAluw,                  // #3    (6.612252%)
    /*032*/ OpAluFlip,               // #81   (0.007453%)
    /*033*/ OpAluFlip,               // #47   (0.138021%)
    /*034*/ OpAluAxImm,              //
    /*035*/ OpAluAxImm,              // #295  (0.000000%)
    /*036*/ OpUd,                    //
    /*037*/ OpAaa,                   //
    /*038*/ OpAluCmp,                // #98   (0.002454%)
    /*039*/ OpAluCmp,                // #2    (6.687374%)
    /*03A*/ OpAluFlipCmp,            // #103  (0.001846%)
    /*03B*/ OpAluFlipCmp,            // #75   (0.010320%)
    /*03C*/ OpCmpAxImm,              // #85   (0.006267%)
    /*03D*/ OpCmpAxImm,              // #42   (0.279462%)
    /*03E*/ OpUd,                    //
    /*03F*/ OpAas,                   //
    /*040*/ OpIncZv,                 //
    /*041*/ OpIncZv,                 //
    /*042*/ OpIncZv,                 //
    /*043*/ OpIncZv,                 //
    /*044*/ OpIncZv,                 //
    /*045*/ OpIncZv,                 //
    /*046*/ OpIncZv,                 //
    /*047*/ OpIncZv,                 //
    /*048*/ OpDecZv,                 //
    /*049*/ OpDecZv,                 //
    /*04A*/ OpDecZv,                 //
    /*04B*/ OpDecZv,                 //
    /*04C*/ OpDecZv,                 //
    /*04D*/ OpDecZv,                 //
    /*04E*/ OpDecZv,                 //
    /*04F*/ OpDecZv,                 //
    /*050*/ OpPushZvq,               // #82   (0.007191%)
    /*051*/ OpPushZvq,               // #91   (0.003740%)
    /*052*/ OpPushZvq,               // #138  (0.000405%)
    /*053*/ OpPushZvq,               // #27   (0.343891%)
    /*054*/ OpPushZvq,               // #30   (0.332411%)
    /*055*/ OpPushZvq,               // #16   (0.661109%)
    /*056*/ OpPushZvq,               // #35   (0.297138%)
    /*057*/ OpPushZvq,               // #38   (0.289927%)
    /*058*/ OpPopZvq,                // #155  (0.000199%)
    /*059*/ OpPopZvq,                // #190  (0.000054%)
    /*05A*/ OpPopZvq,                // #74   (0.011075%)
    /*05B*/ OpPopZvq,                // #28   (0.343770%)
    /*05C*/ OpPopZvq,                // #31   (0.332403%)
    /*05D*/ OpPopZvq,                // #17   (0.659868%)
    /*05E*/ OpPopZvq,                // #36   (0.296997%)
    /*05F*/ OpPopZvq,                // #39   (0.289680%)
    /*060*/ OpPusha,                 //
    /*061*/ OpPopa,                  //
    /*062*/ OpUd,                    //
    /*063*/ OpMovslGdqpEd,           // #58   (0.026117%)
    /*064*/ OpUd,                    //
    /*065*/ OpUd,                    //
    /*066*/ OpUd,                    //
    /*067*/ OpUd,                    //
    /*068*/ OpPushImm,               // #168  (0.000112%)
    /*069*/ OpImulGvqpEvqpImm,       // #147  (0.000253%)
    /*06A*/ OpPushImm,               // #143  (0.000370%)
    /*06B*/ OpImulGvqpEvqpImm,       // #131  (0.000720%)
    /*06C*/ OpIns,                   //
    /*06D*/ OpIns,                   //
    /*06E*/ OpOuts,                  //
    /*06F*/ OpOuts,                  //
    /*070*/ OpJcc,                   // #177  (0.000094%)
    /*071*/ OpJcc,                   // #200  (0.000034%)
    /*072*/ OpJcc,                   // #64   (0.015441%)
    /*073*/ OpJcc,                   // #9    (5.615257%)
    /*074*/ OpJcc,                   // #15   (0.713108%)
    /*075*/ OpJcc,                   // #13   (0.825247%)
    /*076*/ OpJcc,                   // #23   (0.475584%)
    /*077*/ OpJcc,                   // #48   (0.054677%)
    /*078*/ OpJcc,                   // #66   (0.014096%)
    /*079*/ OpJcc,                   // #84   (0.006506%)
    /*07A*/ OpJp,                    // #175  (0.000112%)
    /*07B*/ OpJnp,                   // #174  (0.000112%)
    /*07C*/ OpJcc,                   // #223  (0.000008%)
    /*07D*/ OpJcc,                   // #80   (0.007801%)
    /*07E*/ OpJcc,                   // #70   (0.012536%)
    /*07F*/ OpJcc,                   // #76   (0.010144%)
    /*080*/ OpAlui,                  // #53   (0.033021%)
    /*081*/ OpAlui,                  // #60   (0.018910%)
    /*082*/ OpAlui,                  //
    /*083*/ OpAlui,                  // #4    (6.518845%)
    /*084*/ OpAluTest,               // #54   (0.030642%)
    /*085*/ OpAluTest,               // #18   (0.628547%)
    /*086*/ OpXchgGbEb,              // #219  (0.000011%)
    /*087*/ OpXchgGvqpEvqp,          // #161  (0.000141%)
    /*088*/ OpMovEbGb,               // #49   (0.042510%)
    /*089*/ OpMovEvqpGvqp,           // #1    (22.226650%)
    /*08A*/ OpMovGbEb,               // #51   (0.038177%)
    /*08B*/ OpMovGvqpEvqp,           // #12   (2.903141%)
    /*08C*/ OpMovEvqpSw,             //
    /*08D*/ OpLeaGvqpM,              // #14   (0.800508%)
    /*08E*/ OpMovSwEvqp,             //
    /*08F*/ OpPopEvq,                // #288  (0.000000%)
    /*090*/ OpNop,                   // #218  (0.000011%)
    /*091*/ OpXchgZvqp,              // #278  (0.000001%)
    /*092*/ OpXchgZvqp,              // #284  (0.000001%)
    /*093*/ OpXchgZvqp,              // #213  (0.000018%)
    /*094*/ OpXchgZvqp,              //
    /*095*/ OpXchgZvqp,              //
    /*096*/ OpXchgZvqp,              //
    /*097*/ OpXchgZvqp,              // #286  (0.000001%)
    /*098*/ OpSax,                   // #83   (0.006728%)
    /*099*/ OpConvert,               // #163  (0.000137%)
    /*09A*/ OpCallf,                 //
    /*09B*/ OpFwait,                 //
    /*09C*/ OpPushf,                 //
    /*09D*/ OpPopf,                  //
    /*09E*/ OpSahf,                  //
    /*09F*/ OpLahf,                  //
    /*0A0*/ OpMovAlOb,               //
    /*0A1*/ OpMovRaxOvqp,            //
    /*0A2*/ OpMovObAl,               //
    /*0A3*/ OpMovOvqpRax,            //
    /*0A4*/ OpMovsb,                 // #73   (0.011594%)
    /*0A5*/ OpMovs,                  // #158  (0.000147%)
    /*0A6*/ OpCmps,                  //
    /*0A7*/ OpCmps,                  //
    /*0A8*/ OpTestAxImm,             // #115  (0.001247%)
    /*0A9*/ OpTestAxImm,             // #113  (0.001300%)
    /*0AA*/ OpStosb,                 // #67   (0.013327%)
    /*0AB*/ OpStos,                  // #194  (0.000044%)
    /*0AC*/ OpLods,                  // #198  (0.000035%)
    /*0AD*/ OpLods,                  // #296  (0.000000%)
    /*0AE*/ OpScas,                  // #157  (0.000152%)
    /*0AF*/ OpScas,                  // #292  (0.000000%)
    /*0B0*/ OpMovZbIb,               // #135  (0.000500%)
    /*0B1*/ OpMovZbIb,               // #178  (0.000093%)
    /*0B2*/ OpMovZbIb,               // #176  (0.000099%)
    /*0B3*/ OpMovZbIb,               // #202  (0.000028%)
    /*0B4*/ OpMovZbIb,               //
    /*0B5*/ OpMovZbIb,               // #220  (0.000010%)
    /*0B6*/ OpMovZbIb,               // #136  (0.000488%)
    /*0B7*/ OpMovZbIb,               // #216  (0.000014%)
    /*0B8*/ OpMovZvqpIvqp,           // #33   (0.315404%)
    /*0B9*/ OpMovZvqpIvqp,           // #50   (0.039176%)
    /*0BA*/ OpMovZvqpIvqp,           // #32   (0.315927%)
    /*0BB*/ OpMovZvqpIvqp,           // #93   (0.003549%)
    /*0BC*/ OpMovZvqpIvqp,           // #109  (0.001380%)
    /*0BD*/ OpMovZvqpIvqp,           // #101  (0.002061%)
    /*0BE*/ OpMovZvqpIvqp,           // #68   (0.013223%)
    /*0BF*/ OpMovZvqpIvqp,           // #79   (0.008900%)
    /*0C0*/ OpBsubiImm,              // #111  (0.001368%)
    /*0C1*/ OpBsuwiImm,              // #20   (0.536537%)
    /*0C2*/ OpRetIw,                 //
    /*0C3*/ OpRet,                   // #24   (0.422698%)
    /*0C4*/ OpLes,                   //
    /*0C5*/ OpLds,                   //
    /*0C6*/ OpMovImm,                // #90   (0.004525%)
    /*0C7*/ OpMovImm,                // #45   (0.161349%)
    /*0C8*/ OpUd,                    //
    /*0C9*/ OpLeave,                 // #116  (0.001237%)
    /*0CA*/ OpRetf,                  //
    /*0CB*/ OpRetf,                  //
    /*0CC*/ OpInterrupt3,            //
    /*0CD*/ OpInterruptImm,          //
    /*0CE*/ OpUd,                    //
    /*0CF*/ OpUd,                    //
    /*0D0*/ OpBsubi1,                // #212  (0.000021%)
    /*0D1*/ OpBsuwi1,                // #61   (0.016958%)
    /*0D2*/ OpBsubiCl,               //
    /*0D3*/ OpBsuwiCl,               // #19   (0.621270%)
    /*0D4*/ OpAam,                   //
    /*0D5*/ OpAad,                   //
    /*0D6*/ OpSalc,                  //
    /*0D7*/ OpXlatAlBbb,             //
    /*0D8*/ OpFpu,                   // #258  (0.000005%)
    /*0D9*/ OpFpu,                   // #145  (0.000335%)
    /*0DA*/ OpFpu,                   // #290  (0.000000%)
    /*0DB*/ OpFpu,                   // #139  (0.000399%)
    /*0DC*/ OpFpu,                   // #283  (0.000001%)
    /*0DD*/ OpFpu,                   // #144  (0.000340%)
    /*0DE*/ OpFpu,                   // #193  (0.000046%)
    /*0DF*/ OpFpu,                   // #215  (0.000014%)
    /*0E0*/ OpLoopne,                //
    /*0E1*/ OpLoope,                 //
    /*0E2*/ OpLoop1,                 //
    /*0E3*/ OpJcxz,                  //
    /*0E4*/ OpInAlImm,               //
    /*0E5*/ OpInAxImm,               //
    /*0E6*/ OpOutImmAl,              //
    /*0E7*/ OpOutImmAx,              //
    /*0E8*/ OpCallJvds,              // #25   (0.403872%)
    /*0E9*/ OpJmp,                   // #22   (0.476546%)
    /*0EA*/ OpJmpf,                  //
    /*0EB*/ OpJmp,                   // #6    (6.012044%)
    /*0EC*/ OpInAlDx,                //
    /*0ED*/ OpInAxDx,                //
    /*0EE*/ OpOutDxAl,               //
    /*0EF*/ OpOutDxAx,               //
    /*0F0*/ OpUd,                    //
    /*0F1*/ OpInterrupt1,            //
    /*0F2*/ OpUd,                    //
    /*0F3*/ OpUd,                    //
    /*0F4*/ OpHlt,                   //
    /*0F5*/ OpCmc,                   //
    /*0F6*/ Op0f6,                   // #56   (0.028122%)
    /*0F7*/ Op0f7,                   // #10   (5.484639%)
    /*0F8*/ OpClc,                   // #156  (0.000187%)
    /*0F9*/ OpStc,                   //
    /*0FA*/ OpCli,                   //
    /*0FB*/ OpSti,                   //
    /*0FC*/ OpCld,                   // #142  (0.000379%)
    /*0FD*/ OpStd,                   // #141  (0.000379%)
    /*0FE*/ Op0fe,                   // #181  (0.000083%)
    /*0FF*/ Op0ff,                   // #5    (6.314024%)
    /*100*/ OpUd,                    //
    /*101*/ Op101,                   //
    /*102*/ OpUd,                    //
    /*103*/ OpLsl,                   //
    /*104*/ OpUd,                    //
    /*105*/ OpSyscall,               // #133  (0.000663%)
    /*106*/ OpUd,                    //
    /*107*/ OpUd,                    //
    /*108*/ OpUd,                    //
    /*109*/ OpWbinvd,                //
    /*10A*/ OpUd,                    //
    /*10B*/ OpUd,                    //
    /*10C*/ OpUd,                    //
    /*10D*/ OpHintNopEv,             //
    /*10E*/ OpUd,                    //
    /*10F*/ OpUd,                    //
    /*110*/ OpMov0f10,               // #89   (0.004629%)
    /*111*/ OpMovWpsVps,             // #104  (0.001831%)
    /*112*/ OpMov0f12,               //
    /*113*/ OpMov0f13,               //
    /*114*/ OpUnpcklpsd,             //
    /*115*/ OpUnpckhpsd,             //
    /*116*/ OpMov0f16,               //
    /*117*/ OpMov0f17,               //
    /*118*/ OpHintNopEv,             //
    /*119*/ OpHintNopEv,             //
    /*11A*/ OpHintNopEv,             //
    /*11B*/ OpHintNopEv,             //
    /*11C*/ OpHintNopEv,             //
    /*11D*/ OpHintNopEv,             //
    /*11E*/ OpHintNopEv,             //
    /*11F*/ OpNopEv,                 // #62   (0.016260%)
    /*120*/ OpMovRqCq,               //
    /*121*/ OpUd,                    //
    /*122*/ OpMovCqRq,               //
    /*123*/ OpUd,                    //
    /*124*/ OpUd,                    //
    /*125*/ OpUd,                    //
    /*126*/ OpUd,                    //
    /*127*/ OpUd,                    //
    /*128*/ OpMov0f28,               // #100  (0.002220%)
    /*129*/ OpMovWpsVps,             // #99   (0.002294%)
    /*12A*/ OpCvt0f2a,               // #173  (0.000112%)
    /*12B*/ OpMov0f2b,               //
    /*12C*/ OpCvtt0f2c,              // #172  (0.000112%)
    /*12D*/ OpCvt0f2d,               //
    /*12E*/ OpComissVsWs,            // #153  (0.000223%)
    /*12F*/ OpComissVsWs,            // #152  (0.000223%)
    /*130*/ OpWrmsr,                 //
    /*131*/ OpRdtsc,                 // #214  (0.000016%)
    /*132*/ OpRdmsr,                 //
    /*133*/ OpUd,                    //
    /*134*/ OpUd,                    //
    /*135*/ OpUd,                    //
    /*136*/ OpUd,                    //
    /*137*/ OpUd,                    //
    /*138*/ OpUd,                    //
    /*139*/ OpUd,                    //
    /*13A*/ OpUd,                    //
    /*13B*/ OpUd,                    //
    /*13C*/ OpUd,                    //
    /*13D*/ OpUd,                    //
    /*13E*/ OpUd,                    //
    /*13F*/ OpUd,                    //
    /*140*/ OpCmov,                  //
    /*141*/ OpCmov,                  //
    /*142*/ OpCmov,                  // #69   (0.012667%)
    /*143*/ OpCmov,                  // #276  (0.000002%)
    /*144*/ OpCmov,                  // #134  (0.000584%)
    /*145*/ OpCmov,                  // #132  (0.000700%)
    /*146*/ OpCmov,                  // #125  (0.000945%)
    /*147*/ OpCmov,                  // #40   (0.289378%)
    /*148*/ OpCmov,                  // #130  (0.000774%)
    /*149*/ OpCmov,                  // #149  (0.000228%)
    /*14A*/ OpCmovp,                 //
    /*14B*/ OpCmovnp,                //
    /*14C*/ OpCmov,                  // #102  (0.002008%)
    /*14D*/ OpCmov,                  // #196  (0.000044%)
    /*14E*/ OpCmov,                  // #110  (0.001379%)
    /*14F*/ OpCmov,                  // #121  (0.001029%)
    /*150*/ OpMovmskpsd,             //
    /*151*/ OpSqrtpsd,               //
    /*152*/ OpRsqrtps,               //
    /*153*/ OpRcpps,                 //
    /*154*/ OpAndpsd,                // #171  (0.000112%)
    /*155*/ OpAndnpsd,               //
    /*156*/ OpOrpsd,                 //
    /*157*/ OpXorpsd,                // #148  (0.000245%)
    /*158*/ OpAddpsd,                // #151  (0.000223%)
    /*159*/ OpMulpsd,                // #150  (0.000223%)
    /*15A*/ OpCvt0f5a,               //
    /*15B*/ OpCvt0f5b,               //
    /*15C*/ OpSubpsd,                // #146  (0.000335%)
    /*15D*/ OpMinpsd,                //
    /*15E*/ OpDivpsd,                // #170  (0.000112%)
    /*15F*/ OpMaxpsd,                //
    /*160*/ OpSsePunpcklbw,          // #259  (0.000003%)
    /*161*/ OpSsePunpcklwd,          // #221  (0.000009%)
    /*162*/ OpSsePunpckldq,          // #262  (0.000003%)
    /*163*/ OpSsePacksswb,           // #297  (0.000000%)
    /*164*/ OpSsePcmpgtb,            // #274  (0.000003%)
    /*165*/ OpSsePcmpgtw,            // #273  (0.000003%)
    /*166*/ OpSsePcmpgtd,            // #272  (0.000003%)
    /*167*/ OpSsePackuswb,           // #231  (0.000005%)
    /*168*/ OpSsePunpckhbw,          // #271  (0.000003%)
    /*169*/ OpSsePunpckhwd,          // #261  (0.000003%)
    /*16A*/ OpSsePunpckhdq,          // #260  (0.000003%)
    /*16B*/ OpSsePackssdw,           // #257  (0.000005%)
    /*16C*/ OpSsePunpcklqdq,         // #264  (0.000003%)
    /*16D*/ OpSsePunpckhqdq,         // #263  (0.000003%)
    /*16E*/ OpMov0f6e,               // #289  (0.000000%)
    /*16F*/ OpMov0f6f,               // #191  (0.000051%)
    /*170*/ OpShuffle,               // #164  (0.000131%)
    /*171*/ Op171,                   // #294  (0.000000%)
    /*172*/ Op172,                   // #293  (0.000000%)
    /*173*/ Op173,                   // #211  (0.000022%)
    /*174*/ OpSsePcmpeqb,            // #118  (0.001215%)
    /*175*/ OpSsePcmpeqw,            // #201  (0.000028%)
    /*176*/ OpSsePcmpeqd,            // #222  (0.000008%)
    /*177*/ OpEmms,                  //
    /*178*/ OpUd,                    //
    /*179*/ OpUd,                    //
    /*17A*/ OpUd,                    //
    /*17B*/ OpUd,                    //
    /*17C*/ OpHaddpsd,               //
    /*17D*/ OpHsubpsd,               //
    /*17E*/ OpMov0f7e,               // #122  (0.001005%)
    /*17F*/ OpMov0f7f,               // #192  (0.000048%)
    /*180*/ OpJcc,                   //
    /*181*/ OpJcc,                   //
    /*182*/ OpJcc,                   // #107  (0.001532%)
    /*183*/ OpJcc,                   // #72   (0.011761%)
    /*184*/ OpJcc,                   // #55   (0.029121%)
    /*185*/ OpJcc,                   // #57   (0.027593%)
    /*186*/ OpJcc,                   // #46   (0.147358%)
    /*187*/ OpJcc,                   // #86   (0.005907%)
    /*188*/ OpJcc,                   // #106  (0.001569%)
    /*189*/ OpJcc,                   // #160  (0.000142%)
    /*18A*/ OpJp,                    //
    /*18B*/ OpJnp,                   //
    /*18C*/ OpJcc,                   // #105  (0.001786%)
    /*18D*/ OpJcc,                   // #281  (0.000001%)
    /*18E*/ OpJcc,                   // #77   (0.009607%)
    /*18F*/ OpJcc,                   // #126  (0.000890%)
    /*190*/ OpSetcc,                 // #280  (0.000001%)
    /*191*/ OpSetcc,                 //
    /*192*/ OpSetcc,                 // #26   (0.364366%)
    /*193*/ OpSetcc,                 // #183  (0.000063%)
    /*194*/ OpSetcc,                 // #78   (0.009363%)
    /*195*/ OpSetcc,                 // #94   (0.003096%)
    /*196*/ OpSetcc,                 // #162  (0.000139%)
    /*197*/ OpSetcc,                 // #92   (0.003559%)
    /*198*/ OpSetcc,                 //
    /*199*/ OpSetcc,                 //
    /*19A*/ OpSetp,                  //
    /*19B*/ OpSetnp,                 //
    /*19C*/ OpSetcc,                 // #119  (0.001079%)
    /*19D*/ OpSetcc,                 // #275  (0.000002%)
    /*19E*/ OpSetcc,                 // #167  (0.000112%)
    /*19F*/ OpSetcc,                 // #95   (0.002688%)
    /*1A0*/ OpPushSeg,               //
    /*1A1*/ OpPopSeg,                //
    /*1A2*/ OpCpuid,                 // #285  (0.000001%)
    /*1A3*/ OpBit,                   //
    /*1A4*/ OpDoubleShift,           //
    /*1A5*/ OpDoubleShift,           //
    /*1A6*/ OpUd,                    //
    /*1A7*/ OpUd,                    //
    /*1A8*/ OpPushSeg,               //
    /*1A9*/ OpPopSeg,                //
    /*1AA*/ OpUd,                    //
    /*1AB*/ OpBit,                   // #291  (0.000000%)
    /*1AC*/ OpDoubleShift,           //
    /*1AD*/ OpDoubleShift,           //
    /*1AE*/ Op1ae,                   // #287  (0.000000%)
    /*1AF*/ OpImulGvqpEvqp,          // #34   (0.299503%)
    /*1B0*/ OpCmpxchgEbAlGb,         //
    /*1B1*/ OpCmpxchgEvqpRaxGvqp,    // #87   (0.005376%)
    /*1B2*/ OpUd,                    //
    /*1B3*/ OpBit,                   // #199  (0.000035%)
    /*1B4*/ OpUd,                    //
    /*1B5*/ OpUd,                    //
    /*1B6*/ OpMovzbGvqpEb,           // #37   (0.296523%)
    /*1B7*/ OpMovzwGvqpEw,           // #137  (0.000433%)
    /*1B8*/ Op1b8,                   //
    /*1B9*/ OpUd,                    //
    /*1BA*/ OpBit,                   // #127  (0.000879%)
    /*1BB*/ OpBit,                   //
    /*1BC*/ OpBsf,                   // #88   (0.005117%)
    /*1BD*/ OpBsr,                   // #123  (0.000985%)
    /*1BE*/ OpMovsbGvqpEb,           // #52   (0.035351%)
    /*1BF*/ OpMovswGvqpEw,           // #63   (0.015753%)
    /*1C0*/ OpXaddEbGb,              //
    /*1C1*/ OpXaddEvqpGvqp,          //
    /*1C2*/ OpCmppsd,                //
    /*1C3*/ OpMovntiMdqpGdqp,        //
    /*1C4*/ OpPinsrwVdqEwIb,         // #124  (0.000981%)
    /*1C5*/ OpPextrwGdqpUdqIb,       // #277  (0.000002%)
    /*1C6*/ OpShufpsd,               //
    /*1C7*/ Op1c7,                   // #189  (0.000054%)
    /*1C8*/ OpBswapZvqp,             // #159  (0.000145%)
    /*1C9*/ OpBswapZvqp,             // #182  (0.000069%)
    /*1CA*/ OpBswapZvqp,             // #197  (0.000039%)
    /*1CB*/ OpBswapZvqp,             // #217  (0.000012%)
    /*1CC*/ OpBswapZvqp,             //
    /*1CD*/ OpBswapZvqp,             //
    /*1CE*/ OpBswapZvqp,             // #129  (0.000863%)
    /*1CF*/ OpBswapZvqp,             // #128  (0.000863%)
    /*1D0*/ OpAddsubpsd,             //
    /*1D1*/ OpSsePsrlwv,             // #256  (0.000005%)
    /*1D2*/ OpSsePsrldv,             // #255  (0.000005%)
    /*1D3*/ OpSsePsrlqv,             // #254  (0.000005%)
    /*1D4*/ OpSsePaddq,              // #253  (0.000005%)
    /*1D5*/ OpSsePmullw,             // #188  (0.000054%)
    /*1D6*/ OpMov0fD6,               // #169  (0.000112%)
    /*1D7*/ OpPmovmskbGdqpNqUdq,     // #117  (0.001235%)
    /*1D8*/ OpSsePsubusb,            // #252  (0.000005%)
    /*1D9*/ OpSsePsubusw,            // #251  (0.000005%)
    /*1DA*/ OpSsePminub,             // #250  (0.000005%)
    /*1DB*/ OpSsePand,               // #249  (0.000005%)
    /*1DC*/ OpSsePaddusb,            // #248  (0.000005%)
    /*1DD*/ OpSsePaddusw,            // #247  (0.000005%)
    /*1DE*/ OpSsePmaxub,             // #246  (0.000005%)
    /*1DF*/ OpSsePandn,              // #245  (0.000005%)
    /*1E0*/ OpSsePavgb,              // #244  (0.000005%)
    /*1E1*/ OpSsePsrawv,             // #230  (0.000005%)
    /*1E2*/ OpSsePsradv,             // #224  (0.000008%)
    /*1E3*/ OpSsePavgw,              // #243  (0.000005%)
    /*1E4*/ OpSsePmulhuw,            // #229  (0.000005%)
    /*1E5*/ OpSsePmulhw,             // #187  (0.000054%)
    /*1E6*/ OpCvt0fE6,               //
    /*1E7*/ OpMov0fE7,               //
    /*1E8*/ OpSsePsubsb,             // #242  (0.000005%)
    /*1E9*/ OpSsePsubsw,             // #241  (0.000005%)
    /*1EA*/ OpSsePminsw,             // #240  (0.000005%)
    /*1EB*/ OpSsePor,                // #239  (0.000005%)
    /*1EC*/ OpSsePaddsb,             // #238  (0.000005%)
    /*1ED*/ OpSsePaddsw,             // #228  (0.000006%)
    /*1EE*/ OpSsePmaxsw,             // #237  (0.000005%)
    /*1EF*/ OpSsePxor,               // #195  (0.000044%)
    /*1F0*/ OpLddquVdqMdq,           //
    /*1F1*/ OpSsePsllwv,             // #226  (0.000006%)
    /*1F2*/ OpSsePslldv,             // #225  (0.000006%)
    /*1F3*/ OpSsePsllqv,             // #236  (0.000005%)
    /*1F4*/ OpSsePmuludq,            // #186  (0.000054%)
    /*1F5*/ OpSsePmaddwd,            // #185  (0.000054%)
    /*1F6*/ OpSsePsadbw,             // #270  (0.000003%)
    /*1F7*/ OpMaskMovDiXmmRegXmmRm,  //
    /*1F8*/ OpSsePsubb,              // #235  (0.000005%)
    /*1F9*/ OpSsePsubw,              // #234  (0.000005%)
    /*1FA*/ OpSsePsubd,              // #184  (0.000054%)
    /*1FB*/ OpSsePsubq,              // #269  (0.000003%)
    /*1FC*/ OpSsePaddb,              // #233  (0.000005%)
    /*1FD*/ OpSsePaddw,              // #227  (0.000006%)
    /*1FE*/ OpSsePaddd,              // #232  (0.000005%)
    /*1FF*/ OpUd,                    //
    /*200*/ OpSsePshufb,             // #268  (0.000003%)
    /*201*/ OpSsePhaddw,             // #204  (0.000027%)
    /*202*/ OpSsePhaddd,             // #210  (0.000027%)
    /*203*/ OpSsePhaddsw,            // #203  (0.000027%)
    /*204*/ OpSsePmaddubsw,          // #209  (0.000027%)
    /*205*/ OpSsePhsubw,             // #208  (0.000027%)
    /*206*/ OpSsePhsubd,             // #207  (0.000027%)
    /*207*/ OpSsePhsubsw,            // #206  (0.000027%)
    /*208*/ OpSsePsignb,             // #267  (0.000003%)
    /*209*/ OpSsePsignw,             // #266  (0.000003%)
    /*20A*/ OpSsePsignd,             // #265  (0.000003%)
    /*20B*/ OpSsePmulhrsw,           // #205  (0.000027%)
};

nexgen32e_f GetOp(long op) {
  if (op < ARRAYLEN(kNexgen32e)) {
    return kNexgen32e[op];
  } else {
    switch (op) {
      XLAT(0x21c, OpSsePabsb);
      XLAT(0x21d, OpSsePabsw);
      XLAT(0x21e, OpSsePabsd);
      XLAT(0x22a, OpMovntdqaVdqMdq);
      XLAT(0x240, OpSsePmulld);
      XLAT(0x2f0, Op2f01);
      XLAT(0x2f1, Op2f01);
      XLAT(0x2f5, Op2f5);
      XLAT(0x2f6, Op2f6);
      XLAT(0x2f7, OpShx);
      XLAT(0x30f, OpSsePalignr);
      XLAT(0x344, OpSsePclmulqdq);
      XLAT(0x3f0, OpRorx);
      default:
        return OpUd;
    }
  }
}

static bool CanJit(struct Machine *m) {
  return !IsJitDisabled(&m->system->jit);
}

void JitlessDispatch(P) {
  ASM_LOGF("decoding [%s] at address %" PRIx64, DescribeOp(m, GetPc(m)),
           GetPc(m));
  COSTLY_STATISTIC(++instructions_dispatched);
  LoadInstruction(m, GetPc(m));
  rde = m->xedd->op.rde;
  disp = m->xedd->op.disp;
  uimm0 = m->xedd->op.uimm0;
  m->oplen = Oplength(rde);
  m->ip += Oplength(rde);
  GetOp(Mopcode(rde))(A);
  if (m->stashaddr) CommitStash(m);
  m->oplen = 0;
}

static void GeneralDispatch(P) {
#ifdef HAVE_JIT
  int opclass;
  uintptr_t jitpc = 0;
  bool op_overlaps_page_boundary;
  bool path_would_overlap_page_boundary;
  ASM_LOGF("decoding [%s] at address %" PRIx64, DescribeOp(m, GetPc(m)),
           GetPc(m));
  LoadInstruction(m, GetPc(m));
  rde = m->xedd->op.rde;
  disp = m->xedd->op.disp;
  uimm0 = m->xedd->op.uimm0;
  opclass = ClassifyOp(rde);
  // try to fast-track precious ops, since they hit this every time
  // each jit path should be fully contained within a single page
  op_overlaps_page_boundary =
      (m->ip & -4096) != ((m->ip + Oplength(rde) - 1) & -4096);
  path_would_overlap_page_boundary =
      IsMakingPath(m) && (m->ip & -4096) != (m->path.start & -4096);
  if (IsMakingPath(m) &&
      (opclass == kOpPrecious || opclass == kOpSerializing ||
       op_overlaps_page_boundary || path_would_overlap_page_boundary)) {
    // complete path where last instruction in path is previously run op
    CompletePath(A);
  }
  // if we're in a jit path, or we're able to create a new path
  if (IsMakingPath(m) ||
      (opclass != kOpPrecious && opclass != kOpSerializing &&
       !op_overlaps_page_boundary && CanJit(m) && CreatePath(A))) {
    // begin adding this op to the jit path
    unassert(opclass == kOpNormal || opclass == kOpBranching);
    ++m->path.elements;
    STATISTIC(++path_elements);
    AddPath_StartOp(A);
    jitpc = GetJitPc(m->path.jb);
    JIP_LOGF("adding [%s] from address %" PRIx64
             " to path starting at %" PRIx64,
             DescribeOp(m, GetPc(m)), GetPc(m), m->path.start);
  }
  // advance the instruction pointer
  // record the op length so it can be rewound upon fault
  m->oplen = Oplength(rde);
  m->ip += Oplength(rde);
  // call the c implementation of the opcode
  GetOp(Mopcode(rde))(A);
  // cleanup after ReserveAddress() if a memory access overlapped a page
  if (m->stashaddr) {
    CommitStash(m);
  }
  if (IsMakingPath(m)) {
    // finish adding new element to jit path
    unassert(opclass == kOpNormal || opclass == kOpBranching);
    // did the op generate its own assembly code?
    if (GetJitPc(m->path.jb) != jitpc) {
      // it did; that means we're done
      AddPath_EndOp(A);
    } else {
      // otherwise generate "one size fits all" assembly code
      AddPath(A);
      AddPath_EndOp(A);
      STATISTIC(++path_elements_auto);
    }
    if (opclass == kOpBranching) {
      // branches, calls, and jumps always force end of path
      // unlike precious ops the branching op can be in path
      CompletePath(A);
    }
  }
  m->oplen = 0;
#endif
}

void ExecuteInstruction(struct Machine *m) {
#if LOG_CPU
  LogCpu(m);
#endif
#ifdef HAVE_JIT
  u8 *dst;
  nexgen32e_f func;
  unassert(m->canhalt);
  if (CanJit(m)) {
    if ((func = (nexgen32e_f)GetJitHook(&m->system->jit, m->ip))) {
      if (!IsMakingPath(m)) {
        func(DISPATCH_NOTHING);
        return;
      } else if (func == JitlessDispatch) {
        JIT_LOGF("abandoning path starting at %" PRIx64
                 " due to running into staged path",
                 m->path.start);
        AbandonPath(m);
        func(DISPATCH_NOTHING);
        return;
      } else {
        JIT_LOGF("splicing path starting at %#" PRIx64
                 " into previously created function %p at %#" PRIx64,
                 m->path.start, func, m->ip);
        FlushSkew(DISPATCH_NOTHING);
        AppendJitSetReg(m->path.jb, kJitArg0, kJitSav0);
        STATISTIC(++path_spliced);
        if (RecordJitEdge(&m->system->jit, m->path.start, m->ip)) {
          dst = (u8 *)(uintptr_t)func + GetPrologueSize();
          STATISTIC(++path_connected_directly);
        } else {
          STATISTIC(++path_connected_interpreter);
          dst = (u8 *)m->system->ender;
        }
        AppendJitJump(m->path.jb, dst);
        FinishPath(m);
        func(DISPATCH_NOTHING);
        return;
      }
    }
    GeneralDispatch(DISPATCH_NOTHING);
  } else {
    JitlessDispatch(DISPATCH_NOTHING);
  }
#else
  JitlessDispatch(DISPATCH_NOTHING);
#endif
}

void Actor(struct Machine *mm) {
#ifdef __CYGWIN__
  // TODO: Why does JIT clobber %rbx on Cygwin?
  struct Machine *volatile m;
#else
  struct Machine *m;
#endif
  for (g_machine = mm, m = mm;;) {
#ifndef __CYGWIN__
    STATISTIC(++interps);
#endif
    JIX_LOGF("INTERPRETER");
    if (!atomic_load_explicit(&m->attention, memory_order_acquire)) {
      ExecuteInstruction(m);
    } else {
      CheckForSignals(m);
    }
  }
}

void Blink(struct Machine *m) {
  int rc;
  for (;;) {
    if (!(rc = sigsetjmp(m->onhalt, 1))) {
      m->canhalt = true;
      Actor(m);
    }
    m->sysdepth = 0;
    m->sigdepth = 0;
    m->canhalt = false;
    m->nofault = false;
    m->insyscall = false;
    CollectPageLocks(m);
    CollectGarbage(m, 0);
    if (IsMakingPath(m)) {
      AbandonPath(m);
    }
    if (rc == kMachineFatalSystemSignal) {
      HandleFatalSystemSignal(m, &g_siginfo);
    }
  }
}

void HandleFatalSystemSignal(struct Machine *m, const siginfo_t *si) {
  int sig;
  RestoreIp(m);
  m->faultaddr = ConvertHostToGuestAddress(m->system, si->si_addr, 0);
  sig = UnXlatSignal(si->si_signo);
  DeliverSignalToUser(m, sig, UnXlatSiCode(sig, si->si_code));
}
