/* JIT MOVSD stores must reserve eight writable bytes, not sixteen readable ones. */
#define _GNU_SOURCE
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static __attribute__((noinline)) void CopyDouble(const void *src, void *dst) {
  __asm__ volatile("movsd (%0), %%xmm0\n\t"
                   "movsd %%xmm0, (%1)"
                   : : "r"(src), "r"(dst) : "xmm0", "memory");
}

static int Check(unsigned char *dst) {
  uint64_t expected = UINT64_C(0x41efffffffe00000), actual;
  for (int i = 0; i < 100; ++i) {
    memset(dst, 0, 8);
    CopyDouble(&expected, dst);
    memcpy(&actual, dst, 8);
    if (actual != expected) return 1;
  }
  return 0;
}

int main(void) {
  long page = sysconf(_SC_PAGESIZE);
  unsigned char *p = mmap(0, page * 2, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (p == MAP_FAILED) return 1;
  if (Check(p + page - 8)) return 2;  /* Ends exactly at the page boundary. */
  if (Check(p + page - 4)) return 3;  /* Actual store crosses the boundary. */
  if (mprotect(p + page, page, PROT_NONE)) return 4;
  if (Check(p + page - 8)) return 5;  /* Must not access the following page. */
  if (munmap(p, page * 2)) return 6;
  return 0;
}
