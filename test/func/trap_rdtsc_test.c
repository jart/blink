// test kernel lets us trap rdtsc instruction
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <signal.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <ucontext.h>
#include <unistd.h>

#define rdtsc()                                                  \
  ({                                                             \
    uint64_t rax_, rdx_;                                         \
    asm volatile("rdtsc" : "=a"(rax_), "=d"(rdx_) : : "memory"); \
    rdx_ << 32 | rax_;                                           \
  })

static void OnSigSegv(int sig, siginfo_t *si, void *vctx) {
  ucontext_t *ctx = (ucontext_t *)vctx;
  const char *code = (const char *)ctx->uc_mcontext.gregs[REG_RIP];
  if (code[0] == 0x0f && code[1] == 0x31) {
    ctx->uc_mcontext.gregs[REG_RDX] = 3;
    ctx->uc_mcontext.gregs[REG_RAX] = 0x13370000;
    ctx->uc_mcontext.gregs[REG_RIP] += 2;
  } else {
    _exit(20);
  }
}

int main(int argc, char *argv[]) {
  uint64_t x, y;
  if (prctl(PR_SET_TSC, PR_TSC_ENABLE)) return 1;
  x = rdtsc();
  y = rdtsc();
  if (x == y) return 2;
  if (y < x) return 3;
  struct sigaction sa = {.sa_sigaction = OnSigSegv};
  if (sigaction(SIGSEGV, &sa, 0)) return 4;
  if (prctl(PR_SET_TSC, PR_TSC_SIGSEGV)) return 5;
  if (rdtsc() != 0x313370000) return 6;
  if (rdtsc() != 0x313370000) return 7;
  return 0;
}
