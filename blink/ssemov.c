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

#include "blink/address.h"
#include "blink/builtin.h"
#include "blink/endian.h"
#include "blink/memory.h"
#include "blink/modrm.h"
#include "blink/ssemov.h"

static u32 pmovmskb(const u8 p[16]) {
  u32 i, m;
  for (m = i = 0; i < 16; ++i) {
    if (p[i] & 0x80) m |= 1 << i;
  }
  return m;
}

static void MovdquVdqWdq(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(XmmRexrReg(m, rde),
         GetModrmRegisterXmmPointerRead16(m, DISPATCH_ARGUMENTS), 16);
}

static void MovdquWdqVdq(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(GetModrmRegisterXmmPointerWrite16(m, DISPATCH_ARGUMENTS),
         XmmRexrReg(m, rde), 16);
}

static void MovupsVpsWps(struct Machine *m, DISPATCH_PARAMETERS) {
  MovdquVdqWdq(m, DISPATCH_ARGUMENTS);
}

static void MovupsWpsVps(struct Machine *m, DISPATCH_PARAMETERS) {
  MovdquWdqVdq(m, DISPATCH_ARGUMENTS);
}

static void MovupdVpsWps(struct Machine *m, DISPATCH_PARAMETERS) {
  MovdquVdqWdq(m, DISPATCH_ARGUMENTS);
}

static void MovupdWpsVps(struct Machine *m, DISPATCH_PARAMETERS) {
  MovdquWdqVdq(m, DISPATCH_ARGUMENTS);
}

void OpLddquVdqMdq(struct Machine *m, DISPATCH_PARAMETERS) {
  MovdquVdqWdq(m, DISPATCH_ARGUMENTS);
}

void OpMovntiMdqpGdqp(struct Machine *m, DISPATCH_PARAMETERS) {
  if (Rexw(rde)) {
    memcpy(ComputeReserveAddressWrite8(m, DISPATCH_ARGUMENTS),
           XmmRexrReg(m, rde), 8);
  } else {
    memcpy(ComputeReserveAddressWrite4(m, DISPATCH_ARGUMENTS),
           XmmRexrReg(m, rde), 4);
  }
}

static void MovdqaVdqMdq(struct Machine *m, DISPATCH_PARAMETERS) {
  i64 v;
  u8 *p;
  v = ComputeAddress(m, DISPATCH_ARGUMENTS);
  SetReadAddr(m, v, 16);
  if ((v & 15) || !(p = FindReal(m, v))) {
    ThrowSegmentationFault(m, v);
  }
  memcpy(XmmRexrReg(m, rde), p, 16);
}

static void MovdqaMdqVdq(struct Machine *m, DISPATCH_PARAMETERS) {
  i64 v;
  u8 *p;
  v = ComputeAddress(m, DISPATCH_ARGUMENTS);
  SetWriteAddr(m, v, 16);
  if ((v & 15) || !(p = FindReal(m, v))) {
    ThrowSegmentationFault(m, v);
  }
  memcpy(p, XmmRexrReg(m, rde), 16);
}

static void MovdqaVdqWdq(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsModrmRegister(rde)) {
    memcpy(XmmRexrReg(m, rde), XmmRexbRm(m, rde), 16);
  } else {
    MovdqaVdqMdq(m, DISPATCH_ARGUMENTS);
  }
}

static void MovdqaWdqVdq(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsModrmRegister(rde)) {
    memcpy(XmmRexbRm(m, rde), XmmRexrReg(m, rde), 16);
  } else {
    MovdqaMdqVdq(m, DISPATCH_ARGUMENTS);
  }
}

static void MovntdqMdqVdq(struct Machine *m, DISPATCH_PARAMETERS) {
  MovdqaMdqVdq(m, DISPATCH_ARGUMENTS);
}

static void MovntpsMpsVps(struct Machine *m, DISPATCH_PARAMETERS) {
  MovdqaMdqVdq(m, DISPATCH_ARGUMENTS);
}

static void MovntpdMpdVpd(struct Machine *m, DISPATCH_PARAMETERS) {
  MovdqaMdqVdq(m, DISPATCH_ARGUMENTS);
}

void OpMovntdqaVdqMdq(struct Machine *m, DISPATCH_PARAMETERS) {
  MovdqaVdqMdq(m, DISPATCH_ARGUMENTS);
}

static void MovqPqQq(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(MmReg(m, rde), GetModrmRegisterMmPointerRead8(m, DISPATCH_ARGUMENTS),
         8);
}

static void MovqQqPq(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(GetModrmRegisterMmPointerWrite8(m, DISPATCH_ARGUMENTS), MmReg(m, rde),
         8);
}

static void MovqVdqEqp(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(XmmRexrReg(m, rde),
         GetModrmRegisterWordPointerRead8(m, DISPATCH_ARGUMENTS), 8);
  memset(XmmRexrReg(m, rde) + 8, 0, 8);
}

static void MovdVdqEd(struct Machine *m, DISPATCH_PARAMETERS) {
  memset(XmmRexrReg(m, rde), 0, 16);
  memcpy(XmmRexrReg(m, rde),
         GetModrmRegisterWordPointerRead4(m, DISPATCH_ARGUMENTS), 4);
}

static void MovqPqEqp(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(MmReg(m, rde), GetModrmRegisterWordPointerRead8(m, DISPATCH_ARGUMENTS),
         8);
}

static void MovdPqEd(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(MmReg(m, rde), GetModrmRegisterWordPointerRead4(m, DISPATCH_ARGUMENTS),
         4);
  memset(MmReg(m, rde) + 4, 0, 4);
}

static void MovdEdVdq(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsModrmRegister(rde)) {
    Put64(RegRexbRm(m, rde), Read32(XmmRexrReg(m, rde)));
  } else {
    memcpy(ComputeReserveAddressWrite4(m, DISPATCH_ARGUMENTS),
           XmmRexrReg(m, rde), 4);
  }
}

static void MovqEqpVdq(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(GetModrmRegisterWordPointerWrite8(m, DISPATCH_ARGUMENTS),
         XmmRexrReg(m, rde), 8);
}

static void MovdEdPq(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsModrmRegister(rde)) {
    Put64(RegRexbRm(m, rde), Read32(MmReg(m, rde)));
  } else {
    memcpy(ComputeReserveAddressWrite4(m, DISPATCH_ARGUMENTS), MmReg(m, rde),
           4);
  }
}

static void MovqEqpPq(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(GetModrmRegisterWordPointerWrite8(m, DISPATCH_ARGUMENTS),
         MmReg(m, rde), 8);
}

static void MovntqMqPq(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(ComputeReserveAddressWrite8(m, DISPATCH_ARGUMENTS), MmReg(m, rde), 8);
}

static void MovqVqWq(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(XmmRexrReg(m, rde),
         GetModrmRegisterXmmPointerRead8(m, DISPATCH_ARGUMENTS), 8);
  memset(XmmRexrReg(m, rde) + 8, 0, 8);
}

static void MovssVpsWps(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsModrmRegister(rde)) {
    memcpy(XmmRexrReg(m, rde), XmmRexbRm(m, rde), 4);
  } else {
    memcpy(XmmRexrReg(m, rde),
           ComputeReserveAddressRead4(m, DISPATCH_ARGUMENTS), 4);
    memset(XmmRexrReg(m, rde) + 4, 0, 12);
  }
}

static void MovssWpsVps(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(GetModrmRegisterXmmPointerWrite4(m, DISPATCH_ARGUMENTS),
         XmmRexrReg(m, rde), 4);
}

static void MovsdVpsWps(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsModrmRegister(rde)) {
    memcpy(XmmRexrReg(m, rde), XmmRexbRm(m, rde), 8);
  } else {
    memcpy(XmmRexrReg(m, rde),
           ComputeReserveAddressRead8(m, DISPATCH_ARGUMENTS), 8);
    memset(XmmRexrReg(m, rde) + 8, 0, 8);
  }
}

static void MovsdWpsVps(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(GetModrmRegisterXmmPointerWrite8(m, DISPATCH_ARGUMENTS),
         XmmRexrReg(m, rde), 8);
}

static void MovhlpsVqUq(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(XmmRexrReg(m, rde), XmmRexbRm(m, rde) + 8, 8);
}

static void MovlpsVqMq(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(XmmRexrReg(m, rde), ComputeReserveAddressRead8(m, DISPATCH_ARGUMENTS),
         8);
}

static void MovlpdVqMq(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(XmmRexrReg(m, rde), ComputeReserveAddressRead8(m, DISPATCH_ARGUMENTS),
         8);
}

static void MovddupVqWq(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 *src;
  src = GetModrmRegisterXmmPointerRead8(m, DISPATCH_ARGUMENTS);
  memcpy(XmmRexrReg(m, rde) + 0, src, 8);
  memcpy(XmmRexrReg(m, rde) + 8, src, 8);
}

static void MovsldupVqWq(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 *dst, *src;
  dst = XmmRexrReg(m, rde);
  src = GetModrmRegisterXmmPointerRead16(m, DISPATCH_ARGUMENTS);
  memcpy(dst + 0 + 0, src + 0, 4);
  memcpy(dst + 0 + 4, src + 0, 4);
  memcpy(dst + 8 + 0, src + 8, 4);
  memcpy(dst + 8 + 4, src + 8, 4);
}

static void MovlpsMqVq(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(ComputeReserveAddressWrite8(m, DISPATCH_ARGUMENTS), XmmRexrReg(m, rde),
         8);
}

static void MovlpdMqVq(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(ComputeReserveAddressWrite8(m, DISPATCH_ARGUMENTS), XmmRexrReg(m, rde),
         8);
}

static void MovlhpsVqUq(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(XmmRexrReg(m, rde) + 8, XmmRexbRm(m, rde), 8);
}

static void MovhpsVqMq(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(XmmRexrReg(m, rde) + 8,
         ComputeReserveAddressRead8(m, DISPATCH_ARGUMENTS), 8);
}

static void MovhpdVqMq(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(XmmRexrReg(m, rde) + 8,
         ComputeReserveAddressRead8(m, DISPATCH_ARGUMENTS), 8);
}

static void MovshdupVqWq(struct Machine *m, DISPATCH_PARAMETERS) {
  u8 *dst, *src;
  dst = XmmRexrReg(m, rde);
  src = GetModrmRegisterXmmPointerRead16(m, DISPATCH_ARGUMENTS);
  memcpy(dst + 0 + 0, src + 04, 4);
  memcpy(dst + 0 + 4, src + 04, 4);
  memcpy(dst + 8 + 0, src + 12, 4);
  memcpy(dst + 8 + 4, src + 12, 4);
}

static void MovhpsMqVq(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(ComputeReserveAddressWrite8(m, DISPATCH_ARGUMENTS),
         XmmRexrReg(m, rde) + 8, 8);
}

static void MovhpdMqVq(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(ComputeReserveAddressWrite8(m, DISPATCH_ARGUMENTS),
         XmmRexrReg(m, rde) + 8, 8);
}

static void MovqWqVq(struct Machine *m, DISPATCH_PARAMETERS) {
  if (IsModrmRegister(rde)) {
    memcpy(XmmRexbRm(m, rde), XmmRexrReg(m, rde), 8);
    memset(XmmRexbRm(m, rde) + 8, 0, 8);
  } else {
    memcpy(ComputeReserveAddressWrite8(m, DISPATCH_ARGUMENTS),
           XmmRexrReg(m, rde), 8);
  }
}

static void Movq2dqVdqNq(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(XmmRexrReg(m, rde), MmRm(m, rde), 8);
  memset(XmmRexrReg(m, rde) + 8, 0, 8);
}

static void Movdq2qPqUq(struct Machine *m, DISPATCH_PARAMETERS) {
  memcpy(MmReg(m, rde), XmmRexbRm(m, rde), 8);
}

static void MovapsVpsWps(struct Machine *m, DISPATCH_PARAMETERS) {
  MovdqaVdqWdq(m, DISPATCH_ARGUMENTS);
}

static void MovapdVpdWpd(struct Machine *m, DISPATCH_PARAMETERS) {
  MovdqaVdqWdq(m, DISPATCH_ARGUMENTS);
}

static void MovapsWpsVps(struct Machine *m, DISPATCH_PARAMETERS) {
  MovdqaWdqVdq(m, DISPATCH_ARGUMENTS);
}

static void MovapdWpdVpd(struct Machine *m, DISPATCH_PARAMETERS) {
  MovdqaWdqVdq(m, DISPATCH_ARGUMENTS);
}

void OpMovWpsVps(struct Machine *m, DISPATCH_PARAMETERS) {
  switch (Rep(rde) | Osz(rde)) {
    case 0:
      MovupsWpsVps(m, DISPATCH_ARGUMENTS);
      break;
    case 1:
      MovupdWpsVps(m, DISPATCH_ARGUMENTS);
      break;
    case 2:
      MovsdWpsVps(m, DISPATCH_ARGUMENTS);
      break;
    case 3:
      MovssWpsVps(m, DISPATCH_ARGUMENTS);
      break;
    default:
      __builtin_unreachable();
  }
}

void OpMov0f28(struct Machine *m, DISPATCH_PARAMETERS) {
  if (!Osz(rde)) {
    MovapsVpsWps(m, DISPATCH_ARGUMENTS);
  } else {
    MovapdVpdWpd(m, DISPATCH_ARGUMENTS);
  }
}

void OpMov0f6e(struct Machine *m, DISPATCH_PARAMETERS) {
  if (Osz(rde)) {
    if (Rexw(rde)) {
      MovqVdqEqp(m, DISPATCH_ARGUMENTS);
    } else {
      MovdVdqEd(m, DISPATCH_ARGUMENTS);
    }
  } else {
    if (Rexw(rde)) {
      MovqPqEqp(m, DISPATCH_ARGUMENTS);
    } else {
      MovdPqEd(m, DISPATCH_ARGUMENTS);
    }
  }
}

void OpMov0f6f(struct Machine *m, DISPATCH_PARAMETERS) {
  if (Osz(rde)) {
    MovdqaVdqWdq(m, DISPATCH_ARGUMENTS);
  } else if (Rep(rde) == 3) {
    MovdquVdqWdq(m, DISPATCH_ARGUMENTS);
  } else {
    MovqPqQq(m, DISPATCH_ARGUMENTS);
  }
}

void OpMov0fE7(struct Machine *m, DISPATCH_PARAMETERS) {
  if (!Osz(rde)) {
    MovntqMqPq(m, DISPATCH_ARGUMENTS);
  } else {
    MovntdqMdqVdq(m, DISPATCH_ARGUMENTS);
  }
}

void OpMov0f7e(struct Machine *m, DISPATCH_PARAMETERS) {
  if (Rep(rde) == 3) {
    MovqVqWq(m, DISPATCH_ARGUMENTS);
  } else if (Osz(rde)) {
    if (Rexw(rde)) {
      MovqEqpVdq(m, DISPATCH_ARGUMENTS);
    } else {
      MovdEdVdq(m, DISPATCH_ARGUMENTS);
    }
  } else {
    if (Rexw(rde)) {
      MovqEqpPq(m, DISPATCH_ARGUMENTS);
    } else {
      MovdEdPq(m, DISPATCH_ARGUMENTS);
    }
  }
}

void OpMov0f7f(struct Machine *m, DISPATCH_PARAMETERS) {
  if (Rep(rde) == 3) {
    MovdquWdqVdq(m, DISPATCH_ARGUMENTS);
  } else if (Osz(rde)) {
    MovdqaWdqVdq(m, DISPATCH_ARGUMENTS);
  } else {
    MovqQqPq(m, DISPATCH_ARGUMENTS);
  }
}

void OpMov0f10(struct Machine *m, DISPATCH_PARAMETERS) {
  switch (Rep(rde) | Osz(rde)) {
    case 0:
      MovupsVpsWps(m, DISPATCH_ARGUMENTS);
      break;
    case 1:
      MovupdVpsWps(m, DISPATCH_ARGUMENTS);
      break;
    case 2:
      MovsdVpsWps(m, DISPATCH_ARGUMENTS);
      break;
    case 3:
      MovssVpsWps(m, DISPATCH_ARGUMENTS);
      break;
    default:
      __builtin_unreachable();
  }
}

void OpMov0f29(struct Machine *m, DISPATCH_PARAMETERS) {
  if (!Osz(rde)) {
    MovapsWpsVps(m, DISPATCH_ARGUMENTS);
  } else {
    MovapdWpdVpd(m, DISPATCH_ARGUMENTS);
  }
}

void OpMov0f2b(struct Machine *m, DISPATCH_PARAMETERS) {
  if (!Osz(rde)) {
    MovntpsMpsVps(m, DISPATCH_ARGUMENTS);
  } else {
    MovntpdMpdVpd(m, DISPATCH_ARGUMENTS);
  }
}

void OpMov0f12(struct Machine *m, DISPATCH_PARAMETERS) {
  switch (Rep(rde) | Osz(rde)) {
    case 0:
      if (IsModrmRegister(rde)) {
        MovhlpsVqUq(m, DISPATCH_ARGUMENTS);
      } else {
        MovlpsVqMq(m, DISPATCH_ARGUMENTS);
      }
      break;
    case 1:
      MovlpdVqMq(m, DISPATCH_ARGUMENTS);
      break;
    case 2:
      MovddupVqWq(m, DISPATCH_ARGUMENTS);
      break;
    case 3:
      MovsldupVqWq(m, DISPATCH_ARGUMENTS);
      break;
    default:
      __builtin_unreachable();
  }
}

void OpMov0f13(struct Machine *m, DISPATCH_PARAMETERS) {
  if (Osz(rde)) {
    MovlpdMqVq(m, DISPATCH_ARGUMENTS);
  } else {
    MovlpsMqVq(m, DISPATCH_ARGUMENTS);
  }
}

void OpMov0f16(struct Machine *m, DISPATCH_PARAMETERS) {
  switch (Rep(rde) | Osz(rde)) {
    case 0:
      if (IsModrmRegister(rde)) {
        MovlhpsVqUq(m, DISPATCH_ARGUMENTS);
      } else {
        MovhpsVqMq(m, DISPATCH_ARGUMENTS);
      }
      break;
    case 1:
      MovhpdVqMq(m, DISPATCH_ARGUMENTS);
      break;
    case 3:
      MovshdupVqWq(m, DISPATCH_ARGUMENTS);
      break;
    default:
      OpUdImpl(m);
      break;
  }
}

void OpMov0f17(struct Machine *m, DISPATCH_PARAMETERS) {
  if (Osz(rde)) {
    MovhpdMqVq(m, DISPATCH_ARGUMENTS);
  } else {
    MovhpsMqVq(m, DISPATCH_ARGUMENTS);
  }
}

void OpMov0fD6(struct Machine *m, DISPATCH_PARAMETERS) {
  if (Rep(rde) == 3) {
    Movq2dqVdqNq(m, DISPATCH_ARGUMENTS);
  } else if (Rep(rde) == 2) {
    Movdq2qPqUq(m, DISPATCH_ARGUMENTS);
  } else if (Osz(rde)) {
    MovqWqVq(m, DISPATCH_ARGUMENTS);
  } else {
    OpUdImpl(m);
  }
}

void OpPmovmskbGdqpNqUdq(struct Machine *m, DISPATCH_PARAMETERS) {
  Put64(RegRexrReg(m, rde),
        pmovmskb(XmmRexbRm(m, rde)) & (Osz(rde) ? 0xffff : 0xff));
}

void OpMaskMovDiXmmRegXmmRm(struct Machine *m, DISPATCH_PARAMETERS) {
  void *p[2];
  u64 v;
  unsigned i, n;
  u8 *mem, b[16];
  v = AddressDi(m, DISPATCH_ARGUMENTS);
  n = Osz(rde) ? 16 : 8;
  mem = BeginStore(m, v, n, p, b);
  for (i = 0; i < n; ++i) {
    if (XmmRexbRm(m, rde)[i] & 0x80) {
      mem[i] = XmmRexrReg(m, rde)[i];
    }
  }
  EndStore(m, v, n, p, b);
}
