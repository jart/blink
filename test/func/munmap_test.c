#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define BYTES      10000
#define THREADS    4
#define ITERATIONS 256

_Atomic(void *) buf;

void *Worker(void *arg) {
  void *b;
  int i, fd;
  ssize_t rc;
  if ((fd = open("/dev/zero", O_RDONLY)) == -1) _exit(1);
  for (i = 0; i < ITERATIONS; ++i) {
    b = buf;
    rc = read(fd, b, 10000);
    if (rc == -1 && errno != EFAULT) _exit(2);
  }
  close(fd);
  return 0;
}

void *Alloc(void) {
  return mmap(0, BYTES, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1,
              0);
}

int main(int argc, char *argv[]) {
  int i;
  pthread_t t[THREADS];
  buf = Alloc();
  for (i = 0; i < THREADS; ++i) {
    if (pthread_create(t + i, 0, Worker, 0)) _exit(2);
  }
  for (i = 0; i < ITERATIONS; ++i) {
    munmap(atomic_exchange(&buf, Alloc()), BYTES);
  }
  for (i = 0; i < THREADS; ++i) {
    if (pthread_join(t[i], 0)) _exit(3);
  }
  if (munmap(buf, BYTES)) _exit(4);
  return 0;
}
