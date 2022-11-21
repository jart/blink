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

static void StoreDescriptorTable(struct Machine *m, u32 rde, u16 limit,
                                 u64 base) {
  u64 l;
  l = ComputeAddress(m, rde);
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

static void LoadDescriptorTable(struct Machine *m, u32 rde, u16 *out_limit,
                                u64 *out_base) {
  u16 limit;
  u64 l, base;
  l = ComputeAddress(m, rde);
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

static void SgdtMs(struct Machine *m, u32 rde) {
  StoreDescriptorTable(m, rde, m->system->gdt_limit, m->system->gdt_base);
}

static void LgdtMs(struct Machine *m, u32 rde) {
  LoadDescriptorTable(m, rde, &m->system->gdt_limit, &m->system->gdt_base);
}

static void SidtMs(struct Machine *m, u32 rde) {
  StoreDescriptorTable(m, rde, m->system->idt_limit, m->system->idt_base);
}

static void LidtMs(struct Machine *m, u32 rde) {
  LoadDescriptorTable(m, rde, &m->system->idt_limit, &m->system->idt_base);
}

static void Monitor(struct Machine *m, u32 rde) {
}

static void Mwait(struct Machine *m, u32 rde) {
}

static void Swapgs(struct Machine *m, u32 rde) {
}

static void Vmcall(struct Machine *m, u32 rde) {
}

static void Vmlaunch(struct Machine *m, u32 rde) {
}

static void Vmresume(struct Machine *m, u32 rde) {
}

static void Vmxoff(struct Machine *m, u32 rde) {
}

static void InvlpgM(struct Machine *m, u32 rde) {
  ResetTlb(m);
}

static void Smsw(struct Machine *m, u32 rde, bool ismem) {
  if (ismem) {
    Store16(GetModrmRegisterWordPointerWrite2(m, rde), m->system->cr0);
  } else if (Rexw(rde)) {
    Put64(RegRexrReg(m, rde), m->system->cr0);
  } else if (!Osz(rde)) {
    Put64(RegRexrReg(m, rde), m->system->cr0 & 0xffffffff);
  } else {
    Put16(RegRexrReg(m, rde), m->system->cr0);
  }
}

static void Lmsw(struct Machine *m, u32 rde) {
  m->system->cr0 = Read16(GetModrmRegisterWordPointerRead2(m, rde));
}

void Op101(struct Machine *m, u32 rde) {
  bool ismem;
  ismem = !IsModrmRegister(rde);
  switch (ModrmReg(rde)) {
    case 0:
      if (ismem) {
        SgdtMs(m, rde);
      } else {
        switch (ModrmRm(rde)) {
          case 1:
            Vmcall(m, rde);
            break;
          case 2:
            Vmlaunch(m, rde);
            break;
          case 3:
            Vmresume(m, rde);
            break;
          case 4:
            Vmxoff(m, rde);
            break;
          default:
            OpUd(m, rde);
        }
      }
      break;
    case 1:
      if (ismem) {
        SidtMs(m, rde);
      } else {
        switch (ModrmRm(rde)) {
          case 0:
            Monitor(m, rde);
            break;
          case 1:
            Mwait(m, rde);
            break;
          default:
            OpUd(m, rde);
        }
      }
      break;
    case 2:
      if (ismem) {
        LgdtMs(m, rde);
      } else {
        OpUd(m, rde);
      }
      break;
    case 3:
      if (ismem) {
        LidtMs(m, rde);
      } else {
        OpUd(m, rde);
      }
      break;
    case 4:
      Smsw(m, rde, ismem);
      break;
    case 6:
      Lmsw(m, rde);
      break;
    case 7:
      if (ismem) {
        InvlpgM(m, rde);
      } else {
        switch (ModrmRm(rde)) {
          case 0:
            Swapgs(m, rde);
            break;
          case 1:
            OpRdtscp(m, rde);
            break;
          default:
            OpUd(m, rde);
        }
      }
      break;
    default:
      OpUd(m, rde);
  }
}
