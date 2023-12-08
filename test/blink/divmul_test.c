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
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/macros.h"
#include "test/test.h"

#define CX     1
#define OSZ    00000000040
#define REXW   00000000100
#define RM(x)  (0000001600 & ((x) << 007))
#define MOD(x) (0060000000 & ((x) << 026))

struct Machine m[1];

struct {
  const char *name;
  void (*f)(P);
  u64 rde;
  u64 ax;
  u64 cx;
  u64 dx;
  u64 out_ax;
  u64 out_dx;
  bool out_cf;
  bool out_of;
} kVector[] = {
#define M(f, m, a, c, d, oa, od, cf, of) {#f, f, m, a, c, d, oa, od, cf, of},
#include "test/blink/divmul_imul64_test.inc"
#include "test/blink/divmul_mul64_test.inc"
#include "test/blink/divmul_mul8_test.inc"
};

void SetUp(void) {
}

void TearDown(void) {
}

TEST(divmul, test) {
  int i;
  for (i = 0; i < ARRAYLEN(kVector); ++i) {
    Write64(m->ax, kVector[i].ax);
    Write64(m->cx, kVector[i].cx);
    Write64(m->dx, kVector[i].dx);
    kVector[i].f(m, kVector[i].rde, 0, 0);
    ASSERT_EQ(kVector[i].out_ax, Read64(m->ax), "%d: %s", i, kVector[i].name);
    ASSERT_EQ(kVector[i].out_dx, Read64(m->dx), "%d: %s", i, kVector[i].name);
    ASSERT_EQ(kVector[i].out_cf, GetFlag(m->flags, FLAGS_CF), "%d: %s", i,
              kVector[i].name);
    ASSERT_EQ(kVector[i].out_of, GetFlag(m->flags, FLAGS_OF), "%d: %s", i,
              kVector[i].name);
  }
}
