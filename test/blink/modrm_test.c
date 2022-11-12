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
#include "blink/assert.h"
#include "blink/endian.h"
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/x86.h"
#include "test/test.h"

#define ILD(XEDD, OP, MODE)                                \
  do {                                                     \
    InitializeInstruction(XEDD, MODE);                     \
    ASSERT_EQ(0, DecodeInstruction(XEDD, OP, sizeof(OP))); \
  } while (0)

struct System *s;
struct Machine *m;

void SetUp(void) {
  unassert((s = NewSystem()));
  unassert((m = NewMachine(s, 0)));
  m->xedd = (struct XedDecodedInst *)calloc(1, sizeof(*m->xedd));
}

void TearDown(void) {
  free(m->xedd);
  FreeMachine(m);
  FreeSystem(s);
}

TEST(modrm, testAddressSizeOverride_isNotPresent_keepsWholeExpression) {
  uint8_t op[] = {0x8d, 0x04, 0x03}; /* lea (%rbx,%rax,1),%eax */
  Write64(m->bx, 0x2);
  Write64(m->ax, 0xffffffff);
  ILD(m->xedd, op, XED_MACHINE_MODE_LONG_64);
  EXPECT_EQ(0x100000001, ComputeAddress(m, m->xedd->op.rde));
}

TEST(modrm, testAddressSizeOverride_isPresent_modulesWholeExpression) {
  uint8_t op[] = {0x67, 0x8d, 0x04, 0x03}; /* lea (%ebx,%eax,1),%eax */
  Write64(m->bx, 0x2);
  Write64(m->ax, 0xffffffff);
  ILD(m->xedd, op, XED_MACHINE_MODE_LONG_64);
  EXPECT_EQ(0x000000001, ComputeAddress(m, m->xedd->op.rde));
}

TEST(modrm, testOverflow_doesntTriggerTooling) {
  uint8_t op[] = {0x8d, 0x04, 0x03}; /* lea (%rbx,%rax,1),%eax */
  Write64(m->bx, 0x0000000000000001);
  Write64(m->ax, 0x7fffffffffffffff);
  ILD(m->xedd, op, XED_MACHINE_MODE_LONG_64);
  EXPECT_EQ(0x8000000000000000ull,
            (uint64_t)ComputeAddress(m, m->xedd->op.rde));
}

TEST(modrm, testPuttingOnTheRiz) {
  static uint8_t ops[][15] = {
      {0x8d, 0b00110100, 0b00100110},        // lea (%rsi),%esi
      {0x67, 0x8d, 0b00110100, 0b11100110},  // lea (%esi,%eiz,8),%esi
      {103, 141, 180, 229, 55, 19, 3, 0},    // lea 0x31337(%ebp,%eiz,8),%esi
      {141, 52, 229, 55, 19, 3, 0},          // lea 0x31337,%esi
  };
  Write64(m->si, 0x100000001);
  Write64(m->bp, 0x200000002);
  ILD(m->xedd, ops[0], XED_MACHINE_MODE_LONG_64);
  EXPECT_EQ(0x100000001, ComputeAddress(m, m->xedd->op.rde));
  ILD(m->xedd, ops[1], XED_MACHINE_MODE_LONG_64);
  EXPECT_EQ(0x000000001, ComputeAddress(m, m->xedd->op.rde));
  ILD(m->xedd, ops[2], XED_MACHINE_MODE_LONG_64);
  EXPECT_EQ(0x31339, ComputeAddress(m, m->xedd->op.rde));
  ILD(m->xedd, ops[3], XED_MACHINE_MODE_LONG_64);
  EXPECT_EQ(0x31337, ComputeAddress(m, m->xedd->op.rde));
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
  ILD(m->xedd, op, XED_MACHINE_MODE_LONG_64);
  EXPECT_TRUE(Rexw(m->xedd->op.rde));
  EXPECT_TRUE(Rexr(m->xedd->op.rde));
  EXPECT_FALSE(Rexb(m->xedd->op.rde));
  EXPECT_EQ(0b000, ModrmReg(m->xedd->op.rde));
  EXPECT_EQ(0b100, ModrmRm(m->xedd->op.rde));
  EXPECT_EQ(0x123 * 4, (uint64_t)ComputeAddress(m, m->xedd->op.rde));
}
