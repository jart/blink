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
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PCHECK(x)  \
  do {             \
    int rc_ = (x); \
    if (rc_) {     \
      errno = rc_; \
      perror(#x);  \
      exit(1);     \
    }              \
  } while (0)

static pthread_mutex_t mtx;

static void *OriginalOwnerThread(void *ptr) {
  pthread_mutex_lock(&mtx);
  pthread_exit(NULL);
}

static void WaitForOriginalOwnerThreadToDie(pthread_t thr) {
  int rc;
  for (;;) {
    rc = pthread_kill(thr, 0);
    if (rc == ESRCH) break;
    assert(!rc);
  }
}

int main(int argc, char *argv[]) {
  int rc;
  pthread_t thr;
  pthread_mutexattr_t attr;
  PCHECK(pthread_mutexattr_init(&attr));
  PCHECK(pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST));
  PCHECK(pthread_mutex_init(&mtx, &attr));
  PCHECK(pthread_mutexattr_destroy(&attr));
  PCHECK(pthread_create(&thr, 0, OriginalOwnerThread, 0));
  WaitForOriginalOwnerThreadToDie(thr);
  rc = pthread_mutex_lock(&mtx);
  if (rc == EOWNERDEAD) {
    PCHECK(pthread_mutex_consistent(&mtx));
    PCHECK(pthread_mutex_unlock(&mtx));
    exit(EXIT_SUCCESS);
  } else if (!rc) {
    fprintf(stderr, "pthread_mutex_lock() unexpectedly succeeded\n");
    exit(EXIT_FAILURE);
  } else {
    fprintf(stderr, "pthread_mutex_lock() unexpectedly failed: %s\n",
            strerror(rc));
    exit(1);
  }
}
