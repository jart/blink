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
#include <math.h>

#include "blink/endian.h"
#include "blink/fpu.h"
#include "blink/machine.h"
#include "blink/memory.h"
#include "test/test.h"

uint8_t kPi80[] = {
    0xd9, 0xe8,                          // fld1
    0xb8, 0x0a, 0x00, 0x00, 0x00,        // mov    $0xa,%eax
    0x31, 0xd2,                          // xor    %edx,%edx
    0xd9, 0xee,                          // fldz
    0x48, 0x98,                          // cltq
    0x48, 0x39, 0xc2,                    // cmp    %rax,%rdx
    0xd9, 0x05, 0x1a, 0x00, 0x00, 0x00,  // flds   0x1a(%rip)
    0x7d, 0x13,                          // jge    2b <pi80+0x2b>
    0xde, 0xc1,                          // faddp
    0x48, 0xff, 0xc2,                    // inc    %rdx
    0xd9, 0xfa,                          // fsqrt
    0xd9, 0x05, 0x0f, 0x00, 0x00, 0x00,  // flds   15(%rip)
    0xd8, 0xc9,                          // fmul   %st(1),%st
    0xde, 0xca,                          // fmulp  %st,%st(2)
    0xeb, 0xe2,                          // jmp    d <pi80+0xd>
    0xdd, 0xd9,                          // fstp   %st(1)
    0xde, 0xf1,                          // fdivp
    0xf4,                                // hlt
    0x00, 0x00, 0x00, 0x40,              // .float 2.0
    0x00, 0x00, 0x00, 0x3f,              // .float 0.5
};

uint8_t kTenthprime[] = {
    0x31, 0xd2,                    // xor    %edx,%edx
    0x45, 0x31, 0xc0,              // xor    %r8d,%r8d
    0x31, 0xc9,                    // xor    %ecx,%ecx
    0xbe, 0x03, 0x00, 0x00, 0x00,  // mov    $0x3,%esi
    0x41, 0xff, 0xc0,              // inc    %r8d
    0x44, 0x89, 0xc0,              // mov    %r8d,%eax
    0x83, 0xf9, 0x0a,              // cmp    $0xa,%ecx
    0x74, 0x0b,                    // je     20
    0x99,                          // cltd
    0xf7, 0xfe,                    // idiv   %esi
    0x83, 0xfa, 0x01,              // cmp    $0x1,%edx
    0x83, 0xd9, 0xff,              // sbb    $-1,%ecx
    0xeb, 0xea,                    // jmp    a
    0xf4,                          // hlt
};

uint8_t kTenthprime2[] = {
    0xE8, 0x11, 0x00, 0x00, 0x00,  //
    0xF4,                          //
    0x89, 0xF8,                    //
    0xB9, 0x03, 0x00, 0x00, 0x00,  //
    0x99,                          //
    0xF7, 0xF9,                    //
    0x85, 0xD2,                    //
    0x0F, 0x95, 0xC0,              //
    0xC3,                          //
    0x55,                          //
    0x48, 0x89, 0xE5,              //
    0x31, 0xF6,                    //
    0x45, 0x31, 0xC0,              //
    0x44, 0x89, 0xC7,              //
    0xE8, 0xDF, 0xFF, 0xFF, 0xFF,  //
    0x0F, 0xB6, 0xC0,              //
    0x66, 0x83, 0xF8, 0x01,        //
    0x83, 0xDE, 0xFF,              //
    0x41, 0xFF, 0xC0,              //
    0x83, 0xFE, 0x0A,              //
    0x75, 0xE6,                    //
    0x44, 0x89, 0xC0,              //
    0x5D,                          //
    0xC3,                          //
};

struct Machine *m;

void SetUp(void) {
  m = NewMachine();
  m->mode = XED_MACHINE_MODE_LONG_64;
  m->cr3 = AllocateLinearPage(m);
  ReserveVirtual(m, 0, 4096, 0x0207);
  ASSERT_EQ(0x1007, Read64(m->real.p + 0x0000));  // PML4T
  ASSERT_EQ(0x2007, Read64(m->real.p + 0x1000));  // PDPT
  ASSERT_EQ(0x3007, Read64(m->real.p + 0x2000));  // PDE
  ASSERT_EQ(0x0207, Read64(m->real.p + 0x3000));  // PT
  ASSERT_EQ(0x4000, m->real.i);
  ASSERT_EQ(1, m->memstat.reserved);
  ASSERT_EQ(4, m->memstat.committed);
  ASSERT_EQ(4, m->memstat.allocated);
  ASSERT_EQ(3, m->memstat.pagetables);
  Write64(m->sp, 4096);
}

void TearDown(void) {
  FreeVirtual(m, 0, 4096);
  ASSERT_EQ(0x1007, Read64(m->real.p + 0x0000));  // PML4T
  ASSERT_EQ(0x2007, Read64(m->real.p + 0x1000));  // PDPT
  ASSERT_EQ(0x3007, Read64(m->real.p + 0x2000));  // PDE
  ASSERT_EQ(0x0000, Read64(m->real.p + 0x3000));  // PT
  FreeMachine(m);
}

int ExecuteUntilHalt(struct Machine *m) {
  int rc;
  if (!(rc = setjmp(m->onhalt))) {
    for (;;) {
      LoadInstruction(m);
      ExecuteInstruction(m);
    }
  } else {
    return rc;
  }
}

TEST(x86, size) {
  ASSERT_EQ(48, sizeof(struct XedDecodedInst));
}

TEST(machine, sizeIsReasonable) {
  ASSERT_LE(sizeof(struct Machine), 65536 * 3);
}

TEST(machine, test) {
  VirtualRecv(m, 0, kTenthprime, sizeof(kTenthprime));
  ASSERT_EQ(kMachineHalt, ExecuteUntilHalt(m));
  ASSERT_EQ(15, Read32(m->ax));
}

TEST(machine, fldpi) {
  uint8_t prog[] = {
      0xd9, 0xeb,  // fldpi
      0xf4,        // hlt
  };
  VirtualRecv(m, 0, prog, sizeof(prog));
  ASSERT_EQ(kMachineHalt, ExecuteUntilHalt(m));
  ASSERT_LDBL_EQ(M_PI, FpuPop(m));
}

TEST(x87, flds) {
  // 1 rem -1.5
  uint8_t prog[] = {
      0xd9, 0x05, 0x01, 0x00, 0x00, 0x00,  // flds
      0xf4,                                // hlt
      0x00, 0x00, 0xc0, 0xbf,              // .float -1.5
  };
  VirtualRecv(m, 0, prog, sizeof(prog));
  ASSERT_EQ(kMachineHalt, ExecuteUntilHalt(m));
  ASSERT_LDBL_EQ(-1.5, FpuPop(m));
}

TEST(x87, fprem) {
  // 1 rem -1.5
  uint8_t prog[] = {
      0xd9, 0x05, 0x05, 0x00, 0x00, 0x00,  // flds
      0xd9, 0xe8,                          // fld1
      0xd9, 0xf8,                          // fprem
      0xf4,                                // hlt
      0x00, 0x00, 0xc0, 0xbf,              // .float -1.5
  };
  VirtualRecv(m, 0, prog, sizeof(prog));
  ASSERT_EQ(kMachineHalt, ExecuteUntilHalt(m));
  ASSERT_LDBL_EQ(1, FpuPop(m));
}

TEST(machine, testFpu) {
  long double x;
  VirtualRecv(m, 0, kPi80, sizeof(kPi80));
  ASSERT_EQ(kMachineHalt, ExecuteUntilHalt(m));
  x = FpuPop(m);
  ASSERT_TRUE(fabsl(3.14159 - x) < 0.0001, "%Lg", x);
  m->ip = 0;
  ASSERT_EQ(kMachineHalt, ExecuteUntilHalt(m));
  x = FpuPop(m);
  ASSERT_TRUE(fabsl(3.14159 - x) < 0.0001, "%Lg", x);
}

TEST(x87, fprem1) {
  // 1 rem -1.5
  uint8_t prog[] = {
      0xd9, 0x05, 0x05, 0x00, 0x00, 0x00,  // flds
      0xd9, 0xe8,                          // fld1
      0xd9, 0xf8,                          // fprem
      0xf4,                                // hlt
      0x00, 0x00, 0xc0, 0xbf,              // .float -1.5
  };
  VirtualRecv(m, 0, prog, sizeof(prog));
  ASSERT_EQ(kMachineHalt, ExecuteUntilHalt(m));
  ASSERT_LDBL_EQ(1, FpuPop(m));
}

TEST(x87, fprem2) {
  // 12300000000000000. rem .0000000000000123
  uint8_t prog[] = {
      0xdd, 0x05, 0x11, 0x00, 0x00, 0x00,              // fldl
      0xdd, 0x05, 0x03, 0x00, 0x00, 0x00,              // fldl
      0xd9, 0xf8,                                      // fprem
      0xf4,                                            // hlt
      0x00, 0x60, 0x5e, 0x75, 0x64, 0xd9, 0x45, 0x43,  //
      0x5b, 0x14, 0xea, 0x9d, 0x77, 0xb2, 0x0b, 0x3d,  //
  };
  VirtualRecv(m, 0, prog, sizeof(prog));
  ASSERT_EQ(kMachineHalt, ExecuteUntilHalt(m));
  ASSERT_LDBL_EQ(1.1766221079117338e-14, FpuPop(m));
}

TEST(machine, testCallAddr32) {
  uint8_t kCode[] = {
      0x67, 0xE8, 0x01, 0x00, 0x00, 0x00,  // addr32 call 0x1
      0xF4,                                // hlt
      0xC2,                                // ret
  };
  VirtualRecv(m, 0, kCode, sizeof(kCode));
  ASSERT_EQ(kMachineHalt, ExecuteUntilHalt(m));
}
