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
#include <stdio.h>

#include "blink/builtin.h"
#include "blink/bus.h"
#include "blink/endian.h"
#include "blink/jit.h"
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/time.h"
#include "blink/x86.h"

static void StoreDescriptorTable(P, u16 limit, u64 base) {
  u64 l;
  l = ComputeAddress(A);
  if (l + 10 <= kRealSize) {
    Write16(m->system->real + l, limit);
    if (Mode(rde) == XED_MODE_LONG) {
      Write64(m->system->real + l + 2, base);
      SetWriteAddr(m, l, 10);
    } else {
      Write32(m->system->real + l + 2, base);
      SetWriteAddr(m, l, 6);
    }
  } else {
    ThrowSegmentationFault(m, l);
  }
}

static void LoadDescriptorTable(P, u16 *out_limit, u64 *out_base) {
  u16 limit;
  u64 l, base;
  l = ComputeAddress(A);
  if (l + 10 <= kRealSize) {
    limit = Read16(m->system->real + l);
    // Intel Manual Volume 2A:
    // - "If the operand-size attribute is 16 bits, a 16-bit limit (lower 2
    //   bytes) and a 24-bit base address (third, fourth, and fifth byte) are
    //   loaded."
    // - "In 64-bit mode, the instruction’s operand size is fixed at 8 + 2
    //   bytes (an 8-byte base and a 2-byte limit)."
    if (Mode(rde) == XED_MODE_LONG) {
      base = Read64(m->system->real + l + 2);
      SetReadAddr(m, l, 10);
    } else if (!Osz(rde)) {
      base = Read32(m->system->real + l + 2);
      SetReadAddr(m, l, 6);
    } else {
      base = Read32(m->system->real + l + 2) & 0x00ffffff;
      SetReadAddr(m, l, 6);
    }
    *out_limit = limit;
    *out_base = base;
  } else {
    ThrowSegmentationFault(m, l);
  }
}

static void SgdtMs(P) {
  StoreDescriptorTable(A, m->system->gdt_limit, m->system->gdt_base);
}

static void LgdtMs(P) {
  LoadDescriptorTable(A, &m->system->gdt_limit, &m->system->gdt_base);
}

static void SidtMs(P) {
  StoreDescriptorTable(A, m->system->idt_limit, m->system->idt_base);
}

static void LidtMs(P) {
  LoadDescriptorTable(A, &m->system->idt_limit, &m->system->idt_base);
}

static void Monitor(P) {
}

static void Mwait(P) {
}

static void Swapgs(P) {
}

static void Vmcall(P) {
}

static void Vmlaunch(P) {
}

static void Vmresume(P) {
}

static void Vmxoff(P) {
}

static void InvlpgM(P) {
  i64 virt;
  struct AddrSeg as;
  // if (Cpl(m)) OpUdImpl(m);
  as = LoadEffectiveAddress(A);
  virt = as.seg + as.addr;
  if (Mode(rde) == XED_MODE_LONG &&
      !(-0x800000000000 <= virt && virt < 0x800000000000)) {
    // In 64-bit mode, if the memory address is in non-canonical form,
    // then INVLPG is the same as a NOP. -Quoth Intel §invlpg
    return;
  }
#ifndef DISABLE_JIT
  if (!IsJitDisabled(&m->system->jit)) {
    ResetJitPage(&m->system->jit, virt);
  }
#endif
  InvalidateSystem(m->system, true, true);
}

static void Smsw(P, bool ismem) {
  if (ismem) {
    Store16(GetModrmRegisterWordPointerWrite2(A), m->system->cr0);
  } else if (Rexw(rde)) {
    Put64(RegRexrReg(m, rde), m->system->cr0);
  } else if (!Osz(rde)) {
    Put64(RegRexrReg(m, rde), m->system->cr0 & 0xffffffff);
  } else {
    Put16(RegRexrReg(m, rde), m->system->cr0);
  }
}

static void Lmsw(P) {
  m->system->cr0 = Read16(GetModrmRegisterWordPointerRead2(A));
}

void Op101(P) {
  bool ismem;
  ismem = !IsModrmRegister(rde);
  switch (ModrmReg(rde)) {
#ifndef DISABLE_METAL
    case 0:
      if (ismem) {
        SgdtMs(A);
      } else {
        switch (ModrmRm(rde)) {
          case 1:
            Vmcall(A);
            break;
          case 2:
            Vmlaunch(A);
            break;
          case 3:
            Vmresume(A);
            break;
          case 4:
            Vmxoff(A);
            break;
          default:
            OpUdImpl(m);
        }
      }
      break;
    case 1:
      if (ismem) {
        SidtMs(A);
      } else {
        switch (ModrmRm(rde)) {
          case 0:
            Monitor(A);
            break;
          case 1:
            Mwait(A);
            break;
          default:
            OpUdImpl(m);
        }
      }
      break;
    case 2:
      if (ismem) {
        LgdtMs(A);
      } else {
        OpUdImpl(m);
      }
      break;
    case 3:
      if (ismem) {
        LidtMs(A);
      } else {
        OpUdImpl(m);
      }
      break;
    case 4:
      Smsw(A, ismem);
      break;
    case 6:
      Lmsw(A);
      break;
#endif
    case 7:
      if (ismem) {
#ifndef DISABLE_METAL
        InvlpgM(A);
#else
        OpUdImpl(m);
#endif
      } else {
        switch (ModrmRm(rde)) {
          case 0:
            Swapgs(A);
            break;
          case 1:
            OpRdtscp(A);
            break;
          default:
            OpUdImpl(m);
        }
      }
      break;
    default:
      OpUdImpl(m);
  }
}
