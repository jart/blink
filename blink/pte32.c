/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2023 Justine Alexandra Roberts Tunney                              │
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
#include "blink/builtin.h"
#include "blink/bus.h"
#include "blink/endian.h"
#include "blink/thread.h"

/**
 * @fileoverview page table ops for 32-bit hosts
 */

pthread_mutex_t_ g_ptelock = PTHREAD_MUTEX_INITIALIZER_;

u64 LoadPte32_(const u8 *pte) {
  u64 res;
  LOCK(&g_ptelock);
  res = Read64(pte);
  UNLOCK(&g_ptelock);
  return res;
}

void StorePte32_(u8 *pte, u64 val) {
  LOCK(&g_ptelock);
  Write64(pte, val);
  UNLOCK(&g_ptelock);
}

bool CasPte32_(u8 *pte, u64 oldval, u64 newval) {
  bool res;
  LOCK(&g_ptelock);
  if (Read64(pte) == oldval) {
    Write64(pte, newval);
    res = true;
  } else {
    res = false;
  }
  UNLOCK(&g_ptelock);
  return res;
}
