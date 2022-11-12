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

struct AddrSeg LoadEffectiveAddress(const struct Machine *m, u32 rde) {
  struct AddrSeg res;
  const u8 *s = m->ds;
  u64 i = m->xedd->op.disp;
  unassert(!IsModrmRegister(rde));
  if (Eamode(rde) != XED_MODE_REAL) {
    if (!SibExists(rde)) {
      if (IsRipRelative(rde)) {
        if (Mode(rde) == XED_MODE_LONG) {
          i += m->ip;
        }
      } else {
        i += Read64(RegRexbRm(m, rde));
        if (RexbRm(rde) == 4 || RexbRm(rde) == 5) {
          s = m->ss;
        }
      }
    } else {
      if (SibHasBase(m->xedd, rde)) {
        i += Read64(RegRexbBase(m, rde));
        if (RexbBase(m, rde) == 4 || RexbBase(m, rde) == 5) {
          s = m->ss;
        }
      }
      if (SibHasIndex(m->xedd)) {
        i += Read64(RegRexxIndex(m, rde)) << SibScale(m->xedd);
      }
    }
    if (Eamode(rde) == XED_MODE_LEGACY) {
      i &= 0xffffffff;
    }
  } else {
    switch (ModrmRm(rde)) {
      case 0:
        i += Read16(m->bx);
        i += Read16(m->si);
        break;
      case 1:
        i += Read16(m->bx);
        i += Read16(m->di);
        break;
      case 2:
        s = m->ss;
        i += Read16(m->bp);
        i += Read16(m->si);
        break;
      case 3:
        s = m->ss;
        i += Read16(m->bp);
        i += Read16(m->di);
        break;
      case 4:
        i += Read16(m->si);
        break;
      case 5:
        i += Read16(m->di);
        break;
      case 6:
        if (ModrmMod(rde)) {
          s = m->ss;
          i += Read16(m->bp);
        }
        break;
      case 7:
        i += Read16(m->bx);
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

i64 ComputeAddress(struct Machine *m, u32 rde) {
  struct AddrSeg ea;
  ea = LoadEffectiveAddress(m, rde);
  return AddSegment(m, rde, ea.addr, ea.seg);
}

void *ComputeReserveAddressRead(struct Machine *m, u32 rde, size_t n) {
  i64 v;
  v = ComputeAddress(m, rde);
  SetReadAddr(m, v, n);
  return ReserveAddress(m, v, n);
}

void *ComputeReserveAddressRead1(struct Machine *m, u32 rde) {
  return ComputeReserveAddressRead(m, rde, 1);
}

void *ComputeReserveAddressRead4(struct Machine *m, u32 rde) {
  return ComputeReserveAddressRead(m, rde, 4);
}

void *ComputeReserveAddressRead8(struct Machine *m, u32 rde) {
  return ComputeReserveAddressRead(m, rde, 8);
}

void *ComputeReserveAddressWrite(struct Machine *m, u32 rde, size_t n) {
  i64 v;
  v = ComputeAddress(m, rde);
  SetWriteAddr(m, v, n);
  return ReserveAddress(m, v, n);
}

void *ComputeReserveAddressWrite1(struct Machine *m, u32 rde) {
  return ComputeReserveAddressWrite(m, rde, 1);
}

void *ComputeReserveAddressWrite4(struct Machine *m, u32 rde) {
  return ComputeReserveAddressWrite(m, rde, 4);
}

void *ComputeReserveAddressWrite8(struct Machine *m, u32 rde) {
  return ComputeReserveAddressWrite(m, rde, 8);
}

u8 *GetModrmRegisterMmPointerRead(struct Machine *m, u32 rde,
                                       size_t n) {
  if (IsModrmRegister(rde)) {
    return MmRm(m, rde);
  } else {
    return (u8 *)ComputeReserveAddressRead(m, rde, n);
  }
}

u8 *GetModrmRegisterMmPointerRead8(struct Machine *m, u32 rde) {
  return GetModrmRegisterMmPointerRead(m, rde, 8);
}

u8 *GetModrmRegisterMmPointerWrite(struct Machine *m, u32 rde,
                                        size_t n) {
  if (IsModrmRegister(rde)) {
    return MmRm(m, rde);
  } else {
    return (u8 *)ComputeReserveAddressWrite(m, rde, n);
  }
}

u8 *GetModrmRegisterMmPointerWrite8(struct Machine *m, u32 rde) {
  return GetModrmRegisterMmPointerWrite(m, rde, 8);
}

u8 *GetModrmRegisterBytePointerRead(struct Machine *m, u32 rde) {
  if (IsModrmRegister(rde)) {
    return ByteRexbRm(m, rde);
  } else {
    return (u8 *)ComputeReserveAddressRead1(m, rde);
  }
}

u8 *GetModrmRegisterBytePointerWrite(struct Machine *m, u32 rde) {
  if (IsModrmRegister(rde)) {
    return ByteRexbRm(m, rde);
  } else {
    return (u8 *)ComputeReserveAddressWrite1(m, rde);
  }
}

u8 *GetModrmRegisterWordPointerRead(struct Machine *m, u32 rde,
                                         size_t n) {
  if (IsModrmRegister(rde)) {
    return RegRexbRm(m, rde);
  } else {
    return (u8 *)ComputeReserveAddressRead(m, rde, n);
  }
}

u8 *GetModrmRegisterWordPointerRead2(struct Machine *m, u32 rde) {
  return GetModrmRegisterWordPointerRead(m, rde, 2);
}

u8 *GetModrmRegisterWordPointerRead4(struct Machine *m, u32 rde) {
  return GetModrmRegisterWordPointerRead(m, rde, 4);
}

u8 *GetModrmRegisterWordPointerRead8(struct Machine *m, u32 rde) {
  return GetModrmRegisterWordPointerRead(m, rde, 8);
}

u8 *GetModrmRegisterWordPointerReadOsz(struct Machine *m, u32 rde) {
  if (!Osz(rde)) {
    return GetModrmRegisterWordPointerRead8(m, rde);
  } else {
    return GetModrmRegisterWordPointerRead2(m, rde);
  }
}

u8 *GetModrmRegisterWordPointerReadOszRexw(struct Machine *m,
                                                u32 rde) {
  if (Rexw(rde)) {
    return GetModrmRegisterWordPointerRead8(m, rde);
  } else if (!Osz(rde)) {
    return GetModrmRegisterWordPointerRead4(m, rde);
  } else {
    return GetModrmRegisterWordPointerRead2(m, rde);
  }
}

u8 *GetModrmRegisterWordPointerWrite(struct Machine *m, u32 rde,
                                          size_t n) {
  if (IsModrmRegister(rde)) {
    return RegRexbRm(m, rde);
  } else {
    return (u8 *)ComputeReserveAddressWrite(m, rde, n);
  }
}

u8 *GetModrmRegisterWordPointerWrite2(struct Machine *m, u32 rde) {
  return GetModrmRegisterWordPointerWrite(m, rde, 2);
}

u8 *GetModrmRegisterWordPointerWrite4(struct Machine *m, u32 rde) {
  return GetModrmRegisterWordPointerWrite(m, rde, 4);
}

u8 *GetModrmRegisterWordPointerWrite8(struct Machine *m, u32 rde) {
  return GetModrmRegisterWordPointerWrite(m, rde, 8);
}

u8 *GetModrmRegisterWordPointerWriteOszRexw(struct Machine *m,
                                                 u32 rde) {
  if (Rexw(rde)) {
    return GetModrmRegisterWordPointerWrite(m, rde, 8);
  } else if (!Osz(rde)) {
    return GetModrmRegisterWordPointerWrite(m, rde, 4);
  } else {
    return GetModrmRegisterWordPointerWrite(m, rde, 2);
  }
}

u8 *GetModrmRegisterWordPointerWriteOsz(struct Machine *m, u32 rde) {
  if (!Osz(rde)) {
    return GetModrmRegisterWordPointerWrite(m, rde, 8);
  } else {
    return GetModrmRegisterWordPointerWrite(m, rde, 2);
  }
}

u8 *GetModrmRegisterXmmPointerRead(struct Machine *m, u32 rde,
                                        size_t n) {
  if (IsModrmRegister(rde)) {
    return XmmRexbRm(m, rde);
  } else {
    return (u8 *)ComputeReserveAddressRead(m, rde, n);
  }
}

u8 *GetModrmRegisterXmmPointerRead4(struct Machine *m, u32 rde) {
  return GetModrmRegisterXmmPointerRead(m, rde, 4);
}

u8 *GetModrmRegisterXmmPointerRead8(struct Machine *m, u32 rde) {
  return GetModrmRegisterXmmPointerRead(m, rde, 8);
}

u8 *GetModrmRegisterXmmPointerRead16(struct Machine *m, u32 rde) {
  return GetModrmRegisterXmmPointerRead(m, rde, 16);
}

u8 *GetModrmRegisterXmmPointerWrite(struct Machine *m, u32 rde,
                                         size_t n) {
  if (IsModrmRegister(rde)) {
    return XmmRexbRm(m, rde);
  } else {
    return (u8 *)ComputeReserveAddressWrite(m, rde, n);
  }
}

u8 *GetModrmRegisterXmmPointerWrite4(struct Machine *m, u32 rde) {
  return GetModrmRegisterXmmPointerWrite(m, rde, 4);
}

u8 *GetModrmRegisterXmmPointerWrite8(struct Machine *m, u32 rde) {
  return GetModrmRegisterXmmPointerWrite(m, rde, 8);
}

u8 *GetModrmRegisterXmmPointerWrite16(struct Machine *m, u32 rde) {
  return GetModrmRegisterXmmPointerWrite(m, rde, 16);
}
