#define _GNU_SOURCE
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>

#ifdef __x86_64__
static uint64_t expected[2];
static void OnFpe(int sig, siginfo_t *info, void *context) {
  ucontext_t *uc = context;
  if (sig != SIGFPE || !uc->uc_mcontext.fpregs) _exit(2);
  if (memcmp(&uc->uc_mcontext.fpregs->_xmm[0], expected, 16)) _exit(3);
  _exit(0);
}

static int Check(unsigned mask, uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
  int status;
  pid_t pid = fork();
  if (pid < 0) return 1;
  if (!pid) {
    uint64_t right[2] = {b, d};
    unsigned control = 0x1f80 & ~mask;
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = OnFpe;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGFPE, &action, 0)) _exit(4);
    expected[0] = a;
    expected[1] = c;
    __asm__ volatile("ldmxcsr %0\n\tmovdqu %1,%%xmm0\n\tmovdqu %2,%%xmm1\n\t"
                     "dppd $51,%%xmm1,%%xmm0"
                     : : "m"(control), "m"(expected), "m"(right)
                     : "xmm0", "xmm1", "memory");
    _exit(5);
  }
  if (waitpid(pid, &status, 0) != pid) return 2;
  return !WIFEXITED(status) || WEXITSTATUS(status);
}
#endif

int main(void) {
#ifdef __x86_64__
  uint64_t one = UINT64_C(0x3ff0000000000000);
  if (Check(0x80, UINT64_C(0x7ff0000000000001), one, 0, 0)) return 1;
  if (Check(0x100, 1, one, 0, 0)) return 2;
  if (Check(0x400, UINT64_C(0x7fefffffffffffff), UINT64_C(0x4000000000000000), 0, 0)) return 3;
  if (Check(0x800, UINT64_C(0x0010000000000000), UINT64_C(0x3fb999999999999a), 0, 0)) return 4;
  if (Check(0x1000, one, one, UINT64_C(0x3ca0000000000000), one)) return 5;
#endif
  return 0;
}
