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
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "test/test.h"

/**
 * @fileoverview Large File Tests
 *
 * This test ensures that the Blink Virtual Machine is able to host, on
 * 32-bit platforms, x86_64 binaries that perform 64-bit I/O operations
 */

#define SIZE (5LL * 1024 * 1024 * 1024)

void SetUp(void) {
}

void TearDown(void) {
}

TEST(largefile, test) {
  struct stat st;
  char path[] = "/tmp/blink.test.XXXXXX";
  char b3[8], b2[8], b1[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  ASSERT_EQ(3, mkstemp(path));
  ASSERT_EQ(0, close(3));
  EXPECT_EQ(3, open(path, O_RDWR | O_CREAT | O_TRUNC, 0644));
  EXPECT_EQ(0, ftruncate(3, SIZE));
  EXPECT_EQ(0, fstat(3, &st));
  EXPECT_EQ(SIZE, st.st_size);
  EXPECT_EQ(8, pwrite(3, b1, 8, SIZE - 8));
  EXPECT_EQ(8, pread(3, b2, 8, SIZE - 8));
  EXPECT_EQ(0, memcmp(b1, b2, 8));
  EXPECT_EQ(SIZE - 8, lseek(3, SIZE - 8, SEEK_SET));
  EXPECT_EQ(8, read(3, b3, 8));
  EXPECT_EQ(0, memcmp(b1, b3, 8));
  EXPECT_EQ(0, close(3));
  EXPECT_EQ(0, unlink(path));
}
