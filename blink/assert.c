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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "blink/assert.h"
#include "blink/debug.h"
#include "blink/endian.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"

int asan_backtrace_index;
int asan_backtrace_buffer[1];

static void PrintBacktraceUsingAsan(void) {
  volatile int x;
  x = asan_backtrace_buffer[asan_backtrace_index + 1];
  (void)x;
}

void AssertFailed(const char *file, int line, const char *msg) {
  _Thread_local static bool noreentry;
  char b[512];
  snprintf(b, sizeof(b), "%s:%d: assertion failed: %s\n", file, line, msg);
  b[sizeof(b) - 1] = 0;
  WriteErrorString(b);
  if (g_machine && !noreentry) {
    noreentry = true;
    RestoreIp(g_machine);
    WriteErrorString("\t");
    WriteErrorString(GetBacktrace(g_machine));
    WriteErrorString("\n");
  }
  PrintBacktraceUsingAsan();
  abort();
}
