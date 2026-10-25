/* Exercise host-page table growth while other threads access guest stacks. */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
static void* worker(void* arg) {
  volatile unsigned char data[4096];
  uintptr_t seed = (uintptr_t)arg;
  for (unsigned i = 0; i < sizeof(data); ++i) data[i] = i ^ seed;
  for (unsigned i = 0; i < sizeof(data); ++i)
    if (data[i] != (unsigned char)(i ^ seed)) return (void*)-1;
  return arg;
}
int main(void) {
  for (uintptr_t i = 1; i <= 128; ++i) {
    pthread_t a, b;
    void *x, *y;
    if (pthread_create(&a, 0, worker, (void*)i) ||
        pthread_create(&b, 0, worker, (void*)(i + 128)))
      return 1;
    if (pthread_join(a, &x) || pthread_join(b, &y)) return 2;
    if (x != (void*)i || y != (void*)(i + 128)) return 3;
  }
  puts("PASS 256 thread creations");
  return 0;
}
