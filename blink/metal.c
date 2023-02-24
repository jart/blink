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
#include "blink/bus.h"
#include "blink/endian.h"
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/msr.h"
#include "blink/rde.h"
#include "blink/x86.h"

#ifndef DISABLE_METAL

static relegated const struct DescriptorCache *GetSegment(P, unsigned s) {
  if (s < 6) {
    return m->seg + s;
  } else {
    OpUdImpl(m);
  }
}

static relegated u64 GetDescriptorBase(u64 d) {
  return (d & 0xff00000000000000) >> 32 | (d & 0x000000ffffff0000) >> 16;
}

static relegated int GetDescriptorMode(u64 d) {
  u8 kMode[] = {XED_MODE_REAL, XED_MODE_LONG, XED_MODE_LEGACY, XED_MODE_LONG};
  return kMode[(d & 0x0060000000000000) >> 53];
}

static relegated bool IsProtectedMode(struct Machine *m) {
  return m->system->cr0 & CR0_PE;
}

static relegated bool IsNullSelector(u16 sel) {
  return (sel & -4u) == 0;
}

static relegated void ChangeMachineMode(struct Machine *m, int mode) {
  if (mode == m->mode) return;
  ResetInstructionCache(m);
  SetMachineMode(m, mode);
#ifdef HAVE_JIT
  if (mode == XED_MODE_LONG && FLAG_wantjit) {
    EnableJit(&m->system->jit);
  }
#endif
}

static relegated void SetSegment(P, unsigned sr, u16 sel, bool jumping) {
  u64 descriptor;
  if (sr == 1 && !jumping) OpUdImpl(m);
  if (!IsProtectedMode(m)) {
    m->seg[sr].sel = sel;
    m->seg[sr].base = sel << 4;
  } else if (GetDescriptor(m, sel, &descriptor) != -1) {
    m->seg[sr].sel = sel;
    m->seg[sr].base = GetDescriptorBase(descriptor);
    if (sr == SREG_CS) {
      ChangeMachineMode(m, GetDescriptorMode(descriptor));
    }
  } else if (IsNullSelector(sel)) {
    switch (sr) {
      case SREG_CS:
        ThrowProtectionFault(m);
        break;
      case SREG_SS:
        if (Cpl(m) == 3) ThrowProtectionFault(m);
    }
    m->seg[sr].sel = sel;
  } else {
    ThrowProtectionFault(m);
  }
}

relegated void SetCs(P, u16 sel) {
  SetSegment(A, SREG_CS, sel, true);
}

relegated void OpPushSeg(P) {
  u8 seg = (Opcode(rde) & 070) >> 3;
  Push(A, GetSegment(A, seg)->sel);
}

relegated void OpPopSeg(P) {
  u8 seg = (Opcode(rde) & 070) >> 3;
  SetSegment(A, seg, Pop(A, 0), false);
}

relegated void OpMovEvqpSw(P) {
  WriteRegisterOrMemory(rde, GetModrmRegisterWordPointerWriteOszRexw(A),
                        GetSegment(A, ModrmReg(rde))->sel);
}

relegated void OpMovSwEvqp(P) {
  u64 x;
  x = ReadMemory(rde, GetModrmRegisterWordPointerReadOszRexw(A));
  SetSegment(A, ModrmReg(rde), x, false);
}

relegated void OpJmpf(P) {
  SetCs(A, uimm0);
  m->ip = disp;
  if (m->system->onlongbranch) {
    m->system->onlongbranch(m);
  }
}

static void PutEaxAx(P, u32 x) {
  if (!Osz(rde)) {
    Put64(m->ax, x);
  } else {
    Put16(m->ax, x);
  }
}

static u32 GetEaxAx(P) {
  if (!Osz(rde)) {
    return Get32(m->ax);
  } else {
    return Get16(m->ax);
  }
}

relegated void OpInAlImm(P) {
  Put8(m->ax, OpIn(m, uimm0));
}

relegated void OpInAxImm(P) {
  PutEaxAx(A, OpIn(m, uimm0));
}

relegated void OpInAlDx(P) {
  Put8(m->ax, OpIn(m, Get16(m->dx)));
}

relegated void OpInAxDx(P) {
  PutEaxAx(A, OpIn(m, Get16(m->dx)));
}

relegated void OpOutImmAl(P) {
  OpOut(m, uimm0, Get8(m->ax));
}

relegated void OpOutImmAx(P) {
  OpOut(m, uimm0, GetEaxAx(A));
}

relegated void OpOutDxAl(P) {
  OpOut(m, Get16(m->dx), Get8(m->ax));
}

relegated void OpOutDxAx(P) {
  OpOut(m, Get16(m->dx), GetEaxAx(A));
}

static relegated void LoadFarPointer(P, unsigned sr) {
  unsigned n;
  u8 *p;
  u64 fp;
  switch (Eamode(rde)) {
    case XED_MODE_LONG:
    case XED_MODE_LEGACY:
      OpUdImpl(m);
      break;
    case XED_MODE_REAL:
      n = 1 << WordLog2(rde);
      p = ComputeReserveAddressRead(A, n + 2);
      LockBus(p);
      fp = Load32(p);
      if (n >= 4) {
        fp |= (u64)Load16(p + 4) << 32;
        SetSegment(A, sr, fp >> 32 & 0x0000ffff, false);
      } else {
        SetSegment(A, sr, fp >> 16 & 0x0000ffff, false);
      }
      UnlockBus(p);
      WriteRegister(rde, RegRexrReg(m, rde), fp);  // offset portion
      break;
    default:
      __builtin_unreachable();
  }
}

relegated void OpLes(P) {
  LoadFarPointer(A, 0);
}

relegated void OpLds(P) {
  LoadFarPointer(A, 3);
}

relegated void OpWrmsr(P) {
  switch (Get32(m->cx)) {
    case MSR_IA32_EFER:
      m->system->efer = Get32(m->ax);
      break;
    case MSR_IA32_FS_BASE:
      m->fs.base = (u64)Read32(m->dx) << 32 | Read32(m->ax);
      break;
    case MSR_IA32_GS_BASE:
      m->gs.base = (u64)Read32(m->dx) << 32 | Read32(m->ax);
      break;
    default:
      LOGF("unsupported msr %#x", Get32(m->cx));
      break;
  }
}

relegated void OpRdmsr(P) {
  switch (Get32(m->cx)) {
    case MSR_IA32_EFER:
      Put32(m->ax, m->system->efer);
      break;
    case MSR_IA32_FS_BASE:
      Put32(m->dx, m->fs.base >> 32);
      Put32(m->ax, m->fs.base);
      break;
    case MSR_IA32_GS_BASE:
      Put32(m->dx, m->gs.base >> 32);
      Put32(m->ax, m->gs.base);
      break;
    default:
      LOGF("unsupported msr %#x", Get32(m->cx));
      break;
  }
}

#endif /* DISABLE_METAL */
