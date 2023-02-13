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
#include <errno.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

/*
 * assuming the system is working correctly there's a 1 in
 * 235716896562095165800448 chance this check should flake
 */
#define BYTES 9
#define TRIES 16

void TestDataSeemsRandom(void) {
  ssize_t rc;
  size_t i, j;
  int haszero, fails = 0;
  for (i = 0; i < TRIES; ++i) {
    unsigned char x[BYTES] = {0};
    rc = syscall(SYS_getrandom, x, BYTES, 0);
    if (rc == -1) exit(1);
    if (rc != BYTES) exit(2);
    for (haszero = j = 0; j < BYTES; ++j) {
      if (!x[j]) haszero = 1;
    }
    if (haszero) ++fails;
  }
  if (fails >= TRIES) exit(3);
}

void TestApi(void) {
  long x;
  if (syscall(SYS_getrandom, 0, 0, 0) != 0) exit(4);
  if (syscall(SYS_getrandom, (void *)-1, 1, 0) != -1) exit(5);
  if (errno != EFAULT) exit(6);
  if (syscall(SYS_getrandom, &x, sizeof(x), -1) != -1) exit(7);
  if (errno != EINVAL) exit(8);
}

int main(int argc, char *argv[]) {
  TestDataSeemsRandom();
  TestApi();
  return 0;
}
