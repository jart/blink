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
#include <string.h>

#include "blink/builtin.h"
#include "blink/endian.h"
#include "blink/machine.h"
#include "blink/modrm.h"

static u32 pmovmskb(const u8 p[16]) {
  u32 i, m;
  for (m = i = 0; i < 16; ++i) {
    if (p[i] & 0x80) m |= 1 << i;
  }
  return m;
}

static void MovdquVdqWdq(P) {
  memcpy(XmmRexrReg(m, rde), GetModrmRegisterXmmPointerRead16(A), 16);
  if (IsMakingPath(m)) {
    Jitter(A, "z4B"    // 128-bit GetRegOrMem
              "z4C");  // 128-bit PutReg
  }
}

static void MovdquWdqVdq(P) {
  memcpy(GetModrmRegisterXmmPointerWrite16(A), XmmRexrReg(m, rde), 16);
  if (IsMakingPath(m)) {
    Jitter(A, "z4A"    // 128-bit GetReg
              "z4D");  // 128-bit PutRegOrMem
  }
}

static void MovupsVpsWps(P) {
  MovdquVdqWdq(A);
}

static void MovupsWpsVps(P) {
  MovdquWdqVdq(A);
}

static void MovupdVpsWps(P) {
  MovdquVdqWdq(A);
}

static void MovupdWpsVps(P) {
  MovdquWdqVdq(A);
}

void OpLddquVdqMdq(P) {
  MovdquVdqWdq(A);
}

void OpMovntiMdqpGdqp(P) {
  if (Rexw(rde)) {
    memcpy(ComputeReserveAddressWrite8(A), XmmRexrReg(m, rde), 8);
  } else {
    memcpy(ComputeReserveAddressWrite4(A), XmmRexrReg(m, rde), 4);
  }
}

static void MovdqaVdqWdq(P) {
  memcpy(XmmRexrReg(m, rde), GetXmmAddress(A), 16);
  if (IsMakingPath(m)) {
    Jitter(A, "z4B"    // 128-bit GetRegOrMem
              "z4C");  // 128-bit PutReg
  }
}

static void MovdqaWdqVdq(P) {
  memcpy(GetXmmAddress(A), XmmRexrReg(m, rde), 16);
  if (IsMakingPath(m)) {
    Jitter(A, "z4A"    // 128-bit GetReg
              "z4D");  // 128-bit PutRegOrMem
  }
}

static void MovntdqMdqVdq(P) {
  MovdqaWdqVdq(A);
}

static void MovntpsMpsVps(P) {
  MovdqaWdqVdq(A);
}

static void MovntpdMpdVpd(P) {
  MovdqaWdqVdq(A);
}

void OpMovntdqaVdqMdq(P) {
  MovdqaVdqWdq(A);
}

static void MovqPqQq(P) {
  memcpy(MmReg(m, rde), GetModrmRegisterMmPointerRead8(A), 8);
}

static void MovqQqPq(P) {
  memcpy(GetModrmRegisterMmPointerWrite8(A), MmReg(m, rde), 8);
}

static void MovqVdqEqp(P) {
  memcpy(XmmRexrReg(m, rde), GetModrmRegisterWordPointerRead8(A), 8);
  memset(XmmRexrReg(m, rde) + 8, 0, 8);
}

static void MovdVdqEd(P) {
  memset(XmmRexrReg(m, rde), 0, 16);
  memcpy(XmmRexrReg(m, rde), GetModrmRegisterWordPointerRead4(A), 4);
}

static void MovqPqEqp(P) {
  memcpy(MmReg(m, rde), GetModrmRegisterWordPointerRead8(A), 8);
}

static void MovdPqEd(P) {
  memcpy(MmReg(m, rde), GetModrmRegisterWordPointerRead4(A), 4);
  memset(MmReg(m, rde) + 4, 0, 4);
}

static void MovdEdVdq(P) {
  if (IsModrmRegister(rde)) {
    Put64(RegRexbRm(m, rde), Read32(XmmRexrReg(m, rde)));
  } else {
    memcpy(ComputeReserveAddressWrite4(A), XmmRexrReg(m, rde), 4);
  }
}

static void MovqEqpVdq(P) {
  memcpy(GetModrmRegisterWordPointerWrite8(A), XmmRexrReg(m, rde), 8);
}

static void MovdEdPq(P) {
  if (IsModrmRegister(rde)) {
    Put64(RegRexbRm(m, rde), Read32(MmReg(m, rde)));
  } else {
    memcpy(ComputeReserveAddressWrite4(A), MmReg(m, rde), 4);
  }
}

static void MovqEqpPq(P) {
  memcpy(GetModrmRegisterWordPointerWrite8(A), MmReg(m, rde), 8);
}

static void MovntqMqPq(P) {
  memcpy(ComputeReserveAddressWrite8(A), MmReg(m, rde), 8);
}

static void MovqVqWq(P) {
  memcpy(XmmRexrReg(m, rde), GetModrmRegisterXmmPointerRead8(A), 8);
  memset(XmmRexrReg(m, rde) + 8, 0, 8);
}

static void MovssVpsWps(P) {
  if (IsModrmRegister(rde)) {
    memcpy(XmmRexrReg(m, rde), XmmRexbRm(m, rde), 4);
  } else {
    memcpy(XmmRexrReg(m, rde), ComputeReserveAddressRead4(A), 4);
    memset(XmmRexrReg(m, rde) + 4, 0, 12);
  }
}

static void MovssWpsVps(P) {
  memcpy(GetModrmRegisterXmmPointerWrite4(A), XmmRexrReg(m, rde), 4);
}

static void MovsdVpsWps(P) {
  if (IsModrmRegister(rde)) {
    memcpy(XmmRexrReg(m, rde), XmmRexbRm(m, rde), 8);
  } else {
    memcpy(XmmRexrReg(m, rde), ComputeReserveAddressRead8(A), 8);
    memset(XmmRexrReg(m, rde) + 8, 0, 8);
    if (IsMakingPath(m)) {
      Jitter(A,
             "z3B"   // res0 = Get.....Mem[force64bit](RexbRm)
             "r1i"   // res1 = 0
             "z4C",  // 128-bit PutReg
             0);
    }
  }
}

static void MovsdWpsVps(P) {
  memcpy(GetModrmRegisterXmmPointerWrite8(A), XmmRexrReg(m, rde), 8);
}

static void MovhlpsVqUq(P) {
  memcpy(XmmRexrReg(m, rde), XmmRexbRm(m, rde) + 8, 8);
}

static void MovlpsVqMq(P) {
  memcpy(XmmRexrReg(m, rde), ComputeReserveAddressRead8(A), 8);
}

static void MovlpdVqMq(P) {
  memcpy(XmmRexrReg(m, rde), ComputeReserveAddressRead8(A), 8);
}

static void MovddupVqWq(P) {
  u8 *src = GetModrmRegisterXmmPointerRead8(A);
  memcpy(XmmRexrReg(m, rde) + 0, src, 8);
  memcpy(XmmRexrReg(m, rde) + 8, src, 8);
}

static void MovsldupVqWq(P) {
  u8 *dst, *src;
  dst = XmmRexrReg(m, rde);
  src = GetModrmRegisterXmmPointerRead16(A);
  memcpy(dst + 0 + 0, src + 0, 4);
  memcpy(dst + 0 + 4, src + 0, 4);
  memcpy(dst + 8 + 0, src + 8, 4);
  memcpy(dst + 8 + 4, src + 8, 4);
}

static void MovlpsMqVq(P) {
  memcpy(ComputeReserveAddressWrite8(A), XmmRexrReg(m, rde), 8);
}

static void MovlpdMqVq(P) {
  memcpy(ComputeReserveAddressWrite8(A), XmmRexrReg(m, rde), 8);
}

static void MovlhpsVqUq(P) {
  memcpy(XmmRexrReg(m, rde) + 8, XmmRexbRm(m, rde), 8);
}

static void MovhpsVqMq(P) {
  memcpy(XmmRexrReg(m, rde) + 8, ComputeReserveAddressRead8(A), 8);
}

static void MovhpdVqMq(P) {
  memcpy(XmmRexrReg(m, rde) + 8, ComputeReserveAddressRead8(A), 8);
}

static void MovshdupVqWq(P) {
  u8 *dst, *src;
  dst = XmmRexrReg(m, rde);
  src = GetModrmRegisterXmmPointerRead16(A);
  memcpy(dst + 0 + 0, src + 04, 4);
  memcpy(dst + 0 + 4, src + 04, 4);
  memcpy(dst + 8 + 0, src + 12, 4);
  memcpy(dst + 8 + 4, src + 12, 4);
}

static void MovhpsMqVq(P) {
  memcpy(ComputeReserveAddressWrite8(A), XmmRexrReg(m, rde) + 8, 8);
}

static void MovhpdMqVq(P) {
  memcpy(ComputeReserveAddressWrite8(A), XmmRexrReg(m, rde) + 8, 8);
}

static void MovqWqVq(P) {
  if (IsModrmRegister(rde)) {
    memcpy(XmmRexbRm(m, rde), XmmRexrReg(m, rde), 8);
    memset(XmmRexbRm(m, rde) + 8, 0, 8);
  } else {
    memcpy(ComputeReserveAddressWrite8(A), XmmRexrReg(m, rde), 8);
  }
}

static void Movq2dqVdqNq(P) {
  memcpy(XmmRexrReg(m, rde), MmRm(m, rde), 8);
  memset(XmmRexrReg(m, rde) + 8, 0, 8);
}

static void Movdq2qPqUq(P) {
  memcpy(MmReg(m, rde), XmmRexbRm(m, rde), 8);
}

static void MovapsVpsWps(P) {
  MovdqaVdqWdq(A);
}

static void MovapdVpdWpd(P) {
  MovdqaVdqWdq(A);
}

static void MovapsWpsVps(P) {
  MovdqaWdqVdq(A);
}

static void MovapdWpdVpd(P) {
  MovdqaWdqVdq(A);
}

void OpMovWpsVps(P) {
  switch (Rep(rde) | Osz(rde)) {
    case 0:
      MovupsWpsVps(A);
      break;
    case 1:
      MovupdWpsVps(A);
      break;
    case 2:
      MovsdWpsVps(A);
      break;
    case 3:
      MovssWpsVps(A);
      break;
    default:
      __builtin_unreachable();
  }
}

void OpMov0f28(P) {
  if (!Osz(rde)) {
    MovapsVpsWps(A);
  } else {
    MovapdVpdWpd(A);
  }
}

void OpMov0f6e(P) {
  if (Osz(rde)) {
    if (Rexw(rde)) {
      MovqVdqEqp(A);
    } else {
      MovdVdqEd(A);
    }
  } else {
    if (Rexw(rde)) {
      MovqPqEqp(A);
    } else {
      MovdPqEd(A);
    }
  }
}

void OpMov0f6f(P) {
  if (Osz(rde)) {
    MovdqaVdqWdq(A);
  } else if (Rep(rde) == 3) {
    MovdquVdqWdq(A);
  } else {
    MovqPqQq(A);
  }
}

void OpMov0fE7(P) {
  if (!Osz(rde)) {
    MovntqMqPq(A);
  } else {
    MovntdqMdqVdq(A);
  }
}

void OpMov0f7e(P) {
  if (Rep(rde) == 3) {
    MovqVqWq(A);
  } else if (Osz(rde)) {
    if (Rexw(rde)) {
      MovqEqpVdq(A);
    } else {
      MovdEdVdq(A);
    }
  } else {
    if (Rexw(rde)) {
      MovqEqpPq(A);
    } else {
      MovdEdPq(A);
    }
  }
}

void OpMov0f7f(P) {
  if (Rep(rde) == 3) {
    MovdquWdqVdq(A);
  } else if (Osz(rde)) {
    MovdqaWdqVdq(A);
  } else {
    MovqQqPq(A);
  }
}

void OpMov0f10(P) {
  switch (Rep(rde) | Osz(rde)) {
    case 0:
      MovupsVpsWps(A);
      break;
    case 1:
      MovupdVpsWps(A);
      break;
    case 2:
      MovsdVpsWps(A);
      break;
    case 3:
      MovssVpsWps(A);
      break;
    default:
      __builtin_unreachable();
  }
}

void OpMov0f29(P) {
  if (!Osz(rde)) {
    MovapsWpsVps(A);
  } else {
    MovapdWpdVpd(A);
  }
}

void OpMov0f2b(P) {
  if (!Osz(rde)) {
    MovntpsMpsVps(A);
  } else {
    MovntpdMpdVpd(A);
  }
}

void OpMov0f12(P) {
  switch (Rep(rde) | Osz(rde)) {
    case 0:
      if (IsModrmRegister(rde)) {
        MovhlpsVqUq(A);
      } else {
        MovlpsVqMq(A);
      }
      break;
    case 1:
      MovlpdVqMq(A);
      break;
    case 2:
      MovddupVqWq(A);
      break;
    case 3:
      MovsldupVqWq(A);
      break;
    default:
      __builtin_unreachable();
  }
}

void OpMov0f13(P) {
  if (Osz(rde)) {
    MovlpdMqVq(A);
  } else {
    MovlpsMqVq(A);
  }
}

void OpMov0f16(P) {
  switch (Rep(rde) | Osz(rde)) {
    case 0:
      if (IsModrmRegister(rde)) {
        MovlhpsVqUq(A);
      } else {
        MovhpsVqMq(A);
      }
      break;
    case 1:
      MovhpdVqMq(A);
      break;
    case 3:
      MovshdupVqWq(A);
      break;
    default:
      OpUdImpl(m);
      break;
  }
}

void OpMov0f17(P) {
  if (Osz(rde)) {
    MovhpdMqVq(A);
  } else {
    MovhpsMqVq(A);
  }
}

void OpMov0fD6(P) {
  if (Rep(rde) == 3) {
    Movq2dqVdqNq(A);
  } else if (Rep(rde) == 2) {
    Movdq2qPqUq(A);
  } else if (Osz(rde)) {
    MovqWqVq(A);
  } else {
    OpUdImpl(m);
  }
}

void OpPmovmskbGdqpNqUdq(P) {
  Put64(RegRexrReg(m, rde),
        pmovmskb(XmmRexbRm(m, rde)) & (Osz(rde) ? 0xffff : 0xff));
}

void OpMaskMovDiXmmRegXmmRm(P) {
  void *p[2];
  u64 v;
  unsigned i, n;
  u8 *mem, b[16];
  v = AddressDi(A);
  n = Osz(rde) ? 16 : 8;
  mem = BeginStore(m, v, n, p, b);
  for (i = 0; i < n; ++i) {
    if (XmmRexbRm(m, rde)[i] & 0x80) {
      mem[i] = XmmRexrReg(m, rde)[i];
    }
  }
  EndStore(m, v, n, p, b);
}
