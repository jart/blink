// tests memory can't be executed without exec permission
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef void func_t(void);

const unsigned char kCode[] = {
    0xc3,  // ret
};

void *p;

void OnSigSegv(int sig, siginfo_t *si, void *vctx) {
  if (si->si_addr != p) _exit(5);
  if (si->si_code != SEGV_ACCERR) _exit(6);
  if ((void *)((ucontext_t *)vctx)->uc_mcontext.gregs[REG_RIP] != p) _exit(7);
  _exit(0);
}

int main(int argc, char *argv[]) {
  func_t *f;
  p = mmap(0, 65536, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
  if (p == MAP_FAILED) return 2;
  f = (func_t *)p;
  memcpy(f, kCode, sizeof(kCode));
  struct sigaction sa = {.sa_sigaction = OnSigSegv, .sa_flags = SA_SIGINFO};
  if (sigaction(SIGSEGV, &sa, 0)) return 3;
  f();
  return 4;
}
