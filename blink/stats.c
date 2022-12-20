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
#include "blink/log.h"
#include "blink/stats.h"

#define DEFINE_AVERAGE(S) struct Average S;
#define DEFINE_COUNTER(S) long S;
#include "blink/stats.inc"
#undef DEFINE_AVERAGE
#undef DEFINE_COUNTER

#define APPEND(...) o += snprintf(b + o, n - o, __VA_ARGS__)

void PrintStats(void) {
#ifndef NDEBUG
  char b[2048];
  int n = sizeof(b);
  int o = 0;
  b[0] = 0;
#define DEFINE_COUNTER(S) \
  if (S) APPEND("%-32s = %ld\n", #S, S);
#define DEFINE_AVERAGE(S) \
  if (S.a) APPEND("%-32s = %.6g\n", #S, S.a);
#include "blink/stats.inc"
#undef S
  WriteErrorString(b);
#endif
}
