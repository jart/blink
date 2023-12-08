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
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define SCHECK(x)              \
  do {                         \
    if ((intptr_t)(x) == -1) { \
      perror(#x);              \
      exit(1);                 \
    }                          \
  } while (0)

#define PCHECK(x)  \
  do {             \
    int rc_ = (x); \
    if (rc_) {     \
      errno = rc_; \
      perror(#x);  \
      exit(1);     \
    }              \
  } while (0)

static struct {
  atomic_int ready;
  pthread_mutex_t mtx;
} * shared;

int main(int argc, char *argv[]) {
  int rc, pid;
  pthread_mutexattr_t attr;
  if (1) return 0;  // TODO: Figure out how robust list works.
  SCHECK(shared = mmap(0, 65536, PROT_READ | PROT_WRITE,
                       MAP_SHARED | MAP_ANONYMOUS, -1, 0));
  PCHECK(pthread_mutexattr_init(&attr));
  PCHECK(pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST));
  PCHECK(pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED));
  PCHECK(pthread_mutex_init(&shared->mtx, &attr));
  PCHECK(pthread_mutexattr_destroy(&attr));
  SCHECK(pid = fork());
  if (!pid) {
    PCHECK(pthread_mutex_lock(&shared->mtx));
    shared->ready = 1;
    for (;;) getppid();
  }
  while (!shared->ready) {
  }
  fprintf(stderr, "killing\n");
  SCHECK(kill(pid, SIGTERM));
  SCHECK(wait(0));
  fprintf(stderr, "locking\n");
  rc = pthread_mutex_lock(&shared->mtx);
  fprintf(stderr, "locked\n");
  if (rc == EOWNERDEAD) {
    fprintf(stderr, "good\n");
    PCHECK(pthread_mutex_consistent(&shared->mtx));
    PCHECK(pthread_mutex_unlock(&shared->mtx));
    exit(0);
  } else if (!rc) {
    fprintf(stderr, "pthread_mutex_lock() unexpectedly succeeded\n");
    exit(1);
  } else {
    fprintf(stderr, "pthread_mutex_lock() unexpectedly failed: %s\n",
            strerror(rc));
    exit(1);
  }
}
