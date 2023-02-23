// test kernel lets us trap cpuid instruction
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <asm/prctl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#define ARCH_GET_CPUID_ 0x1011
#define ARCH_SET_CPUID_ 0x1012

static void OnSigSegv(int sig, siginfo_t *si, void *vctx) {
  ucontext_t *ctx = (ucontext_t *)vctx;
  const char *code = (const char *)ctx->uc_mcontext.gregs[REG_RIP];
  if (code[0] == 0x0F && code[1] == (char)0xA2) {
    // we successfully trapped the cpuid instruction
    ctx->uc_mcontext.gregs[REG_RAX] = 0x13370001;
    ctx->uc_mcontext.gregs[REG_RBX] = 0x13370002;
    ctx->uc_mcontext.gregs[REG_RCX] = 0x13370003;
    ctx->uc_mcontext.gregs[REG_RDX] = 0x13370004;
    ctx->uc_mcontext.gregs[REG_RIP] += 2;
  } else {
    _exit(20);
  }
}

int main(int argc, char *argv[]) {
  int ax, bx, cx, dx;
  ax = cx = 0;
  asm volatile("cpuid" : "+a"(ax), "=b"(bx), "+c"(cx), "=d"(dx));
  if (syscall(SYS_arch_prctl, ARCH_GET_CPUID_) != 1) return 1;
  if (syscall(SYS_arch_prctl, ARCH_SET_CPUID_, 0) != 0) return 2;
  if (syscall(SYS_arch_prctl, ARCH_GET_CPUID_) != 0) return 3;
  struct sigaction sa = {.sa_sigaction = OnSigSegv};
  if (sigaction(SIGSEGV, &sa, 0)) return 4;
  ax = cx = 0;
  asm volatile("cpuid" : "+a"(ax), "=b"(bx), "+c"(cx), "=d"(dx));
  if (ax != 0x13370001) return 5;
  if (bx != 0x13370002) return 6;
  if (cx != 0x13370003) return 7;
  if (dx != 0x13370004) return 8;
  return 0;
}
