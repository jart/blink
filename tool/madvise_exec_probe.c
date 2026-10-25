#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int main(void) {
  long n = sysconf(_SC_PAGESIZE), i;
  unsigned char first[] = {0xb8, 7, 0, 0, 0, 0xc3};
  unsigned char second[] = {0xb8, 11, 0, 0, 0, 0xc3};
  unsigned char *p = mmap(0, n, PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  int (*run)(void) = (int (*)(void))p;
  if (p == MAP_FAILED) return 1;
  memcpy(p, first, sizeof(first));
  for (i = 0; i < 1000; ++i) if (run() != 7) return 2;
  if (madvise(p, n, MADV_DONTNEED)) return 3;
  for (i = 0; i < n; ++i) if (p[i]) return 4;
  memcpy(p, second, sizeof(second));
  for (i = 0; i < 1000; ++i) if (run() != 11) return 5;
  return munmap(p, n) ? 6 : 0;
}
