#define _GNU_SOURCE
#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

/* Private anonymous cache growth, including relocation and lazy pages. */
int main(void) {
  size_t i, page = sysconf(_SC_PAGESIZE);
  unsigned char *p, *q, *block;
  p = mmap(0, 4 * page, PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (p == MAP_FAILED) return 1;
  /* Leave two lazy pages in the source; force relocation with a live guard. */
  if (munmap(p + 3 * page, page)) return 2;
  block = mmap(p + 3 * page, page, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  if (block != p + 3 * page) return 3;
  block[0] = 99;
  for (i = 0; i < page; ++i) p[i] = (i % 251) + 1;
  errno = 0;
  if (mremap(p, 3 * page, 5 * page, 0) != MAP_FAILED || errno != ENOMEM)
    return 4;
  q = mremap(p, 3 * page, 5 * page, MREMAP_MAYMOVE);
  if (q == MAP_FAILED || q == p) return 5;
  for (i = 0; i < page; ++i) if (q[i] != (i % 251) + 1) return 6;
  for (i = page; i < 5 * page; ++i) if (q[i]) return 7;
  if (block[0] != 99) return 8;
  if (mremap(q, 5 * page, page, 0) != q) return 9;
  if (mremap(q, page, 2 * page, 0) != q) return 10;
  for (i = page; i < 2 * page; ++i) if (q[i]) return 11;
  errno = 0;
  if (mremap(q + 1, page, 2 * page, 0) != MAP_FAILED || errno != EINVAL)
    return 12;
  errno = 0;
  if (mremap(q, 2 * page, 0, 0) != MAP_FAILED || errno != EINVAL) return 13;
  if (mremap(q, 2 * page, 2 * page, 0) != q) return 14;
  if (mprotect(q, 2 * page, PROT_READ)) return 15;
  p = mremap(q, 2 * page, 3 * page, MREMAP_MAYMOVE);
  if (p == MAP_FAILED || p[0] != 1 || p[2 * page]) return 16;
  if (mprotect(p, 3 * page, PROT_READ | PROT_WRITE)) return 17;
  p[2 * page] = 42;
  if (munmap(p, 3 * page) || munmap(block, page)) return 18;
  /* Rejected shared mappings must remain intact. */
  p = mmap(0, page, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (p == MAP_FAILED) return 19;
  p[0] = 73;
  if (mremap(p, page, 2 * page, MREMAP_MAYMOVE) != MAP_FAILED || p[0] != 73)
    return 20;
  if (munmap(p, page)) return 21;
  return 0;
}
