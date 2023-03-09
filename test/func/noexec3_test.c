// test munmap() prevents code from being executed
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef int math_f(int, int);

const unsigned char kAdd[] = {
    0x89, 0xf8,  // mov %edi,%eax
    0x01, 0xf0,  // add %esi,%eax
    0xc3,        // ret
};

void *p;

void OnSigSegv(int sig, siginfo_t *si, void *vctx) {
  if (si->si_addr != p) _exit(8);
  if (si->si_code != SEGV_MAPERR) _exit(9);
  _exit(0);
}

int main(int argc, char *argv[]) {
  int i;
  math_f *f;
  p = mmap(0, 65536, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
  if (p == MAP_FAILED) return 1;
  f = (math_f *)p;
  memcpy(p, kAdd, sizeof(kAdd));
  if (mprotect(p, 65536, PROT_READ | PROT_EXEC)) return 2;
  if (f(20, 3) != 23) return 3;
  if (f(20, 3) != 23) return 4;
  struct sigaction sa = {.sa_sigaction = OnSigSegv, .sa_flags = SA_SIGINFO};
  if (sigaction(SIGSEGV, &sa, 0)) return 5;
  if (munmap(p, 65536)) return 6;
  f(20, 3);
  return 7;
}
