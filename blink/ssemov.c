/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
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

#include "blink/biosrom.h"
#include "blink/builtin.h"
#include "blink/endian.h"
#include "blink/intrin.h"
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/stats.h"
#include "blink/tsan.h"

static u32 pmovmskb(const u8 p[16]) {
#if X86_INTRINSICS
  return __builtin_ia32_pmovmskb128(*(char_xmma_t *)p);
#else
  u32 i, m;
  for (m = i = 0; i < 16; ++i) {
    if (p[i] & 0x80) m |= 1 << i;
  }
  return m;
#endif
}

static void MovdquVdqWdq(P) {
  IGNORE_RACES_START();
  memcpy(XmmRexrReg(m, rde), GetModrmRegisterXmmPointerRead16(A), 16);
  IGNORE_RACES_END();
  if (IsMakingPath(m)) {
    Jitter(A, "z4B"    // 128-bit GetRegOrMem
              "z4C");  // 128-bit PutReg
  }
}

static void MovdquWdqVdq(P) {
  IGNORE_RACES_START();
  memcpy(GetModrmRegisterXmmPointerWrite16(A), XmmRexrReg(m, rde), 16);
  IGNORE_RACES_END();
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
  IGNORE_RACES_START();
  if (Rexw(rde)) {
    memcpy(ComputeReserveAddressWrite8(A), XmmRexrReg(m, rde), 8);
  } else {
    memcpy(ComputeReserveAddressWrite4(A), XmmRexrReg(m, rde), 4);
  }
  IGNORE_RACES_END();
}

static void MovdqaVdqWdq(P) {
  IGNORE_RACES_START();
  memcpy(XmmRexrReg(m, rde), GetXmmAddress(A), 16);
  IGNORE_RACES_END();
  if (IsMakingPath(m)) {
    Jitter(A, "z4B"    // 128-bit GetRegOrMem
              "z4C");  // 128-bit PutReg
  }
}

static void MovdqaWdqVdq(P) {
  u8 *dst;
  IGNORE_RACES_START();
  dst = GetXmmAddress(A);
  if (!IsRomAddress(m, dst)) memcpy(dst, XmmRexrReg(m, rde), 16);
  IGNORE_RACES_END();
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
  IGNORE_RACES_START();
  memcpy(MmReg(m, rde), GetModrmRegisterMmPointerRead8(A), 8);
  IGNORE_RACES_END();
}

static void MovqQqPq(P) {
  IGNORE_RACES_START();
  memcpy(GetModrmRegisterMmPointerWrite8(A), MmReg(m, rde), 8);
  IGNORE_RACES_END();
}

static void MovqVdqEqp(P) {
  IGNORE_RACES_START();
  memcpy(XmmRexrReg(m, rde), GetModrmRegisterWordPointerRead8(A), 8);
  IGNORE_RACES_END();
  memset(XmmRexrReg(m, rde) + 8, 0, 8);
  if (IsMakingPath(m)) {
    Jitter(A,
           "z3B"   // res0 = GetRegOrMem[force64bit](RexbRm)
           "r1i"   // res1 = zero
           "z4C",  // PutReg[force128bit](RexrReg, res0, res1)
           0);
  }
}

static void MovdVdqEd(P) {
  memset(XmmRexrReg(m, rde), 0, 16);
  IGNORE_RACES_START();
  memcpy(XmmRexrReg(m, rde), GetModrmRegisterWordPointerRead4(A), 4);
  IGNORE_RACES_END();
}

static void MovqPqEqp(P) {
  IGNORE_RACES_START();
  memcpy(MmReg(m, rde), GetModrmRegisterWordPointerRead8(A), 8);
  IGNORE_RACES_END();
}

static void MovdPqEd(P) {
  IGNORE_RACES_START();
  memcpy(MmReg(m, rde), GetModrmRegisterWordPointerRead4(A), 4);
  IGNORE_RACES_END();
  memset(MmReg(m, rde) + 4, 0, 4);
}

static void MovdEdVdq(P) {
  if (IsModrmRegister(rde)) {
    Put64(RegRexbRm(m, rde), Read32(XmmRexrReg(m, rde)));
  } else {
    IGNORE_RACES_START();
    memcpy(ComputeReserveAddressWrite4(A), XmmRexrReg(m, rde), 4);
    IGNORE_RACES_END();
  }
}

static void MovqEqpVdq(P) {
  IGNORE_RACES_START();
  memcpy(GetModrmRegisterWordPointerWrite8(A), XmmRexrReg(m, rde), 8);
  IGNORE_RACES_END();
  if (IsMakingPath(m)) {
    Jitter(A, "z4A"      // res0,res1 = GetReg[force128bit](RexrReg)
              "r0z3D");  // PutRegOrMem[force64bit](RexbRm, res0)
  }
}

static void MovdEdPq(P) {
  if (IsModrmRegister(rde)) {
    Put64(RegRexbRm(m, rde), Read32(MmReg(m, rde)));
  } else {
    IGNORE_RACES_START();
    memcpy(ComputeReserveAddressWrite4(A), MmReg(m, rde), 4);
    IGNORE_RACES_END();
  }
}

static void MovqEqpPq(P) {
  IGNORE_RACES_START();
  memcpy(GetModrmRegisterWordPointerWrite8(A), MmReg(m, rde), 8);
  IGNORE_RACES_END();
}

static void MovntqMqPq(P) {
  IGNORE_RACES_START();
  memcpy(ComputeReserveAddressWrite8(A), MmReg(m, rde), 8);
  IGNORE_RACES_END();
}

static void MovqVqWq(P) {
  IGNORE_RACES_START();
  memcpy(XmmRexrReg(m, rde), GetModrmRegisterXmmPointerRead8(A), 8);
  IGNORE_RACES_END();
  memset(XmmRexrReg(m, rde) + 8, 0, 8);
}

static void MovssVpsWps(P) {
  if (IsModrmRegister(rde)) {
    memcpy(XmmRexrReg(m, rde), XmmRexbRm(m, rde), 4);
  } else {
    IGNORE_RACES_START();
    memcpy(XmmRexrReg(m, rde), ComputeReserveAddressRead4(A), 4);
    IGNORE_RACES_END();
    memset(XmmRexrReg(m, rde) + 4, 0, 12);
  }
}

static void MovssWpsVps(P) {
  IGNORE_RACES_START();
  memcpy(GetModrmRegisterXmmPointerWrite4(A), XmmRexrReg(m, rde), 4);
  IGNORE_RACES_END();
}

static void MovsdVpsWps(P) {
  if (IsModrmRegister(rde)) {
    memcpy(XmmRexrReg(m, rde), XmmRexbRm(m, rde), 8);
  } else {
    IGNORE_RACES_START();
    memcpy(XmmRexrReg(m, rde), ComputeReserveAddressRead8(A), 8);
    IGNORE_RACES_END();
    memset(XmmRexrReg(m, rde) + 8, 0, 8);
    if (IsMakingPath(m)) {
      Jitter(A,
             "z3B"   // res0 = Get.....Mem[force64bit](RexbRm)
             "r1i"   // res1 = 0
             "z4C",  // 128-bit PutReg
             (u64)0);
    }
  }
}

static void MovsdWpsVps(P) {
  IGNORE_RACES_START();
  memcpy(GetModrmRegisterXmmPointerWrite8(A), XmmRexrReg(m, rde), 8);
  IGNORE_RACES_END();
  if (IsMakingPath(m)) {
    Jitter(A,
           "z4P"    // res0 = GetXmmOrMemPointer(RexbRm)
           "a2i"    // arg2 = RexrReg(rde)
           "s0a1="  // arg1 = machine
           "t"      // arg0 = res0
           "m",     // call function (d1)
           RexrReg(rde), MovsdWpsVpsOp);
  }
}

static void MovhlpsVqUq(P) {
  memcpy(XmmRexrReg(m, rde), XmmRexbRm(m, rde) + 8, 8);
}

static void MovlpsVqMq(P) {
  IGNORE_RACES_START();
  memcpy(XmmRexrReg(m, rde), ComputeReserveAddressRead8(A), 8);
  IGNORE_RACES_END();
}

static void MovlpdVqMq(P) {
  IGNORE_RACES_START();
  memcpy(XmmRexrReg(m, rde), ComputeReserveAddressRead8(A), 8);
  IGNORE_RACES_END();
}

static void MovddupVqWq(P) {
  u8 *src = GetModrmRegisterXmmPointerRead8(A);
  IGNORE_RACES_START();
  memcpy(XmmRexrReg(m, rde) + 0, src, 8);
  memcpy(XmmRexrReg(m, rde) + 8, src, 8);
  IGNORE_RACES_END();
}

static void MovsldupVqWq(P) {
  u8 *dst, *src;
  dst = XmmRexrReg(m, rde);
  src = GetModrmRegisterXmmPointerRead16(A);
  IGNORE_RACES_START();
  memcpy(dst + 0 + 0, src + 0, 4);
  memcpy(dst + 0 + 4, src + 0, 4);
  memcpy(dst + 8 + 0, src + 8, 4);
  memcpy(dst + 8 + 4, src + 8, 4);
  IGNORE_RACES_END();
}

static void MovlpsMqVq(P) {
  IGNORE_RACES_START();
  memcpy(ComputeReserveAddressWrite8(A), XmmRexrReg(m, rde), 8);
  IGNORE_RACES_END();
}

static void MovlpdMqVq(P) {
  IGNORE_RACES_START();
  memcpy(ComputeReserveAddressWrite8(A), XmmRexrReg(m, rde), 8);
  IGNORE_RACES_END();
}

static void MovlhpsVqUq(P) {
  memcpy(XmmRexrReg(m, rde) + 8, XmmRexbRm(m, rde), 8);
}

static void MovhpsVqMq(P) {
  IGNORE_RACES_START();
  memcpy(XmmRexrReg(m, rde) + 8, ComputeReserveAddressRead8(A), 8);
  IGNORE_RACES_END();
}

static void MovhpdVqMq(P) {
  IGNORE_RACES_START();
  memcpy(XmmRexrReg(m, rde) + 8, ComputeReserveAddressRead8(A), 8);
  IGNORE_RACES_END();
}

static void MovshdupVqWq(P) {
  u8 *dst, *src;
  dst = XmmRexrReg(m, rde);
  src = GetModrmRegisterXmmPointerRead16(A);
  IGNORE_RACES_START();
  memcpy(dst + 0 + 0, src + 04, 4);
  memcpy(dst + 0 + 4, src + 04, 4);
  memcpy(dst + 8 + 0, src + 12, 4);
  memcpy(dst + 8 + 4, src + 12, 4);
  IGNORE_RACES_END();
}

static void MovhpsMqVq(P) {
  IGNORE_RACES_START();
  memcpy(ComputeReserveAddressWrite8(A), XmmRexrReg(m, rde) + 8, 8);
  IGNORE_RACES_END();
}

static void MovhpdMqVq(P) {
  IGNORE_RACES_START();
  memcpy(ComputeReserveAddressWrite8(A), XmmRexrReg(m, rde) + 8, 8);
  IGNORE_RACES_END();
}

static void MovqWqVq(P) {
  if (IsModrmRegister(rde)) {
    memcpy(XmmRexbRm(m, rde), XmmRexrReg(m, rde), 8);
    memset(XmmRexbRm(m, rde) + 8, 0, 8);
  } else {
    IGNORE_RACES_START();
    memcpy(ComputeReserveAddressWrite8(A), XmmRexrReg(m, rde), 8);
    IGNORE_RACES_END();
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
      MovsdWpsVps(A);  // hot
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
