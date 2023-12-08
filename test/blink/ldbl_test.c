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
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "blink/ldbl.h"
#include "test/test.h"

void SetUp(void) {
}

void TearDown(void) {
}

double RoundTrip(double x) {
  uint8_t b[10];
  return DeserializeLdbl(SerializeLdbl(b, x));
}

TEST(SerializeLdbl, testRoundTrip) {
  EXPECT_STREQ("0", Format("%g", RoundTrip(0)));
  EXPECT_STREQ("-0", Format("%g", RoundTrip(-0.)));
  EXPECT_STREQ("nan", Format("%g", RoundTrip(NAN)));
  EXPECT_STREQ("inf", Format("%g", RoundTrip(INFINITY)));
  EXPECT_STREQ("-inf", Format("%g", RoundTrip(-INFINITY)));
  EXPECT_STREQ(Format("%.17g", DBL_MIN), Format("%.17g", RoundTrip(DBL_MIN)));
  EXPECT_STREQ(Format("%.17g", DBL_MAX), Format("%.17g", RoundTrip(DBL_MAX)));
  // TODO(jart): What's up with Apple Silicon here?
  // EXPECT_STREQ("-nan", Format("%g", RoundTrip(-NAN)));
}
