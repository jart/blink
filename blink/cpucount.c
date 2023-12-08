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
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>

#include "blink/atomic.h"
#include "blink/builtin.h"
#include "blink/macros.h"
#include "blink/util.h"

#ifdef HAVE_SCHED_H
#include <sched.h>
#endif

#ifdef HAVE_SYSCTL
#include <sys/sysctl.h>
#endif

static _Atomic(int) g_cpucount;

static int GetCpuCountImpl(void) {
#ifdef HAVE_SCHED_GETAFFINITY
  cpu_set_t s;
  if (sched_getaffinity(0, sizeof(s), &s) == -1) return -1;
  return CPU_COUNT(&s);
#elif defined(HAVE_SYSCTL)
  int x;
  size_t n = sizeof(x);
  int mib[] = {CTL_HW, HW_NCPU};
  if (sysctl(mib, ARRAYLEN(mib), &x, &n, 0, 0) == -1) return -1;
  return x;
#elif defined(_SC_NPROCESSORS_ONLN)
  return sysconf(_SC_NPROCESSORS_ONLN);
#else
  return 1;
#endif /* HAVE_SYSCTL */
}

int GetCpuCount(void) {
  int rc;
  if (!(rc = g_cpucount)) {
    if ((rc = GetCpuCountImpl()) < 1) rc = 1;
    g_cpucount = rc;
  }
  return rc;
}
