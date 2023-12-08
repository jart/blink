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
#include "blink/assert.h"
#include "blink/endian.h"
#include "blink/machine.h"
#include "blink/map.h"
#include "blink/modrm.h"
#include "blink/x86.h"
#include "test/test.h"

struct System *s;
struct Machine *m;
struct XedDecodedInst xedd;

void SetUp(void) {
  InitMap();
  unassert((s = NewSystem(XED_MACHINE_MODE_LONG)));
  unassert((m = NewMachine(s, 0)));
  m->xedd = &xedd;
  memset(&xedd, 0, sizeof(xedd));
}

void TearDown(void) {
  FreeMachine(m);
}

TEST(modrm, testAddressSizeOverride_isNotPresent_keepsWholeExpression) {
  uint8_t op[] = {0x8d, 0x04, 0x03}; /* lea (%rbx,%rax,1),%eax */
  Write64(m->bx, 0x2);
  Write64(m->ax, 0xffffffff);
  ASSERT_EQ(0, DecodeInstruction(m->xedd, op, sizeof(op), XED_MODE_LONG));
  EXPECT_EQ(0x100000001, ComputeAddress(m, m->xedd->op.rde, m->xedd->op.disp,
                                        m->xedd->op.uimm0));
}

TEST(modrm, testAddressSizeOverride_isPresent_modulesWholeExpression) {
  uint8_t op[] = {0x67, 0x8d, 0x04, 0x03}; /* lea (%ebx,%eax,1),%eax */
  Write64(m->bx, 0x2);
  Write64(m->ax, 0xffffffff);
  ASSERT_EQ(0, DecodeInstruction(m->xedd, op, sizeof(op), XED_MODE_LONG));
  EXPECT_EQ(0x000000001, ComputeAddress(m, m->xedd->op.rde, m->xedd->op.disp,
                                        m->xedd->op.uimm0));
}

TEST(modrm, testOverflow_doesntTriggerTooling) {
  uint8_t op[] = {0x8d, 0x04, 0x03}; /* lea (%rbx,%rax,1),%eax */
  Write64(m->bx, 0x0000000000000001);
  Write64(m->ax, 0x7fffffffffffffff);
  ASSERT_EQ(0, DecodeInstruction(m->xedd, op, sizeof(op), XED_MODE_LONG));
  EXPECT_EQ(0x8000000000000000ull,
            (uint64_t)ComputeAddress(m, m->xedd->op.rde, m->xedd->op.disp,
                                     m->xedd->op.uimm0));
}

TEST(modrm, testPuttingOnTheRiz) {
  static uint8_t op[][15] = {
      {0x8d, 0b00110100, 0b00100110},        // lea (%rsi),%esi
      {0x67, 0x8d, 0b00110100, 0b11100110},  // lea (%esi,%eiz,8),%esi
      {103, 141, 180, 229, 55, 19, 3, 0},    // lea 0x31337(%ebp,%eiz,8),%esi
      {141, 52, 229, 55, 19, 3, 0},          // lea 0x31337,%esi
  };
  Write64(m->si, 0x100000001);
  Write64(m->bp, 0x200000002);
  ASSERT_EQ(0, DecodeInstruction(m->xedd, op[0], sizeof(op[0]), XED_MODE_LONG));
  EXPECT_EQ(0x100000001, ComputeAddress(m, m->xedd->op.rde, m->xedd->op.disp,
                                        m->xedd->op.uimm0));
  ASSERT_EQ(0, DecodeInstruction(m->xedd, op[1], sizeof(op[1]), XED_MODE_LONG));
  EXPECT_EQ(0x000000001, ComputeAddress(m, m->xedd->op.rde, m->xedd->op.disp,
                                        m->xedd->op.uimm0));
  ASSERT_EQ(0, DecodeInstruction(m->xedd, op[2], sizeof(op[2]), XED_MODE_LONG));
  EXPECT_EQ(0x31339, ComputeAddress(m, m->xedd->op.rde, m->xedd->op.disp,
                                    m->xedd->op.uimm0));
  ASSERT_EQ(0, DecodeInstruction(m->xedd, op[3], sizeof(op[3]), XED_MODE_LONG));
  EXPECT_EQ(0x31337, ComputeAddress(m, m->xedd->op.rde, m->xedd->op.disp,
                                    m->xedd->op.uimm0));
}

TEST(modrm, testSibIndexOnly) {
  // lea 0x0(,%rcx,4),%r8
  //       mod                  = 0b00  (0)
  //       reg                  = 0b000 (0)
  //       rm                   = 0b100 (4)
  //       scale                = 0b10  (2)
  //       index                = 0b001 (1)
  //       base                 = 0b101 (5)
  uint8_t op[] = {0x4c, 0x8d, 0x04, 0x8d, 0, 0, 0, 0};
  Write64(m->bp, 0x123);
  Write64(m->cx, 0x123);
  ASSERT_EQ(0, DecodeInstruction(m->xedd, op, sizeof(op), XED_MODE_LONG));
  EXPECT_TRUE(Rexw(m->xedd->op.rde));
  EXPECT_TRUE(Rexr(m->xedd->op.rde));
  EXPECT_FALSE(Rexb(m->xedd->op.rde));
  EXPECT_EQ(0b000, ModrmReg(m->xedd->op.rde));
  EXPECT_EQ(0b100, ModrmRm(m->xedd->op.rde));
  EXPECT_EQ(0x123 * 4,
            (uint64_t)ComputeAddress(m, m->xedd->op.rde, m->xedd->op.disp,
                                     m->xedd->op.uimm0));
}
