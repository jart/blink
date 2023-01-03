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
#include <pthread.h>
#include <stdatomic.h>

#include "test/test.h"

#define WORKERS    4
#define ITERATIONS 100000

void SetUp(void) {
}

void TearDown(void) {
}

static atomic_ulong word;

////////////////////////////////////////////////////////////////////////////////

static void *AddWorker(void *arg) {
  int i;
  unsigned long x;
  for (i = 0; i < ITERATIONS; ++i) {
    x = atomic_load_explicit(&word, memory_order_relaxed);
    ASSERT_EQ(x & 0xffffffff, x >> 32);
    x += 1ul | 1ul << 32;
    atomic_store_explicit(&word, x, memory_order_relaxed);
  }
  return 0;
}

TEST(mem64, add) {
  int i;
  pthread_t t[WORKERS];
  for (i = 0; i < WORKERS; ++i) {
    ASSERT_EQ(0, pthread_create(t + i, 0, AddWorker, 0));
  }
  for (i = 0; i < WORKERS; ++i) {
    ASSERT_EQ(0, pthread_join(t[i], 0));
  }
}

////////////////////////////////////////////////////////////////////////////////

static void *XaddWorker(void *arg) {
  int i;
  unsigned long x;
  for (i = 0; i < ITERATIONS; ++i) {
    x = atomic_fetch_add_explicit(&word, 1ul | 1ul << 32, memory_order_relaxed);
    ASSERT_EQ(x & 0xffffffff, x >> 32);
  }
  return 0;
}

TEST(mem64, xadd) {
  int i;
  pthread_t t[WORKERS];
  for (i = 0; i < WORKERS; ++i) {
    ASSERT_EQ(0, pthread_create(t + i, 0, XaddWorker, 0));
  }
  for (i = 0; i < WORKERS; ++i) {
    ASSERT_EQ(0, pthread_join(t[i], 0));
  }
}
