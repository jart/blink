#include <stdint.h>
#include <sys/auxv.h>
#include <sys/syscall.h>
#include <unistd.h>

/*
 * In non-linear mode Blink advertises 4096-byte guest pages, including on
 * hosts with larger pages. Growing the heap by one guest page must not
 * round the returned break up to the host page size. glibc malloc relies
 * on the amount of heap obtained matching its accounting.
 *
 * Reproduces on macOS ARM64 with blink -m and blink -jm. No allocator
 * operations are performed while the program break is temporarily changed.
 */
int main(void) {
  long pagesize;
  intptr_t initial, wanted, actual;
  pagesize = getauxval(AT_PAGESZ);
  if (pagesize <= 0) return 1;
  initial = syscall(SYS_brk, 0);
  wanted = initial + pagesize;
  actual = syscall(SYS_brk, wanted);
  syscall(SYS_brk, initial);
  if (actual != wanted) return 2;
  return 0;
}
