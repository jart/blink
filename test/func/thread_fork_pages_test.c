#include <pthread.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
static atomic_int stop;
static void* worker(void* arg) {
  while (!atomic_load(&stop)) {
    volatile char* p = mmap(0, 65536, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) _exit(2);
    for (int i = 0; i < 65536; i += 4096) p[i] = 1;
    munmap((void*)p, 65536);
  }
  return 0;
}
int main(void) {
  pthread_t a, b;
  if (pthread_create(&a, 0, worker, 0) || pthread_create(&b, 0, worker, 0))
    return 1;
  for (int i = 0; i < 20; ++i) {
    pid_t p = fork();
    int status;
    if (p < 0) return 3;
    if (!p) {
      volatile char* q = mmap(0, 4096, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      if (q == MAP_FAILED) _exit(4);
      *q = 42;
      _exit(*q == 42 ? 0 : 5);
    }
    if (waitpid(p, &status, 0) != p || !WIFEXITED(status) ||
        WEXITSTATUS(status))
      return 6;
  }
  atomic_store(&stop, 1);
  pthread_join(a, 0);
  pthread_join(b, 0);
  return 0;
}
