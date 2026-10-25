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
#include <math.h>
#include <string.h>

#include "blink/builtin.h"
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/fpu.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/modrm.h"
#include "blink/pun.h"
#include "blink/stats.h"
#include "blink/tsan.h"

static void pshufw(i16 b[4], const i16 a[4], int m) {
  i16 t[4];
  t[0] = a[(m & 0003) >> 0];
  t[1] = a[(m & 0014) >> 2];
  t[2] = a[(m & 0060) >> 4];
  t[3] = a[(m & 0300) >> 6];
  b[0] = t[0];
  b[1] = t[1];
  b[2] = t[2];
  b[3] = t[3];
}

static void pshufd(i32 b[4], const i32 a[4], int m) {
  i32 t[4];
  t[0] = a[(m & 0003) >> 0];
  t[1] = a[(m & 0014) >> 2];
  t[2] = a[(m & 0060) >> 4];
  t[3] = a[(m & 0300) >> 6];
  b[0] = t[0];
  b[1] = t[1];
  b[2] = t[2];
  b[3] = t[3];
}

static void pshuflw(i16 b[8], const i16 a[8], int m) {
  i16 t[4];
  t[0] = a[(m & 0003) >> 0];
  t[1] = a[(m & 0014) >> 2];
  t[2] = a[(m & 0060) >> 4];
  t[3] = a[(m & 0300) >> 6];
  b[0] = t[0];
  b[1] = t[1];
  b[2] = t[2];
  b[3] = t[3];
  b[4] = a[4];
  b[5] = a[5];
  b[6] = a[6];
  b[7] = a[7];
}

static void pshufhw(i16 b[8], const i16 a[8], int m) {
  i16 t[4];
  t[0] = a[4 + ((m & 0003) >> 0)];
  t[1] = a[4 + ((m & 0014) >> 2)];
  t[2] = a[4 + ((m & 0060) >> 4)];
  t[3] = a[4 + ((m & 0300) >> 6)];
  b[0] = a[0];
  b[1] = a[1];
  b[2] = a[2];
  b[3] = a[3];
  b[4] = t[0];
  b[5] = t[1];
  b[6] = t[2];
  b[7] = t[3];
}

void OpUnpcklpsd(P) {
  u8 *a, *b;
  a = XmmRexrReg(m, rde);
  b = GetModrmRegisterXmmPointerRead8(A);
  IGNORE_RACES_START();
  if (Osz(rde)) {
    memcpy(a + 8, b, 8);
  } else {
    memcpy(a + 4 * 3, b + 4, 4);
    memcpy(a + 4 * 2, a + 4, 4);
    memcpy(a + 4 * 1, b + 0, 4);
  }
  IGNORE_RACES_END();
}

void OpUnpckhpsd(P) {
  u8 *a, *b;
  a = XmmRexrReg(m, rde);
  b = GetModrmRegisterXmmPointerRead16(A);
  IGNORE_RACES_START();
  if (Osz(rde)) {
    memcpy(a + 0, a + 8, 8);
    memcpy(a + 8, b + 8, 8);
  } else {
    memcpy(a + 4 * 0, a + 4 * 2, 4);
    memcpy(a + 4 * 1, b + 4 * 2, 4);
    memcpy(a + 4 * 2, a + 4 * 3, 4);
    memcpy(a + 4 * 3, b + 4 * 3, 4);
  }
  IGNORE_RACES_END();
}

void OpPextrwGdqpUdqIb(P) {
  u8 i;
  i = uimm0;
  i &= Osz(rde) ? 7 : 3;
  Put64(RegRexrReg(m, rde), Get16(XmmRexbRm(m, rde) + i * 2));
}

void OpPextrbw(P) {
  unsigned size, index;
  u32 value;
  u8 *source, *destination;
  if (!Osz(rde) || Rep(rde)) return OpUd(A);
  size = (Opcode(rde) & 1) ? 2 : 1;
  index = uimm0 & (size == 2 ? 7 : 15);
  source = XmmRexrReg(m, rde) + index * size;
  value = size == 2 ? Get16(source) : *source;
  destination = GetModrmRegisterWordPointerWrite(A, size);
  IGNORE_RACES_START();
  if (IsModrmRegister(rde)) {
    Put64(destination, value);
  } else if (size == 2) {
    Put16(destination, value);
  } else {
    *destination = value;
  }
  IGNORE_RACES_END();
}

void OpExtractpsEdVpsIb(P) {
  u32 value;
  u8 *destination;
  if (!Osz(rde) || Rep(rde)) return OpUd(A);
  value = Get32(XmmRexrReg(m, rde) + (uimm0 & 3) * 4);
  destination = GetModrmRegisterWordPointerWrite(A, 4);
  IGNORE_RACES_START();
  if (IsModrmRegister(rde)) {
    Put64(destination, value);
  } else {
    Put32(destination, value);
  }
  IGNORE_RACES_END();
}

void OpPextrdEdqpVdqIb(P) {
  unsigned size, index;
  u64 value;
  u8 *source, *destination;
  if (!Osz(rde) || Rep(rde)) return OpUd(A);
  size = Rexw(rde) ? 8 : 4;
  index = uimm0 & (Rexw(rde) ? 1 : 3);
  source = XmmRexrReg(m, rde) + index * size;
  value = Rexw(rde) ? Get64(source) : Get32(source);
  destination = GetModrmRegisterWordPointerWrite(A, size);
  IGNORE_RACES_START();
  if (Rexw(rde) || IsModrmRegister(rde)) {
    Put64(destination, value);
  } else {
    Put32(destination, value);
  }
  IGNORE_RACES_END();
}

void OpPinsrwVdqEwIb(P) {
  u8 i;
  i = uimm0;
  i &= Osz(rde) ? 7 : 3;
  IGNORE_RACES_START();
  Put16(XmmRexrReg(m, rde) + i * 2,
        Read16(GetModrmRegisterWordPointerRead2(A)));
  IGNORE_RACES_END();
}

void OpPinsrbVdqEdIb(P) {
  if (!Osz(rde) || Rep(rde)) return OpUd(A);
  IGNORE_RACES_START();
  // Register sources are the low byte of r32, never AH/CH/DH/BH.
  XmmRexrReg(m, rde)[uimm0 & 15] = *GetModrmRegisterWordPointerRead(A, 1);
  IGNORE_RACES_END();
}

void OpPinsrdVdqEdqpIb(P) {
  unsigned size, index;
  if (!Osz(rde) || Rep(rde)) return OpUd(A);
  size = Rexw(rde) ? 8 : 4;
  index = uimm0 & (Rexw(rde) ? 1 : 3);
  IGNORE_RACES_START();
  memcpy(XmmRexrReg(m, rde) + index * size,
         GetModrmRegisterWordPointerRead(A, size), size);
  IGNORE_RACES_END();
}

void OpMpsadbw(P) {
  unsigned i, j, source_offset, destination_offset, sum;
  int difference;
  u8 left[16], right[16];
  if (!Osz(rde) || Rep(rde)) return OpUd(A);
  IGNORE_RACES_START();
  memcpy(left, XmmRexrReg(m, rde), 16);
  memcpy(right, GetModrmRegisterXmmPointerRead16(A), 16);
  IGNORE_RACES_END();
  source_offset = (uimm0 & 3) * 4;
  destination_offset = ((uimm0 >> 2) & 1) * 4;
  for (i = 0; i < 8; ++i) {
    sum = 0;
    for (j = 0; j < 4; ++j) {
      difference = left[destination_offset + i + j] - right[source_offset + j];
      sum += difference < 0 ? -difference : difference;
    }
    Put16(XmmRexrReg(m, rde) + i * 2, sum);
  }
}

void OpPhminposuw(P) {
  unsigned i, index, minimum, value;
  u8 source[16];
  if (!Osz(rde) || Rep(rde)) return OpUd(A);
  IGNORE_RACES_START();
  memcpy(source, GetModrmRegisterXmmPointerRead16(A), 16);
  IGNORE_RACES_END();
  minimum = Get16(source);
  index = 0;
  for (i = 1; i < 8; ++i) {
    if ((value = Get16(source + i * 2)) < minimum) {
      minimum = value;
      index = i;
    }
  }
  memset(XmmRexrReg(m, rde), 0, 16);
  Put16(XmmRexrReg(m, rde), minimum);
  Put16(XmmRexrReg(m, rde) + 2, index);
}

void OpInsertps(P) {
  unsigned i;
  u32 value;
  u8 result[16];
  if (!Osz(rde) || Rep(rde)) return OpUd(A);
  IGNORE_RACES_START();
  memcpy(result, XmmRexrReg(m, rde), 16);
  if (IsModrmRegister(rde)) {
    value = Get32(XmmRexbRm(m, rde) + ((uimm0 >> 6) & 3) * 4);
  } else {
    value = Get32(GetModrmRegisterXmmPointerRead4(A));
  }
  IGNORE_RACES_END();
  Put32(result + ((uimm0 >> 4) & 3) * 4, value);
  for (i = 0; i < 4; ++i) {
    if (uimm0 & (1 << i)) Put32(result + i * 4, 0);
  }
  memcpy(XmmRexrReg(m, rde), result, 16);
}

void OpPmuldq(P) {
  unsigned i;
  u8 left[16], right[16];
  if (!Osz(rde) || Rep(rde)) return OpUd(A);
  IGNORE_RACES_START();
  memcpy(left, XmmRexrReg(m, rde), 16);
  memcpy(right, GetModrmRegisterXmmPointerRead16(A), 16);
  IGNORE_RACES_END();
  for (i = 0; i < 16; i += 8) {
    Put64(XmmRexrReg(m, rde) + i,
          (i64)(i32)Get32(left + i) * (i64)(i32)Get32(right + i));
  }
}

void OpPcmpeqq(P) {
  unsigned i;
  u8 left[16], right[16];
  if (!Osz(rde) || Rep(rde)) return OpUd(A);
  IGNORE_RACES_START();
  memcpy(left, XmmRexrReg(m, rde), 16);
  memcpy(right, GetModrmRegisterXmmPointerRead16(A), 16);
  IGNORE_RACES_END();
  for (i = 0; i < 16; i += 8) {
    Put64(XmmRexrReg(m, rde) + i,
          Get64(left + i) == Get64(right + i) ? -1 : 0);
  }
}

void OpPcmpgtq(P) {
  unsigned i;
  u8 left[16], right[16];
  if (!Osz(rde) || Rep(rde)) return OpUd(A);
  IGNORE_RACES_START();
  memcpy(left, XmmRexrReg(m, rde), 16);
  memcpy(right, GetModrmRegisterXmmPointerRead16(A), 16);
  IGNORE_RACES_END();
  for (i = 0; i < 16; i += 8) {
    Put64(XmmRexrReg(m, rde) + i,
          (i64)Get64(left + i) > (i64)Get64(right + i) ? -1 : 0);
  }
}

void OpPackusdw(P) {
  unsigned i;
  i32 value;
  u8 left[16], right[16], result[16];
  if (!Osz(rde) || Rep(rde)) return OpUd(A);
  IGNORE_RACES_START();
  memcpy(left, XmmRexrReg(m, rde), 16);
  memcpy(right, GetModrmRegisterXmmPointerRead16(A), 16);
  IGNORE_RACES_END();
  for (i = 0; i < 8; ++i) {
    value = Get32((i < 4 ? left : right) + (i & 3) * 4);
    Put16(result + i * 2, value < 0 ? 0 : value > 65535 ? 65535 : value);
  }
  memcpy(XmmRexrReg(m, rde), result, 16);
}

void OpPminmax(P) {
  static const u8 widths[4] = {1, 4, 2, 4};
  unsigned kind, width, i;
  bool sign, maximum;
  i64 a, b;
  u8 left[16], right[16];
  if (!Osz(rde) || Rep(rde)) return OpUd(A);
  kind = Opcode(rde) & 3;
  width = widths[kind];
  sign = kind < 2;
  maximum = Opcode(rde) & 4;
  IGNORE_RACES_START();
  memcpy(left, XmmRexrReg(m, rde), 16);
  memcpy(right, GetModrmRegisterXmmPointerRead16(A), 16);
  IGNORE_RACES_END();
  for (i = 0; i < 16; i += width) {
    if (width == 1) {
      a = (i8)left[i];
      b = (i8)right[i];
    } else if (width == 2) {
      a = Get16(left + i);
      b = Get16(right + i);
    } else {
      a = Get32(left + i);
      b = Get32(right + i);
      if (sign) {
        a = (i32)a;
        b = (i32)b;
      }
    }
    if (maximum ? b > a : b < a) memcpy(left + i, right + i, width);
  }
  memcpy(XmmRexrReg(m, rde), left, 16);
}

void OpBlend(P) {
  unsigned op, width, i;
  bool selected;
  u8 left[16], right[16], mask[16];
  if (!Osz(rde) || Rep(rde)) return OpUd(A);
  op = Opcode(rde);
  switch (op) {
    case 0x10: width = 1; break;  // PBLENDVB
    case 0x0e: width = 2; break;  // PBLENDW
    case 0x0c: case 0x14: width = 4; break;  // BLENDPS / BLENDVPS
    case 0x0d: case 0x15: width = 8; break;  // BLENDPD / BLENDVPD
    default: return OpUd(A);
  }
  IGNORE_RACES_START();
  memcpy(left, XmmRexrReg(m, rde), 16);
  memcpy(right, GetModrmRegisterXmmPointerRead16(A), 16);
  memcpy(mask, m->xmm[0], 16);
  IGNORE_RACES_END();
  for (i = 0; i < 16; i += width) {
    selected = op < 0x10 ? (uimm0 >> (i / width)) & 1
                         : !!(mask[i + width - 1] & 128);
    if (selected) memcpy(left + i, right + i, width);
  }
  memcpy(XmmRexrReg(m, rde), left, 16);
}

void OpPtest(P) {
  u64 a[2], b[2];
  u8 *p;
  if (!Osz(rde) || Rep(rde)) return OpUd(A);
  IGNORE_RACES_START();
  p = XmmRexrReg(m, rde);
  a[0] = Get64(p);
  a[1] = Get64(p + 8);
  p = GetModrmRegisterXmmPointerRead16(A);
  b[0] = Get64(p);
  b[1] = Get64(p + 8);
  IGNORE_RACES_END();
  m->flags = SetFlag(m->flags, FLAGS_ZF, !((a[0] & b[0]) | (a[1] & b[1])));
  m->flags = SetFlag(m->flags, FLAGS_CF, !((~a[0] & b[0]) | (~a[1] & b[1])));
  m->flags = SetFlag(m->flags, FLAGS_OF, false);
  m->flags = SetFlag(m->flags, FLAGS_SF, false);
  m->flags = SetFlag(m->flags, FLAGS_AF, false);
  m->flags = SetFlag(m->flags, FLAGS_PF, false);
}

// Round IEEE binary32/binary64 encodings without changing the host FP mode.
static u64 RoundIntegral(u64 bits, unsigned fracbits, unsigned expbits,
                         unsigned mode, bool daz, bool suppress,
                         unsigned *exceptions) {
  u64 sign, mag, fraction, expmask, exponent, bias, unit, tail, result;
  int exponent2;
  bool increment;
  sign = bits & (1ull << (fracbits + expbits));
  mag = bits ^ sign;
  fraction = mag & ((1ull << fracbits) - 1);
  expmask = (1u << expbits) - 1;
  exponent = mag >> fracbits;
  bias = expmask >> 1;
  if (exponent == expmask) {
    if (fraction && !(fraction & (1ull << (fracbits - 1)))) {
      *exceptions |= kMxcsrIe;
      bits |= 1ull << (fracbits - 1);
    }
    return bits;
  }
  if (!exponent && daz) return sign;
  if (!mag || exponent >= bias + fracbits) return bits;
  exponent2 = (int)exponent - (int)bias;
  if (exponent2 < 0) {
    increment = mode == 0 ? mag > ((bias - 1) << fracbits)
                         : mode == 1 ? !!sign : mode == 2 ? !sign : false;
    result = increment ? bias << fracbits : 0;
  } else {
    unit = 1ull << (fracbits - exponent2);
    tail = mag & (unit - 1);
    if (!tail) return bits;
    result = mag & ~(unit - 1);
    increment = mode == 0 ? tail > unit / 2 ||
                               (tail == unit / 2 && (result & unit))
                         : mode == 1 ? !!sign : mode == 2 ? !sign : false;
    if (increment) result += unit;
  }
  if (!suppress) *exceptions |= kMxcsrPe;
  return sign | result;
}

void OpRound(P) {
  unsigned width, size, mode, i, exceptions = 0;
  u8 source[16], result[16];
  u64 bits;
  if (!Osz(rde) || Rep(rde)) return OpUd(A);
  width = (Opcode(rde) & 1) ? 8 : 4;
  size = (Opcode(rde) & 2) ? width : 16;
  mode = uimm0 & 4 ? (m->mxcsr >> 13) & 3 : uimm0 & 3;
  IGNORE_RACES_START();
  memcpy(source, GetModrmRegisterXmmPointerRead(A, size), size);
  IGNORE_RACES_END();
  for (i = 0; i < size; i += width) {
    bits = width == 8 ? Get64(source + i) : Get32(source + i);
    bits = RoundIntegral(bits, width == 8 ? 52 : 23, width == 8 ? 11 : 8,
                         mode, !!(m->mxcsr & kMxcsrDaz), !!(uimm0 & 8),
                         &exceptions);
    if (width == 8) {
      Put64(result + i, bits);
    } else {
      Put32(result + i, bits);
    }
  }
  m->mxcsr |= exceptions & kMxcsrIe;
  if ((exceptions & kMxcsrIe) && !(m->mxcsr & kMxcsrIm)) {
    HaltMachine(m, kMachineSimdException);
  }
  m->mxcsr |= exceptions & kMxcsrPe;
  if ((exceptions & kMxcsrPe) && !(m->mxcsr & kMxcsrPm)) {
    HaltMachine(m, kMachineSimdException);
  }
  memcpy(XmmRexrReg(m, rde), result, size);
}

void OpPmovx(P) {
  static const u8 widths[6][2] = {{1, 2}, {1, 4}, {1, 8},
                                 {2, 4}, {2, 8}, {4, 8}};
  unsigned kind, srcsize, dstsize, i;
  bool sign;
  u8 source[8], result[16];
  u64 value;
  if (!Osz(rde) || Rep(rde)) return OpUd(A);
  kind = Opcode(rde) & 15;
  if (kind >= 6) return OpUd(A);
  sign = !(Opcode(rde) & 16);
  srcsize = widths[kind][0];
  dstsize = widths[kind][1];
  IGNORE_RACES_START();
  memcpy(source, GetModrmRegisterXmmPointerRead(A, 16 / dstsize * srcsize),
         16 / dstsize * srcsize);
  IGNORE_RACES_END();
  for (i = 0; i < 16 / dstsize; ++i) {
    if (srcsize == 1) {
      value = sign ? (i64)(i8)source[i] : source[i];
    } else if (srcsize == 2) {
      value = Get16(source + i * 2);
      if (sign) value = (i64)(i16)value;
    } else {
      value = Get32(source + i * 4);
      if (sign) value = (i64)(i32)value;
    }
    if (dstsize == 2) {
      Put16(result + i * 2, value);
    } else if (dstsize == 4) {
      Put32(result + i * 4, value);
    } else {
      Put64(result + i * 8, value);
    }
  }
  memcpy(XmmRexrReg(m, rde), result, 16);
}

// Intel SDM Vol. 2B, section 4.1: packed string comparison controls.
static unsigned StringLength(i64 length, unsigned limit) {
  if (length >= limit || length <= -(i64)limit) return limit;
  return length < 0 ? -length : length;
}

void OpPcmpstr(P) {
  int a[16], b[16];
  unsigned i, j, n, na, nb, mask = 0, mode, width, op;
  bool match;
  u8 left[16], right[16], result[16] = {0};
  if (!Osz(rde) || Rep(rde)) return OpUd(A);
  op = Opcode(rde);
  width = 1 + (uimm0 & 1);
  n = 16 / width;
  mode = (uimm0 >> 2) & 3;
  IGNORE_RACES_START();
  memcpy(left, XmmRexrReg(m, rde), 16);
  memcpy(right, GetModrmRegisterXmmPointerRead16(A), 16);
  IGNORE_RACES_END();
  for (i = 0; i < n; ++i) {
    a[i] = width == 2 ? Get16(left + i * 2) : left[i];
    b[i] = width == 2 ? Get16(right + i * 2) : right[i];
    if (uimm0 & 2) {
      a[i] = width == 2 ? (i16)a[i] : (i8)a[i];
      b[i] = width == 2 ? (i16)b[i] : (i8)b[i];
    }
  }
  if (op & 2) {
    for (na = 0; na < n && a[na]; ++na) {}
    for (nb = 0; nb < n && b[nb]; ++nb) {}
  } else {
    na = StringLength(Rexw(rde) ? (i64)Read64(m->ax) : (i32)Read32(m->ax), n);
    nb = StringLength(Rexw(rde) ? (i64)Read64(m->dx) : (i32)Read32(m->dx), n);
  }
  for (j = 0; j < n; ++j) {
    match = false;
    switch (mode) {
      case 0:  // equal any
        for (i = 0; i < na && j < nb; ++i) match |= a[i] == b[j];
        break;
      case 1:  // ranges; both bounds must be valid
        for (i = 0; i + 1 < na && j < nb; i += 2) {
          match |= a[i] <= b[j] && b[j] <= a[i + 1];
        }
        break;
      case 2:  // equal each; two invalid elements compare equal
        match = j < na && j < nb ? a[j] == b[j] : j >= na && j >= nb;
        break;
      case 3:  // equal ordered, including partial matches at the vector end
        match = true;
        for (i = 0; i < na && i + j < n; ++i) {
          match &= i + j < nb && a[i] == b[i + j];
        }
        break;
    }
    mask |= (unsigned)match << j;
  }
  if (uimm0 & 16) mask ^= (1u << ((uimm0 & 32) ? nb : n)) - 1;
  m->flags = SetFlag(m->flags, FLAGS_CF, mask != 0);
  m->flags = SetFlag(m->flags, FLAGS_ZF, nb < n);
  m->flags = SetFlag(m->flags, FLAGS_SF, na < n);
  m->flags = SetFlag(m->flags, FLAGS_OF, mask & 1);
  m->flags = SetFlag(m->flags, FLAGS_AF, false);
  m->flags = SetFlag(m->flags, FLAGS_PF, false);
  if (op & 1) {
    i = n;
    for (j = 0; j < n; ++j) {
      if (mask & (1u << j)) {
        i = j;
        if (!(uimm0 & 64)) break;
      }
    }
    Put64(m->cx, i);
  } else {
    if (uimm0 & 64) {
      for (i = 0; i < 16; ++i) result[i] = -(!!(mask & (1u << (i / width))));
    } else {
      Put16(result, mask);
    }
    memcpy(m->xmm[0], result, 16);
  }
}

void OpShuffle(P) {
  i16 q16[4];
  void *kernel;
  IGNORE_RACES_START();
  switch (Rep(rde) | Osz(rde)) {
    case 0:
      memcpy(q16, GetModrmRegisterXmmPointerRead8(A), 8);
      pshufw(q16, q16, uimm0);
      memcpy(XmmRexrReg(m, rde), q16, 8);
      kernel = 0;
      break;
    case 1:
      pshufd((i32 *)XmmRexrReg(m, rde),
             (i32 *)GetModrmRegisterXmmPointerRead16(A), uimm0);
      kernel = (void *)(uintptr_t)pshufd;
      break;
    case 2:
      pshuflw((i16 *)XmmRexrReg(m, rde),
              (i16 *)GetModrmRegisterXmmPointerRead16(A), uimm0);
      kernel = (void *)(uintptr_t)pshuflw;
      break;
    case 3:
      pshufhw((i16 *)XmmRexrReg(m, rde),
              (i16 *)GetModrmRegisterXmmPointerRead16(A), uimm0);
      kernel = (void *)(uintptr_t)pshufhw;
      break;
    default:
      __builtin_unreachable();
  }
  IGNORE_RACES_END();
  if (IsMakingPath(m) && kernel) {
    Jitter(A,
           "z4P"    // res0 = GetXmmOrMemPointer(RexbRm)
           "r0s1="  // sav1 = res0
           "z4Q"    // res0 = GetXmmPointer(RexrReg)
           "a2i"    // arg2 = uimm0
           "s1a1="  // arg1 = sav1
           "t"      // arg0 = res0
           "c",     // call function
           uimm0, kernel);
  }
}

static void Shufps(P) {
  u8 *p;
  union FloatPun x[4], y[4], z[4];
  p = GetModrmRegisterXmmPointerRead16(A);
  IGNORE_RACES_START();
  y[0].i = Read32(p + 0 * 4);
  y[1].i = Read32(p + 1 * 4);
  y[2].i = Read32(p + 2 * 4);
  y[3].i = Read32(p + 3 * 4);
  p = XmmRexrReg(m, rde);
  x[0].i = Read32(p + 0 * 4);
  x[1].i = Read32(p + 1 * 4);
  x[2].i = Read32(p + 2 * 4);
  x[3].i = Read32(p + 3 * 4);
  z[0].f = x[(uimm0 & 0003) >> 0].f;
  z[1].f = x[(uimm0 & 0014) >> 2].f;
  z[2].f = y[(uimm0 & 0060) >> 4].f;
  z[3].f = y[(uimm0 & 0300) >> 6].f;
  Write32(p + 0 * 4, z[0].i);
  Write32(p + 1 * 4, z[1].i);
  Write32(p + 2 * 4, z[2].i);
  Write32(p + 3 * 4, z[3].i);
  IGNORE_RACES_END();
}

static void Shufpd(P) {
  u8 *p;
  union DoublePun x[2], y[2], z[2];
  p = GetModrmRegisterXmmPointerRead16(A);
  IGNORE_RACES_START();
  y[0].i = Read64(p + 0 * 8);
  y[1].i = Read64(p + 1 * 8);
  p = XmmRexrReg(m, rde);
  x[0].i = Read64(p + 0 * 8);
  x[1].i = Read64(p + 1 * 8);
  z[0].f = x[(uimm0 & 0001) >> 0].f;
  z[1].f = y[(uimm0 & 0002) >> 1].f;
  Write64(p + 0 * 8, z[0].i);
  Write64(p + 1 * 8, z[1].i);
  IGNORE_RACES_END();
}

void OpShufpsd(P) {
  if (Osz(rde)) {
    Shufpd(A);
  } else {
    Shufps(A);
  }
}

static void Movmskps(P) {
  u8 *p = GetModrmRegisterXmmPointerRead16(A);
  IGNORE_RACES_START();
  Put64(RegRexrReg(m, rde), (!!(p[15] & 0x80) << 3 | !!(p[11] & 0x80) << 2 |
                             !!(p[7] & 0x80) << 1 | !!(p[3] & 0x80)));
  IGNORE_RACES_END();
}

static void Movmskpd(P) {
  u8 *p = GetModrmRegisterXmmPointerRead16(A);
  IGNORE_RACES_START();
  Put64(RegRexrReg(m, rde), !!(p[15] & 0x80) << 1 | !!(p[7] & 0x80));
  IGNORE_RACES_END();
}

void OpMovmskpsd(P) {
  if (Osz(rde)) {
    Movmskpd(A);
  } else {
    Movmskps(A);
  }
}

void OpSqrtpsd(P) {
  IGNORE_RACES_START();
  switch (Rep(rde) | Osz(rde)) {
    case 0: {
      int i;
      u8 *p;
      union FloatPun u[4];
      p = GetModrmRegisterXmmPointerRead16(A);
      u[0].i = Read32(p + 0 * 4);
      u[1].i = Read32(p + 1 * 4);
      u[2].i = Read32(p + 2 * 4);
      u[3].i = Read32(p + 3 * 4);
      for (i = 0; i < 4; ++i) u[i].f = sqrtf(u[i].f);
      p = XmmRexrReg(m, rde);
      Write32(p + 0 * 4, u[0].i);
      Write32(p + 1 * 4, u[1].i);
      Write32(p + 2 * 4, u[2].i);
      Write32(p + 3 * 4, u[3].i);
      break;
    }
    case 1: {
      int i;
      u8 *p;
      union DoublePun u[2];
      p = GetModrmRegisterXmmPointerRead16(A);
      u[0].i = Read32(p + 0 * 8);
      u[1].i = Read32(p + 1 * 8);
      for (i = 0; i < 2; ++i) u[i].f = sqrt(u[i].f);
      p = XmmRexrReg(m, rde);
      Write32(p + 0 * 8, u[0].i);
      Write32(p + 1 * 8, u[1].i);
      break;
    }
    case 2: {
      union DoublePun u;
      u.i = Read64(GetModrmRegisterXmmPointerRead8(A));
      u.f = sqrt(u.f);
      Write64(XmmRexrReg(m, rde), u.i);
      break;
    }
    case 3: {
      union FloatPun u;
      u.i = Read32(GetModrmRegisterXmmPointerRead4(A));
      u.f = sqrtf(u.f);
      Write32(XmmRexrReg(m, rde), u.i);
      break;
    }
    default:
      __builtin_unreachable();
  }
  IGNORE_RACES_END();
}

void OpRsqrtps(P) {
  IGNORE_RACES_START();
  if (Rep(rde) != 3) {
    int i;
    u8 *p;
    union FloatPun u[4];
    p = GetModrmRegisterXmmPointerRead16(A);
    u[0].i = Read32(p + 0 * 4);
    u[1].i = Read32(p + 1 * 4);
    u[2].i = Read32(p + 2 * 4);
    u[3].i = Read32(p + 3 * 4);
    for (i = 0; i < 4; ++i) u[i].f = 1.f / sqrtf(u[i].f);
    p = XmmRexrReg(m, rde);
    Write32(p + 0 * 4, u[0].i);
    Write32(p + 1 * 4, u[1].i);
    Write32(p + 2 * 4, u[2].i);
    Write32(p + 3 * 4, u[3].i);
  } else {
    union FloatPun u;
    u.i = Read32(GetModrmRegisterXmmPointerRead4(A));
    u.f = 1.f / sqrtf(u.f);
    Write32(XmmRexrReg(m, rde), u.i);
  }
  IGNORE_RACES_END();
}

void OpRcpps(P) {
  IGNORE_RACES_START();
  if (Rep(rde) != 3) {
    int i;
    u8 *p;
    union FloatPun u[4];
    p = GetModrmRegisterXmmPointerRead16(A);
    u[0].i = Read32(p + 0 * 4);
    u[1].i = Read32(p + 1 * 4);
    u[2].i = Read32(p + 2 * 4);
    u[3].i = Read32(p + 3 * 4);
    for (i = 0; i < 4; ++i) u[i].f = 1.f / u[i].f;
    p = XmmRexrReg(m, rde);
    Write32(p + 0 * 4, u[0].i);
    Write32(p + 1 * 4, u[1].i);
    Write32(p + 2 * 4, u[2].i);
    Write32(p + 3 * 4, u[3].i);
  } else {
    union FloatPun u;
    u.i = Read32(GetModrmRegisterXmmPointerRead4(A));
    u.f = 1.f / u.f;
    Write32(XmmRexrReg(m, rde), u.i);
  }
  IGNORE_RACES_END();
}

static void ComissKernel(const u8 rxr[8], const u8 reg[8], struct Machine *m) {
  bool zf, cf;
  union DoublePun xd, yd;
  m->mxcsr &= ~kMxcsrIe;
  xd.i = Read64(rxr);
  yd.i = Read64(reg);
  if (!isunordered(xd.f, yd.f)) {
    zf = xd.f == yd.f;
    cf = xd.f < yd.f;
    m->flags = SetFlag(m->flags, FLAGS_ZF, zf);
    m->flags = SetFlag(m->flags, FLAGS_CF, cf);
    m->flags = SetFlag(m->flags, FLAGS_PF, false);
    m->flags = SetFlag(m->flags, FLAGS_SF, false);
    m->flags = SetFlag(m->flags, FLAGS_OF, false);
  } else {
    m->flags = SetFlag(m->flags, FLAGS_ZF, true);
    m->flags = SetFlag(m->flags, FLAGS_CF, true);
    m->flags = SetFlag(m->flags, FLAGS_PF, true);
    m->flags = SetFlag(m->flags, FLAGS_SF, false);
    m->flags = SetFlag(m->flags, FLAGS_OF, false);
    m->mxcsr |= kMxcsrIe;
    if (!(m->mxcsr & kMxcsrIm)) {
      HaltMachine(m, kMachineSimdException);
    }
  }
}

void OpComissVsWs(P) {
  bool isucomiss;
  u8 zf, cf, pf, ie;
  isucomiss = ~Opcode(rde) & 1;
  IGNORE_RACES_START();
  if (!Osz(rde)) {
    union FloatPun xf, yf;
    xf.i = Read32(XmmRexrReg(m, rde));
    yf.i = Read32(GetModrmRegisterXmmPointerRead4(A));
    if (!isunordered(xf.f, yf.f)) {
      zf = xf.f == yf.f;
      cf = xf.f < yf.f;
      pf = false;
      ie = false;
    } else {
      zf = cf = pf = ie = true;
    }
  } else {
    union DoublePun xd, yd;
    xd.i = Read64(XmmRexrReg(m, rde));
    yd.i = Read64(GetModrmRegisterXmmPointerRead8(A));
    if (!isunordered(xd.f, yd.f)) {
      zf = xd.f == yd.f;
      cf = xd.f < yd.f;
      pf = false;
      ie = false;
    } else {
      zf = cf = pf = ie = true;
    }
    if (IsMakingPath(m) && !isucomiss) {
      Jitter(A,
             "z4P"    // res0 = GetXmmOrMemPointer(RexbRm)
             "r0s1="  // sav1 = res0
             "z4Q"    // res0 = GetXmmPointer(RexrReg)
             "s0a2="  // arg2 = sav0 (machine)
             "s1a1="  // arg1 = sav1
             "t"      // arg0 = res0
             "c",     // call function
             ComissKernel);
    }
  }
  m->flags = SetFlag(m->flags, FLAGS_ZF, zf);
  m->flags = SetFlag(m->flags, FLAGS_PF, pf);
  m->flags = SetFlag(m->flags, FLAGS_CF, cf);
  m->flags = SetFlag(m->flags, FLAGS_SF, false);
  m->flags = SetFlag(m->flags, FLAGS_OF, false);
  if (!isucomiss) {
    m->mxcsr &= ~kMxcsrIe;
    if (ie) {
      m->mxcsr |= kMxcsrIe;
      if (!(m->mxcsr & kMxcsrIm)) {
        HaltMachine(m, kMachineSimdException);
      }
    }
  }
  IGNORE_RACES_END();
}

static void OpPsd(P, float fs(float x, float y), double fd(double x, double y),
                  void s1(u8 *, struct Machine *, long),
                  void d1(u8 *, struct Machine *, long)) {
  IGNORE_RACES_START();
  if (Rep(rde) == 2) {
    d1(GetModrmRegisterXmmPointerRead8(A), m, RexrReg(rde));
    if (IsMakingPath(m)) {
      Jitter(A,
             "z4P"    // res0 = GetXmmOrMemPointer(RexbRm)
             "a2i"    // arg2 = RexrReg(rde)
             "s0a1="  // arg1 = machine
             "t"      // arg0 = res0
             "m",     // call function (d1)
             RexrReg(rde), d1);
    }
  } else if (Rep(rde) == 3) {
    union FloatPun x, y;
    y.i = Read32(GetModrmRegisterXmmPointerRead4(A));
    x.i = Read32(XmmRexrReg(m, rde));
    x.f = fs(x.f, y.f);
    Write32(XmmRexrReg(m, rde), x.i);
    if (IsMakingPath(m)) {
      Jitter(A,
             "z4P"    // res0 = GetXmmOrMemPointer(RexbRm)
             "a2i"    // arg2 = RexrReg(rde)
             "s0a1="  // arg1 = machine
             "t"      // arg0 = res0
             "m",     // call function (d1)
             RexrReg(rde), s1);
    }
  } else if (Osz(rde)) {
    u8 *p;
    union DoublePun x[2], y[2];
    p = GetModrmRegisterXmmPointerRead16(A);
    y[0].i = Read64(p + 0 * 8);
    y[1].i = Read64(p + 1 * 8);
    p = XmmRexrReg(m, rde);
    x[0].i = Read64(p + 0 * 8);
    x[1].i = Read64(p + 1 * 8);
    x[0].f = fd(x[0].f, y[0].f);
    x[1].f = fd(x[1].f, y[1].f);
    Write64(p + 0 * 8, x[0].i);
    Write64(p + 1 * 8, x[1].i);
  } else {
    u8 *p;
    union FloatPun x[4], y[4];
    p = GetModrmRegisterXmmPointerRead16(A);
    y[0].i = Read32(p + 0 * 4);
    y[1].i = Read32(p + 1 * 4);
    y[2].i = Read32(p + 2 * 4);
    y[3].i = Read32(p + 3 * 4);
    p = XmmRexrReg(m, rde);
    x[0].i = Read32(p + 0 * 4);
    x[1].i = Read32(p + 1 * 4);
    x[2].i = Read32(p + 2 * 4);
    x[3].i = Read32(p + 3 * 4);
    x[0].f = fs(x[0].f, y[0].f);
    x[1].f = fs(x[1].f, y[1].f);
    x[2].f = fs(x[2].f, y[2].f);
    x[3].f = fs(x[3].f, y[3].f);
    Write32(p + 0 * 4, x[0].i);
    Write32(p + 1 * 4, x[1].i);
    Write32(p + 2 * 4, x[2].i);
    Write32(p + 3 * 4, x[3].i);
  }
  IGNORE_RACES_END();
}

static inline float Adds(float x, float y) {
  return x + y;
}

static inline double Addd(double x, double y) {
  return x + y;
}

void OpAddpsd(P) {
  OpPsd(A, Adds, Addd, OpPsdAdds1, OpPsdAddd1);
}

static inline float Subs(float x, float y) {
  return x - y;
}

static inline double Subd(double x, double y) {
  return x - y;
}

void OpSubpsd(P) {
  OpPsd(A, Subs, Subd, OpPsdSubs1, OpPsdSubd1);
}

static inline float Muls(float x, float y) {
  return x * y;
}

static inline double Muld(double x, double y) {
  return x * y;
}

void OpMulpsd(P) {
  OpPsd(A, Muls, Muld, OpPsdMuls1, OpPsdMuld1);
}

static inline float Divs(float x, float y) {
  return x / y;
}

static inline double Divd(double x, double y) {
  return x / y;
}

void OpDivpsd(P) {
  OpPsd(A, Divs, Divd, OpPsdDivs1, OpPsdDivd1);
}

static inline float Mins(float x, float y) {
  return MIN(x, y);
}

static inline double Mind(double x, double y) {
  return MIN(x, y);
}

void OpMinpsd(P) {
  OpPsd(A, Mins, Mind, OpPsdMins1, OpPsdMind1);
}

static inline float Maxs(float x, float y) {
  return MAX(x, y);
}

static inline double Maxd(double x, double y) {
  return MAX(x, y);
}

void OpMaxpsd(P) {
  OpPsd(A, Maxs, Maxd, OpPsdMaxs1, OpPsdMaxd1);
}

static int Cmps(int imm, float x, float y) {
  switch (imm) {
    case 0:
      return x == y ? -1 : 0;
    case 1:
      return x < y ? -1 : 0;
    case 2:
      return x <= y ? -1 : 0;
    case 3:
      return isnan(x) || isnan(y) ? -1 : 0;
    case 4:
      return x != y ? -1 : 0;
    case 5:
      return !(x < y) ? -1 : 0;
    case 6:
      return !(x <= y) ? -1 : 0;
    case 7:
      return !(isnan(x) || isnan(y)) ? -1 : 0;
    default:
      return 0;
  }
}

static i32 Cmpd(int imm, double x, double y) {
  switch (imm) {
    case 0:
      return x == y ? -1 : 0;
    case 1:
      return x < y ? -1 : 0;
    case 2:
      return x <= y ? -1 : 0;
    case 3:
      return isnan(x) || isnan(y) ? -1 : 0;
    case 4:
      return x != y ? -1 : 0;
    case 5:
      return !(x < y) ? -1 : 0;
    case 6:
      return !(x <= y) ? -1 : 0;
    case 7:
      return !(isnan(x) || isnan(y)) ? -1 : 0;
    default:
      return 0;
  }
}

void OpCmppsd(P) {
  IGNORE_RACES_START();
  int imm = uimm0;
  if (Rep(rde) == 2) {
    union DoublePun x, y;
    y.i = Read64(GetModrmRegisterXmmPointerRead8(A));
    x.i = Read64(XmmRexrReg(m, rde));
    x.i = Cmpd(imm, x.f, y.f);
    Write64(XmmRexrReg(m, rde), x.i);
  } else if (Rep(rde) == 3) {
    union FloatPun x, y;
    y.i = Read32(GetModrmRegisterXmmPointerRead4(A));
    x.i = Read32(XmmRexrReg(m, rde));
    x.i = Cmps(imm, x.f, y.f);
    Write32(XmmRexrReg(m, rde), x.i);
  } else if (Osz(rde)) {
    u8 *p;
    union DoublePun x[2], y[2];
    p = GetModrmRegisterXmmPointerRead16(A);
    y[0].i = Read64(p + 0 * 8);
    y[1].i = Read64(p + 1 * 8);
    p = XmmRexrReg(m, rde);
    x[0].i = Read64(p + 0 * 8);
    x[1].i = Read64(p + 1 * 8);
    x[0].i = Cmpd(imm, x[0].f, y[0].f);
    x[1].i = Cmpd(imm, x[1].f, y[1].f);
    Write64(p + 0 * 8, x[0].i);
    Write64(p + 1 * 8, x[1].i);
  } else {
    u8 *p;
    union FloatPun x[4], y[4];
    p = GetModrmRegisterXmmPointerRead16(A);
    y[0].i = Read32(p + 0 * 4);
    y[1].i = Read32(p + 1 * 4);
    y[2].i = Read32(p + 2 * 4);
    y[3].i = Read32(p + 3 * 4);
    p = XmmRexrReg(m, rde);
    x[0].i = Read32(p + 0 * 4);
    x[1].i = Read32(p + 1 * 4);
    x[2].i = Read32(p + 2 * 4);
    x[3].i = Read32(p + 3 * 4);
    x[0].i = Cmps(imm, x[0].f, y[0].f);
    x[1].i = Cmps(imm, x[1].f, y[1].f);
    x[2].i = Cmps(imm, x[2].f, y[2].f);
    x[3].i = Cmps(imm, x[3].f, y[3].f);
    Write32(p + 0 * 4, x[0].i);
    Write32(p + 1 * 4, x[1].i);
    Write32(p + 2 * 4, x[2].i);
    Write32(p + 3 * 4, x[3].i);
  }
  IGNORE_RACES_END();
}

void OpAndpsd(P) {
  IGNORE_RACES_START();
  u64 x[2], y[2];
  memcpy(x, XmmRexrReg(m, rde), 16);
  memcpy(y, GetModrmRegisterXmmPointerRead16(A), 16);
  x[0] &= y[0];
  x[1] &= y[1];
  memcpy(XmmRexrReg(m, rde), x, 16);
  IGNORE_RACES_END();
}

void OpAndnpsd(P) {
  IGNORE_RACES_START();
  u64 x[2], y[2];
  memcpy(x, XmmRexrReg(m, rde), 16);
  memcpy(y, GetModrmRegisterXmmPointerRead16(A), 16);
  x[0] = ~x[0] & y[0];
  x[1] = ~x[1] & y[1];
  memcpy(XmmRexrReg(m, rde), x, 16);
  IGNORE_RACES_END();
}

void OpOrpsd(P) {
  IGNORE_RACES_START();
  u64 x[2], y[2];
  memcpy(x, XmmRexrReg(m, rde), 16);
  memcpy(y, GetModrmRegisterXmmPointerRead16(A), 16);
  x[0] |= y[0];
  x[1] |= y[1];
  memcpy(XmmRexrReg(m, rde), x, 16);
  IGNORE_RACES_END();
}

void OpXorpsd(P) {
  IGNORE_RACES_START();
  u64 x[2], y[2];
  memcpy(x, XmmRexrReg(m, rde), 16);
  memcpy(y, GetModrmRegisterXmmPointerRead16(A), 16);
  x[0] ^= y[0];
  x[1] ^= y[1];
  memcpy(XmmRexrReg(m, rde), x, 16);
  IGNORE_RACES_END();
}

void OpHaddpsd(P) {
  IGNORE_RACES_START();
  u8 *p;
  if (Rep(rde) == 2) {
    union FloatPun x[4], y[4], z[4];
    p = GetModrmRegisterXmmPointerRead16(A);
    y[0].i = Read32(p + 0 * 4);
    y[1].i = Read32(p + 1 * 4);
    y[2].i = Read32(p + 2 * 4);
    y[3].i = Read32(p + 3 * 4);
    p = XmmRexrReg(m, rde);
    x[0].i = Read32(p + 0 * 4);
    x[1].i = Read32(p + 1 * 4);
    x[2].i = Read32(p + 2 * 4);
    x[3].i = Read32(p + 3 * 4);
    z[0].f = x[0].f + x[1].f;
    z[1].f = x[2].f + x[3].f;
    z[2].f = y[0].f + y[1].f;
    z[3].f = y[2].f + y[3].f;
    Write32(p + 0 * 4, z[0].i);
    Write32(p + 1 * 4, z[1].i);
    Write32(p + 2 * 4, z[2].i);
    Write32(p + 3 * 4, z[3].i);
  } else if (Osz(rde)) {
    union DoublePun x[2], y[2], z[2];
    p = GetModrmRegisterXmmPointerRead16(A);
    y[0].i = Read64(p + 0 * 8);
    y[1].i = Read64(p + 1 * 8);
    p = XmmRexrReg(m, rde);
    x[0].i = Read64(p + 0 * 8);
    x[1].i = Read64(p + 1 * 8);
    z[0].f = x[0].f + x[1].f;
    z[1].f = y[0].f + y[1].f;
    Write64(p + 0 * 8, z[0].i);
    Write64(p + 1 * 8, z[1].i);
  } else {
    OpUdImpl(m);
  }
  IGNORE_RACES_END();
}

void OpHsubpsd(P) {
  IGNORE_RACES_START();
  u8 *p;
  if (Rep(rde) == 2) {
    union FloatPun x[4], y[4], z[4];
    p = GetModrmRegisterXmmPointerRead16(A);
    y[0].i = Read32(p + 0 * 4);
    y[1].i = Read32(p + 1 * 4);
    y[2].i = Read32(p + 2 * 4);
    y[3].i = Read32(p + 3 * 4);
    p = XmmRexrReg(m, rde);
    x[0].i = Read32(p + 0 * 4);
    x[1].i = Read32(p + 1 * 4);
    x[2].i = Read32(p + 2 * 4);
    x[3].i = Read32(p + 3 * 4);
    z[0].f = x[0].f - x[1].f;
    z[1].f = x[2].f - x[3].f;
    z[2].f = y[0].f - y[1].f;
    z[3].f = y[2].f - y[3].f;
    Write32(p + 0 * 4, z[0].i);
    Write32(p + 1 * 4, z[1].i);
    Write32(p + 2 * 4, z[2].i);
    Write32(p + 3 * 4, z[3].i);
  } else if (Osz(rde)) {
    union DoublePun x[2], y[2], z[2];
    p = GetModrmRegisterXmmPointerRead16(A);
    y[0].i = Read64(p + 0 * 8);
    y[1].i = Read64(p + 1 * 8);
    p = XmmRexrReg(m, rde);
    x[0].i = Read64(p + 0 * 8);
    x[1].i = Read64(p + 1 * 8);
    z[0].f = x[0].f - x[1].f;
    z[1].f = y[0].f - y[1].f;
    Write64(p + 0 * 8, z[0].i);
    Write64(p + 1 * 8, z[1].i);
  } else {
    OpUdImpl(m);
  }
  IGNORE_RACES_END();
}

void OpAddsubpsd(P) {
  IGNORE_RACES_START();
  u8 *p;
  if (Rep(rde) == 2) {
    union FloatPun x[4], y[4], z[4];
    p = GetModrmRegisterXmmPointerRead16(A);
    y[0].i = Read32(p + 0 * 4);
    y[1].i = Read32(p + 1 * 4);
    y[2].i = Read32(p + 2 * 4);
    y[3].i = Read32(p + 3 * 4);
    p = XmmRexrReg(m, rde);
    x[0].i = Read32(p + 0 * 4);
    x[1].i = Read32(p + 1 * 4);
    x[2].i = Read32(p + 2 * 4);
    x[3].i = Read32(p + 3 * 4);
    z[0].f = x[0].f - y[0].f;
    z[1].f = x[1].f + y[1].f;
    z[2].f = x[2].f - y[2].f;
    z[3].f = x[3].f + y[3].f;
    Write32(p + 0 * 4, z[0].i);
    Write32(p + 1 * 4, z[1].i);
    Write32(p + 2 * 4, z[2].i);
    Write32(p + 3 * 4, z[3].i);
  } else if (Osz(rde)) {
    union DoublePun x[2], y[2], z[2];
    p = GetModrmRegisterXmmPointerRead16(A);
    y[0].i = Read64(p + 0 * 8);
    y[1].i = Read64(p + 1 * 8);
    p = XmmRexrReg(m, rde);
    x[0].i = Read64(p + 0 * 8);
    x[1].i = Read64(p + 1 * 8);
    z[0].f = x[0].f - y[0].f;
    z[1].f = x[1].f + y[1].f;
    Write64(p + 0 * 8, z[0].i);
    Write64(p + 1 * 8, z[1].i);
  } else {
    OpUdImpl(m);
  }
  IGNORE_RACES_END();
}
