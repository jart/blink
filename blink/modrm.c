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
#include "blink/modrm.h"
#include "blink/x86.h"

struct AddrSeg LoadEffectiveAddress(const P) {
  u64 i = disp;
  u64 s = m->ds;
  struct AddrSeg res;
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

i64 ComputeAddress(P) {
  struct AddrSeg ea = LoadEffectiveAddress(A);
  return AddSegment(A, ea.addr, ea.seg);
}

u8 *ComputeReserveAddressRead(P, size_t n) {
  i64 v;
  v = ComputeAddress(A);
  SetReadAddr(m, v, n);
  return ReserveAddress(m, v, n, false);
}

u8 *ComputeReserveAddressRead1(P) {
  return ComputeReserveAddressRead(A, 1);
}

u8 *ComputeReserveAddressRead4(P) {
  return ComputeReserveAddressRead(A, 4);
}

u8 *ComputeReserveAddressRead8(P) {
  return ComputeReserveAddressRead(A, 8);
}

u8 *ComputeReserveAddressWrite(P, size_t n) {
  i64 v;
  v = ComputeAddress(A);
  SetWriteAddr(m, v, n);
  return ReserveAddress(m, v, n, true);
}

u8 *ComputeReserveAddressWrite1(P) {
  return ComputeReserveAddressWrite(A, 1);
}

u8 *ComputeReserveAddressWrite4(P) {
  return ComputeReserveAddressWrite(A, 4);
}

u8 *ComputeReserveAddressWrite8(P) {
  return ComputeReserveAddressWrite(A, 8);
}

u8 *GetModrmRegisterMmPointerRead(P, size_t n) {
  if (IsModrmRegister(rde)) {
    return MmRm(m, rde);
  } else {
    return ComputeReserveAddressRead(A, n);
  }
}

u8 *GetModrmRegisterMmPointerRead8(P) {
  return GetModrmRegisterMmPointerRead(A, 8);
}

u8 *GetModrmRegisterMmPointerWrite(P, size_t n) {
  if (IsModrmRegister(rde)) {
    return MmRm(m, rde);
  } else {
    return ComputeReserveAddressWrite(A, n);
  }
}

u8 *GetModrmRegisterMmPointerWrite8(P) {
  return GetModrmRegisterMmPointerWrite(A, 8);
}

u8 *GetModrmRegisterBytePointerRead1(P) {
  if (IsModrmRegister(rde)) {
    return ByteRexbRm(m, rde);
  } else {
    return ComputeReserveAddressRead1(A);
  }
}

u8 *GetModrmRegisterBytePointerWrite1(P) {
  if (IsModrmRegister(rde)) {
    return ByteRexbRm(m, rde);
  } else {
    return ComputeReserveAddressWrite1(A);
  }
}

u8 *GetModrmRegisterWordPointerRead(P, size_t n) {
  if (IsModrmRegister(rde)) {
    return RegRexbRm(m, rde);
  } else {
    return ComputeReserveAddressRead(A, n);
  }
}

u8 *GetModrmRegisterWordPointerRead2(P) {
  return GetModrmRegisterWordPointerRead(A, 2);
}

u8 *GetModrmRegisterWordPointerRead4(P) {
  return GetModrmRegisterWordPointerRead(A, 4);
}

u8 *GetModrmRegisterWordPointerRead8(P) {
  return GetModrmRegisterWordPointerRead(A, 8);
}

u8 *GetModrmRegisterWordPointerReadOsz(P) {
  if (!Osz(rde)) {
    return GetModrmRegisterWordPointerRead8(A);
  } else {
    return GetModrmRegisterWordPointerRead2(A);
  }
}

u8 *GetModrmRegisterWordPointerReadOszRexw(P) {
  if (Rexw(rde)) {
    return GetModrmRegisterWordPointerRead8(A);
  } else if (!Osz(rde)) {
    return GetModrmRegisterWordPointerRead4(A);
  } else {
    return GetModrmRegisterWordPointerRead2(A);
  }
}

u8 *GetModrmRegisterWordPointerWrite(P, size_t n) {
  if (IsModrmRegister(rde)) {
    return RegRexbRm(m, rde);
  } else {
    return ComputeReserveAddressWrite(A, n);
  }
}

u8 *GetModrmRegisterWordPointerWrite2(P) {
  return GetModrmRegisterWordPointerWrite(A, 2);
}

u8 *GetModrmRegisterWordPointerWrite4(P) {
  return GetModrmRegisterWordPointerWrite(A, 4);
}

u8 *GetModrmRegisterWordPointerWrite8(P) {
  return GetModrmRegisterWordPointerWrite(A, 8);
}

u8 *GetModrmRegisterWordPointerWriteOszRexw(P) {
  if (Rexw(rde)) {
    return GetModrmRegisterWordPointerWrite(A, 8);
  } else if (!Osz(rde)) {
    return GetModrmRegisterWordPointerWrite(A, 4);
  } else {
    return GetModrmRegisterWordPointerWrite(A, 2);
  }
}

u8 *GetModrmRegisterWordPointerWriteOsz(P) {
  if (!Osz(rde)) {
    return GetModrmRegisterWordPointerWrite(A, 8);
  } else {
    return GetModrmRegisterWordPointerWrite(A, 2);
  }
}

static u8 *GetModrmRegisterXmmPointerRead(P, size_t n) {
  if (IsModrmRegister(rde)) {
    return XmmRexbRm(m, rde);
  } else {
    return ComputeReserveAddressRead(A, n);
  }
}

u8 *GetModrmRegisterXmmPointerRead4(P) {
  return GetModrmRegisterXmmPointerRead(A, 4);
}

u8 *GetModrmRegisterXmmPointerRead8(P) {
  return GetModrmRegisterXmmPointerRead(A, 8);
}

u8 *GetModrmRegisterXmmPointerRead16(P) {
  return GetModrmRegisterXmmPointerRead(A, 16);
}

static u8 *GetModrmRegisterXmmPointerWrite(P, size_t n) {
  if (IsModrmRegister(rde)) {
    return XmmRexbRm(m, rde);
  } else {
    return ComputeReserveAddressWrite(A, n);
  }
}

u8 *GetModrmRegisterXmmPointerWrite4(P) {
  return GetModrmRegisterXmmPointerWrite(A, 4);
}

u8 *GetModrmRegisterXmmPointerWrite8(P) {
  return GetModrmRegisterXmmPointerWrite(A, 8);
}

u8 *GetModrmRegisterXmmPointerWrite16(P) {
  return GetModrmRegisterXmmPointerWrite(A, 16);
}

static u8 *GetVectorAddress(P, size_t n) {
  u8 *p;
  i64 v;
  if (IsModrmRegister(rde)) {
    p = XmmRexbRm(m, rde);
  } else {
    v = ComputeAddress(A);
    SetReadAddr(m, v, n);
    if ((v & (n - 1)) || !(p = LookupAddress(m, v))) {
      ThrowSegmentationFault(m, v);
    }
  }
  return p;
}

u8 *GetMmxAddress(P) {
  return GetVectorAddress(A, 8);
}

u8 *GetXmmAddress(P) {
  return GetVectorAddress(A, 16);
}
