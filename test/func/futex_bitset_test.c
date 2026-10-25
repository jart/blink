#define _GNU_SOURCE
#include <errno.h>
#include <linux/futex.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

static _Atomic int word;
static void *wake(void *arg) {
  struct timespec delay = {0, 20000000};
  (void)arg;
  nanosleep(&delay, 0);
  atomic_store(&word, 1);
  syscall(SYS_futex, &word, FUTEX_WAKE_BITSET | FUTEX_PRIVATE_FLAG, 1,
          0, 0, FUTEX_BITSET_MATCH_ANY);
  return 0;
}

int main(void) {
  struct timespec past = {0, 0}, deadline;
  pthread_t thread;
  int rc;
  if (syscall(SYS_futex, &word, FUTEX_WAIT_BITSET, 0, &past,
              0, FUTEX_BITSET_MATCH_ANY) != -1 || errno != ETIMEDOUT) return 1;
  if (syscall(SYS_futex, &word, FUTEX_WAIT_BITSET | FUTEX_CLOCK_REALTIME, 0,
              &past, 0, FUTEX_BITSET_MATCH_ANY) != -1 || errno != ETIMEDOUT) return 2;
  if (syscall(SYS_futex, &word, FUTEX_WAIT_BITSET, 1, 0,
              0, FUTEX_BITSET_MATCH_ANY) != -1 || errno != EAGAIN) return 3;
  if (syscall(SYS_futex, &word, FUTEX_WAIT_BITSET, 0, 0,
              0, 0) != -1 || errno != EINVAL) return 4;
  if (clock_gettime(CLOCK_MONOTONIC, &deadline)) return 5;
  deadline.tv_sec += 2;
  if (pthread_create(&thread, 0, wake, 0)) return 6;
  rc = syscall(SYS_futex, &word, FUTEX_WAIT_BITSET | FUTEX_PRIVATE_FLAG, 0,
               &deadline, 0, FUTEX_BITSET_MATCH_ANY);
  if (rc && errno != EAGAIN) return 7;
  if (pthread_join(thread, 0) || atomic_load(&word) != 1) return 8;
  return 0;
}
