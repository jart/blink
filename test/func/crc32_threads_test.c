#include <pthread.h>
#include <stdint.h>

#ifdef __x86_64__
static pthread_barrier_t barrier;

static uint32_t Reference(uint32_t crc, uint64_t value) {
  unsigned i;
  for (i = 0; i < 64; ++i) {
    unsigned bit = (crc ^ value) & 1;
    crc >>= 1;
    if (bit) crc ^= 0x82f63b78;
    value >>= 1;
  }
  return crc;
}

static void *Worker(void *arg) {
  uintptr_t index = (uintptr_t)arg;
  uint64_t value, actual;
  uint32_t seed;
  unsigned i;
  int rc = pthread_barrier_wait(&barrier);
  if (rc && rc != PTHREAD_BARRIER_SERIAL_THREAD) return (void *)1;
  for (i = 0; i < 256; ++i) {
    value = UINT64_C(0xfedcba9876543210) ^ ((uint64_t)index << 32) ^ i;
    seed = 0x12345678 ^ (index * 0x10203 + i);
    actual = UINT64_C(0xdeadbeef00000000) | seed;
    __asm__ volatile("crc32q %1,%0" : "+r"(actual) : "r"(value));
    if (actual != Reference(seed, value)) return (void *)2;
  }
  return 0;
}
#endif

int main(void) {
#ifdef __x86_64__
  pthread_t threads[16];
  uintptr_t i;
  void *result;
  if (pthread_barrier_init(&barrier, 0, 16)) return 1;
  for (i = 0; i < 16; ++i) {
    if (pthread_create(&threads[i], 0, Worker, (void *)i)) return 2;
  }
  for (i = 0; i < 16; ++i) {
    if (pthread_join(threads[i], &result) || result) return 3;
  }
  if (pthread_barrier_destroy(&barrier)) return 4;
#endif
  return 0;
}
