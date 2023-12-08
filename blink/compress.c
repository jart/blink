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
#include <stdlib.h>
#include <zlib.h>

#include "blink/assert.h"
#include "blink/util.h"

void *Deflate(const void *data, unsigned size, unsigned *out_size) {
  void *res;
  uLong bound;
  z_stream zs = {0};
  bound = compressBound(size);
  unassert(res = malloc(bound));
  unassert(deflateInit2(&zs, 4, Z_DEFLATED, -MAX_WBITS, 8,
                        Z_DEFAULT_STRATEGY) == Z_OK);
  zs.next_in = (z_const Bytef *)data;
  zs.avail_in = size;
  zs.avail_out = bound;
  zs.next_out = (Bytef *)res;
  unassert(deflate(&zs, Z_FINISH) == Z_STREAM_END);
  unassert(deflateEnd(&zs) == Z_OK);
  unassert(res = realloc(res, zs.total_out));
  *out_size = zs.total_out;
  return res;
}

void Inflate(void *out, unsigned outsize, const void *in, unsigned insize) {
  z_stream zs;
  zs.next_in = (z_const Bytef *)in;
  zs.avail_in = insize;
  zs.total_in = insize;
  zs.next_out = (Bytef *)out;
  zs.avail_out = outsize;
  zs.total_out = outsize;
  zs.zalloc = Z_NULL;
  zs.zfree = Z_NULL;
  unassert(inflateInit2(&zs, -MAX_WBITS) == Z_OK);
  unassert(inflate(&zs, Z_FINISH) == Z_STREAM_END);
  unassert(inflateEnd(&zs) == Z_OK);
}
