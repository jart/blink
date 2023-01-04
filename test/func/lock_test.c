#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define THREADS    4
#define ITERATIONS 10000

_Alignas(128) static int a;
_Alignas(128) static int b;
_Alignas(128) static atomic_int lock;

static void Lock(void) {
  int x;
  for (;;) {
    x = atomic_exchange_explicit(&lock, 1, memory_order_acquire);
    if (!x) break;
  }
}

static void Unlock(void) {
  atomic_store_explicit(&lock, 0, memory_order_release);
}

static void *Worker(void *arg) {
  int i;
  for (i = 0; i < ITERATIONS; ++i) {
    Lock();
    assert(b == a * 2);
    ++a;
    b = a * 2;
    Unlock();
  }
  return 0;
}

int main(int argc, char *argv[]) {
  int i;
  pthread_t t[THREADS];
  for (i = 0; i < THREADS; ++i) {
    pthread_create(t + i, 0, Worker, 0);
  }
  for (i = 0; i < THREADS; ++i) {
    pthread_join(t[i], 0);
  }
}
