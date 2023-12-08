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
#include "blink/modrm.h"
#include "blink/types.h"

// Byte register offsets.
//
// for (i = 0; i < 2; ++i) {      // rex
//   for (j = 0; j < 2; ++j) {    // rexb, or rexr
//     for (k = 0; k < 8; ++k) {  // reg, rm, or srm
//       kByteReg[i << 4 | j << 3 | k] =
//           i ? (j << 3 | k) * 8 : (k & 3) * 8 + ((k & 4) >> 2);
//     }
//   }
// }
const u8 kByteReg[32] = {0000, 0010, 0020, 0030, 0001, 0011, 0021, 0031,
                         0000, 0010, 0020, 0030, 0001, 0011, 0021, 0031,
                         0000, 0010, 0020, 0030, 0040, 0050, 0060, 0070,
                         0100, 0110, 0120, 0130, 0140, 0150, 0160, 0170};
