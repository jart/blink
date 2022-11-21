/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2018 Intel Corporation                                             │
│ Copyright 2022 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Licensed under the Apache License, Version 2.0 (the "License");              │
│ you may not use this file except in compliance with the License.             │
│ You may obtain a copy of the License at                                      │
│                                                                              │
│     http://www.apache.org/licenses/LICENSE-2.0                               │
│                                                                              │
│ Unless required by applicable law or agreed to in writing, software          │
│ distributed under the License is distributed on an "AS IS" BASIS,            │
│ WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.     │
│ See the License for the specific language governing permissions and          │
│ limitations under the License.                                               │
╚─────────────────────────────────────────────────────────────────────────────*/
#include <string.h>

#include "blink/assert.h"
#include "blink/bitscan.h"
#include "blink/builtin.h"
#include "blink/endian.h"
#include "blink/macros.h"
#include "blink/modrm.h"
#include "blink/x86.h"

const char kXedCopyright[] = "\
Xed (Apache 2.0)\n\
Copyright 2018 Intel Corporation\n\
Copyright 2022 Justine Alexandra Roberts Tunney\n\
Modifications: Removed as much code and functionality as possible";

#define XED_ILD_HASMODRM_IGNORE_MOD 2

static const u32 xed_prefix_table_bit[8] = {
    0x00000000, 0x40404040, 0x0000ffff, 0x000000f0,
    0x00000000, 0x00000000, 0x00000000, 0x000d0000,
};

static const struct XedDenseMagnums {
  u8 eamode[2][3];
  u8 has_sib_table[3][4][8];
  u8 has_disp_regular[3][4][8];
  u8 imm_bits_2d[2][256];
  u8 has_modrm_2d[XED_ILD_MAP2][256];
  u8 disp_bits_2d[XED_ILD_MAP2][256];
  u8 BRDISPz_BRDISP_WIDTH[4];
  u8 MEMDISPv_DISP_WIDTH[4];
  u8 SIMMz_IMM_WIDTH[4];
  u8 UIMMv_IMM_WIDTH[4];
  u8 ASZ_NONTERM_EASZ[2][3];
  u8 OSZ_NONTERM_DF64_EOSZ[2][2][3];
  u8 OSZ_NONTERM_EOSZ[2][2][3];
} kXed = {
    .eamode = {{XED_MODE_REAL, XED_MODE_LEGACY, XED_MODE_LONG},
               {XED_MODE_LEGACY, XED_MODE_REAL, XED_MODE_LEGACY}},
    .has_sib_table = {{{0, 0, 0, 0, 0, 0, 0, 0},
                       {0, 0, 0, 0, 0, 0, 0, 0},
                       {0, 0, 0, 0, 0, 0, 0, 0},
                       {0, 0, 0, 0, 0, 0, 0, 0}},
                      {{0, 0, 0, 0, 1, 0, 0, 0},
                       {0, 0, 0, 0, 1, 0, 0, 0},
                       {0, 0, 0, 0, 1, 0, 0, 0},
                       {0, 0, 0, 0, 0, 0, 0, 0}},
                      {{0, 0, 0, 0, 1, 0, 0, 0},
                       {0, 0, 0, 0, 1, 0, 0, 0},
                       {0, 0, 0, 0, 1, 0, 0, 0},
                       {0, 0, 0, 0, 0, 0, 0, 0}}},
    .has_disp_regular = {{{0, 0, 0, 0, 0, 0, 2, 0},
                          {1, 1, 1, 1, 1, 1, 1, 1},
                          {2, 2, 2, 2, 2, 2, 2, 2},
                          {0, 0, 0, 0, 0, 0, 0, 0}},
                         {{0, 0, 0, 0, 0, 4, 0, 0},
                          {1, 1, 1, 1, 1, 1, 1, 1},
                          {4, 4, 4, 4, 4, 4, 4, 4},
                          {0, 0, 0, 0, 0, 0, 0, 0}},
                         {{0, 0, 0, 0, 0, 4, 0, 0},
                          {1, 1, 1, 1, 1, 1, 1, 1},
                          {4, 4, 4, 4, 4, 4, 4, 4},
                          {0, 0, 0, 0, 0, 0, 0, 0}}},
    .imm_bits_2d =
        {{1,  1, 1, 1, 5,  7,  1,  1,  1,  1,  1,  1,  9, 7, 1, 0, 1, 1, 1, 1,
          5,  7, 1, 1, 1,  1,  1,  1,  5,  7,  1,  1,  1, 1, 1, 1, 5, 7, 0, 1,
          1,  1, 1, 1, 5,  7,  0,  1,  1,  1,  1,  1,  9, 7, 0, 1, 1, 1, 1, 1,
          5,  7, 0, 1, 1,  1,  1,  1,  1,  1,  1,  1,  1, 1, 1, 1, 1, 1, 1, 1,
          1,  1, 1, 1, 1,  1,  1,  1,  1,  1,  1,  1,  1, 1, 1, 1, 1, 1, 1, 1,
          0,  0, 0, 0, 6,  7,  5,  5,  1,  1,  1,  1,  1, 1, 1, 1, 1, 1, 1, 1,
          1,  1, 1, 1, 1,  1,  1,  1,  5,  7,  5,  5,  1, 1, 1, 1, 1, 1, 1, 1,
          1,  1, 1, 1, 1,  1,  1,  1,  1,  1,  1,  1,  1, 1, 8, 1, 1, 1, 1, 1,
          1,  1, 1, 1, 1,  1,  1,  1,  5,  7,  1,  1,  1, 1, 1, 1, 9, 9, 9, 9,
          9,  9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 9, 9, 8, 1, 1, 1, 9, 2,
          11, 1, 8, 1, 1,  9,  1,  1,  1,  1,  1,  1,  9, 9, 1, 1, 1, 1, 1, 1,
          1,  1, 1, 1, 1,  1,  1,  1,  9,  9,  9,  9,  1, 1, 8, 1, 1, 1, 1, 1,
          0,  1, 0, 0, 1,  1,  3,  4,  1,  1,  1,  1,  1, 1, 1, 1},
         {1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0,  1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,  1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
          1, 1, 9, 9, 9, 9, 1, 1, 1, 1, 12, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 9,  1, 0, 0, 1, 1, 1, 1, 9, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 9,  1, 1, 1, 1, 1, 1, 1, 9, 1, 9, 9,
          9, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1}},
    .has_modrm_2d =
        {{1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 3, 1, 1, 1, 1, 0, 0,
          0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 3, 0, 1, 1, 1, 1,
          0, 0, 3, 0, 1, 1, 1, 1, 0, 0, 3, 0, 1, 1, 1, 1, 0, 0, 3, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 3, 3, 3, 3, 0, 1, 0, 1, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1,
          1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1,
          1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0,
          3, 3, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1},
         {1, 1, 1, 1, 3, 0, 0, 0, 0, 0, 3, 0, 3, 1, 0, 3, 1, 1, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 1, 1, 1, 1,
          1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 3, 0, 3, 3, 3, 3, 3, 3, 3, 3, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 3, 3, 0, 0, 0, 1, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
          1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    .disp_bits_2d =
        {{4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4,
          4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4,
          4, 4, 0, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4,
          4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
          4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4,
          4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4, 4, 4, 4,
          4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
          2, 4, 4, 4, 4, 4, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
          4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
          4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
          4, 4, 4, 4, 1, 1, 1, 1, 4, 4, 4, 4, 3, 3, 2, 1, 4, 4, 4, 4, 0, 4,
          0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4},
         {4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 0, 4, 0, 4, 4, 0, 4, 4, 4, 4, 4, 4,
          4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 4, 4, 4, 4,
          4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4,
          4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
          4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
          4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3,
          3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
          4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4,
          4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
          4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
          4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
          4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4}},
    .BRDISPz_BRDISP_WIDTH = {0, 16, 32, 32},
    .MEMDISPv_DISP_WIDTH = {0, 16, 32, 64},
    .SIMMz_IMM_WIDTH = {0x00, 0x10, 0x20, 0x20},
    .UIMMv_IMM_WIDTH = {0x00, 0x10, 0x20, 0x40},
    .ASZ_NONTERM_EASZ = {{1, 2, 3}, {2, 1, 2}},
    .OSZ_NONTERM_DF64_EOSZ = {{{1, 2, 3}, {2, 1, 1}}, {{1, 2, 3}, {2, 1, 3}}},
    .OSZ_NONTERM_EOSZ = {{{1, 2, 2}, {2, 1, 1}}, {{1, 2, 3}, {2, 1, 3}}},
};

static size_t xed_bits2bytes(unsigned bits) {
  return bits >> 3;
}

static size_t xed_bytes2bits(unsigned bytes) {
  return bytes << 3;
}

static u8 xed_get_prefix_table_bit(u8 a) {
  return (xed_prefix_table_bit[a >> 5] >> (a & 0x1F)) & 1;
}

static void xed_set_mopcode(struct XedDecodedInst *x, u64 mopcode) {
  x->op.rde |= mopcode << 40;
}

static int xed_too_short(struct XedDecodedInst *x) {
  if (x->op.max_bytes >= 15) {
    return XED_ERROR_INSTR_TOO_LONG;
  } else {
    return XED_ERROR_BUFFER_TOO_SHORT;
  }
}

static void xed_set_simmz_imm_width_eosz(struct XedDecodedInst *x,
                                         const u8 eosz[2][2][3], u8 *imm_width,
                                         u8 *imm_signed) {
  *imm_width = kXed.SIMMz_IMM_WIDTH[eosz[Rexw(x->op.rde)][Osz(x->op.rde)]
                                        [Mode(x->op.rde)]];
  *imm_signed = 1;
}

static int xed_set_imm_bytes(struct XedDecodedInst *x, u8 *imm_width,
                             u8 *imm_signed) {
  if (!*imm_width && Opmap(x->op.rde) < XED_ILD_MAP2) {
    switch (kXed.imm_bits_2d[Opmap(x->op.rde)][Opcode(x->op.rde)]) {
      case 0:
        return XED_ERROR_GENERAL_ERROR;
      case 1:
        *imm_width = 0;
        return XED_ERROR_NONE;
      case 2:
        switch (ModrmReg(x->op.rde)) {
          case 0:
            xed_set_simmz_imm_width_eosz(x, kXed.OSZ_NONTERM_EOSZ, imm_width,
                                         imm_signed);
            return XED_ERROR_NONE;
          case 7:
            *imm_width = 0;
            return XED_ERROR_NONE;
          default:
            return XED_ERROR_NONE;
        }
      case 3:
        if (ModrmReg(x->op.rde) <= 1) {
          *imm_width = 8;
          *imm_signed = 1;
        } else if (2 <= ModrmReg(x->op.rde) && ModrmReg(x->op.rde) <= 7) {
          *imm_width = 0;
        }
        return XED_ERROR_NONE;
      case 4:
        if (ModrmReg(x->op.rde) <= 1) {
          xed_set_simmz_imm_width_eosz(x, kXed.OSZ_NONTERM_EOSZ, imm_width,
                                       imm_signed);
        } else if (2 <= ModrmReg(x->op.rde) && ModrmReg(x->op.rde) <= 7) {
          *imm_width = 0;
        }
        return XED_ERROR_NONE;
      case 5:
        *imm_width = 8;
        *imm_signed = 1;
        return XED_ERROR_NONE;
      case 6:
        xed_set_simmz_imm_width_eosz(x, kXed.OSZ_NONTERM_DF64_EOSZ, imm_width,
                                     imm_signed);
        return XED_ERROR_NONE;
      case 7:
        xed_set_simmz_imm_width_eosz(x, kXed.OSZ_NONTERM_EOSZ, imm_width,
                                     imm_signed);
        return XED_ERROR_NONE;
      case 8:
        *imm_width = 16;
        return XED_ERROR_NONE;
      case 9:
        *imm_width = 8;
        return XED_ERROR_NONE;
      case 10:
        *imm_width = kXed.UIMMv_IMM_WIDTH[kXed.OSZ_NONTERM_EOSZ[Rexw(
            x->op.rde)][Osz(x->op.rde)][Mode(x->op.rde)]];
        return XED_ERROR_NONE;
      case 11:
        *imm_width = xed_bytes2bits(2);
        return XED_ERROR_NONE;
      case 12:
        if (Osz(x->op.rde) || Rep(x->op.rde) == 2) {
          *imm_width = xed_bytes2bits(1);
        }
        return XED_ERROR_NONE;
      default:
        __builtin_unreachable();
    }
  } else {
    return XED_ERROR_NONE;
  }
}

static int xed_prefix_scanner(struct XedDecodedInst *x) {
  u32 rde;
  u8 b, rep, max_bytes, length, islong;
  u8 asz, osz, rex, rexw, rexr, rexx, rexb;
  rex = 0;
  rep = 0;
  length = 0;
  rde = x->op.rde;
  max_bytes = x->op.max_bytes;
  islong = Mode(rde) == XED_MODE_LONG;
  while (length < max_bytes) {
    b = x->bytes[length];
    if (xed_get_prefix_table_bit(b) == 0) goto out;
    switch (b) {
      case 0x66:  // osz
        rex = 0;
        osz = 1;
        rde |= osz << 5;
        break;
      case 0x67:  // asz
        rex = 0;
        asz = 1;
        rde |= asz << 21;
        break;
      case 0x2E:  // cs
        if (1 || !islong) {
          rde &= 037770777777;
          rde |= 000002000000;
        }
        rex = 0;
        break;
      case 0x3E:  // ds
        if (1 || !islong) {
          rde &= 037770777777;
          rde |= 000004000000;
        }
        rex = 0;
        break;
      case 0x26:  // es
        if (1 || !islong) {
          rde &= 037770777777;
          rde |= 000001000000;
        }
        rex = 0;
        break;
      case 0x36:  // ss
        if (1 || !islong) {
          rde &= 037770777777;
          rde |= 000003000000;
        }
        rex = 0;
        break;
      case 0x64:  // fs
        rde &= 037770777777;
        rde |= 000005000000;
        rex = 0;
        break;
      case 0x65:  // gs
        rde &= 037770777777;
        rde |= 000006000000;
        rex = 0;
        break;
      case 0xF0:  // lock
        rde |= 020000000000;
        rex = 0;
        break;
      case 0xF2:  // rep
      case 0xF3:
        rep = b & 3;
        rex = 0;
        break;
      default:
        if (islong && (b & 0xf0) == 0x40) {
          rex = b;
          break;
        } else {
          goto out;
        }
    }
    length++;
  }
out:
  x->length = length;
  if (rex) {
    rexw = (rex >> 3) & 1;
    rexr = (rex >> 2) & 1;
    rexx = (rex >> 1) & 1;
    rexb = rex & 1;
    rex = 1;
    rde |= rexx << 17 | rex << 16 | rexb << 15 | rex << 11 | rexb << 10 |
           rexw << 6 | rex << 4 | rexr << 3;
  }
  x->op.rde = (u64)rep << 51 | rde;
  if (length < max_bytes) {
    return XED_ERROR_NONE;
  } else {
    return xed_too_short(x);
  }
}

static int xed_get_next_as_opcode(struct XedDecodedInst *x, int map) {
  u8 length;
  length = x->length;
  if (length < x->op.max_bytes) {
    xed_set_mopcode(x, map << 8 | x->bytes[length]);
    x->length++;
    return XED_ERROR_NONE;
  } else {
    return xed_too_short(x);
  }
}

static int xed_opcode_scanner(struct XedDecodedInst *x, u8 *imm_width) {
  u8 b, length;
  length = x->length;
  if ((b = x->bytes[length]) != 0x0F) {
    xed_set_mopcode(x, XED_ILD_MAP0 << 8 | b);
    x->op.pos_opcode = length;
    x->length++;
    return XED_ERROR_NONE;
  } else {
    length++;
    x->op.pos_opcode = length;
    if (length < x->op.max_bytes) {
      switch ((b = x->bytes[length])) {
        case 0x38:
          x->length = ++length;
          return xed_get_next_as_opcode(x, XED_ILD_MAP2);
        case 0x3A:
          x->length = ++length;
          *imm_width = xed_bytes2bits(1);
          return xed_get_next_as_opcode(x, XED_ILD_MAP3);
        case 0x3B:
        case 0x39:
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
        case 0x0F:
          length++;
          x->length = length;
          xed_get_next_as_opcode(x, XED_ILD_BAD_MAP);
          return XED_ERROR_BAD_MAP;
        default:
          xed_set_mopcode(x, XED_ILD_MAP1 << 8 | b);
          x->length = ++length;
          return XED_ERROR_NONE;
      }
    } else {
      return xed_too_short(x);
    }
  }
}

static u64 xed_read_number(u8 *p, size_t n, unsigned s) {
  switch (s << 2 | bsr(n)) {
    case 0:
      return Read8(p);
    case 1:
      return Read16(p);
    case 2:
      return Read32(p);
    case 3:
      return Read64(p);
    case 4:
      return (i8)Read8(p);
    case 5:
      return (i16)Read16(p);
    case 6:
      return (i32)Read32(p);
    case 7:
      return (i64)Read64(p);
    default:
      __builtin_unreachable();
  }
}

static void xed_set_has_modrm(struct XedDecodedInst *x) {
  if (Opmap(x->op.rde) < ARRAYLEN(kXed.has_modrm_2d)) {
    x->op.has_modrm = kXed.has_modrm_2d[Opmap(x->op.rde)][Opcode(x->op.rde)];
  } else {
    x->op.has_modrm = 1;
  }
}

static int xed_modrm_scanner(struct XedDecodedInst *x, u8 *disp_width,
                             u8 *has_sib) {
  u8 b, rm, reg, mod, eamode, length, has_modrm;
  xed_set_has_modrm(x);
  has_modrm = x->op.has_modrm;
  if (has_modrm) {
    length = x->length;
    if (length < x->op.max_bytes) {
      b = x->bytes[length];
      x->length++;
      rm = b & 0007;
      reg = (b & 0070) >> 3;
      mod = (b & 0300) >> 6;
      x->op.rde &= ~1;
      x->op.rde |= mod << 22 | rm << 7 | reg;
      if (has_modrm != XED_ILD_HASMODRM_IGNORE_MOD) {
        eamode = kXed.eamode[Asz(x->op.rde)][Mode(x->op.rde)];
        *disp_width = xed_bytes2bits(kXed.has_disp_regular[eamode][mod][rm]);
        *has_sib = kXed.has_sib_table[eamode][mod][rm];
      }
      return XED_ERROR_NONE;
    } else {
      return xed_too_short(x);
    }
  } else {
    return XED_ERROR_NONE;
  }
}

static int xed_sib_scanner(struct XedDecodedInst *x, u8 *disp_width) {
  u8 b, length;
  length = x->length;
  if (length < x->op.max_bytes) {
    b = x->bytes[length];
    x->op.rde |= (u64)b << 32;  // set sib byte
    x->length++;
    if ((b & 7) == 5) {
      if (!ModrmMod(x->op.rde)) {
        *disp_width = xed_bytes2bits(4);
      }
    }
    return XED_ERROR_NONE;
  } else {
    return xed_too_short(x);
  }
}

static u8 XED_LF_BRDISPz_BRDISP_WIDTH_OSZ_NONTERM_EOSZ_l2(
    struct XedDecodedInst *x) {
  return kXed.BRDISPz_BRDISP_WIDTH[kXed.OSZ_NONTERM_EOSZ[Rexw(x->op.rde)][Osz(
      x->op.rde)][Mode(x->op.rde)]];
}

static int xed_disp_scanner(struct XedDecodedInst *x, u8 disp_width) {
  u8 disp_unsigned = 0;
  u8 length, disp_bytes;
  length = x->length;
  if (Opmap(x->op.rde) < XED_ILD_MAP2) {
    switch (kXed.disp_bits_2d[Opmap(x->op.rde)][Opcode(x->op.rde)]) {
      case 0:
        return XED_ERROR_GENERAL_ERROR;
      case 1:
        disp_width = 8;
        break;
      case 2:
        disp_width = XED_LF_BRDISPz_BRDISP_WIDTH_OSZ_NONTERM_EOSZ_l2(x);
        disp_unsigned = 1;
        break;
      case 3:
        if (Mode(x->op.rde) <= XED_MODE_LEGACY) {
          disp_width = XED_LF_BRDISPz_BRDISP_WIDTH_OSZ_NONTERM_EOSZ_l2(x);
        } else if (Mode(x->op.rde) == XED_MODE_LONG) {
          disp_width = 0x20;
        }
        break;
      case 4:
        break;
      case 5:
        disp_width =
            kXed.MEMDISPv_DISP_WIDTH[kXed.ASZ_NONTERM_EASZ[Asz(x->op.rde)]
                                                          [Mode(x->op.rde)]];
        disp_unsigned = 1;
        break;
      case 6:
        switch (ModrmReg(x->op.rde)) {
          case 0:
            break;
          case 7:
            disp_width = XED_LF_BRDISPz_BRDISP_WIDTH_OSZ_NONTERM_EOSZ_l2(x);
            disp_unsigned = 1;
            break;
          default:
            break;
        }
        break;
      default:
        __builtin_unreachable();
    }
  }
  disp_bytes = xed_bits2bytes(disp_width);
  if (!disp_bytes) {
    return XED_ERROR_NONE;
  } else if (length + disp_bytes <= x->op.max_bytes) {
    x->op.disp = xed_read_number(x->bytes + length, disp_bytes, !disp_unsigned);
    x->length = length + disp_bytes;
    return XED_ERROR_NONE;
  } else {
    return xed_too_short(x);
  }
}

static int xed_imm_scanner(struct XedDecodedInst *x, u8 imm_width) {
  u8 *p;
  int e;
  u8 imm_signed = 0;
  u8 i, length, imm_bytes;
  p = x->bytes;
  if ((e = xed_set_imm_bytes(x, &imm_width, &imm_signed))) return e;
  i = length = x->length;
  imm_bytes = xed_bits2bytes(imm_width);
  if (!imm_bytes) {
    return XED_ERROR_NONE;
  } else if (length + imm_bytes <= x->op.max_bytes) {
    length += imm_bytes;
    x->length = length;
    x->op.uimm0 = xed_read_number(p + i, imm_bytes, imm_signed);
    return XED_ERROR_NONE;
  } else {
    return xed_too_short(x);
  }
}

static int xed_decode_instruction_length(struct XedDecodedInst *x) {
  int e;
  u8 has_sib = 0;
  u8 imm_width = 0;
  u8 disp_width = 0;
  if ((e = xed_prefix_scanner(x))) return e;
  if ((e = xed_opcode_scanner(x, &imm_width))) return e;
  if ((e = xed_modrm_scanner(x, &disp_width, &has_sib))) return e;
  if (has_sib && (e = xed_sib_scanner(x, &disp_width))) return e;
  if ((e = xed_disp_scanner(x, disp_width))) return e;
  return xed_imm_scanner(x, imm_width);
}

/**
 * Decodes machine instruction.
 *
 * This function also decodes other useful attributes, such as the
 * offsets of displacement, immediates, etc. It works for all ISAs from
 * 1977 to 2020.
 *
 * @note binary footprint increases ~4kb if this is used
 * @see biggest code in gdb/clang/tensorflow binaries
 */
int DecodeInstruction(struct XedDecodedInst *x, const void *itext, size_t bytes,
                      u64 mode) {
  int rc;
  u64 rde;
  u8 kWordLog2[2][2][2] = {{{2, 3}, {1, 3}}};
  unassert(mode == XED_MODE_LONG ||    //
           mode == XED_MODE_LEGACY ||  //
           mode == XED_MODE_REAL);
  x->op.rde = mode << 26;
  x->op.disp = 0;
  x->op.uimm0 = 0;
  x->op.has_modrm = 0;
  x->op.pos_opcode = 0;
  if (bytes >= 15) {
    x->op.max_bytes = 15;
    memcpy(x->bytes, itext, 15);
  } else {
    x->op.max_bytes = bytes;
    memcpy(x->bytes, itext, bytes);
  }
  rc = xed_decode_instruction_length(x);
  rde = x->op.rde;
  rde |= (Opcode(rde) & 7) << 12;
  rde ^= (Mode(rde) == XED_MODE_REAL) << 5;  // osz ^= real
  rde |= kWordLog2[~Opcode(rde) & 1][Osz(rde)][Rexw(rde)] << 28;
  rde |= kXed.eamode[Asz(rde)][Mode(rde)] << 24;
  rde |= (u64)x->length << 53;
  x->op.rde = rde;
  return rc;
}
