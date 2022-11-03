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
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "blink/ldbl.h"
#include "test/test.h"

char *fmt(const char *fmt, ...) {
  char *s;
  va_list va;
  s = malloc(64);
  va_start(va, fmt);
  vsnprintf(s, 64, fmt, va);
  va_end(va);
  return s;
}

double RoundTrip(double x) {
  uint8_t b[10];
  return DeserializeLdbl(SerializeLdbl(b, x));
}

TEST(SerializeLdbl, testRoundTrip) {
  EXPECT_STREQ("0", fmt("%g", RoundTrip(0)));
  EXPECT_STREQ("-0", fmt("%g", RoundTrip(-0.)));
  EXPECT_STREQ("nan", fmt("%g", RoundTrip(NAN)));
  EXPECT_STREQ("-nan", fmt("%g", RoundTrip(-NAN)));
  EXPECT_STREQ("inf", fmt("%g", RoundTrip(INFINITY)));
  EXPECT_STREQ("-inf", fmt("%g", RoundTrip(-INFINITY)));
  EXPECT_STREQ(fmt("%.17g", DBL_MIN), fmt("%.17g", RoundTrip(DBL_MIN)));
  EXPECT_STREQ(fmt("%.17g", DBL_MAX), fmt("%.17g", RoundTrip(DBL_MAX)));
}
