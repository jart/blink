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
#include <sys/mman.h>

#include "blink/macros.h"
#include "test/test.h"

long pagesize;

void SetUp(void) {
  pagesize = sysconf(_SC_PAGESIZE);
}

void TearDown(void) {
}

TEST(mmap, file_shared) {
  FILE *f;
  u8 *a, *b, *p;
  // we initialize the file to contain this
  ASSERT_NOTNULL(a = (u8 *)malloc(pagesize * 3));
  memset(a, 'a', pagesize * 3);
  // we're going to change the file to have this
  ASSERT_NOTNULL(b = (u8 *)malloc(pagesize * 3));
  memset(b, 'a', pagesize * 1);
  memset(b + pagesize, 'b', pagesize * 2);
  // create the file
  ASSERT_NOTNULL(f = tmpfile());
  ASSERT_EQ(3, fwrite(a, pagesize, 3, f));
  // map the file into memory starting at the second page
  EXPECT_NE((intptr_t)MAP_FAILED,
            (intptr_t)(p = (u8 *)mmap(0, pagesize * 2, PROT_READ | PROT_WRITE,
                                      MAP_SHARED, fileno(f), pagesize)));
  // change the file via the memory mapping
  memset(p, 'b', pagesize * 2);
  EXPECT_NE(-1, msync(p, pagesize * 2, MS_SYNC));
  // read the file back into memory and verify it's coherent
  rewind(f);
  ASSERT_EQ(3, fread(a, pagesize, 3, f));
  ASSERT_EQ(0, memcmp(b, a, pagesize));
  ASSERT_EQ(0, memcmp(b + pagesize, a + pagesize, pagesize));
  ASSERT_EQ(0, memcmp(b + pagesize * 2, a + pagesize * 2, pagesize));
  // perform cleanup duties
  ASSERT_EQ(0, munmap(p, pagesize * 2));
  ASSERT_EQ(0, fclose(f));
  free(b);
  free(a);
}

TEST(mmap, multiprocess_private_vs_shared) {
  int ws, pid;
  int stackvar;
  int *sharedvar;
  int *privatevar;
  EXPECT_NE((intptr_t)MAP_FAILED,
            (intptr_t)(sharedvar = mmap(0, pagesize, PROT_READ | PROT_WRITE,
                                        MAP_SHARED | MAP_ANONYMOUS, -1, 0)));
  EXPECT_NE((intptr_t)MAP_FAILED,
            (intptr_t)(privatevar = mmap(0, pagesize, PROT_READ | PROT_WRITE,
                                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)));
  stackvar = 1;
  *sharedvar = 1;
  *privatevar = 1;
  EXPECT_NE(-1, (pid = fork()));
  if (!pid) {
    ++stackvar;
    ++*sharedvar;
    ++*privatevar;
    msync((void *)ROUNDDOWN((intptr_t)&stackvar, pagesize), pagesize, MS_SYNC);
    EXPECT_NE(-1, msync(privatevar, pagesize, MS_SYNC));
    EXPECT_NE(-1, msync(sharedvar, pagesize, MS_SYNC));
    _exit(0);
  }
  EXPECT_NE(-1, waitpid(pid, &ws, 0));
  EXPECT_EQ(1, stackvar);
  EXPECT_EQ(2, *sharedvar);
  EXPECT_EQ(1, *privatevar);
  EXPECT_NE(-1, munmap(sharedvar, pagesize));
  EXPECT_NE(-1, munmap(privatevar, pagesize));
}
