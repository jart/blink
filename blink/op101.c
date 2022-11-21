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

#include "blink/endian.h"
#include "blink/machine.h"
#include "blink/memory.h"
#include "blink/modrm.h"
#include "blink/mop.h"
#include "blink/real.h"
#include "blink/time.h"

static void StoreDescriptorTable(struct Machine *m, DISPATCH_PARAMETERS, u16 limit,
                                 u64 base) {
  u64 l;
  l = ComputeAddress(m, DISPATCH_ARGUMENTS);
  if (l + 10 <= GetRealMemorySize(m->system)) {
    Write16(m->system->real.p + l, limit);
    if (Rexw(rde)) {
      Write64(m->system->real.p + l + 2, base);
      SetWriteAddr(m, l, 10);
    } else if (!Osz(rde)) {
      Write32(m->system->real.p + l + 2, base);
      SetWriteAddr(m, l, 6);
    } else {
      Write16(m->system->real.p + l + 2, base);
      SetWriteAddr(m, l, 4);
    }
  } else {
    ThrowSegmentationFault(m, l);
  }
}

static void LoadDescriptorTable(struct Machine *m, DISPATCH_PARAMETERS, u16 *out_limit,
                                u64 *out_base) {
  u16 limit;
  u64 l, base;
  l = ComputeAddress(m, DISPATCH_ARGUMENTS);
  if (l + 10 <= GetRealMemorySize(m->system)) {
    limit = Read16(m->system->real.p + l);
    if (Rexw(rde)) {
      base = Read64(m->system->real.p + l + 2) & 0x00ffffff;
      SetReadAddr(m, l, 10);
    } else if (!Osz(rde)) {
      base = Read32(m->system->real.p + l + 2);
      SetReadAddr(m, l, 6);
    } else {
      base = Read16(m->system->real.p + l + 2);
      SetReadAddr(m, l, 4);
    }
    if (base + limit <= GetRealMemorySize(m->system)) {
      *out_limit = limit;
      *out_base = base;
    } else {
      ThrowProtectionFault(m);
    }
  } else {
    ThrowSegmentationFault(m, l);
  }
}

static void SgdtMs(struct Machine *m, DISPATCH_PARAMETERS) {
  StoreDescriptorTable(m, DISPATCH_ARGUMENTS, m->system->gdt_limit, m->system->gdt_base);
}

static void LgdtMs(struct Machine *m, DISPATCH_PARAMETERS) {
  LoadDescriptorTable(m, DISPATCH_ARGUMENTS, &m->system->gdt_limit, &m->system->gdt_base);
}

static void SidtMs(struct Machine *m, DISPATCH_PARAMETERS) {
  StoreDescriptorTable(m, DISPATCH_ARGUMENTS, m->system->idt_limit, m->system->idt_base);
}

static void LidtMs(struct Machine *m, DISPATCH_PARAMETERS) {
  LoadDescriptorTable(m, DISPATCH_ARGUMENTS, &m->system->idt_limit, &m->system->idt_base);
}

static void Monitor(struct Machine *m, DISPATCH_PARAMETERS) {
}

static void Mwait(struct Machine *m, DISPATCH_PARAMETERS) {
}

static void Swapgs(struct Machine *m, DISPATCH_PARAMETERS) {
}

static void Vmcall(struct Machine *m, DISPATCH_PARAMETERS) {
}

static void Vmlaunch(struct Machine *m, DISPATCH_PARAMETERS) {
}

static void Vmresume(struct Machine *m, DISPATCH_PARAMETERS) {
}

static void Vmxoff(struct Machine *m, DISPATCH_PARAMETERS) {
}

static void InvlpgM(struct Machine *m, DISPATCH_PARAMETERS) {
  ResetTlb(m);
}

static void Smsw(struct Machine *m, DISPATCH_PARAMETERS, bool ismem) {
  if (ismem) {
    Store16(GetModrmRegisterWordPointerWrite2(m, DISPATCH_ARGUMENTS), m->system->cr0);
  } else if (Rexw(rde)) {
    Put64(RegRexrReg(m, rde), m->system->cr0);
  } else if (!Osz(rde)) {
    Put64(RegRexrReg(m, rde), m->system->cr0 & 0xffffffff);
  } else {
    Put16(RegRexrReg(m, rde), m->system->cr0);
  }
}

static void Lmsw(struct Machine *m, DISPATCH_PARAMETERS) {
  m->system->cr0 = Read16(GetModrmRegisterWordPointerRead2(m, DISPATCH_ARGUMENTS));
}

void Op101(struct Machine *m, DISPATCH_PARAMETERS) {
  bool ismem;
  ismem = !IsModrmRegister(rde);
  switch (ModrmReg(rde)) {
    case 0:
      if (ismem) {
        SgdtMs(m, DISPATCH_ARGUMENTS);
      } else {
        switch (ModrmRm(rde)) {
          case 1:
            Vmcall(m, DISPATCH_ARGUMENTS);
            break;
          case 2:
            Vmlaunch(m, DISPATCH_ARGUMENTS);
            break;
          case 3:
            Vmresume(m, DISPATCH_ARGUMENTS);
            break;
          case 4:
            Vmxoff(m, DISPATCH_ARGUMENTS);
            break;
          default:
            OpUdImpl(m);
        }
      }
      break;
    case 1:
      if (ismem) {
        SidtMs(m, DISPATCH_ARGUMENTS);
      } else {
        switch (ModrmRm(rde)) {
          case 0:
            Monitor(m, DISPATCH_ARGUMENTS);
            break;
          case 1:
            Mwait(m, DISPATCH_ARGUMENTS);
            break;
          default:
            OpUdImpl(m);
        }
      }
      break;
    case 2:
      if (ismem) {
        LgdtMs(m, DISPATCH_ARGUMENTS);
      } else {
        OpUdImpl(m);
      }
      break;
    case 3:
      if (ismem) {
        LidtMs(m, DISPATCH_ARGUMENTS);
      } else {
        OpUdImpl(m);
      }
      break;
    case 4:
      Smsw(m, DISPATCH_ARGUMENTS, ismem);
      break;
    case 6:
      Lmsw(m, DISPATCH_ARGUMENTS);
      break;
    case 7:
      if (ismem) {
        InvlpgM(m, DISPATCH_ARGUMENTS);
      } else {
        switch (ModrmRm(rde)) {
          case 0:
            Swapgs(m, DISPATCH_ARGUMENTS);
            break;
          case 1:
            OpRdtscp(m, DISPATCH_ARGUMENTS);
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
