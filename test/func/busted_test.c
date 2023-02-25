// 1. test SIGBUS is delivered to guest correctly
// 2. test `blink -m` can reverse map a host addr
#include <signal.h>
#include <stdlib.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <unistd.h>

char *map;
long pagesz;

void OnBusted(int sig, siginfo_t *si, void *vctx) {
  if (sig != SIGBUS) _exit(8);
  if (si->si_signo != SIGBUS) _exit(9);
  if (si->si_code != BUS_ADRERR) _exit(10);
  if (si->si_addr != map + pagesz) _exit(11);
  _exit(0);
}

int main(int argc, char *argv[]) {
  char path[] = "/tmp/blink.test.XXXXXX";
  if (mkstemp(path) != 3) return 1;
  if (unlink(path)) return 2;
  if ((pagesz = getauxval(AT_PAGESZ)) == -1) return 4;
  map = (char *)mmap(0, pagesz * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE, 3, 0);
  if (map == MAP_FAILED) return 5;
  // ensure first page in file exists
  if (ftruncate(3, 1)) return 3;
  map[0] = 1;  // trigger page fault that loads the first page into memory
  map[1] = 2;  // modifying memory past file eof is ok if it's within page
  struct sigaction sa = {.sa_sigaction = OnBusted, .sa_flags = SA_SIGINFO};
  if (sigaction(SIGBUS, &sa, 0)) return 4;
  map[pagesz] = 1;  // trigger page fault to load second page of file
  asm("int3");      // it triggers SIGBUS immediately instead of this
  return 7;
}
