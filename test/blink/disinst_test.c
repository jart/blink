/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2020 Justine Alexandra Roberts Tunney                              │
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
#include "blink/dis.h"
#include "blink/high.h"
#include "blink/modrm.h"
#include "test/test.h"

#define ILD(OP, MODE) \
  ASSERT_EQ(0, DecodeInstruction(d->xedd, OP, sizeof(OP), MODE))

char b1[64];
char b2[64];
struct Dis d[1];

void SetUp(void) {
  d->notab = true;
  g_high.enabled = false;
}

void TearDown(void) {
}

TEST(DisInst, testInt3) {
  u8 op[] = {0xcc};
  ILD(op, XED_MODE_LONG);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("int3 ", b1);
}

TEST(DisInst, testImmMem_needsSuffix) {
  u8 op[] = {0x80, 0x3c, 0x07, 0x00};
  ILD(op, XED_MODE_LONG);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("cmpb $0,(%rdi,%rax)", b1);
}

TEST(DisInst, testImmReg_doesntNeedSuffix) {
  u8 op[] = {0xb8, 0x08, 0x70, 0x40, 0x00};
  ILD(op, XED_MODE_LONG);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("mov $0x407008,%eax", b1);
}

TEST(DisInst, testPuttingOnTheRiz) {
  static u8 ops[][15] = {
      {0x8d, 0b00110100, 0b00100110},
      {0x67, 0x8d, 0b00110100, 0b11100110},
      {0x67, 0x8d, 0b10110100, 0b11100101, 0, 0, 0, 0},
      {0x8d, 0b00110100, 0b11100101, 0x37, 0x13, 0x03, 0},
      {0x8d, 0b10110100, 0b11100101, 0, 0, 0, 0},
  };
  ILD(ops[0], XED_MODE_LONG);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("lea (%rsi),%esi", b1);
  ILD(ops[1], XED_MODE_LONG);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("lea (%esi,%eiz,8),%esi", b1);
  ILD(ops[2], XED_MODE_LONG);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("lea 0(%ebp,%eiz,8),%esi", b1);
  ILD(ops[3], XED_MODE_LONG);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("lea 0x31337,%esi", b1);
  ILD(ops[4], XED_MODE_LONG);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("lea 0(%rbp,%riz,8),%esi", b1);
}

TEST(DisInst, testSibIndexOnly) {
  u8 op[] = {76, 141, 4, 141, 0, 0, 0, 0};
  ILD(op, XED_MODE_LONG);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("lea 0(,%rcx,4),%r8", b1);
}

TEST(DisInst, testRealMode) {
  u8 op[] = {0x89, 0xe5};
  ILD(op, XED_MODE_REAL);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("mov %sp,%bp", b1);
}

TEST(DisInst, testNop) {
  u8 op[] = {0x66, 0x2e, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00};
  ILD(op, XED_MODE_LONG);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("nopw %cs:0(%rax,%rax)", b1);
}

TEST(DisInst, testJne) {
  u8 op[] = {0x75, (u8)-24};
  ILD(op, XED_MODE_LONG);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("jne -24", b1);
}

TEST(DisInst, testPush) {
  u8 op[] = {0x41, 0x5c};
  ILD(op, XED_MODE_LONG);
  EXPECT_EQ(4, ModrmSrm(d->xedd->op.rde));
  EXPECT_EQ(1, Rexb(d->xedd->op.rde));
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("pop %r12", b1);
}

TEST(DisInst, testMovb) {
  u8 op[] = {0x8a, 0x1e, 0x0c, 0x32};
  ILD(op, XED_MODE_LONG);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("mov (%rsi),%bl", b1);
  ILD(op, XED_MODE_REAL);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("mov 0x320c,%bl", b1);
}

TEST(DisInst, testLes) {
  u8 op[] = {0xc4, 0x3e, 0x16, 0x32};
  ILD(op, XED_MODE_REAL);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("les 0x3216,%di", b1);
}

TEST(DisInst, testStosbLong) {
  u8 op[] = {0xAA};
  ILD(op, XED_MODE_LONG);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("stosb %al,(%rdi)", b1);
}

TEST(DisInst, testStosbReal) {
  u8 op[] = {0xAA};
  ILD(op, XED_MODE_REAL);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("stosb %al,(%di)", b1);
}

TEST(DisInst, testStosbLegacy) {
  u8 op[] = {0xAA};
  ILD(op, XED_MODE_LEGACY);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("stosb %al,(%edi)", b1);
}

TEST(DisInst, testStosbLongAsz) {
  u8 op[] = {0x67, 0xAA};
  ILD(op, XED_MODE_LONG);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("stosb %al,(%edi)", b1);
}

TEST(DisInst, testAddLong) {
  u8 op[] = {0x01, 0xff};
  ILD(op, XED_MODE_LONG);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("add %edi,%edi", b1);
}

TEST(DisInst, testAddLegacy) {
  u8 op[] = {0x01, 0xff};
  ILD(op, XED_MODE_LEGACY);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("add %edi,%edi", b1);
}

TEST(DisInst, testAddReal) {
  u8 op[] = {0x01, 0xff};
  ILD(op, XED_MODE_REAL);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("add %di,%di", b1);
}

TEST(DisInst, testAddLongOsz) {
  u8 op[] = {0x66, 0x01, 0xff};
  ILD(op, XED_MODE_LONG);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("add %di,%di", b1);
}

TEST(DisInst, testAddLegacyOsz) {
  u8 op[] = {0x66, 0x01, 0xff};
  ILD(op, XED_MODE_LEGACY);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("add %di,%di", b1);
}

TEST(DisInst, testAddRealOsz) {
  u8 op[] = {0x66, 0x01, 0xff};
  ILD(op, XED_MODE_REAL);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("add %edi,%edi", b1);
}

TEST(DisInst, testFxam) {
  u8 op[] = {0xd9, 0xe5};
  ILD(op, XED_MODE_LONG);
  ASSERT_EQ(4, ModrmReg(d->xedd->op.rde));
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("fxam ", b1);
}

TEST(DisInst, testOrImmCode16gcc) {
  u8 op[] = {0x67, 0x81, 0x4c, 0x24, 0x0c, 0x00, 0x0c};
  ILD(op, XED_MODE_REAL);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("orw $0xc00,12(%esp)", b1);
}

TEST(DisInst, testPause) {
  u8 op[] = {0xf3, 0x90};
  ILD(op, XED_MODE_LONG);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("pause ", b1);
}

TEST(DisInst, testJmpEw) {
  u8 op[] = {0xff, 0xe0};
  ILD(op, XED_MODE_REAL);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("jmp %ax", b1);
}

TEST(DisInst, testJmpEv16) {
  u8 op[] = {0x66, 0xff, 0xe0};
  ILD(op, XED_MODE_REAL);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("jmp %eax", b1);
}

TEST(DisInst, testJmpEv32) {
  u8 op[] = {0xff, 0xe0};
  ILD(op, XED_MODE_LEGACY);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("jmp %eax", b1);
}

TEST(DisInst, testJmpEq) {
  u8 op[] = {0x66, 0xff, 0xe0};
  ILD(op, XED_MODE_LONG);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("jmp %rax", b1);
}

TEST(DisInst, testMovswSs) {
  u8 op[] = {0x36, 0xA5};
  ILD(op, XED_MODE_REAL);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("movs %ss:(%si),(%di)", b1);
}

TEST(DisInst, testRealModrm_sibOverlap_showsNoDisplacement) {
  u8 op[] = {0x8b, 0b00100101};
  ILD(op, XED_MODE_REAL);
  DisInst(d, b1, DisSpec(d->xedd, b2));
  EXPECT_STREQ("mov (%di),%sp", b1);
}
