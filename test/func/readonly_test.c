// test read-only memory protection works
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

void *p;

void OnSigSegv(int sig, siginfo_t *si, void *vctx) {
  if (si->si_addr != p) _exit(5);
  if (si->si_code != SEGV_ACCERR) _exit(6);
  _exit(0);
}

int main(int argc, char *argv[]) {
  p = mmap(0, 65536, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
  if (p == MAP_FAILED) return 2;
  struct sigaction sa = {.sa_sigaction = OnSigSegv, .sa_flags = SA_SIGINFO};
  if (sigaction(SIGSEGV, &sa, 0)) return 3;
  *(int *)p = 1;
  return 4;
}
