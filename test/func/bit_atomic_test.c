#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
static uint64_t word;
static int width, toggle;
static pthread_barrier_t barrier;
static void* bits(void* unused) {
  for (int pass = 0; pass < 6; ++pass) {
    pthread_barrier_wait(&barrier);
    for (int i = 0; i < 10000; ++i) {
#define BITS(SUFFIX, T)                                                     \
  do {                                                                      \
    if (toggle)                                                             \
      __asm__ volatile("lock btc" SUFFIX " $0,%0; lock btc" SUFFIX " $0,%0" \
                       : "+m"(*(T*)&word)                                   \
                       :                                                    \
                       : "cc", "memory");                                   \
    else                                                                    \
      __asm__ volatile("lock bts" SUFFIX " $0,%0; lock btr" SUFFIX " $0,%0" \
                       : "+m"(*(T*)&word)                                   \
                       :                                                    \
                       : "cc", "memory");                                   \
  } while (0)
      if (width == 2)
        BITS("w", uint16_t);
      else if (width == 4)
        BITS("l", uint32_t);
      else
        BITS("q", uint64_t);
#undef BITS
    }
    pthread_barrier_wait(&barrier);
  }
  return 0;
}
static void* adds(void* unused) {
  for (int pass = 0; pass < 6; ++pass) {
    pthread_barrier_wait(&barrier);
    for (int i = 0; i < 10000; ++i) {
      if (width == 2)
        __asm__ volatile("lock addw $2,%0"
                         : "+m"(*(uint16_t*)&word)
                         :
                         : "cc", "memory");
      else if (width == 4)
        __asm__ volatile("lock addl $2,%0"
                         : "+m"(*(uint32_t*)&word)
                         :
                         : "cc", "memory");
      else
        __asm__ volatile("lock addq $2,%0" : "+m"(word) : : "cc", "memory");
    }
    pthread_barrier_wait(&barrier);
  }
  return 0;
}
int main(void) {
  pthread_t a, b;
  if (pthread_barrier_init(&barrier, 0, 3)) return 1;
  if (pthread_create(&a, 0, bits, 0) || pthread_create(&b, 0, adds, 0))
    return 2;
  for (width = 2; width <= 8; width *= 2) {
    for (toggle = 0; toggle < 2; ++toggle) {
      word = 0;
      pthread_barrier_wait(&barrier);
      pthread_barrier_wait(&barrier);
      if (word != 20000) {
        printf("width=%d toggle=%d expected=20000 actual=%llu\n", width, toggle,
               (unsigned long long)word);
        return 3;
      }
    }
  }
  pthread_join(a, 0);
  pthread_join(b, 0);
  pthread_barrier_destroy(&barrier);
  return 0;
}
