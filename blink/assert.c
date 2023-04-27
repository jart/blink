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

#include <errno.h>
#include <stdio.h>

#include "blink/debug.h"
#include "blink/flag.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/thread.h"
#include "blink/util.h"

void AssertFailed(const char *file, int line, const char *msg) {
  _Thread_local static bool noreentry;
  _Thread_local static char bp[20000];
  WriteErrorString("assertion failed\n");
  if (!noreentry) {
    noreentry = true;
    FLAG_nologstderr = false;
    RestoreIp(g_machine);
    snprintf(bp, sizeof(bp),
             "%s:%d:%d assertion failed: %s (%s)\n"
             "\t%s\n"
             "\t%s\n",
             file, line, g_machine ? g_machine->tid : 666, msg,
             DescribeHostErrno(errno), GetBacktrace(g_machine),
             GetBlinkBacktrace());
    WriteErrorString(bp);
  }
  Abort();
}
