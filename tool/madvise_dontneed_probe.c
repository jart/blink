#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int main(void) {
  long n = sysconf(_SC_PAGESIZE);
  unsigned char *p = mmap(0, n * 3, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  long i;
  if (p == MAP_FAILED) return 1;
  memset(p, 0xa5, n * 3);
  if (madvise(p + n, 1, MADV_DONTNEED)) return 2;
  for (i = 0; i < n; ++i) {
    if (p[i] != 0xa5 || p[n+i] != 0 || p[2*n+i] != 0xa5) return 3;
  }
  memset(p + n, 0x5a, n);
  if (mprotect(p + n, n, PROT_NONE)) return 4;
  if (madvise(p + n, n, MADV_DONTNEED)) return 5;
  if (mprotect(p + n, n, PROT_READ)) return 6;
  for (i = 0; i < n; ++i) if (p[n+i]) return 7;
  errno = 0;
  if (madvise(p + 1, n, MADV_DONTNEED) != -1 || errno != EINVAL) return 8;
  if (munmap(p, n * 3)) return 9;
  errno = 0;
  if (madvise(p, n, MADV_DONTNEED) != -1 || errno != ENOMEM) return 10;
  p = mmap(0, n, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (p == MAP_FAILED) return 11;
  memset(p, 0x7b, n);
  // Unsupported shared mappings may be rejected, but must never be zeroed.
  if (madvise(p, n, MADV_DONTNEED) && errno != EINVAL) return 12;
  for (i = 0; i < n; ++i) if (p[i] != 0x7b) return 13;
  if (munmap(p, n)) return 14;
  return 0;
}
