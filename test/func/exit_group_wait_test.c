/* exit_group must interrupt another thread blocked in a host syscall. */
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>

static int pipefds[2];
static atomic_int ready;

static void *reader(void *arg) {
  char byte;
  atomic_store(&ready, 1);
  read(pipefds[0], &byte, 1);
  return 0;
}

int main(void) {
  pthread_t thread;
  if (pipe(pipefds) || pthread_create(&thread, 0, reader, 0)) return 1;
  while (!atomic_load(&ready)) usleep(1000);
  usleep(100000);
  _exit(0);
}
