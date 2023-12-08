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
#include <string.h>

#include "blink/macros.h"
#include "blink/types.h"
#include "blink/util.h"

/**
 * Decimates array using John Costella's Magic Kernel Sharp.
 *
 * The Magic Kernel Sharp is the combination of two kernels:
 *
 * - `{1, 3, 3, 1}` for scaling
 * - `{1, 6, 1}` for sharpening
 *
 * Together they form the kernel `{-1, -3, 3, 17, 17, 3, -3, -1}` which
 * resembles the sinc function. Here's an example of how it works. Let's
 * say we want to shrink the following array in half:
 *
 *     u8 A[] = {20, 20, 0, 0, 20, 20, 0, 0, 20, 20};
 *
 * Using Magikarp, we'd write the following code:
 *
 *     int i, n = Magikarp(A, ARRAYLEN(A));
 *     for (i = 0; i < n; ++i) {
 *       if (i) printf(", ");
 *       printf("%d", A[i]);
 *     }
 *     printf("\n");
 *
 * Which would then print the new array:
 *
 *     20, 0, 20, 0, 20
 *
 * @param p is array of unsigned bytes, which is modified in place
 * @param n is number of bytes in `p`
 * @return new length of array
 */
long Magikarp(u8 *p, long n) {
  u8 *q;
  long h, i;
  if (n < 2) return n;
  q = (u8 *)malloc(3 + n + 4);
  q[0] = p[0];
  q[1] = p[0];
  q[2] = p[0];
  memcpy(q + 3, p, n);
  q[3 + n + 0] = p[n - 1];
  q[3 + n + 1] = p[n - 1];
  q[3 + n + 2] = p[n - 1];
  q[3 + n + 3] = p[n - 1];
  q += 3;
  h = (n + 1) >> 1;
  for (i = 0; i < h; ++i) {
    short x0, x1, x2, x3, x4, x5, x6, x7;
    x0 = q[i * 2 - 3];
    x1 = q[i * 2 - 2];
    x2 = q[i * 2 - 1];
    x3 = q[i * 2 + 0];
    x4 = q[i * 2 + 1];
    x5 = q[i * 2 + 2];
    x6 = q[i * 2 + 3];
    x7 = q[i * 2 + 4];
    x0 *= -1;
    x1 *= -3;
    x2 *= +3;
    x3 *= 17;
    x4 *= 17;
    x5 *= +3;
    x6 *= -3;
    x7 *= -1;
    x0 += x1;
    x2 += x3;
    x4 += x5;
    x6 += x7;
    x0 += x2;
    x4 += x6;
    x0 += x4;
    x0 += 1 << 4;
    x0 >>= 5;
    p[i] = MIN(255, MAX(0, x0));
  }
  free(q - 3);
  return h;
}
