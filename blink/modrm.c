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
#include "blink/address.h"
#include "blink/assert.h"
#include "blink/builtin.h"
#include "blink/endian.h"
#include "blink/machine.h"
#include "blink/memory.h"
#include "blink/modrm.h"
#include "blink/x86.h"

struct AddrSeg LoadEffectiveAddress(const struct Machine *m, u64 rde) {
  struct AddrSeg res;
  u64 s = m->ds;
  u64 i = m->xedd->op.disp;
  unassert(!IsModrmRegister(rde));
  if (Eamode(rde) != XED_MODE_REAL) {
    if (!SibExists(rde)) {
      if (IsRipRelative(rde)) {
        if (Mode(rde) == XED_MODE_LONG) {
          i += m->ip;
        }
      } else {
        i += Get64(RegRexbRm(m, rde));
        if (RexbRm(rde) == 4 || RexbRm(rde) == 5) {
          s = m->ss;
        }
      }
    } else {
      if (SibHasBase(rde)) {
        i += Get64(RegRexbBase(m, rde));
        if (RexbBase(rde) == 4 || RexbBase(rde) == 5) {
          s = m->ss;
        }
      }
      if (SibHasIndex(rde)) {
        i += Get64(RegRexxIndex(m, rde)) << SibScale(rde);
      }
    }
    if (Eamode(rde) == XED_MODE_LEGACY) {
      i &= 0xffffffff;
    }
  } else {
    switch (ModrmRm(rde)) {
      case 0:
        i += Get16(m->bx);
        i += Get16(m->si);
        break;
      case 1:
        i += Get16(m->bx);
        i += Get16(m->di);
        break;
      case 2:
        s = m->ss;
        i += Get16(m->bp);
        i += Get16(m->si);
        break;
      case 3:
        s = m->ss;
        i += Get16(m->bp);
        i += Get16(m->di);
        break;
      case 4:
        i += Get16(m->si);
        break;
      case 5:
        i += Get16(m->di);
        break;
      case 6:
        if (ModrmMod(rde)) {
          s = m->ss;
          i += Get16(m->bp);
        }
        break;
      case 7:
        i += Get16(m->bx);
        break;
      default:
        __builtin_unreachable();
    }
    i &= 0xffff;
  }
  res.addr = i;
  res.seg = s;
  return res;
}

i64 ComputeAddress(struct Machine *m, u64 rde) {
  struct AddrSeg ea;
  ea = LoadEffectiveAddress(m, rde);
  return AddSegment(m, rde, ea.addr, ea.seg);
}

u8 *ComputeReserveAddressRead(struct Machine *m, u64 rde, size_t n) {
  i64 v;
  v = ComputeAddress(m, rde);
  SetReadAddr(m, v, n);
  return ReserveAddress(m, v, n);
}

u8 *ComputeReserveAddressRead1(struct Machine *m, u64 rde) {
  return ComputeReserveAddressRead(m, rde, 1);
}

u8 *ComputeReserveAddressRead4(struct Machine *m, u64 rde) {
  return ComputeReserveAddressRead(m, rde, 4);
}

u8 *ComputeReserveAddressRead8(struct Machine *m, u64 rde) {
  return ComputeReserveAddressRead(m, rde, 8);
}

u8 *ComputeReserveAddressWrite(struct Machine *m, u64 rde, size_t n) {
  i64 v;
  v = ComputeAddress(m, rde);
  SetWriteAddr(m, v, n);
  return ReserveAddress(m, v, n);
}

u8 *ComputeReserveAddressWrite1(struct Machine *m, u64 rde) {
  return ComputeReserveAddressWrite(m, rde, 1);
}

u8 *ComputeReserveAddressWrite4(struct Machine *m, u64 rde) {
  return ComputeReserveAddressWrite(m, rde, 4);
}

u8 *ComputeReserveAddressWrite8(struct Machine *m, u64 rde) {
  return ComputeReserveAddressWrite(m, rde, 8);
}

u8 *GetModrmRegisterMmPointerRead(struct Machine *m, u64 rde, size_t n) {
  if (IsModrmRegister(rde)) {
    return MmRm(m, rde);
  } else {
    return ComputeReserveAddressRead(m, rde, n);
  }
}

u8 *GetModrmRegisterMmPointerRead8(struct Machine *m, u64 rde) {
  return GetModrmRegisterMmPointerRead(m, rde, 8);
}

u8 *GetModrmRegisterMmPointerWrite(struct Machine *m, u64 rde, size_t n) {
  if (IsModrmRegister(rde)) {
    return MmRm(m, rde);
  } else {
    return ComputeReserveAddressWrite(m, rde, n);
  }
}

u8 *GetModrmRegisterMmPointerWrite8(struct Machine *m, u64 rde) {
  return GetModrmRegisterMmPointerWrite(m, rde, 8);
}

u8 *GetModrmRegisterBytePointerRead(struct Machine *m, u64 rde) {
  if (IsModrmRegister(rde)) {
    return ByteRexbRm(m, rde);
  } else {
    return ComputeReserveAddressRead1(m, rde);
  }
}

u8 *GetModrmRegisterBytePointerWrite(struct Machine *m, u64 rde) {
  if (IsModrmRegister(rde)) {
    return ByteRexbRm(m, rde);
  } else {
    return ComputeReserveAddressWrite1(m, rde);
  }
}

u8 *GetModrmRegisterWordPointerRead(struct Machine *m, u64 rde, size_t n) {
  if (IsModrmRegister(rde)) {
    return RegRexbRm(m, rde);
  } else {
    return ComputeReserveAddressRead(m, rde, n);
  }
}

u8 *GetModrmRegisterWordPointerRead2(struct Machine *m, u64 rde) {
  return GetModrmRegisterWordPointerRead(m, rde, 2);
}

u8 *GetModrmRegisterWordPointerRead4(struct Machine *m, u64 rde) {
  return GetModrmRegisterWordPointerRead(m, rde, 4);
}

u8 *GetModrmRegisterWordPointerRead8(struct Machine *m, u64 rde) {
  return GetModrmRegisterWordPointerRead(m, rde, 8);
}

u8 *GetModrmRegisterWordPointerReadOsz(struct Machine *m, u64 rde) {
  if (!Osz(rde)) {
    return GetModrmRegisterWordPointerRead8(m, rde);
  } else {
    return GetModrmRegisterWordPointerRead2(m, rde);
  }
}

u8 *GetModrmRegisterWordPointerReadOszRexw(struct Machine *m, u64 rde) {
  if (Rexw(rde)) {
    return GetModrmRegisterWordPointerRead8(m, rde);
  } else if (!Osz(rde)) {
    return GetModrmRegisterWordPointerRead4(m, rde);
  } else {
    return GetModrmRegisterWordPointerRead2(m, rde);
  }
}

u8 *GetModrmRegisterWordPointerWrite(struct Machine *m, u64 rde, size_t n) {
  if (IsModrmRegister(rde)) {
    return RegRexbRm(m, rde);
  } else {
    return ComputeReserveAddressWrite(m, rde, n);
  }
}

u8 *GetModrmRegisterWordPointerWrite2(struct Machine *m, u64 rde) {
  return GetModrmRegisterWordPointerWrite(m, rde, 2);
}

u8 *GetModrmRegisterWordPointerWrite4(struct Machine *m, u64 rde) {
  return GetModrmRegisterWordPointerWrite(m, rde, 4);
}

u8 *GetModrmRegisterWordPointerWrite8(struct Machine *m, u64 rde) {
  return GetModrmRegisterWordPointerWrite(m, rde, 8);
}

u8 *GetModrmRegisterWordPointerWriteOszRexw(struct Machine *m, u64 rde) {
  if (Rexw(rde)) {
    return GetModrmRegisterWordPointerWrite(m, rde, 8);
  } else if (!Osz(rde)) {
    return GetModrmRegisterWordPointerWrite(m, rde, 4);
  } else {
    return GetModrmRegisterWordPointerWrite(m, rde, 2);
  }
}

u8 *GetModrmRegisterWordPointerWriteOsz(struct Machine *m, u64 rde) {
  if (!Osz(rde)) {
    return GetModrmRegisterWordPointerWrite(m, rde, 8);
  } else {
    return GetModrmRegisterWordPointerWrite(m, rde, 2);
  }
}

u8 *GetModrmRegisterXmmPointerRead(struct Machine *m, u64 rde, size_t n) {
  if (IsModrmRegister(rde)) {
    return XmmRexbRm(m, rde);
  } else {
    return ComputeReserveAddressRead(m, rde, n);
  }
}

u8 *GetModrmRegisterXmmPointerRead4(struct Machine *m, u64 rde) {
  return GetModrmRegisterXmmPointerRead(m, rde, 4);
}

u8 *GetModrmRegisterXmmPointerRead8(struct Machine *m, u64 rde) {
  return GetModrmRegisterXmmPointerRead(m, rde, 8);
}

u8 *GetModrmRegisterXmmPointerRead16(struct Machine *m, u64 rde) {
  return GetModrmRegisterXmmPointerRead(m, rde, 16);
}

u8 *GetModrmRegisterXmmPointerWrite(struct Machine *m, u64 rde, size_t n) {
  if (IsModrmRegister(rde)) {
    return XmmRexbRm(m, rde);
  } else {
    return ComputeReserveAddressWrite(m, rde, n);
  }
}

u8 *GetModrmRegisterXmmPointerWrite4(struct Machine *m, u64 rde) {
  return GetModrmRegisterXmmPointerWrite(m, rde, 4);
}

u8 *GetModrmRegisterXmmPointerWrite8(struct Machine *m, u64 rde) {
  return GetModrmRegisterXmmPointerWrite(m, rde, 8);
}

u8 *GetModrmRegisterXmmPointerWrite16(struct Machine *m, u64 rde) {
  return GetModrmRegisterXmmPointerWrite(m, rde, 16);
}
