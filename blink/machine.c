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
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blink/address.h"
#include "blink/alu.h"
#include "blink/assert.h"
#include "blink/bitscan.h"
#include "blink/case.h"
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/fpu.h"
#include "blink/likely.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/memory.h"
#include "blink/modrm.h"
#include "blink/mop.h"
#include "blink/random.h"
#include "blink/real.h"
#include "blink/signal.h"
#include "blink/sse.h"
#include "blink/ssefloat.h"
#include "blink/ssemov.h"
#include "blink/stats.h"
#include "blink/string.h"
#include "blink/swap.h"
#include "blink/syscall.h"
#include "blink/time.h"
#include "blink/util.h"

_Thread_local struct Machine *g_machine;

static void OpNoop(struct Machine *m, DISPATCH_PARAMETERS) {
}

static void OpHintNopEv(struct Machine *m, DISPATCH_PARAMETERS) {
}

static void OpCmc(struct Machine *m, DISPATCH_PARAMETERS) {
  m->flags = SetFlag(m->flags, FLAGS_CF, !GetFlag(m->flags, FLAGS_CF));
}

static void OpClc(struct Machine *m, DISPATCH_PARAMETERS) {
  m->flags = SetFlag(m->flags, FLAGS_CF, false);
}

static void OpStc(struct Machine *m, DISPATCH_PARAMETERS) {
  m->flags = SetFlag(m->flags, FLAGS_CF, true);
}

static void OpCli(struct Machine *m, DISPATCH_PARAMETERS) {
  m->flags = SetFlag(m->flags, FLAGS_IF, false);
}

static void OpSti(struct Machine *m, DISPATCH_PARAMETERS) {
  m->flags = SetFlag(m->flags, FLAGS_IF, true);
}

static void OpCld(struct Machine *m, DISPATCH_PARAMETERS) {
  m->flags = SetFlag(m->flags, FLAGS_DF, false);
}

static void OpStd(struct Machine *m, DISPATCH_PARAMETERS) {
  m->flags = SetFlag(m->flags, FLAGS_DF, true);
}

static void OpPushf(struct Machine *m, DISPATCH_PARAMETERS) {
  Push(m, DISPATCH_ARGUMENTS, ExportFlags(m->flags) & 0xFCFFFF);
}

static void OpPopf(struct Machine *m, DISPATCH_PARAMETERS) {
  if (!Osz(rde)) {
    ImportFlags(m, Pop(m, DISPATCH_ARGUMENTS, 0));
  } else {
    ImportFlags(m, (m->flags & ~0xffff) | Pop(m, DISPATCH_ARGUMENTS, 0));
  }
}

static void OpLahf(struct Machine *m, DISPATCH_PARAMETERS) {
  Put8(m->ax + 1, ExportFlags(m->flags));
}

static void OpSahf(struct Machine *m, DISPATCH_PARAMETERS) {
  ImportFlags(m, (m->flags & ~0xff) | m->ax[1]);
}

static void OpLeaGvqpM(struct Machine *m, DISPATCH_PARAMETERS) {
  WriteRegister(rde, RegRexrReg(m, rde),
                LoadEffectiveAddress(m, DISPATCH_ARGUMENTS).addr);
}

static void OpPushSeg(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 seg = (Opcode(rde) & 070) >> 3;
  Push(m, DISPATCH_ARGUMENTS, *GetSegment(m, DISPATCH_ARGUMENTS, seg) >> 4);
}

static void OpPopSeg(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 seg = (Opcode(rde) & 070) >> 3;
  *GetSegment(m, DISPATCH_ARGUMENTS, seg) = Pop(m, DISPATCH_ARGUMENTS, 0) << 4;
}

static void OpMovEvqpSw(struct Machine *m, DISPATCH_PARAMETERS) {
  WriteRegisterOrMemory(
      rde, GetModrmRegisterWordPointerWriteOszRexw(m, DISPATCH_ARGUMENTS),
      *GetSegment(m, DISPATCH_ARGUMENTS, ModrmReg(rde)) >> 4);
}

static int GetDescriptor(struct Machine *m, int selector, u64 *out_descriptor) {
  unassert(m->system->gdt_base + m->system->gdt_limit <=
           GetRealMemorySize(m->system));
  selector &= -8;
  if (8 <= selector && selector + 8 <= m->system->gdt_limit) {
    SetReadAddr(m, m->system->gdt_base + selector, 8);
    *out_descriptor =
        Load64(m->system->real.p + m->system->gdt_base + selector);
    return 0;
  } else {
    return -1;
  }
}

static u64 GetDescriptorBase(u64 d) {
  return (d & 0xff00000000000000) >> 32 | (d & 0x000000ffffff0000) >> 16;
}

static u64 GetDescriptorLimit(u64 d) {
  return (d & 0x000f000000000000) >> 32 | (d & 0xffff);
}

static int GetDescriptorMode(u64 d) {
  u8 kMode[] = {XED_MODE_REAL, XED_MODE_LONG, XED_MODE_LEGACY, XED_MODE_LONG};
  return kMode[(d & 0x0060000000000000) >> 53];
}

static bool IsProtectedMode(struct Machine *m) {
  return m->system->cr0 & 1;
}

static void OpMovSwEvqp(struct Machine *m, DISPATCH_PARAMETERS) {
  u64 x, d;
  x = ReadMemory(rde,
                 GetModrmRegisterWordPointerReadOszRexw(m, DISPATCH_ARGUMENTS));
  if (!IsProtectedMode(m)) {
    x <<= 4;
  } else if (GetDescriptor(m, x, &d) != -1) {
    x = GetDescriptorBase(d);
  } else {
    ThrowProtectionFault(m);
  }
  *GetSegment(m, DISPATCH_ARGUMENTS, ModrmReg(rde)) = x;
}

static void OpLsl(struct Machine *m, DISPATCH_PARAMETERS) {
  u64 descriptor;
  if (GetDescriptor(
          m, Load16(GetModrmRegisterWordPointerRead2(m, DISPATCH_ARGUMENTS)),
          &descriptor) != -1) {
    WriteRegister(rde, RegRexrReg(m, rde), GetDescriptorLimit(descriptor));
    m->flags = SetFlag(m->flags, FLAGS_ZF, true);
  } else {
    m->flags = SetFlag(m->flags, FLAGS_ZF, false);
  }
}

static void ChangeMachineMode(struct Machine *m, int mode) {
  if (mode == m->mode) return;
  ResetInstructionCache(m);
  m->mode = mode;
}

static void OpJmpf(struct Machine *m, DISPATCH_PARAMETERS) {
  u64 descriptor;
  if (!IsProtectedMode(m)) {
    m->cs = uimm0 << 4;
    m->ip = disp;
  } else if (GetDescriptor(m, uimm0, &descriptor) != -1) {
    m->cs = GetDescriptorBase(descriptor);
    m->ip = disp;
    ChangeMachineMode(m, GetDescriptorMode(descriptor));
  } else {
    ThrowProtectionFault(m);
  }
  if (m->system->onlongbranch) {
    m->system->onlongbranch(m);
  }
}

static void OpXlatAlBbb(struct Machine *m, DISPATCH_PARAMETERS) {
  u64 v;
  v = MaskAddress(Eamode(rde), Get64(m->bx) + Get8(m->ax));
  v = DataSegment(m, DISPATCH_ARGUMENTS, v);
  SetReadAddr(m, v, 1);
  m->al = Load8(ResolveAddress(m, v));
}

static void PutEaxAx(struct Machine *m, DISPATCH_PARAMETERS, u32 x) {
  if (!Osz(rde)) {
    Put64(m->ax, x);
  } else {
    Put16(m->ax, x);
  }
}

static u32 GetEaxAx(struct Machine *m, DISPATCH_PARAMETERS) {
  if (!Osz(rde)) {
    return Get32(m->ax);
  } else {
    return Get16(m->ax);
  }
}

static void OpInAlImm(struct Machine *m, DISPATCH_PARAMETERS) {
  Put8(m->ax, OpIn(m, uimm0));
}

static void OpInAxImm(struct Machine *m, DISPATCH_PARAMETERS) {
  PutEaxAx(m, DISPATCH_ARGUMENTS, OpIn(m, uimm0));
}

static void OpInAlDx(struct Machine *m, DISPATCH_PARAMETERS) {
  Put8(m->ax, OpIn(m, Get16(m->dx)));
}

static void OpInAxDx(struct Machine *m, DISPATCH_PARAMETERS) {
  PutEaxAx(m, DISPATCH_ARGUMENTS, OpIn(m, Get16(m->dx)));
}

static void OpOutImmAl(struct Machine *m, DISPATCH_PARAMETERS) {
  OpOut(m, uimm0, Get8(m->ax));
}

static void OpOutImmAx(struct Machine *m, DISPATCH_PARAMETERS) {
  OpOut(m, uimm0, GetEaxAx(m, DISPATCH_ARGUMENTS));
}

static void OpOutDxAl(struct Machine *m, DISPATCH_PARAMETERS) {
  OpOut(m, Get16(m->dx), Get8(m->ax));
}

static void OpOutDxAx(struct Machine *m, DISPATCH_PARAMETERS) {
  OpOut(m, Get16(m->dx), GetEaxAx(m, DISPATCH_ARGUMENTS));
}

static void OpXchgZvqp(struct Machine *m, DISPATCH_PARAMETERS) {
  u64 x, y;
  x = Get64(m->ax);
  y = Get64(RegRexbSrm(m, rde));
  WriteRegister(rde, m->ax, y);
  WriteRegister(rde, RegRexbSrm(m, rde), x);
}

static void Op1c7(struct Machine *m, DISPATCH_PARAMETERS) {
  bool ismem;
  ismem = !IsModrmRegister(rde);
  switch (ModrmReg(rde)) {
    case 6:
      if (!ismem) {
        OpRdrand(m, DISPATCH_ARGUMENTS);
      } else {
        OpUdImpl(m);
      }
      break;
    case 7:
      if (!ismem) {
        if (Rep(rde) == 3) {
          OpRdpid(m, DISPATCH_ARGUMENTS);
        } else {
          OpRdseed(m, DISPATCH_ARGUMENTS);
        }
      } else {
        OpUdImpl(m);
      }
      break;
    default:
      OpUdImpl(m);
  }
}

static u64 Bts(u64 x, u64 y) {
  return x | y;
}

static u64 Btr(u64 x, u64 y) {
  return x & ~y;
}

static u64 Btc(u64 x, u64 y) {
  return (x & ~y) | (~x & y);
}

static void OpBit(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 *p;
  int op;
  i64 bitdisp;
  unsigned bit;
  u64 v, x, y, z;
  u8 w, W[2][2] = {{2, 3}, {1, 3}};
  unassert(!Lock(rde));
  w = W[Osz(rde)][Rexw(rde)];
  if (Opcode(rde) == 0xBA) {
    op = ModrmReg(rde);
    bit = uimm0 & ((8 << w) - 1);
    bitdisp = 0;
  } else {
    op = (Opcode(rde) & 070) >> 3;
    bitdisp = ReadRegisterSigned(rde, RegRexrReg(m, rde));
    bit = bitdisp & ((8 << w) - 1);
    bitdisp &= -(8 << w);
    bitdisp >>= 3;
  }
  if (IsModrmRegister(rde)) {
    p = RegRexbRm(m, rde);
  } else {
    v = MaskAddress(Eamode(rde),
                    ComputeAddress(m, DISPATCH_ARGUMENTS) + bitdisp);
    p = ReserveAddress(m, v, 1 << w);
    if (op == 4) {
      SetReadAddr(m, v, 1 << w);
    } else {
      SetWriteAddr(m, v, 1 << w);
    }
  }
  y = 1;
  y <<= bit;
  x = ReadMemory(rde, p);
  m->flags = SetFlag(m->flags, FLAGS_CF, !!(y & x));
  switch (op) {
    case 4:
      return;
    case 5:
      z = Bts(x, y);
      break;
    case 6:
      z = Btr(x, y);
      break;
    case 7:
      z = Btc(x, y);
      break;
    default:
      OpUdImpl(m);
  }
  WriteRegisterOrMemory(rde, p, z);
}

static void OpSax(struct Machine *m, DISPATCH_PARAMETERS) {
  if (Rexw(rde)) {
    Put64(m->ax, (i32)Get32(m->ax));
  } else if (!Osz(rde)) {
    Put64(m->ax, (u32)(i16)Get16(m->ax));
  } else {
    Put16(m->ax, (i8)Get8(m->ax));
  }
}

static void OpConvert(struct Machine *m, DISPATCH_PARAMETERS) {
  if (Rexw(rde)) {
    Put64(m->dx, Get64(m->ax) & 0x8000000000000000 ? 0xffffffffffffffff : 0);
  } else if (!Osz(rde)) {
    Put64(m->dx, Get32(m->ax) & 0x80000000 ? 0xffffffff : 0);
  } else {
    Put16(m->dx, Get16(m->ax) & 0x8000 ? 0xffff : 0);
  }
}

static void OpBswapZvqp(struct Machine *m, DISPATCH_PARAMETERS) {
  u64 x = Get64(RegRexbSrm(m, rde));
  if (Rexw(rde)) {
    Put64(RegRexbSrm(m, rde), SWAP64(x));
  } else if (!Osz(rde)) {
    Put64(RegRexbSrm(m, rde), SWAP32(x));
  } else {
    Put16(RegRexbSrm(m, rde), SWAP16(x));
  }
}

static void OpMovEbIb(struct Machine *m, DISPATCH_PARAMETERS) {
  Store8(GetModrmRegisterBytePointerWrite(m, DISPATCH_ARGUMENTS), uimm0);
}

static void OpMovAlOb(struct Machine *m, DISPATCH_PARAMETERS) {
  i64 addr;
  addr = AddressOb(m, DISPATCH_ARGUMENTS);
  SetWriteAddr(m, addr, 1);
  Put8(m->ax, Load8(ResolveAddress(m, addr)));
}

static void OpMovObAl(struct Machine *m, DISPATCH_PARAMETERS) {
  i64 addr;
  addr = AddressOb(m, DISPATCH_ARGUMENTS);
  SetReadAddr(m, addr, 1);
  Store8(ResolveAddress(m, addr), Get8(m->ax));
}

static void OpMovRaxOvqp(struct Machine *m, DISPATCH_PARAMETERS) {
  u64 v;
  v = DataSegment(m, DISPATCH_ARGUMENTS, disp);
  SetReadAddr(m, v, 1 << RegLog2(rde));
  WriteRegister(rde, m->ax, ReadMemory(rde, ResolveAddress(m, v)));
}

static void OpMovOvqpRax(struct Machine *m, DISPATCH_PARAMETERS) {
  u64 v = DataSegment(m, DISPATCH_ARGUMENTS, disp);
  SetWriteAddr(m, v, 1 << RegLog2(rde));
  WriteMemory(rde, ResolveAddress(m, v), Get64(m->ax));
}

static void OpMovEbGb(struct Machine *m, DISPATCH_PARAMETERS) {
  Store8(GetModrmRegisterBytePointerWrite(m, DISPATCH_ARGUMENTS),
         Get8(ByteRexrReg(m, rde)));
}

static void OpMovGbEb(struct Machine *m, DISPATCH_PARAMETERS) {
  Put8(ByteRexrReg(m, rde),
       Load8(GetModrmRegisterBytePointerRead(m, DISPATCH_ARGUMENTS)));
}

static void OpMovZbIb(struct Machine *m, DISPATCH_PARAMETERS) {
  Put8(ByteRexbSrm(m, rde), uimm0);
}

static void OpMovZvqpIvqp(struct Machine *m, DISPATCH_PARAMETERS) {
  WriteRegister(rde, RegRexbSrm(m, rde), uimm0);
}

static void OpIncZv(struct Machine *m, DISPATCH_PARAMETERS) {
  if (!Osz(rde)) {
    Put32(RegSrm(m, rde), Inc32(Get32(RegSrm(m, rde)), 0, &m->flags));
  } else {
    Put16(RegSrm(m, rde), Inc16(Get16(RegSrm(m, rde)), 0, &m->flags));
  }
}

static void OpDecZv(struct Machine *m, DISPATCH_PARAMETERS) {
  if (!Osz(rde)) {
    Put32(RegSrm(m, rde), Dec32(Get32(RegSrm(m, rde)), 0, &m->flags));
  } else {
    Put16(RegSrm(m, rde), Dec16(Get16(RegSrm(m, rde)), 0, &m->flags));
  }
}

static void OpMovEvqpIvds(struct Machine *m, DISPATCH_PARAMETERS) {
  WriteRegisterOrMemory(
      rde, GetModrmRegisterWordPointerWriteOszRexw(m, DISPATCH_ARGUMENTS),
      uimm0);
}

static void OpMovEvqpGvqp(struct Machine *m, DISPATCH_PARAMETERS) {
  WriteRegisterOrMemory(
      rde, GetModrmRegisterWordPointerWriteOszRexw(m, DISPATCH_ARGUMENTS),
      ReadMemory(rde, RegRexrReg(m, rde)));
}

static void OpMovzbGvqpEb(struct Machine *m, DISPATCH_PARAMETERS) {
  WriteRegister(rde, RegRexrReg(m, rde),
                Load8(GetModrmRegisterBytePointerRead(m, DISPATCH_ARGUMENTS)));
}

static void OpMovzwGvqpEw(struct Machine *m, DISPATCH_PARAMETERS) {
  WriteRegister(
      rde, RegRexrReg(m, rde),
      Load16(GetModrmRegisterWordPointerRead2(m, DISPATCH_ARGUMENTS)));
}

static void OpMovsbGvqpEb(struct Machine *m, DISPATCH_PARAMETERS) {
  WriteRegister(
      rde, RegRexrReg(m, rde),
      (i8)Load8(GetModrmRegisterBytePointerRead(m, DISPATCH_ARGUMENTS)));
}

static void OpMovswGvqpEw(struct Machine *m, DISPATCH_PARAMETERS) {
  WriteRegister(
      rde, RegRexrReg(m, rde),
      (i16)Load16(GetModrmRegisterWordPointerRead2(m, DISPATCH_ARGUMENTS)));
}

static void OpMovsxdGdqpEd(struct Machine *m, DISPATCH_PARAMETERS) {
  Put64(RegRexrReg(m, rde),
        (i32)Load32(GetModrmRegisterWordPointerRead4(m, DISPATCH_ARGUMENTS)));
}

static void AlubRo(struct Machine *m, DISPATCH_PARAMETERS, aluop_f op) {
  op(Load8(GetModrmRegisterBytePointerRead(m, DISPATCH_ARGUMENTS)),
     Get8(ByteRexrReg(m, rde)), &m->flags);
}

static void OpAlubCmp(struct Machine *m, DISPATCH_PARAMETERS) {
  AlubRo(m, DISPATCH_ARGUMENTS, Sub8);
}

static void OpAlubTest(struct Machine *m, DISPATCH_PARAMETERS) {
  AlubRo(m, DISPATCH_ARGUMENTS, And8);
}

static void AlubFlip(struct Machine *m, DISPATCH_PARAMETERS, aluop_f op) {
  Put8(ByteRexrReg(m, rde),
       op(Get8(ByteRexrReg(m, rde)),
          Load8(GetModrmRegisterBytePointerRead(m, DISPATCH_ARGUMENTS)),
          &m->flags));
}

static void OpAlubFlipAdd(struct Machine *m, DISPATCH_PARAMETERS) {
  AlubFlip(m, DISPATCH_ARGUMENTS, Add8);
}

static void OpAlubFlipOr(struct Machine *m, DISPATCH_PARAMETERS) {
  AlubFlip(m, DISPATCH_ARGUMENTS, Or8);
}

static void OpAlubFlipAdc(struct Machine *m, DISPATCH_PARAMETERS) {
  AlubFlip(m, DISPATCH_ARGUMENTS, Adc8);
}

static void OpAlubFlipSbb(struct Machine *m, DISPATCH_PARAMETERS) {
  AlubFlip(m, DISPATCH_ARGUMENTS, Sbb8);
}

static void OpAlubFlipAnd(struct Machine *m, DISPATCH_PARAMETERS) {
  AlubFlip(m, DISPATCH_ARGUMENTS, And8);
}

static void OpAlubFlipSub(struct Machine *m, DISPATCH_PARAMETERS) {
  AlubFlip(m, DISPATCH_ARGUMENTS, Sub8);
}

static void OpAlubFlipXor(struct Machine *m, DISPATCH_PARAMETERS) {
  AlubFlip(m, DISPATCH_ARGUMENTS, Xor8);
}

static void AlubFlipRo(struct Machine *m, DISPATCH_PARAMETERS, aluop_f op) {
  op(Get8(ByteRexrReg(m, rde)),
     Load8(GetModrmRegisterBytePointerRead(m, DISPATCH_ARGUMENTS)), &m->flags);
}

static void OpAlubFlipCmp(struct Machine *m, DISPATCH_PARAMETERS) {
  AlubFlipRo(m, DISPATCH_ARGUMENTS, Sub8);
}

static void Alubi(struct Machine *m, DISPATCH_PARAMETERS, aluop_f op) {
  u8 *a = GetModrmRegisterBytePointerWrite(m, DISPATCH_ARGUMENTS);
  Store8(a, op(Load8(a), uimm0, &m->flags));
}

static void AlubiRo(struct Machine *m, DISPATCH_PARAMETERS, aluop_f op) {
  op(Load8(GetModrmRegisterBytePointerRead(m, DISPATCH_ARGUMENTS)), uimm0,
     &m->flags);
}

static void OpAlubiTest(struct Machine *m, DISPATCH_PARAMETERS) {
  AlubiRo(m, DISPATCH_ARGUMENTS, And8);
}

static void OpAlubiReg(struct Machine *m, DISPATCH_PARAMETERS) {
  if (ModrmReg(rde) == ALU_CMP) {
    AlubiRo(m, DISPATCH_ARGUMENTS, kAlu[ModrmReg(rde)][0]);
  } else {
    Alubi(m, DISPATCH_ARGUMENTS, kAlu[ModrmReg(rde)][0]);
  }
}

static void AluwRo(struct Machine *m, DISPATCH_PARAMETERS,
                   const aluop_f ops[4]) {
  ops[RegLog2(rde)](ReadMemory(rde, GetModrmRegisterWordPointerReadOszRexw(
                                        m, DISPATCH_ARGUMENTS)),
                    Get64(RegRexrReg(m, rde)), &m->flags);
}

static void OpAluwCmp(struct Machine *m, DISPATCH_PARAMETERS) {
  AluwRo(m, DISPATCH_ARGUMENTS, kAlu[ALU_SUB]);
}

static void OpAluwTest(struct Machine *m, DISPATCH_PARAMETERS) {
  AluwRo(m, DISPATCH_ARGUMENTS, kAlu[ALU_AND]);
}

static void OpAluwFlip(struct Machine *m, DISPATCH_PARAMETERS) {
  WriteRegister(rde, RegRexrReg(m, rde),
                kAlu[(Opcode(rde) & 070) >> 3][RegLog2(rde)](
                    Get64(RegRexrReg(m, rde)),
                    ReadMemory(rde, GetModrmRegisterWordPointerReadOszRexw(
                                        m, DISPATCH_ARGUMENTS)),
                    &m->flags));
}

static void AluwFlipRo(struct Machine *m, DISPATCH_PARAMETERS,
                       const aluop_f ops[4]) {
  ops[RegLog2(rde)](Get64(RegRexrReg(m, rde)),
                    ReadMemory(rde, GetModrmRegisterWordPointerReadOszRexw(
                                        m, DISPATCH_ARGUMENTS)),
                    &m->flags);
}

static void OpAluwFlipCmp(struct Machine *m, DISPATCH_PARAMETERS) {
  AluwFlipRo(m, DISPATCH_ARGUMENTS, kAlu[ALU_SUB]);
}

static void Aluwi(struct Machine *m, DISPATCH_PARAMETERS,
                  const aluop_f ops[4]) {
  u8 *a;
  a = GetModrmRegisterWordPointerWriteOszRexw(m, DISPATCH_ARGUMENTS);
  WriteRegisterOrMemory(
      rde, a, ops[RegLog2(rde)](ReadMemory(rde, a), uimm0, &m->flags));
}

static void AluwiRo(struct Machine *m, DISPATCH_PARAMETERS,
                    const aluop_f ops[4]) {
  ops[RegLog2(rde)](ReadMemory(rde, GetModrmRegisterWordPointerReadOszRexw(
                                        m, DISPATCH_ARGUMENTS)),
                    uimm0, &m->flags);
}

static void OpAluwiReg(struct Machine *m, DISPATCH_PARAMETERS) {
  if (ModrmReg(rde) == ALU_CMP) {
    AluwiRo(m, DISPATCH_ARGUMENTS, kAlu[ModrmReg(rde)]);
  } else {
    Aluwi(m, DISPATCH_ARGUMENTS, kAlu[ModrmReg(rde)]);
  }
}

static void AluAlIb(struct Machine *m, DISPATCH_PARAMETERS, aluop_f op) {
  Put8(m->ax, op(Get8(m->ax), uimm0, &m->flags));
}

static void OpAluAlIbAdd(struct Machine *m, DISPATCH_PARAMETERS) {
  AluAlIb(m, DISPATCH_ARGUMENTS, Add8);
}

static void OpAluAlIbOr(struct Machine *m, DISPATCH_PARAMETERS) {
  AluAlIb(m, DISPATCH_ARGUMENTS, Or8);
}

static void OpAluAlIbAdc(struct Machine *m, DISPATCH_PARAMETERS) {
  AluAlIb(m, DISPATCH_ARGUMENTS, Adc8);
}

static void OpAluAlIbSbb(struct Machine *m, DISPATCH_PARAMETERS) {
  AluAlIb(m, DISPATCH_ARGUMENTS, Sbb8);
}

static void OpAluAlIbAnd(struct Machine *m, DISPATCH_PARAMETERS) {
  AluAlIb(m, DISPATCH_ARGUMENTS, And8);
}

static void OpAluAlIbSub(struct Machine *m, DISPATCH_PARAMETERS) {
  AluAlIb(m, DISPATCH_ARGUMENTS, Sub8);
}

static void OpAluAlIbXor(struct Machine *m, DISPATCH_PARAMETERS) {
  AluAlIb(m, DISPATCH_ARGUMENTS, Xor8);
}

static void OpAluRaxIvds(struct Machine *m, DISPATCH_PARAMETERS) {
  WriteRegister(rde, m->ax,
                kAlu[(Opcode(rde) & 070) >> 3][RegLog2(rde)](
                    ReadRegister(rde, m->ax), uimm0, &m->flags));
}

static void OpCmpAlIb(struct Machine *m, DISPATCH_PARAMETERS) {
  Sub8(Get8(m->ax), uimm0, &m->flags);
}

static void OpCmpRaxIvds(struct Machine *m, DISPATCH_PARAMETERS) {
  kAlu[ALU_SUB][RegLog2(rde)](ReadRegister(rde, m->ax), uimm0, &m->flags);
}

static void OpTestAlIb(struct Machine *m, DISPATCH_PARAMETERS) {
  And8(Get8(m->ax), uimm0, &m->flags);
}

static void OpTestRaxIvds(struct Machine *m, DISPATCH_PARAMETERS) {
  kAlu[ALU_AND][RegLog2(rde)](ReadRegister(rde, m->ax), uimm0, &m->flags);
}

static void Bsuwi(struct Machine *m, DISPATCH_PARAMETERS, u64 y) {
  u8 *p;
  p = GetModrmRegisterWordPointerWriteOszRexw(m, DISPATCH_ARGUMENTS);
  WriteRegisterOrMemory(
      rde, p,
      kBsu[ModrmReg(rde)][RegLog2(rde)](ReadMemory(rde, p), y, &m->flags));
}

static void OpBsuwi1(struct Machine *m, DISPATCH_PARAMETERS) {
  Bsuwi(m, DISPATCH_ARGUMENTS, 1);
}

static void OpBsuwiCl(struct Machine *m, DISPATCH_PARAMETERS) {
  Bsuwi(m, DISPATCH_ARGUMENTS, Get8(m->cx));
}

static void OpBsuwiImm(struct Machine *m, DISPATCH_PARAMETERS) {
  Bsuwi(m, DISPATCH_ARGUMENTS, uimm0);
}

static void Bsubi(struct Machine *m, DISPATCH_PARAMETERS, u64 y) {
  u8 *a = GetModrmRegisterBytePointerWrite(m, DISPATCH_ARGUMENTS);
  Store8(a, kBsu[ModrmReg(rde)][RegLog2(rde)](Load8(a), y, &m->flags));
}

static void OpBsubi1(struct Machine *m, DISPATCH_PARAMETERS) {
  Bsubi(m, DISPATCH_ARGUMENTS, 1);
}

static void OpBsubiCl(struct Machine *m, DISPATCH_PARAMETERS) {
  Bsubi(m, DISPATCH_ARGUMENTS, Get8(m->cx));
}

static void OpBsubiImm(struct Machine *m, DISPATCH_PARAMETERS) {
  Bsubi(m, DISPATCH_ARGUMENTS, uimm0);
}

static void OpPushImm(struct Machine *m, DISPATCH_PARAMETERS) {
  Push(m, DISPATCH_ARGUMENTS, uimm0);
}

static void Interrupt(struct Machine *m, DISPATCH_PARAMETERS, int i) {
  HaltMachine(m, i);
}

static void OpInterruptImm(struct Machine *m, DISPATCH_PARAMETERS) {
  Interrupt(m, DISPATCH_ARGUMENTS, uimm0);
}

static void OpInterrupt1(struct Machine *m, DISPATCH_PARAMETERS) {
  Interrupt(m, DISPATCH_ARGUMENTS, 1);
}

static void OpInterrupt3(struct Machine *m, DISPATCH_PARAMETERS) {
  Interrupt(m, DISPATCH_ARGUMENTS, 3);
}

static void OpJmp(struct Machine *m, DISPATCH_PARAMETERS) {
  m->ip += disp;
}

static void OpJe(struct Machine *m, DISPATCH_PARAMETERS) {
  if (GetFlag(m->flags, FLAGS_ZF)) {
    OpJmp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpJne(struct Machine *m, DISPATCH_PARAMETERS) {
  if (!GetFlag(m->flags, FLAGS_ZF)) {
    OpJmp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpJb(struct Machine *m, DISPATCH_PARAMETERS) {
  if (GetFlag(m->flags, FLAGS_CF)) {
    OpJmp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpJbe(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsBelowOrEqual(m)) {
    OpJmp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpJo(struct Machine *m, DISPATCH_PARAMETERS) {
  if (GetFlag(m->flags, FLAGS_OF)) {
    OpJmp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpJno(struct Machine *m, DISPATCH_PARAMETERS) {
  if (!GetFlag(m->flags, FLAGS_OF)) {
    OpJmp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpJae(struct Machine *m, DISPATCH_PARAMETERS) {
  if (!GetFlag(m->flags, FLAGS_CF)) {
    OpJmp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpJa(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsAbove(m)) {
    OpJmp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpJs(struct Machine *m, DISPATCH_PARAMETERS) {
  if (GetFlag(m->flags, FLAGS_SF)) {
    OpJmp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpJns(struct Machine *m, DISPATCH_PARAMETERS) {
  if (!GetFlag(m->flags, FLAGS_SF)) {
    OpJmp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpJp(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsParity(m)) {
    OpJmp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpJnp(struct Machine *m, DISPATCH_PARAMETERS) {
  if (!IsParity(m)) {
    OpJmp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpJl(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsLess(m)) {
    OpJmp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpJge(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsGreaterOrEqual(m)) {
    OpJmp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpJle(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsLessOrEqual(m)) {
    OpJmp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpJg(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsGreater(m)) {
    OpJmp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpMovGvqpEvqp(struct Machine *m, DISPATCH_PARAMETERS) {
  WriteRegister(rde, RegRexrReg(m, rde),
                ReadMemory(rde, GetModrmRegisterWordPointerReadOszRexw(
                                    m, DISPATCH_ARGUMENTS)));
}

static void OpCmovo(struct Machine *m, DISPATCH_PARAMETERS) {
  if (GetFlag(m->flags, FLAGS_OF)) {
    OpMovGvqpEvqp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpCmovno(struct Machine *m, DISPATCH_PARAMETERS) {
  if (!GetFlag(m->flags, FLAGS_OF)) {
    OpMovGvqpEvqp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpCmovb(struct Machine *m, DISPATCH_PARAMETERS) {
  if (GetFlag(m->flags, FLAGS_CF)) {
    OpMovGvqpEvqp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpCmovae(struct Machine *m, DISPATCH_PARAMETERS) {
  if (!GetFlag(m->flags, FLAGS_CF)) {
    OpMovGvqpEvqp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpCmove(struct Machine *m, DISPATCH_PARAMETERS) {
  if (GetFlag(m->flags, FLAGS_ZF)) {
    OpMovGvqpEvqp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpCmovne(struct Machine *m, DISPATCH_PARAMETERS) {
  if (!GetFlag(m->flags, FLAGS_ZF)) {
    OpMovGvqpEvqp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpCmovbe(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsBelowOrEqual(m)) {
    OpMovGvqpEvqp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpCmova(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsAbove(m)) {
    OpMovGvqpEvqp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpCmovs(struct Machine *m, DISPATCH_PARAMETERS) {
  if (GetFlag(m->flags, FLAGS_SF)) {
    OpMovGvqpEvqp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpCmovns(struct Machine *m, DISPATCH_PARAMETERS) {
  if (!GetFlag(m->flags, FLAGS_SF)) {
    OpMovGvqpEvqp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpCmovp(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsParity(m)) {
    OpMovGvqpEvqp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpCmovnp(struct Machine *m, DISPATCH_PARAMETERS) {
  if (!IsParity(m)) {
    OpMovGvqpEvqp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpCmovl(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsLess(m)) {
    OpMovGvqpEvqp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpCmovge(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsGreaterOrEqual(m)) {
    OpMovGvqpEvqp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpCmovle(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsLessOrEqual(m)) {
    OpMovGvqpEvqp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpCmovg(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsGreater(m)) {
    OpMovGvqpEvqp(m, DISPATCH_ARGUMENTS);
  }
}

static void SetEb(struct Machine *m, DISPATCH_PARAMETERS, bool x) {
  Store8(GetModrmRegisterBytePointerWrite(m, DISPATCH_ARGUMENTS), x);
}

static void OpSeto(struct Machine *m, DISPATCH_PARAMETERS) {
  SetEb(m, DISPATCH_ARGUMENTS, GetFlag(m->flags, FLAGS_OF));
}

static void OpSetno(struct Machine *m, DISPATCH_PARAMETERS) {
  SetEb(m, DISPATCH_ARGUMENTS, !GetFlag(m->flags, FLAGS_OF));
}

static void OpSetb(struct Machine *m, DISPATCH_PARAMETERS) {
  SetEb(m, DISPATCH_ARGUMENTS, GetFlag(m->flags, FLAGS_CF));
}

static void OpSetae(struct Machine *m, DISPATCH_PARAMETERS) {
  SetEb(m, DISPATCH_ARGUMENTS, !GetFlag(m->flags, FLAGS_CF));
}

static void OpSete(struct Machine *m, DISPATCH_PARAMETERS) {
  SetEb(m, DISPATCH_ARGUMENTS, GetFlag(m->flags, FLAGS_ZF));
}

static void OpSetne(struct Machine *m, DISPATCH_PARAMETERS) {
  SetEb(m, DISPATCH_ARGUMENTS, !GetFlag(m->flags, FLAGS_ZF));
}

static void OpSetbe(struct Machine *m, DISPATCH_PARAMETERS) {
  SetEb(m, DISPATCH_ARGUMENTS, IsBelowOrEqual(m));
}

static void OpSeta(struct Machine *m, DISPATCH_PARAMETERS) {
  SetEb(m, DISPATCH_ARGUMENTS, IsAbove(m));
}

static void OpSets(struct Machine *m, DISPATCH_PARAMETERS) {
  SetEb(m, DISPATCH_ARGUMENTS, GetFlag(m->flags, FLAGS_SF));
}

static void OpSetns(struct Machine *m, DISPATCH_PARAMETERS) {
  SetEb(m, DISPATCH_ARGUMENTS, !GetFlag(m->flags, FLAGS_SF));
}

static void OpSetp(struct Machine *m, DISPATCH_PARAMETERS) {
  SetEb(m, DISPATCH_ARGUMENTS, IsParity(m));
}

static void OpSetnp(struct Machine *m, DISPATCH_PARAMETERS) {
  SetEb(m, DISPATCH_ARGUMENTS, !IsParity(m));
}

static void OpSetl(struct Machine *m, DISPATCH_PARAMETERS) {
  SetEb(m, DISPATCH_ARGUMENTS, IsLess(m));
}

static void OpSetge(struct Machine *m, DISPATCH_PARAMETERS) {
  SetEb(m, DISPATCH_ARGUMENTS, IsGreaterOrEqual(m));
}

static void OpSetle(struct Machine *m, DISPATCH_PARAMETERS) {
  SetEb(m, DISPATCH_ARGUMENTS, IsLessOrEqual(m));
}

static void OpSetg(struct Machine *m, DISPATCH_PARAMETERS) {
  SetEb(m, DISPATCH_ARGUMENTS, IsGreater(m));
}

static void OpJcxz(struct Machine *m, DISPATCH_PARAMETERS) {
  if (!MaskAddress(Eamode(rde), Get64(m->cx))) {
    OpJmp(m, DISPATCH_ARGUMENTS);
  }
}

static u64 AluPopcnt(struct Machine *m, DISPATCH_PARAMETERS, u64 x) {
  m->flags = SetFlag(m->flags, FLAGS_ZF, !x);
  m->flags = SetFlag(m->flags, FLAGS_CF, false);
  m->flags = SetFlag(m->flags, FLAGS_SF, false);
  m->flags = SetFlag(m->flags, FLAGS_OF, false);
  m->flags = SetFlag(m->flags, FLAGS_PF, false);
  return popcount(x);
}

static u64 AluBsr(struct Machine *m, DISPATCH_PARAMETERS, u64 x) {
  unsigned n;
  if (Rexw(rde)) {
    x &= 0xffffffffffffffff;
    n = 64;
  } else if (!Osz(rde)) {
    x &= 0xffffffff;
    n = 32;
  } else {
    x &= 0xffff;
    n = 16;
  }
  if (Rep(rde) == 3) {
    if (!x) {
      m->flags = SetFlag(m->flags, FLAGS_CF, true);
      m->flags = SetFlag(m->flags, FLAGS_ZF, false);
      return n;
    } else {
      m->flags = SetFlag(m->flags, FLAGS_CF, false);
      m->flags = SetFlag(m->flags, FLAGS_ZF, x == 1);
    }
  } else {
    m->flags = SetFlag(m->flags, FLAGS_ZF, !x);
    if (!x) return 0;
  }
  return bsr(x);
}

static u64 AluBsf(struct Machine *m, DISPATCH_PARAMETERS, u64 x) {
  unsigned n;
  if (Rexw(rde)) {
    x &= 0xffffffffffffffff;
    n = 64;
  } else if (!Osz(rde)) {
    x &= 0xffffffff;
    n = 32;
  } else {
    x &= 0xffff;
    n = 16;
  }
  if (Rep(rde) == 3) {
    if (!x) {
      m->flags = SetFlag(m->flags, FLAGS_CF, true);
      m->flags = SetFlag(m->flags, FLAGS_ZF, false);
      return n;
    } else {
      m->flags = SetFlag(m->flags, FLAGS_CF, false);
      m->flags = SetFlag(m->flags, FLAGS_ZF, x & 1);
    }
  } else {
    m->flags = SetFlag(m->flags, FLAGS_ZF, !x);
    if (!x) return 0;
  }
  return bsf(x);
}

static void Bitscan(struct Machine *m, DISPATCH_PARAMETERS,
                    u64 op(struct Machine *, DISPATCH_PARAMETERS, u64)) {
  WriteRegister(rde, RegRexrReg(m, rde),
                op(m, DISPATCH_ARGUMENTS,
                   ReadMemory(rde, GetModrmRegisterWordPointerReadOszRexw(
                                       m, DISPATCH_ARGUMENTS))));
}

static void OpBsf(struct Machine *m, DISPATCH_PARAMETERS) {
  Bitscan(m, DISPATCH_ARGUMENTS, AluBsf);
}

static void OpBsr(struct Machine *m, DISPATCH_PARAMETERS) {
  Bitscan(m, DISPATCH_ARGUMENTS, AluBsr);
}

static void Op1b8(struct Machine *m, DISPATCH_PARAMETERS) {
  if (Rep(rde) == 3) {
    Bitscan(m, DISPATCH_ARGUMENTS, AluPopcnt);
  } else {
    OpUdImpl(m);
  }
}

static void LoadFarPointer(struct Machine *m, DISPATCH_PARAMETERS, u64 *seg) {
  u32 fp = Load32(ComputeReserveAddressRead4(m, DISPATCH_ARGUMENTS));
  *seg = (fp & 0x0000ffff) << 4;
  Put16(RegRexrReg(m, rde), fp >> 16);
}

static void OpLes(struct Machine *m, DISPATCH_PARAMETERS) {
  LoadFarPointer(m, DISPATCH_ARGUMENTS, &m->es);
}

static void OpLds(struct Machine *m, DISPATCH_PARAMETERS) {
  LoadFarPointer(m, DISPATCH_ARGUMENTS, &m->ds);
}

static void Loop(struct Machine *m, DISPATCH_PARAMETERS, bool cond) {
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
    OpJmp(m, DISPATCH_ARGUMENTS);
  }
}

static void OpLoope(struct Machine *m, DISPATCH_PARAMETERS) {
  Loop(m, DISPATCH_ARGUMENTS, GetFlag(m->flags, FLAGS_ZF));
}

static void OpLoopne(struct Machine *m, DISPATCH_PARAMETERS) {
  Loop(m, DISPATCH_ARGUMENTS, !GetFlag(m->flags, FLAGS_ZF));
}

static void OpLoop1(struct Machine *m, DISPATCH_PARAMETERS) {
  Loop(m, DISPATCH_ARGUMENTS, true);
}

static const nexgen32e_f kOp0f6[] = {
    OpAlubiTest,
    OpAlubiTest,
    OpNotEb,
    OpNegEb,
    OpMulAxAlEbUnsigned,
    OpMulAxAlEbSigned,
    OpDivAlAhAxEbUnsigned,
    OpDivAlAhAxEbSigned,
};

static void Op0f6(struct Machine *m, DISPATCH_PARAMETERS) {
  kOp0f6[ModrmReg(rde)](m, DISPATCH_ARGUMENTS);
}

static void OpTestEvqpIvds(struct Machine *m, DISPATCH_PARAMETERS) {
  AluwiRo(m, DISPATCH_ARGUMENTS, kAlu[ALU_AND]);
}

static const nexgen32e_f kOp0f7[] = {
    OpTestEvqpIvds,
    OpTestEvqpIvds,
    OpNotEvqp,
    OpNegEvqp,
    OpMulRdxRaxEvqpUnsigned,
    OpMulRdxRaxEvqpSigned,
    OpDivRdxRaxEvqpUnsigned,
    OpDivRdxRaxEvqpSigned,
};

static void Op0f7(struct Machine *m, DISPATCH_PARAMETERS) {
  kOp0f7[ModrmReg(rde)](m, DISPATCH_ARGUMENTS);
}

static const nexgen32e_f kOp0ff[] = {OpIncEvqp, OpDecEvqp, OpCallEq,  OpUd,
                                     OpJmpEq,   OpUd,      OpPushEvq, OpUd};

static void Op0ff(struct Machine *m, DISPATCH_PARAMETERS) {
  kOp0ff[ModrmReg(rde)](m, DISPATCH_ARGUMENTS);
}

static void OpDoubleShift(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 *p;
  u8 W[2][2] = {{2, 3}, {1, 3}};
  p = GetModrmRegisterWordPointerWriteOszRexw(m, DISPATCH_ARGUMENTS);
  WriteRegisterOrMemory(
      rde, p,
      BsuDoubleShift(W[Osz(rde)][Rexw(rde)], ReadMemory(rde, p),
                     ReadRegister(rde, RegRexrReg(m, rde)),
                     Opcode(rde) & 1 ? Get8(m->cx) : uimm0, Opcode(rde) & 8,
                     &m->flags));
}

static void OpFxsave(struct Machine *m, DISPATCH_PARAMETERS) {
  i64 v;
  u8 buf[32];
  memset(buf, 0, 32);
  Write16(buf + 0, m->fpu.cw);
  Write16(buf + 2, m->fpu.sw);
  Write8(buf + 4, m->fpu.tw);
  Write16(buf + 6, m->fpu.op);
  Write32(buf + 8, m->fpu.ip);
  Write32(buf + 24, m->mxcsr);
  v = ComputeAddress(m, DISPATCH_ARGUMENTS);
  VirtualRecv(m, v + 0, buf, 32);
  VirtualRecv(m, v + 32, m->fpu.st, 128);
  VirtualRecv(m, v + 160, m->xmm, 256);
  SetWriteAddr(m, v, 416);
}

static void OpFxrstor(struct Machine *m, DISPATCH_PARAMETERS) {
  i64 v;
  u8 buf[32];
  v = ComputeAddress(m, DISPATCH_ARGUMENTS);
  SetReadAddr(m, v, 416);
  VirtualSend(m, buf, v + 0, 32);
  VirtualSend(m, m->fpu.st, v + 32, 128);
  VirtualSend(m, m->xmm, v + 160, 256);
  m->fpu.cw = Load16(buf + 0);
  m->fpu.sw = Load16(buf + 2);
  m->fpu.tw = Load8(buf + 4);
  m->fpu.op = Load16(buf + 6);
  m->fpu.ip = Load32(buf + 8);
  m->mxcsr = Load32(buf + 24);
}

static void OpXsave(struct Machine *m, DISPATCH_PARAMETERS) {
}

static void OpLdmxcsr(struct Machine *m, DISPATCH_PARAMETERS) {
  m->mxcsr = Load32(ComputeReserveAddressRead4(m, DISPATCH_ARGUMENTS));
}

static void OpStmxcsr(struct Machine *m, DISPATCH_PARAMETERS) {
  Store32(ComputeReserveAddressWrite4(m, DISPATCH_ARGUMENTS), m->mxcsr);
}

static void OpRdfsbase(struct Machine *m, DISPATCH_PARAMETERS) {
  WriteRegister(rde, RegRexbRm(m, rde), m->fs);
}

static void OpRdgsbase(struct Machine *m, DISPATCH_PARAMETERS) {
  WriteRegister(rde, RegRexbRm(m, rde), m->gs);
}

static void OpWrfsbase(struct Machine *m, DISPATCH_PARAMETERS) {
  m->fs = ReadRegister(rde, RegRexbRm(m, rde));
}

static void OpWrgsbase(struct Machine *m, DISPATCH_PARAMETERS) {
  m->gs = ReadRegister(rde, RegRexbRm(m, rde));
}

static void OpMfence(struct Machine *m, DISPATCH_PARAMETERS) {
  atomic_thread_fence(memory_order_seq_cst);
}

static void OpLfence(struct Machine *m, DISPATCH_PARAMETERS) {
  OpMfence(m, DISPATCH_ARGUMENTS);
}

static void OpSfence(struct Machine *m, DISPATCH_PARAMETERS) {
  OpMfence(m, DISPATCH_ARGUMENTS);
}

static void OpClflush(struct Machine *m, DISPATCH_PARAMETERS) {
  OpMfence(m, DISPATCH_ARGUMENTS);
}

static void Op1ae(struct Machine *m, DISPATCH_PARAMETERS) {
  bool ismem;
  ismem = !IsModrmRegister(rde);
  switch (ModrmReg(rde)) {
    case 0:
      if (ismem) {
        OpFxsave(m, DISPATCH_ARGUMENTS);
      } else {
        OpRdfsbase(m, DISPATCH_ARGUMENTS);
      }
      break;
    case 1:
      if (ismem) {
        OpFxrstor(m, DISPATCH_ARGUMENTS);
      } else {
        OpRdgsbase(m, DISPATCH_ARGUMENTS);
      }
      break;
    case 2:
      if (ismem) {
        OpLdmxcsr(m, DISPATCH_ARGUMENTS);
      } else {
        OpWrfsbase(m, DISPATCH_ARGUMENTS);
      }
      break;
    case 3:
      if (ismem) {
        OpStmxcsr(m, DISPATCH_ARGUMENTS);
      } else {
        OpWrgsbase(m, DISPATCH_ARGUMENTS);
      }
      break;
    case 4:
      if (ismem) {
        OpXsave(m, DISPATCH_ARGUMENTS);
      } else {
        OpUdImpl(m);
      }
      break;
    case 5:
      OpLfence(m, DISPATCH_ARGUMENTS);
      break;
    case 6:
      OpMfence(m, DISPATCH_ARGUMENTS);
      break;
    case 7:
      if (ismem) {
        OpClflush(m, DISPATCH_ARGUMENTS);
      } else {
        OpSfence(m, DISPATCH_ARGUMENTS);
      }
      break;
    default:
      OpUdImpl(m);
  }
}

static void OpSalc(struct Machine *m, DISPATCH_PARAMETERS) {
  if (GetFlag(m->flags, FLAGS_CF)) {
    m->al = 255;
  } else {
    m->al = 0;
  }
}

static void OpBofram(struct Machine *m, DISPATCH_PARAMETERS) {
  if (disp) {
    m->bofram[0] = m->ip;
    m->bofram[1] = m->ip + (disp & 0xff);
  } else {
    m->bofram[0] = 0;
    m->bofram[1] = 0;
  }
}

static void OpBinbase(struct Machine *m, DISPATCH_PARAMETERS) {
  if (m->system->onbinbase) {
    m->system->onbinbase(m);
  }
}

static void OpNopEv(struct Machine *m, DISPATCH_PARAMETERS) {
  switch (ModrmMod(rde) << 6 | ModrmReg(rde) << 3 | ModrmRm(rde)) {
    case 0105:
      OpBofram(m, DISPATCH_ARGUMENTS);
      break;
    case 0007:
    case 0107:
    case 0207:
      OpBinbase(m, DISPATCH_ARGUMENTS);
      break;
    default:
      OpNoop(m, DISPATCH_ARGUMENTS);
  }
}

static void OpNop(struct Machine *m, DISPATCH_PARAMETERS) {
  if (Rexb(rde)) {
    OpXchgZvqp(m, DISPATCH_ARGUMENTS);
  } else if (Rep(rde) == 3) {
    OpPause(m, DISPATCH_ARGUMENTS);
  } else {
    OpNoop(m, DISPATCH_ARGUMENTS);
  }
}

static void OpMovRqCq(struct Machine *m, DISPATCH_PARAMETERS) {
  switch (ModrmReg(rde)) {
    case 0:
      Put64(RegRexbRm(m, rde), m->system->cr0);
      break;
    case 2:
      Put64(RegRexbRm(m, rde), m->system->cr2);
      break;
    case 3:
      Put64(RegRexbRm(m, rde), m->system->cr3);
      break;
    case 4:
      Put64(RegRexbRm(m, rde), m->system->cr4);
      break;
    default:
      OpUdImpl(m);
  }
}

static void OpMovCqRq(struct Machine *m, DISPATCH_PARAMETERS) {
  i64 cr3;
  switch (ModrmReg(rde)) {
    case 0:
      m->system->cr0 = Get64(RegRexbRm(m, rde));
      break;
    case 2:
      m->system->cr2 = Get64(RegRexbRm(m, rde));
      break;
    case 3:
      cr3 = Get64(RegRexbRm(m, rde));
      if (0 <= cr3 && cr3 + 512 * 8 <= GetRealMemorySize(m->system)) {
        m->system->cr3 = cr3;
      } else {
        ThrowProtectionFault(m);
      }
      break;
    case 4:
      m->system->cr4 = Get64(RegRexbRm(m, rde));
      break;
    default:
      OpUdImpl(m);
  }
}

static void OpWrmsr(struct Machine *m, DISPATCH_PARAMETERS) {
}

static void OpRdmsr(struct Machine *m, DISPATCH_PARAMETERS) {
  Put32(m->dx, 0);
  Put32(m->ax, 0);
}

static void OpEmms(struct Machine *m, DISPATCH_PARAMETERS) {
  m->fpu.tw = -1;
}

static int ClassifyOp(u64 rde) {
  int op = Mopcode(rde);
  switch (op) {
    default:
      return kOpNormal;
    case 0x070:  // OpJo
    case 0x071:  // OpJno
    case 0x072:  // OpJb
    case 0x073:  // OpJae
    case 0x074:  // OpJe
    case 0x075:  // OpJne
    case 0x076:  // OpJbe
    case 0x077:  // OpJa
    case 0x078:  // OpJs
    case 0x079:  // OpJns
    case 0x07A:  // OpJp
    case 0x07B:  // OpJnp
    case 0x07C:  // OpJl
    case 0x07D:  // OpJge
    case 0x07E:  // OpJle
    case 0x07F:  // OpJg
    case 0x09A:  // OpCallf
    case 0x0C2:  // OpRetIw
    case 0x0C3:  // OpRet
    case 0x0CA:  // OpRetf
    case 0x0CB:  // OpRetf
    case 0x0E0:  // OpLoopne
    case 0x0E1:  // OpLoope
    case 0x0E2:  // OpLoop1
    case 0x0E3:  // OpJcxz
    case 0x0EA:  // OpJmpf
    case 0x0cf:  // OpIret
    case 0x180:  // OpJo
    case 0x181:  // OpJno
    case 0x182:  // OpJb
    case 0x183:  // OpJae
    case 0x184:  // OpJe
    case 0x185:  // OpJne
    case 0x186:  // OpJbe
    case 0x187:  // OpJa
    case 0x188:  // OpJs
    case 0x189:  // OpJns
    case 0x18A:  // OpJp
    case 0x18B:  // OpJnp
    case 0x18C:  // OpJl
    case 0x18D:  // OpJge
    case 0x18E:  // OpJle
    case 0x18F:  // OpJg
      return kOpBranching;
    case 0x0FF:  // Op0ff
      switch (ModrmReg(rde)) {
        case 2:  // call Ev
        case 4:  // jmp Ev
          return kOpBranching;
        default:
          return kOpNormal;
      }
    case 0x105:  // OpSyscall (don't want to clone jit path making)
    case 0x11F:  // OpNopEv (used by ftrace, nopl locks, et.c)
      return kOpPrecious;
  }
}

static const nexgen32e_f kNexgen32e[] = {
    /*000*/ OpAlubAdd,               //
    /*001*/ OpAluw,                  // #8    (5.653689%)
    /*002*/ OpAlubFlipAdd,           // #180  (0.000087%)
    /*003*/ OpAluwFlip,              // #7    (5.840835%)
    /*004*/ OpAluAlIbAdd,            //
    /*005*/ OpAluRaxIvds,            // #166  (0.000114%)
    /*006*/ OpPushSeg,               //
    /*007*/ OpPopSeg,                //
    /*008*/ OpAlubOr,                // #154  (0.000207%)
    /*009*/ OpAluw,                  // #21   (0.520082%)
    /*00A*/ OpAlubFlipOr,            // #120  (0.001072%)
    /*00B*/ OpAluwFlip,              // #114  (0.001252%)
    /*00C*/ OpAluAlIbOr,             //
    /*00D*/ OpAluRaxIvds,            // #282  (0.000001%)
    /*00E*/ OpPushSeg,               //
    /*00F*/ OpPopSeg,                //
    /*010*/ OpAlubAdc,               //
    /*011*/ OpAluw,                  // #11   (5.307809%)
    /*012*/ OpAlubFlipAdc,           //
    /*013*/ OpAluwFlip,              // #108  (0.001526%)
    /*014*/ OpAluAlIbAdc,            // #97   (0.002566%)
    /*015*/ OpAluRaxIvds,            //
    /*016*/ OpPushSeg,               //
    /*017*/ OpPopSeg,                //
    /*018*/ OpAlubSbb,               //
    /*019*/ OpAluw,                  // #65   (0.015300%)
    /*01A*/ OpAlubFlipSbb,           //
    /*01B*/ OpAluwFlip,              // #44   (0.241806%)
    /*01C*/ OpAluAlIbSbb,            // #96   (0.002566%)
    /*01D*/ OpAluRaxIvds,            //
    /*01E*/ OpPushSeg,               //
    /*01F*/ OpPopSeg,                //
    /*020*/ OpAlubAnd,               // #165  (0.000130%)
    /*021*/ OpAluw,                  // #59   (0.019691%)
    /*022*/ OpAlubFlipAnd,           //
    /*023*/ OpAluwFlip,              // #41   (0.279852%)
    /*024*/ OpAluAlIbAnd,            // #279  (0.000001%)
    /*025*/ OpAluRaxIvds,            // #43   (0.275823%)
    /*026*/ OpPushSeg,               //
    /*027*/ OpPopSeg,                //
    /*028*/ OpAlubSub,               //
    /*029*/ OpAluw,                  // #29   (0.334693%)
    /*02A*/ OpAlubFlipSub,           // #179  (0.000087%)
    /*02B*/ OpAluwFlip,              // #71   (0.012465%)
    /*02C*/ OpAluAlIbSub,            //
    /*02D*/ OpAluRaxIvds,            // #112  (0.001317%)
    /*02E*/ OpUd,                    //
    /*02F*/ OpDas,                   //
    /*030*/ OpAlubXor,               // #140  (0.000397%)
    /*031*/ OpAluw,                  // #3    (6.612252%)
    /*032*/ OpAlubFlipXor,           // #81   (0.007453%)
    /*033*/ OpAluwFlip,              // #47   (0.138021%)
    /*034*/ OpAluAlIbXor,            //
    /*035*/ OpAluRaxIvds,            // #295  (0.000000%)
    /*036*/ OpUd,                    //
    /*037*/ OpAaa,                   //
    /*038*/ OpAlubCmp,               // #98   (0.002454%)
    /*039*/ OpAluwCmp,               // #2    (6.687374%)
    /*03A*/ OpAlubFlipCmp,           // #103  (0.001846%)
    /*03B*/ OpAluwFlipCmp,           // #75   (0.010320%)
    /*03C*/ OpCmpAlIb,               // #85   (0.006267%)
    /*03D*/ OpCmpRaxIvds,            // #42   (0.279462%)
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
    /*063*/ OpMovsxdGdqpEd,          // #58   (0.026117%)
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
    /*070*/ OpJo,                    // #177  (0.000094%)
    /*071*/ OpJno,                   // #200  (0.000034%)
    /*072*/ OpJb,                    // #64   (0.015441%)
    /*073*/ OpJae,                   // #9    (5.615257%)
    /*074*/ OpJe,                    // #15   (0.713108%)
    /*075*/ OpJne,                   // #13   (0.825247%)
    /*076*/ OpJbe,                   // #23   (0.475584%)
    /*077*/ OpJa,                    // #48   (0.054677%)
    /*078*/ OpJs,                    // #66   (0.014096%)
    /*079*/ OpJns,                   // #84   (0.006506%)
    /*07A*/ OpJp,                    // #175  (0.000112%)
    /*07B*/ OpJnp,                   // #174  (0.000112%)
    /*07C*/ OpJl,                    // #223  (0.000008%)
    /*07D*/ OpJge,                   // #80   (0.007801%)
    /*07E*/ OpJle,                   // #70   (0.012536%)
    /*07F*/ OpJg,                    // #76   (0.010144%)
    /*080*/ OpAlubiReg,              // #53   (0.033021%)
    /*081*/ OpAluwiReg,              // #60   (0.018910%)
    /*082*/ OpAlubiReg,              //
    /*083*/ OpAluwiReg,              // #4    (6.518845%)
    /*084*/ OpAlubTest,              // #54   (0.030642%)
    /*085*/ OpAluwTest,              // #18   (0.628547%)
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
    /*0A8*/ OpTestAlIb,              // #115  (0.001247%)
    /*0A9*/ OpTestRaxIvds,           // #113  (0.001300%)
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
    /*0C2*/ OpRet,                   //
    /*0C3*/ OpRet,                   // #24   (0.422698%)
    /*0C4*/ OpLes,                   //
    /*0C5*/ OpLds,                   //
    /*0C6*/ OpMovEbIb,               // #90   (0.004525%)
    /*0C7*/ OpMovEvqpIvds,           // #45   (0.161349%)
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
    /*109*/ OpUd,                    //
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
    /*140*/ OpCmovo,                 //
    /*141*/ OpCmovno,                //
    /*142*/ OpCmovb,                 // #69   (0.012667%)
    /*143*/ OpCmovae,                // #276  (0.000002%)
    /*144*/ OpCmove,                 // #134  (0.000584%)
    /*145*/ OpCmovne,                // #132  (0.000700%)
    /*146*/ OpCmovbe,                // #125  (0.000945%)
    /*147*/ OpCmova,                 // #40   (0.289378%)
    /*148*/ OpCmovs,                 // #130  (0.000774%)
    /*149*/ OpCmovns,                // #149  (0.000228%)
    /*14A*/ OpCmovp,                 //
    /*14B*/ OpCmovnp,                //
    /*14C*/ OpCmovl,                 // #102  (0.002008%)
    /*14D*/ OpCmovge,                // #196  (0.000044%)
    /*14E*/ OpCmovle,                // #110  (0.001379%)
    /*14F*/ OpCmovg,                 // #121  (0.001029%)
    /*150*/ OpUd,                    //
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
    /*180*/ OpJo,                    //
    /*181*/ OpJno,                   //
    /*182*/ OpJb,                    // #107  (0.001532%)
    /*183*/ OpJae,                   // #72   (0.011761%)
    /*184*/ OpJe,                    // #55   (0.029121%)
    /*185*/ OpJne,                   // #57   (0.027593%)
    /*186*/ OpJbe,                   // #46   (0.147358%)
    /*187*/ OpJa,                    // #86   (0.005907%)
    /*188*/ OpJs,                    // #106  (0.001569%)
    /*189*/ OpJns,                   // #160  (0.000142%)
    /*18A*/ OpJp,                    //
    /*18B*/ OpJnp,                   //
    /*18C*/ OpJl,                    // #105  (0.001786%)
    /*18D*/ OpJge,                   // #281  (0.000001%)
    /*18E*/ OpJle,                   // #77   (0.009607%)
    /*18F*/ OpJg,                    // #126  (0.000890%)
    /*190*/ OpSeto,                  // #280  (0.000001%)
    /*191*/ OpSetno,                 //
    /*192*/ OpSetb,                  // #26   (0.364366%)
    /*193*/ OpSetae,                 // #183  (0.000063%)
    /*194*/ OpSete,                  // #78   (0.009363%)
    /*195*/ OpSetne,                 // #94   (0.003096%)
    /*196*/ OpSetbe,                 // #162  (0.000139%)
    /*197*/ OpSeta,                  // #92   (0.003559%)
    /*198*/ OpSets,                  //
    /*199*/ OpSetns,                 //
    /*19A*/ OpSetp,                  //
    /*19B*/ OpSetnp,                 //
    /*19C*/ OpSetl,                  // #119  (0.001079%)
    /*19D*/ OpSetge,                 // #275  (0.000002%)
    /*19E*/ OpSetle,                 // #167  (0.000112%)
    /*19F*/ OpSetg,                  // #95   (0.002688%)
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

static nexgen32e_f GetOp(long op) {
  if (op < ARRAYLEN(kNexgen32e)) {
    return kNexgen32e[op];
  } else {
    switch (op) {
      XLAT(0x21c, OpSsePabsb);
      XLAT(0x21d, OpSsePabsw);
      XLAT(0x21e, OpSsePabsd);
      XLAT(0x22a, OpMovntdqaVdqMdq);
      XLAT(0x240, OpSsePmulld);
      XLAT(0x30f, OpSsePalignr);
      XLAT(0x344, OpSsePclmulqdq);
      default:
        return OpUd;
    }
  }
}

static void StartPath(struct Machine *m) {
  JIT_LOGF("%" PRIx64 " <path>", m->ip);
}

void StartOp(struct Machine *m, DISPATCH_PARAMETERS) {
  JIT_LOGF("%" PRIx64 "   <op>", m->ip);
  JIT_LOGF("%" PRIx64 "     %s", m->ip, DescribeOp(m));
  STATISTIC(++instructions_jitted);
  unassert(!m->path.jp);
  m->oldip = m->ip;
  m->ip += disp;
}

void EndOp(struct Machine *m) {
  JIT_LOGF("%" PRIx64 "   </op>", m->ip);
  if (m->opcache->stashaddr) {
    CommitStash(m);
  }
  m->oldip = INT64_MIN;
}

static void EndPath(struct Machine *m) {
  JIT_LOGF("%" PRIx64 "   %s", m->ip, DescribeOp(m));
  JIT_LOGF("%" PRIx64 " </path>", m->ip);
}

static bool CreatePath(struct Machine *m) {
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

static void CommitPath(struct Machine *m, intptr_t splice) {
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

static void AbandonPath(struct Machine *m) {
  unassert(m->path.jp);
  STATISTIC(++path_abandoned);
  AbandonJit(&m->system->jit, m->path.jp);
  m->path.jp = 0;
}

static bool AddPath(struct Machine *m, DISPATCH_PARAMETERS) {
  unassert(m->path.jp);
  JIT_LOGF("adding [%s] from address %" PRIx64 " to path starting at %" PRIx64,
           DescribeOp(m), GetPc(m), m->path.start);
  ++m->path.elements;
  STATISTIC(++path_elements);
  AppendJitSetArg(m->path.jp, kParamDisp, Oplength(rde));
  AppendJitCall(m->path.jp, (void *)StartOp);
  AppendJitSetArg(m->path.jp, kParamRde, rde);
  AppendJitSetArg(m->path.jp, kParamDisp, disp);
  AppendJitSetArg(m->path.jp, kParamUimm0, uimm0);
  AppendJitCall(m->path.jp, (void *)kNexgen32e[Mopcode(rde)]);
  AppendJitCall(m->path.jp, (void *)EndOp);
  return true;
}

static bool CanJit(struct Machine *m) {
  if (m->mode == XED_MODE_LONG) {
    return !IsJitDisabled(&m->system->jit);
  } else {
    LOG_ONCE(LOGF("jit is only supported in long mode"));
    return false;
  }
}

void JitlessDispatch(struct Machine *m, DISPATCH_PARAMETERS) {
  ASM_LOGF("decoding [%s] at address %" PRIx64, DescribeOp(m), GetPc(m));
  LoadInstruction(m);
  m->oldip = m->ip;
  darg = 0;
  rde = m->xedd->op.rde;
  disp = m->xedd->op.disp;
  uimm0 = m->xedd->op.uimm0;
  m->ip += Oplength(rde);
  GetOp(Mopcode(rde))(m, DISPATCH_ARGUMENTS);
  if (m->opcache->stashaddr) CommitStash(m);
  m->oldip = INT64_MIN;
}

void GeneralDispatch(struct Machine *m, DISPATCH_PARAMETERS) {
  i64 newip;
  int opclass;
  intptr_t jitpc;
  ASM_LOGF("decoding [%s] at address %" PRIx64, DescribeOp(m), GetPc(m));
  LoadInstruction(m);
  m->oldip = m->ip;
  darg = 0;
  rde = m->xedd->op.rde;
  disp = m->xedd->op.disp;
  uimm0 = m->xedd->op.uimm0;
  opclass = ClassifyOp(rde);
  if (CanJit(m) && (m->path.jp || (opclass == kOpNormal && CreatePath(m)))) {
    jitpc = GetJitPc(m->path.jp);
  } else {
    jitpc = 0;
  }
  m->ip += Oplength(rde);
  GetOp(Mopcode(rde))(m, DISPATCH_ARGUMENTS);
  if (m->opcache->stashaddr) CommitStash(m);
  if (jitpc) {
    newip = m->ip;
    m->ip = m->oldip;
    if (GetJitPc(m->path.jp) == jitpc) {
      if (opclass == kOpNormal || opclass == kOpBranching) {
        AddPath(m, DISPATCH_ARGUMENTS);
      } else {
        JIT_LOGF("won't add [%" PRIx64 " %s] so path started at %" PRIx64,
                 GetPc(m), DescribeOp(m), m->path.start);
      }
    }
    if (opclass == kOpPrecious || opclass == kOpBranching) {
      CommitPath(m, 0);
    }
    m->ip = newip;
  }
  m->oldip = INT64_MIN;
}

void ExecuteInstruction(struct Machine *m) {
  STATISTIC(++instructions_dispatched);
  u64 pc;
  nexgen32e_f func;
  if ((pc = GetPc(m)) - m->system->codestart < m->system->codesize) {
    func = m->fun[pc];
    unassert(func != NULL);
    if (!m->path.jp) {
      func(m, 0, 0, 0, 0);
      return;
    } else if (func == JitlessDispatch) {
      JIT_LOGF("abandoning path starting at %" PRIx64
               " due to running into staged path",
               m->path.start);
      AbandonPath(m);
      func(m, 0, 0, 0, 0);
      return;
    } else if (func != GeneralDispatch) {
      JIT_LOGF("splicing path starting at %" PRIx64
               " into previously created function %p",
               m->path.start, func);
      CommitPath(m, (intptr_t)func);
      func(m, 0, 0, 0, 0);
      return;
    }
  }
  GeneralDispatch(m, 0, 0, 0, 0);
}

void Actor(struct Machine *m) {
  int sig;
  // Put some distance between ourselves and the calling function,
  // because it calls setjmp(). Another thread could theoretically
  // longjmp() into this allocation. We must try something better.
  void *volatile lol = alloca(256);
  (void)lol;
  // main exception loop
  for (g_machine = m;;) {
    ExecuteInstruction(m);
    if (UNLIKELY(m->signals) && (sig = ConsumeSignal(m))) {
      TerminateSignal(m, sig);
    }
  }
}
