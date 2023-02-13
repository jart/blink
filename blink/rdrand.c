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
#include "blink/bus.h"
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/lock.h"
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/random.h"
#include "blink/util.h"

#define RESEED_INTERVAL 16

static struct Rdrand {
  pthread_mutex_t lock;
  u64 state;
  unsigned count;
} g_rdrand = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
};

static void OpRand(P, u64 x) {
  WriteRegister(rde, RegRexbRm(m, rde), x);
  m->flags = SetFlag(m->flags, FLAGS_CF, true);
}

void OpRdrand(P) {
  LOCK(&g_rdrand.lock);
  if (!(g_rdrand.count++ % RESEED_INTERVAL)) {
    unassert(GetRandom(&g_rdrand.state, 8, 0) == 8);
  }
  OpRand(A, Vigna(&g_rdrand.state));
  UNLOCK(&g_rdrand.lock);
}

void OpRdseed(P) {
  u64 x;
  unassert(GetRandom(&x, 8, 0) == 8);
  OpRand(A, x);
}
