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
#include <stdio.h>
#include <string.h>

#include "blink/builtin.h"
#include "blink/high.h"

struct High g_high = {
    .enabled = true,
    .active = true,
#ifdef __APPLE__
    .keyword = 40,
#else
    .keyword = 155,
#endif
    .reg = 215,
    .literal = 182,
    .label = 221,
    .comment = 112,
    .quote = 215,
};

char *HighStart(char *p, int h) {
  if (g_high.enabled) {
    if (h) {
      p = stpcpy(p, "\033[38;5;");
      p += snprintf(p, 12, "%u", h);
      p = stpcpy(p, "m");
      g_high.active = true;
    }
  }
  return p;
}

char *HighEnd(char *p) {
  if (g_high.enabled) {
    if (g_high.active) {
      p = stpcpy(p, "\033[39m");
      g_high.active = false;
    }
  }
  return p;
}
