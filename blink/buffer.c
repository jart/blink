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
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blink/buffer.h"
#include "blink/macros.h"
#include "blink/memcpy.h"
#include "blink/stats.h"
#include "blink/util.h"
#include "blink/vfs.h"

static bool GrowBuffer(struct Buffer *b, int need) {
  char *p;
  unsigned n;
  n = MAX(b->i + need, MAX(16, b->n + (b->n >> 1)));
  if (!(p = (char *)realloc(b->p, n))) return false;
  b->p = p;
  b->n = n;
  return true;
}

void AppendData(struct Buffer *b, const char *data, int len) {
  if (b->i + len + 1 > b->n && !GrowBuffer(b, len + 1)) return;
  memcpy(b->p + b->i, data, len);
  b->p[b->i += len] = 0;
}

int AppendChar(struct Buffer *b, char c) {
  if (b->i + 2 > b->n && !GrowBuffer(b, 2)) return 0;
  b->p[b->i++] = c;
  b->p[b->i] = 0;
  return 1;
}

int AppendStr(struct Buffer *b, const char *s) {
  int n = strlen(s);
  AppendData(b, s, n);
  return n;
}

int AppendWide(struct Buffer *b, wint_t wc) {
  u64 wb;
  if (0 <= wc && wc <= 0x7f) {
    AppendChar(b, wc);
  } else {
    if (b->i + 8 > b->n && !GrowBuffer(b, 8)) return 0;
    wb = tpenc(wc);
    do {
      b->p[b->i++] = wb & 0xFF;
    } while ((wb >>= 8));
    b->p[b->i] = 0;
  }
  return 1;
}

int AppendFmt(struct Buffer *b, const char *fmt, ...) {
  int n;
  va_list va, vb;
  va_start(va, fmt);
  va_copy(vb, va);
  n = vsnprintf(b->p + b->i, b->n - b->i, fmt, va);
  if (b->i + n + 1 > b->n) {
    do {
      if (b->n) {
        b->n += b->n >> 1;
      } else {
        b->n = 16;
      }
    } while (b->i + n + 1 > b->n);
    b->p = (char *)realloc(b->p, b->n);
    vsnprintf(b->p + b->i, b->n - b->i, fmt, vb);
  }
  va_end(vb);
  va_end(va);
  b->i += n;
  return n;
}

ssize_t UninterruptibleWrite(int fd, const void *p, size_t n) {
  size_t i;
  ssize_t rc;
  for (i = 0; i < n; i += rc) {
  TryAgain:
    rc = VfsWrite(fd, (const char *)p + i, n - i);
    if (rc == -1 && errno == EINTR) goto TryAgain;
    if (rc == -1) return -1;
  }
  return n;
}
