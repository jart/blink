/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
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
#include <fcntl.h>
#include <sys/uio.h>

#include "test/test.h"

void SetUp(void) {
}

void TearDown(void) {
}

TEST(preadv, errors) {
  signed char b3[8];
  ASSERT_EQ(3, open("/tmp", O_RDWR | O_TMPFILE, 0644));
  ASSERT_EQ(8, pwritev(3, (struct iovec[]){{b3, 8}}, 1, 0));
  ASSERT_EQ(-1, preadv(3, (struct iovec[]){{0, 8}}, 1, 0));
  ASSERT_EQ(EFAULT, errno);
  ASSERT_EQ(0, preadv(3, (struct iovec[]){{b3, 8}}, 0, 0));
  ASSERT_EQ(-1, preadv(3, (struct iovec[]){{b3, 8}}, -1, 0));
  ASSERT_EQ(EINVAL, errno);
  ASSERT_EQ(-1, preadv(3, (struct iovec[]){{b3, 8}}, 1, -1));
  ASSERT_EQ(EINVAL, errno);
  ASSERT_EQ(-1, preadv(3, (struct iovec[]){{b3, 8}}, 99999, 0));
  ASSERT_EQ(EINVAL, errno);
  ASSERT_EQ(0, close(3));
}

TEST(preadv, test) {
  signed char b1[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  signed char b2[8] = {-1, -2, -3, -4, -5, -6, -7, -8};
  signed char b3[8];
  ASSERT_EQ(3, open("/tmp", O_RDWR | O_TMPFILE, 0644));
  ASSERT_EQ(16, pwritev(3, (struct iovec[]){{b1, 8}, {b2, 8}}, 2, 0));
  ASSERT_EQ(8, preadv(3, (struct iovec[]){{b3, 8}}, 1, 8));
  ASSERT_EQ(0, memcmp(b3, b2, 8));
  ASSERT_EQ(8, preadv(3, (struct iovec[]){{b3, 4}, {b3 + 4, 4}}, 2, 0));
  ASSERT_EQ(0, memcmp(b3, b1, 8));
  ASSERT_EQ(0, close(3));
}
