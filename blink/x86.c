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

#include "blink/bitscan.h"
#include "blink/builtin.h"
#include "blink/endian.h"
#include "blink/macros.h"
#include "blink/modrm.h"
#include "blink/x86.h"

#define XED_ILD_HASMODRM_IGNORE_MOD 2

#define XED_I_LF_BRDISP8_BRDISP_WIDTH_CONST_l2            1
#define XED_I_LF_BRDISPz_BRDISP_WIDTH_OSZ_NONTERM_EOSZ_l2 2
#define XED_I_LF_DISP_BUCKET_0_l1                         3
#define XED_I_LF_EMPTY_DISP_CONST_l2                      4
#define XED_I_LF_MEMDISPv_DISP_WIDTH_ASZ_NONTERM_EASZ_l2  5
#define XED_I_LF_RESOLVE_BYREG_DISP_map0x0_op0xc7_l1      6

#define XED_I_LF_0_IMM_WIDTH_CONST_l2                     1
#define XED_I_LF_RESOLVE_BYREG_IMM_WIDTH_map0x0_op0xc7_l1 2
#define XED_I_LF_RESOLVE_BYREG_IMM_WIDTH_map0x0_op0xf6_l1 3
#define XED_I_LF_RESOLVE_BYREG_IMM_WIDTH_map0x0_op0xf7_l1 4
#define XED_I_LF_SIMM8_IMM_WIDTH_CONST_l2                 5
#define XED_I_LF_SIMMz_IMM_WIDTH_OSZ_NONTERM_DF64_EOSZ_l2 6
#define XED_I_LF_SIMMz_IMM_WIDTH_OSZ_NONTERM_EOSZ_l2      7
#define XED_I_LF_UIMM16_IMM_WIDTH_CONST_l2                8
#define XED_I_LF_UIMM8_IMM_WIDTH_CONST_l2                 9
#define XED_I_LF_UIMMv_IMM_WIDTH_OSZ_NONTERM_EOSZ_l2      10
#define xed_i_ild_hasimm_map0x0_op0xc8_l1                 11
#define xed_i_ild_hasimm_map0x0F_op0x78_l1                12

#define XED_LF_SIMMz_IMM_WIDTH_OSZ_NONTERM_IGNORE66_EOSZ_l2(X) \
  xed_set_simmz_imm_width_eosz(X, kXed.OSZ_NONTERM_IGNORE66_EOSZ)
#define XED_LF_SIMMz_IMM_WIDTH_OSZ_NONTERM_REFINING66_EOSZ_l2(X) \
  xed_set_simmz_imm_width_eosz(X, kXed.OSZ_NONTERM_REFINING66_EOSZ)
#define XED_LF_SIMMz_IMM_WIDTH_OSZ_NONTERM_DF64_FORCE64_EOSZ_l2(X) \
  xed_set_simmz_imm_width_eosz(X, kXed.OSZ_NONTERM_DF64_FORCE64_EOSZ)
#define XED_LF_SIMMz_IMM_WIDTH_OSZ_NONTERM_FORCE64_EOSZ_l2(X) \
  xed_set_simmz_imm_width_eosz(X, kXed.OSZ_NONTERM_FORCE64_EOSZ)
#define XED_LF_SIMMz_IMM_WIDTH_OSZ_NONTERM_EOSZ_l2(X) \
  xed_set_simmz_imm_width_eosz(X, kXed.OSZ_NONTERM_EOSZ)
#define XED_LF_SIMMz_IMM_WIDTH_OSZ_NONTERM_REFINING66_CR_WIDTH_EOSZ_l2(X) \
  xed_set_simmz_imm_width_eosz(X, kXed.OSZ_NONTERM_REFINING66_CR_WIDTH_EOSZ)
#define XED_LF_SIMMz_IMM_WIDTH_OSZ_NONTERM_DF64_EOSZ_l2(X) \
  xed_set_simmz_imm_width_eosz(X, kXed.OSZ_NONTERM_DF64_EOSZ)
#define XED_LF_SIMMz_IMM_WIDTH_OSZ_NONTERM_DF64_IMMUNE66_LOOP64_EOSZ_l2(X) \
  xed_set_simmz_imm_width_eosz(X, kXed.OSZ_NONTERM_DF64_IMMUNE66_LOOP64_EOSZ)
#define XED_LF_SIMMz_IMM_WIDTH_OSZ_NONTERM_IMMUNE66_EOSZ_l2(X) \
  xed_set_simmz_imm_width_eosz(X, kXed.OSZ_NONTERM_IMMUNE66_EOSZ)
#define XED_LF_SIMMz_IMM_WIDTH_OSZ_NONTERM_CR_WIDTH_EOSZ_l2(X) \
  xed_set_simmz_imm_width_eosz(X, kXed.OSZ_NONTERM_CR_WIDTH_EOSZ)
#define XED_LF_SIMMz_IMM_WIDTH_OSZ_NONTERM_IMMUNE_REXW_EOSZ_l2(X) \
  xed_set_simmz_imm_width_eosz(X, kXed.OSZ_NONTERM_IMMUNE_REXW_EOSZ)
#define XED_LF_UIMMv_IMM_WIDTH_OSZ_NONTERM_IGNORE66_EOSZ_l2(X) \
  xed_set_uimmv_imm_width_eosz(X, kXed.OSZ_NONTERM_IGNORE66_EOSZ)
#define XED_LF_UIMMv_IMM_WIDTH_OSZ_NONTERM_REFINING66_EOSZ_l2(X) \
  xed_set_uimmv_imm_width_eosz(X, kXed.OSZ_NONTERM_REFINING66_EOSZ)
#define XED_LF_UIMMv_IMM_WIDTH_OSZ_NONTERM_DF64_FORCE64_EOSZ_l2(X) \
  xed_set_uimmv_imm_width_eosz(X, kXed.OSZ_NONTERM_DF64_FORCE64_EOSZ)
#define XED_LF_UIMMv_IMM_WIDTH_OSZ_NONTERM_FORCE64_EOSZ_l2(X) \
  xed_set_uimmv_imm_width_eosz(X, kXed.OSZ_NONTERM_FORCE64_EOSZ)
#define XED_LF_UIMMv_IMM_WIDTH_OSZ_NONTERM_EOSZ_l2(X) \
  xed_set_uimmv_imm_width_eosz(X, kXed.OSZ_NONTERM_EOSZ)
#define XED_LF_UIMMv_IMM_WIDTH_OSZ_NONTERM_REFINING66_CR_WIDTH_EOSZ_l2(X) \
  xed_set_uimmv_imm_width_eosz(X, kXed.OSZ_NONTERM_REFINING66_CR_WIDTH_EOSZ)
#define XED_LF_UIMMv_IMM_WIDTH_OSZ_NONTERM_DF64_EOSZ_l2(X) \
  xed_set_uimmv_imm_width_eosz(X, kXed.OSZ_NONTERM_DF64_EOSZ)
#define XED_LF_UIMMv_IMM_WIDTH_OSZ_NONTERM_DF64_IMMUNE66_LOOP64_EOSZ_l2(X) \
  xed_set_uimmv_imm_width_eosz(X, kXed.OSZ_NONTERM_DF64_IMMUNE66_LOOP64_EOSZ)
#define XED_LF_UIMMv_IMM_WIDTH_OSZ_NONTERM_IMMUNE66_EOSZ_l2(X) \
  xed_set_uimmv_imm_width_eosz(X, kXed.OSZ_NONTERM_IMMUNE66_EOSZ)
#define XED_LF_UIMMv_IMM_WIDTH_OSZ_NONTERM_CR_WIDTH_EOSZ_l2(X) \
  xed_set_uimmv_imm_width_eosz(X, kXed.OSZ_NONTERM_CR_WIDTH_EOSZ)
#define XED_LF_UIMMv_IMM_WIDTH_OSZ_NONTERM_IMMUNE_REXW_EOSZ_l2(X) \
  xed_set_uimmv_imm_width_eosz(X, kXed.OSZ_NONTERM_IMMUNE_REXW_EOSZ)

typedef int xed_int_t;
typedef unsigned int xed_uint_t;
typedef unsigned int xed_uint_t;
typedef unsigned char xed_bits_t;
typedef intptr_t xed_addr_t;
typedef unsigned char xed_bool_t;

static const u8 kXedEamode[2][3] = {
    {
        XED_MODE_REAL,    // kXedEamode[0][XED_MODE_REAL]
        XED_MODE_LEGACY,  // kXedEamode[0][XED_MODE_LEGACY]
        XED_MODE_LONG,    // kXedEamode[0][XED_MODE_LONG]
    },
    {
        XED_MODE_LEGACY,  // kXedEamode[asz][XED_MODE_REAL]
        XED_MODE_REAL,    // kXedEamode[asz][XED_MODE_LEGACY]
        XED_MODE_LEGACY,  // kXedEamode[asz][XED_MODE_LONG]
    },
};

static const u32 xed_prefix_table_bit[8] = {
    0x00000000, 0x40404040, 0x0000ffff, 0x000000f0,
    0x00000000, 0x00000000, 0x00000000, 0x000d0000,
};

static const u8 xed_has_sib_table[3][4][8] = {
    {{0, 0, 0, 0, 0, 0, 0, 0},
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
     {0, 0, 0, 0, 0, 0, 0, 0}},
};

static const u8 xed_has_disp_regular[3][4][8] = {
    {{0, 0, 0, 0, 0, 0, 2, 0},
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
     {0, 0, 0, 0, 0, 0, 0, 0}},
};

static const u8 xed_imm_bits_2d[2][256] = {
    {1, 1, 1,  1, 5, 7, 1, 1, 1,  1,  1,  1,  9,  7,  1,  0,  1, 1, 1, 1, 5, 7,
     1, 1, 1,  1, 1, 1, 5, 7, 1,  1,  1,  1,  1,  1,  5,  7,  0, 1, 1, 1, 1, 1,
     5, 7, 0,  1, 1, 1, 1, 1, 9,  7,  0,  1,  1,  1,  1,  1,  5, 7, 0, 1, 1, 1,
     1, 1, 1,  1, 1, 1, 1, 1, 1,  1,  1,  1,  1,  1,  1,  1,  1, 1, 1, 1, 1, 1,
     1, 1, 1,  1, 1, 1, 1, 1, 1,  1,  1,  1,  0,  0,  0,  0,  6, 7, 5, 5, 1, 1,
     1, 1, 1,  1, 1, 1, 1, 1, 1,  1,  1,  1,  1,  1,  1,  1,  1, 1, 5, 7, 5, 5,
     1, 1, 1,  1, 1, 1, 1, 1, 1,  1,  1,  1,  1,  1,  1,  1,  1, 1, 1, 1, 1, 1,
     8, 1, 1,  1, 1, 1, 1, 1, 1,  1,  1,  1,  1,  1,  5,  7,  1, 1, 1, 1, 1, 1,
     9, 9, 9,  9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 9, 9, 8, 1, 1, 1,
     9, 2, 11, 1, 8, 1, 1, 9, 1,  1,  1,  1,  1,  1,  9,  9,  1, 1, 1, 1, 1, 1,
     1, 1, 1,  1, 1, 1, 1, 1, 9,  9,  9,  9,  1,  1,  8,  1,  1, 1, 1, 1, 0, 1,
     0, 0, 1,  1, 3, 4, 1, 1, 1,  1,  1,  1,  1,  1},
    {1,  1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
     1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
     1,  1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
     1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 9, 9, 9, 9, 1, 1, 1, 1,
     12, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 9, 1, 0, 0,
     1,  1, 1, 1, 9, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 9, 1, 1, 1, 1, 1,
     1,  1, 9, 1, 9, 9, 9, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
};

static const u8 xed_has_modrm_2d[XED_ILD_MAP2][256] = {
    {1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 3, 1, 1, 1, 1, 0, 0, 0, 0,
     1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 3, 0, 1, 1, 1, 1, 0, 0, 3, 0,
     1, 1, 1, 1, 0, 0, 3, 0, 1, 1, 1, 1, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 1, 1, 3, 3, 3, 3, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0,
     1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     3, 0, 3, 3, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1},
    {1, 1, 1, 1, 3, 0, 0, 0, 0, 0, 3, 0, 3, 1, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1,
     0, 0, 0, 0, 0, 0, 3, 0, 3, 3, 3, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
     1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 3, 3,
     0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
};

static const u8 xed_disp_bits_2d[XED_ILD_MAP2][256] = {
    {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, 0, 4,
     4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 4, 4, 4, 4, 5, 5, 5, 5, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 1, 1, 1, 1, 4, 4, 4, 4, 3, 3, 2, 1, 4, 4, 4, 4,
     0, 4, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4},
    {4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 0, 4, 0, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4},
};

static const struct XedDenseMagnums {
  unsigned vex_prefix_recoding[4];
  xed_bits_t BRDISPz_BRDISP_WIDTH[4];
  xed_bits_t MEMDISPv_DISP_WIDTH[4];
  xed_bits_t SIMMz_IMM_WIDTH[4];
  xed_bits_t UIMMv_IMM_WIDTH[4];
  xed_bits_t ASZ_NONTERM_EASZ[2][3];
  xed_bits_t OSZ_NONTERM_CR_WIDTH_EOSZ[2][2][3];
  xed_bits_t OSZ_NONTERM_DF64_EOSZ[2][2][3];
  xed_bits_t OSZ_NONTERM_DF64_FORCE64_EOSZ[2][2][3];
  xed_bits_t OSZ_NONTERM_DF64_IMMUNE66_LOOP64_EOSZ[2][2][3];
  xed_bits_t OSZ_NONTERM_EOSZ[2][2][3];
  xed_bits_t OSZ_NONTERM_FORCE64_EOSZ[2][2][3];
  xed_bits_t OSZ_NONTERM_IGNORE66_EOSZ[2][2][3];
  xed_bits_t OSZ_NONTERM_IMMUNE66_EOSZ[2][2][3];
  xed_bits_t OSZ_NONTERM_IMMUNE_REXW_EOSZ[2][2][3];
  xed_bits_t OSZ_NONTERM_REFINING66_CR_WIDTH_EOSZ[2][2][3];
  xed_bits_t OSZ_NONTERM_REFINING66_EOSZ[2][2][3];
} kXed = {
    .vex_prefix_recoding = {0, 1, 3, 2},
    .BRDISPz_BRDISP_WIDTH = {0, 16, 32, 32},
    .MEMDISPv_DISP_WIDTH = {0, 16, 32, 64},
    .SIMMz_IMM_WIDTH = {0x00, 0x10, 0x20, 0x20},
    .UIMMv_IMM_WIDTH = {0x00, 0x10, 0x20, 0x40},
    .ASZ_NONTERM_EASZ = {{1, 2, 3}, {2, 1, 2}},
    .OSZ_NONTERM_CR_WIDTH_EOSZ = {{{2, 2, 3}, {2, 2, 3}},
                                  {{2, 2, 3}, {2, 2, 3}}},
    .OSZ_NONTERM_DF64_EOSZ = {{{1, 2, 3}, {2, 1, 1}}, {{1, 2, 3}, {2, 1, 3}}},
    .OSZ_NONTERM_DF64_FORCE64_EOSZ = {{{1, 2, 3}, {2, 1, 3}},
                                      {{1, 2, 3}, {2, 1, 3}}},
    .OSZ_NONTERM_DF64_IMMUNE66_LOOP64_EOSZ = {{{1, 2, 3}, {2, 1, 3}},
                                              {{1, 2, 3}, {2, 1, 3}}},
    .OSZ_NONTERM_EOSZ = {{{1, 2, 2}, {2, 1, 1}}, {{1, 2, 3}, {2, 1, 3}}},
    .OSZ_NONTERM_FORCE64_EOSZ = {{{1, 2, 3}, {2, 1, 3}},
                                 {{1, 2, 3}, {2, 1, 3}}},
    .OSZ_NONTERM_IGNORE66_EOSZ = {{{1, 2, 2}, {1, 2, 2}},
                                  {{1, 2, 3}, {1, 2, 3}}},
    .OSZ_NONTERM_IMMUNE66_EOSZ = {{{2, 2, 2}, {2, 2, 2}},
                                  {{2, 2, 3}, {2, 2, 3}}},
    .OSZ_NONTERM_IMMUNE_REXW_EOSZ = {{{1, 2, 2}, {2, 1, 1}},
                                     {{1, 2, 2}, {2, 1, 2}}},
    .OSZ_NONTERM_REFINING66_CR_WIDTH_EOSZ = {{{2, 2, 3}, {2, 2, 3}},
                                             {{2, 2, 3}, {2, 2, 3}}},
    .OSZ_NONTERM_REFINING66_EOSZ = {{{1, 2, 2}, {1, 2, 2}},
                                    {{1, 2, 3}, {1, 2, 3}}},
};

static void xed_too_short(struct XedDecodedInst *d) {
  d->op.out_of_bytes = 1;
  if (d->op.max_bytes >= 15) {
    d->op.error = XED_ERROR_INSTR_TOO_LONG;
  } else {
    d->op.error = XED_ERROR_BUFFER_TOO_SHORT;
  }
}

static void xed_bad_map(struct XedDecodedInst *d) {
  d->op.map = XED_ILD_MAP_INVALID;
  d->op.error = XED_ERROR_BAD_MAP;
}

static xed_bits_t xed_get_prefix_table_bit(xed_bits_t a) {
  return (xed_prefix_table_bit[a >> 5] >> (a & 0x1F)) & 1;
}

static size_t xed_bits2bytes(unsigned bits) {
  return bits >> 3;
}

static size_t xed_bytes2bits(unsigned bytes) {
  return bytes << 3;
}

static void XED_LF_SIMM8_IMM_WIDTH_CONST_l2(struct XedDecodedInst *x) {
  x->op.imm_width = 8;
  x->op.imm_signed = 1;
}

static void XED_LF_UIMM16_IMM_WIDTH_CONST_l2(struct XedDecodedInst *x) {
  x->op.imm_width = 16;
}

static void xed_set_simmz_imm_width_eosz(struct XedDecodedInst *x,
                                         const xed_bits_t eosz[2][2][3]) {
  x->op.imm_width = kXed.SIMMz_IMM_WIDTH[eosz[Rexw(x->op.rde)][Osz(x->op.rde)]
                                             [Mode(x->op.rde)]];
  x->op.imm_signed = 1;
}

static void xed_set_uimmv_imm_width_eosz(struct XedDecodedInst *x,
                                         const xed_bits_t eosz[2][2][3]) {
  x->op.imm_width = kXed.UIMMv_IMM_WIDTH[eosz[Rexw(x->op.rde)][Osz(x->op.rde)]
                                             [Mode(x->op.rde)]];
}

static void XED_LF_UIMM8_IMM_WIDTH_CONST_l2(struct XedDecodedInst *x) {
  x->op.imm_width = 8;
}

static void XED_LF_0_IMM_WIDTH_CONST_l2(struct XedDecodedInst *x) {
  x->op.imm_width = 0;
}

static void XED_LF_RESOLVE_BYREG_IMM_WIDTH_map0x0_op0xc7_l1(
    struct XedDecodedInst *x) {
  switch (ModrmReg(x->op.rde)) {
    case 0:
      XED_LF_SIMMz_IMM_WIDTH_OSZ_NONTERM_EOSZ_l2(x);
      break;
    case 7:
      XED_LF_0_IMM_WIDTH_CONST_l2(x);
      break;
    default:
      break;
  }
}

static void XED_LF_RESOLVE_BYREG_IMM_WIDTH_map0x0_op0xf6_l1(
    struct XedDecodedInst *x) {
  if (ModrmReg(x->op.rde) <= 1) {
    XED_LF_SIMM8_IMM_WIDTH_CONST_l2(x);
  } else if (2 <= ModrmReg(x->op.rde) && ModrmReg(x->op.rde) <= 7) {
    XED_LF_0_IMM_WIDTH_CONST_l2(x);
  }
}

static void XED_LF_RESOLVE_BYREG_IMM_WIDTH_map0x0_op0xf7_l1(
    struct XedDecodedInst *x) {
  if (ModrmReg(x->op.rde) <= 1) {
    XED_LF_SIMMz_IMM_WIDTH_OSZ_NONTERM_EOSZ_l2(x);
  } else if (2 <= ModrmReg(x->op.rde) && ModrmReg(x->op.rde) <= 7) {
    XED_LF_0_IMM_WIDTH_CONST_l2(x);
  }
}

static void xed_ild_hasimm_map0x0F_op0x78_l1(struct XedDecodedInst *x) {
  if (Osz(x->op.rde) || x->op.rep == 2) {
    x->op.imm_width = xed_bytes2bits(1);
  }
}

static void xed_ild_hasimm_map0x0_op0xc8_l1(struct XedDecodedInst *x) {
  x->op.imm_width = xed_bytes2bits(2);
}

static void xed_set_imm_bytes(struct XedDecodedInst *d) {
  if (!d->op.imm_width && d->op.map < XED_ILD_MAP2) {
    switch (xed_imm_bits_2d[d->op.map][d->op.opcode]) {
      case XED_I_LF_0_IMM_WIDTH_CONST_l2:
        XED_LF_0_IMM_WIDTH_CONST_l2(d);
        break;
      case XED_I_LF_RESOLVE_BYREG_IMM_WIDTH_map0x0_op0xc7_l1:
        XED_LF_RESOLVE_BYREG_IMM_WIDTH_map0x0_op0xc7_l1(d);
        break;
      case XED_I_LF_RESOLVE_BYREG_IMM_WIDTH_map0x0_op0xf6_l1:
        XED_LF_RESOLVE_BYREG_IMM_WIDTH_map0x0_op0xf6_l1(d);
        break;
      case XED_I_LF_RESOLVE_BYREG_IMM_WIDTH_map0x0_op0xf7_l1:
        XED_LF_RESOLVE_BYREG_IMM_WIDTH_map0x0_op0xf7_l1(d);
        break;
      case XED_I_LF_SIMM8_IMM_WIDTH_CONST_l2:
        XED_LF_SIMM8_IMM_WIDTH_CONST_l2(d);
        break;
      case XED_I_LF_SIMMz_IMM_WIDTH_OSZ_NONTERM_DF64_EOSZ_l2:
        XED_LF_SIMMz_IMM_WIDTH_OSZ_NONTERM_DF64_EOSZ_l2(d);
        break;
      case XED_I_LF_SIMMz_IMM_WIDTH_OSZ_NONTERM_EOSZ_l2:
        XED_LF_SIMMz_IMM_WIDTH_OSZ_NONTERM_EOSZ_l2(d);
        break;
      case XED_I_LF_UIMM16_IMM_WIDTH_CONST_l2:
        XED_LF_UIMM16_IMM_WIDTH_CONST_l2(d);
        break;
      case XED_I_LF_UIMM8_IMM_WIDTH_CONST_l2:
        XED_LF_UIMM8_IMM_WIDTH_CONST_l2(d);
        break;
      case XED_I_LF_UIMMv_IMM_WIDTH_OSZ_NONTERM_EOSZ_l2:
        XED_LF_UIMMv_IMM_WIDTH_OSZ_NONTERM_EOSZ_l2(d);
        break;
      case xed_i_ild_hasimm_map0x0_op0xc8_l1:
        xed_ild_hasimm_map0x0_op0xc8_l1(d);
        break;
      case xed_i_ild_hasimm_map0x0F_op0x78_l1:
        xed_ild_hasimm_map0x0F_op0x78_l1(d);
        break;
      default:
        d->op.error = XED_ERROR_GENERAL_ERROR;
        return;
    }
  }
}

static void xed_prefix_scanner(struct XedDecodedInst *d) {
  u32 rde;
  xed_bits_t b, max_bytes, length, islong;
  xed_bits_t asz, osz, rex, rexw, rexr, rexx, rexb;
  rex = 0;
  rde = d->op.rde;
  length = d->length;
  max_bytes = d->op.max_bytes;
  islong = Mode(rde) == XED_MODE_LONG;
  while (length < max_bytes) {
    b = d->bytes[length];
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
        if (!islong) {
          rde &= 037770777777;
          rde |= 000002000000;
        }
        rex = 0;
        break;
      case 0x3E:  // ds
        if (!islong) {
          rde &= 037770777777;
          rde |= 000004000000;
        }
        rex = 0;
        break;
      case 0x26:  // es
        if (!islong) {
          rde &= 037770777777;
          rde |= 000001000000;
        }
        rex = 0;
        break;
      case 0x36:  // ss
        if (!islong) {
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
        rde &= 017777777777;
        rde |= 020000000000;
        rex = 0;
        break;
      case 0xF2:  // rep
      case 0xF3:
        d->op.rep = b & 3;
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
  d->length = length;
  if (rex) {
    rexw = (rex >> 3) & 1;
    rexr = (rex >> 2) & 1;
    rexx = (rex >> 1) & 1;
    rexb = rex & 1;
    rex = 1;
    rde |= rexx << 17 | rex << 16 | rexb << 15 | rex << 11 | rexb << 10 |
           rexw << 6 | rex << 4 | rexr << 3;
  }
  d->op.rde = rde;
  if (length >= max_bytes) {
    xed_too_short(d);
  }
}

static void xed_get_next_as_opcode(struct XedDecodedInst *d) {
  xed_bits_t b, length;
  length = d->length;
  if (length < d->op.max_bytes) {
    b = d->bytes[length];
    d->op.opcode = b;
    d->length++;
  } else {
    xed_too_short(d);
  }
}

static void xed_opcode_scanner(struct XedDecodedInst *d) {
  xed_bits_t b, length;
  length = d->length;
  if ((b = d->bytes[length]) != 0x0F) {
    d->op.map = XED_ILD_MAP0;
    d->op.opcode = b;
    d->op.pos_opcode = length;
    d->length++;
  } else {
    length++;
    d->op.pos_opcode = length;
    if (length < d->op.max_bytes) {
      switch ((b = d->bytes[length])) {
        case 0x38:
          d->length = ++length;
          d->op.map = XED_ILD_MAP2;
          xed_get_next_as_opcode(d);
          return;
        case 0x3A:
          d->length = ++length;
          d->op.map = XED_ILD_MAP3;
          d->op.imm_width = xed_bytes2bits(1);
          xed_get_next_as_opcode(d);
          return;
        case 0x3B:
          xed_bad_map(d);
          d->length = ++length;
          xed_get_next_as_opcode(d);
          return;
        case 0x39:
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
          xed_bad_map(d);
          d->length = ++length;
          xed_get_next_as_opcode(d);
          return;
        case 0x0F:
          xed_bad_map(d);
          d->length = ++length;
          break;
        default:
          d->op.opcode = b;
          d->length = ++length;
          d->op.map = XED_ILD_MAP1;
          break;
      }
    } else {
      xed_too_short(d);
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

static void xed_set_has_modrm(struct XedDecodedInst *d) {
  if (d->op.map < ARRAYLEN(xed_has_modrm_2d)) {
    d->op.has_modrm = xed_has_modrm_2d[d->op.map][d->op.opcode];
  } else {
    d->op.has_modrm = 1;
  }
}

static void xed_modrm_scanner(struct XedDecodedInst *d) {
  u8 b;
  xed_bits_t rm, reg, mod, eamode, length, has_modrm;
  xed_set_has_modrm(d);
  has_modrm = d->op.has_modrm;
  if (has_modrm) {
    length = d->length;
    if (length < d->op.max_bytes) {
      b = d->bytes[length];
      d->length++;
      rm = b & 0007;
      reg = (b & 0070) >> 3;
      mod = (b & 0300) >> 6;
      d->op.rde &= ~1;
      d->op.rde |= mod << 22 | rm << 7 | reg;
      if (has_modrm != XED_ILD_HASMODRM_IGNORE_MOD) {
        eamode = kXedEamode[Asz(d->op.rde)][Mode(d->op.rde)];
        d->op.disp_width =
            xed_bytes2bits(xed_has_disp_regular[eamode][mod][rm]);
        d->op.has_sib = xed_has_sib_table[eamode][mod][rm];
      }
    } else {
      xed_too_short(d);
    }
  }
}

static void xed_sib_scanner(struct XedDecodedInst *d) {
  u8 b;
  xed_bits_t length;
  if (d->op.has_sib) {
    length = d->length;
    if (length < d->op.max_bytes) {
      b = d->bytes[length];
      d->op.sib = b;
      d->length++;
      if (xed_sib_base(b) == 5) {
        if (!ModrmMod(d->op.rde)) {
          d->op.disp_width = xed_bytes2bits(4);
        }
      }
    } else {
      xed_too_short(d);
    }
  }
}

static void XED_LF_EMPTY_DISP_CONST_l2(struct XedDecodedInst *x) {
  /* This function does nothing for map-opcodes whose
   * disp_bytes value is set earlier in xed-ild.c
   * (regular displacement resolution by modrm/sib)*/
  (void)x;
}

static void XED_LF_BRDISP8_BRDISP_WIDTH_CONST_l2(struct XedDecodedInst *x) {
  x->op.disp_width = 0x8;
}

static void XED_LF_BRDISPz_BRDISP_WIDTH_OSZ_NONTERM_EOSZ_l2(
    struct XedDecodedInst *x) {
  x->op.disp_width = kXed.BRDISPz_BRDISP_WIDTH[kXed.OSZ_NONTERM_EOSZ[Rexw(
      x->op.rde)][Osz(x->op.rde)][Mode(x->op.rde)]];
  x->op.disp_unsigned = 1;
}

static void XED_LF_RESOLVE_BYREG_DISP_map0x0_op0xc7_l1(
    struct XedDecodedInst *x) {
  switch (ModrmReg(x->op.rde)) {
    case 0:
      XED_LF_EMPTY_DISP_CONST_l2(x);
      break;
    case 7:
      XED_LF_BRDISPz_BRDISP_WIDTH_OSZ_NONTERM_EOSZ_l2(x);
      break;
    default:
      break;
  }
}

static void XED_LF_MEMDISPv_DISP_WIDTH_ASZ_NONTERM_EASZ_l2(
    struct XedDecodedInst *x) {
  x->op.disp_width =
      kXed.MEMDISPv_DISP_WIDTH[kXed.ASZ_NONTERM_EASZ[Asz(x->op.rde)]
                                                    [Mode(x->op.rde)]];
  x->op.disp_unsigned = 1;
}

static void XED_LF_BRDISP32_BRDISP_WIDTH_CONST_l2(struct XedDecodedInst *x) {
  x->op.disp_width = 0x20;
}

static void XED_LF_DISP_BUCKET_0_l1(struct XedDecodedInst *x) {
  if (Mode(x->op.rde) <= XED_MODE_LEGACY) {
    XED_LF_BRDISPz_BRDISP_WIDTH_OSZ_NONTERM_EOSZ_l2(x);
    x->op.disp_unsigned = 0;
  } else if (Mode(x->op.rde) == XED_MODE_LONG) {
    XED_LF_BRDISP32_BRDISP_WIDTH_CONST_l2(x);
  }
}

static void xed_disp_scanner(struct XedDecodedInst *d) {
  xed_bits_t length, disp_bytes;
  length = d->length;
  if (d->op.map < XED_ILD_MAP2) {
    switch (xed_disp_bits_2d[d->op.map][d->op.opcode]) {
      case XED_I_LF_BRDISP8_BRDISP_WIDTH_CONST_l2:
        XED_LF_BRDISP8_BRDISP_WIDTH_CONST_l2(d);
        break;
      case XED_I_LF_BRDISPz_BRDISP_WIDTH_OSZ_NONTERM_EOSZ_l2:
        XED_LF_BRDISPz_BRDISP_WIDTH_OSZ_NONTERM_EOSZ_l2(d);
        break;
      case XED_I_LF_DISP_BUCKET_0_l1:
        XED_LF_DISP_BUCKET_0_l1(d);
        break;
      case XED_I_LF_EMPTY_DISP_CONST_l2:
        XED_LF_EMPTY_DISP_CONST_l2(d);
        break;
      case XED_I_LF_MEMDISPv_DISP_WIDTH_ASZ_NONTERM_EASZ_l2:
        XED_LF_MEMDISPv_DISP_WIDTH_ASZ_NONTERM_EASZ_l2(d);
        break;
      case XED_I_LF_RESOLVE_BYREG_DISP_map0x0_op0xc7_l1:
        XED_LF_RESOLVE_BYREG_DISP_map0x0_op0xc7_l1(d);
        break;
      default:
        d->op.error = XED_ERROR_GENERAL_ERROR;
        return;
    }
  }
  disp_bytes = xed_bits2bytes(d->op.disp_width);
  if (disp_bytes) {
    if (length + disp_bytes <= d->op.max_bytes) {
      d->op.disp =
          xed_read_number(d->bytes + length, disp_bytes, !d->op.disp_unsigned);
      d->length = length + disp_bytes;
    } else {
      xed_too_short(d);
    }
  }
}

static void xed_imm_scanner(struct XedDecodedInst *d) {
  u8 *p;
  xed_bits_t i, length, imm_bytes;
  p = d->bytes;
  xed_set_imm_bytes(d);
  i = length = d->length;
  imm_bytes = xed_bits2bytes(d->op.imm_width);
  if (imm_bytes) {
    if (length + imm_bytes <= d->op.max_bytes) {
      length += imm_bytes;
      d->length = length;
      d->op.uimm0 = xed_read_number(p + i, imm_bytes, d->op.imm_signed);
    } else {
      xed_too_short(d);
    }
  }
}

static void xed_decode_instruction_length(struct XedDecodedInst *ild) {
  xed_prefix_scanner(ild);
  if (!ild->op.out_of_bytes) {
    if (!ild->op.error) {
      xed_opcode_scanner(ild);
    }
    xed_modrm_scanner(ild);
    xed_sib_scanner(ild);
    xed_disp_scanner(ild);
    xed_imm_scanner(ild);
  }
}

/**
 * Clears instruction decoder state.
 */
struct XedDecodedInst *InitializeInstruction(struct XedDecodedInst *x,
                                             int mmode) {
  int mode, real = 0;
  memset(x, 0, sizeof(*x));
  switch (mmode) {
    default:
    case XED_MACHINE_MODE_LONG_64:
      mode = XED_MODE_LONG;
      break;
    case XED_MACHINE_MODE_LEGACY_32:
    case XED_MACHINE_MODE_LONG_COMPAT_32:
      mode = XED_MODE_LEGACY;
      break;
    case XED_MACHINE_MODE_REAL:
      real = 1;
      mode = XED_MODE_REAL;
      break;
    case XED_MACHINE_MODE_UNREAL:
      real = 1;
      mode = XED_MODE_LEGACY;
      break;
    case XED_MACHINE_MODE_LEGACY_16:
    case XED_MACHINE_MODE_LONG_COMPAT_16:
      mode = XED_MODE_REAL;
      break;
  }
  x->op.realmode = real;
  x->op.rde = mode << 26;
  return x;
}

/**
 * Decodes machine instruction length.
 *
 * This function also decodes other useful attributes, such as the
 * offsets of displacement, immediates, etc. It works for all ISAs from
 * 1977 to 2020.
 *
 * @note binary footprint increases ~4kb if this is used
 * @see biggest code in gdb/clang/tensorflow binaries
 */
enum XedError DecodeInstruction(struct XedDecodedInst *x, const void *itext,
                                size_t bytes) {
  u8 kWordLog2[2][2][2] = {{{2, 3}, {1, 3}}};
  memcpy(x->bytes, itext, MIN(15, bytes));
  x->op.max_bytes = MIN(15, bytes);
  xed_decode_instruction_length(x);
  x->op.rde |= (x->op.opcode & 7) << 12;
  x->op.rde ^= x->op.realmode << 5;
  x->op.rde |= kWordLog2[~x->op.opcode & 1][Osz(x->op.rde)][Rexw(x->op.rde)]
               << 28;
  x->op.rde |= kXedEamode[Asz(x->op.rde)][Mode(x->op.rde)] << 24;
  if (!x->op.out_of_bytes) {
    if (x->op.map != XED_ILD_MAP_INVALID) {
      return (enum XedError)x->op.error;
    } else {
      return XED_ERROR_GENERAL_ERROR;
    }
  } else {
    return XED_ERROR_BUFFER_TOO_SHORT;
  }
}
