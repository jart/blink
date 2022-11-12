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
#include <sched.h>
#include <time.h>

#include "blink/endian.h"
#include "blink/modrm.h"
#include "blink/time.h"

void OpPause(struct Machine *m, u32 rde) {
  sched_yield();
}

void OpRdtsc(struct Machine *m, u32 rde) {
  i64 c;
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  c = ts.tv_sec;
  c *= 1000000000;
  c += ts.tv_nsec;
  Write64(m->ax, (c & 0x00000000ffffffff) >> 000);
  Write64(m->dx, (c & 0xffffffff00000000) >> 040);
}

static i64 GetTscAux(struct Machine *m) {
  u32 core, node;
  core = 0;
  node = 0;
  return (node & 0xfff) << 12 | (core & 0xfff);
}

void OpRdtscp(struct Machine *m, u32 rde) {
  OpRdtsc(m, rde);
  Write64(m->cx, GetTscAux(m));
}

void OpRdpid(struct Machine *m, u32 rde) {
  Write64(RegRexbRm(m, rde), GetTscAux(m));
}
