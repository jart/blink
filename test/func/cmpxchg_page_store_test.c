/* A JIT CMPXCHG crossing guest pages must commit its writable reservation. */
#define _GNU_SOURCE
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static __attribute__((noinline)) uint32_t Change(void *p, uint32_t expected,
                                                uint32_t replacement,
                                                int locked) {
  if (locked) {
    __asm__ volatile("lock cmpxchgl %2, (%1)"
                     : "+a"(expected)
                     : "r"(p), "r"(replacement)
                     : "cc", "memory");
  } else {
    __asm__ volatile("cmpxchgl %2, (%1)"
                     : "+a"(expected)
                     : "r"(p), "r"(replacement)
                     : "cc", "memory");
  }
  return expected;
}

int main(void) {
  const uint32_t value = UINT32_C(0x12345678);
  long page = sysconf(_SC_PAGESIZE);
  unsigned char *p = mmap(0, page * 2, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (p == MAP_FAILED) return 1;
  for (int locked = 0; locked < 2; ++locked) {
    for (int i = 0; i < 100; ++i) {
      uint32_t actual;
      memset(p + page - 2, 0, 4);
      if (Change(p + page - 2, 0, value, locked) != 0) return 2;
      memcpy(&actual, p + page - 2, 4);
      if (actual != value) return 3;
      if (Change(p + page - 2, 1, 2, locked) != value) return 4;
      memcpy(&actual, p + page - 2, 4);
      if (actual != value) return 5;
    }
  }
  return munmap(p, page * 2) ? 6 : 0;
}
