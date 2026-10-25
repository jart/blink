#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static void OnFpe(int sig) {
  _exit(sig == SIGFPE ? 0 : 2);
}

static int CheckTrap(int invalid) {
  int status;
  pid_t pid = fork();
  if (pid == -1) return 1;
  if (!pid) {
    uint64_t input = invalid ? UINT64_C(0x7ff0000000000001)
                             : UINT64_C(0x3fe0000000000000);
    unsigned csr = 0x1f80 & ~(invalid ? 0x80 : 0x1000);
    signal(SIGFPE, OnFpe);
    asm volatile("ldmxcsr %0\n\tmovq %1,%%xmm0\n\troundsd $0,%%xmm0,%%xmm1"
                 : : "m"(csr), "m"(input) : "xmm0", "xmm1", "memory");
    _exit(1);
  }
  if (waitpid(pid, &status, 0) != pid) return 2;
  return !WIFEXITED(status) || WEXITSTATUS(status);
}

int main(void) {
  uint64_t input = UINT64_C(0x3fe0000000000000), output;
  unsigned saved, csr = 0x0f80, status;
  long pagesize = sysconf(_SC_PAGESIZE);
  char *pages;
  if (CheckTrap(1)) return 1;
  if (CheckTrap(0)) return 2;
  asm volatile("stmxcsr %0" : "=m"(saved));
  // Suppression must override an unmasked precision exception.
  asm volatile("ldmxcsr %2\n\tmovq %3,%%xmm0\n\troundsd $8,%%xmm0,%%xmm1\n\t"
               "movq %%xmm1,%0\n\tstmxcsr %1"
               : "=m"(output), "=m"(status)
               : "m"(csr), "m"(input) : "xmm0", "xmm1", "memory");
  if (output || status != csr) return 3;
  csr = 0x1fa0;
  input = UINT64_C(0x3ff0000000000000);
  asm volatile("ldmxcsr %1\n\troundsd $0,%2,%%xmm0\n\tstmxcsr %0"
               : "=m"(status) : "m"(csr), "m"(input) : "xmm0", "memory");
  if (status != csr) return 4;  // Existing precision status is sticky.
  pages = mmap(0, pagesize * 2, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (pages == MAP_FAILED) return 5;
  if (mprotect(pages + pagesize, pagesize, PROT_NONE)) return 6;
  *(uint64_t *)(pages + pagesize - 8) = UINT64_C(0x3ff8000000000000);
  asm volatile("roundsd $8,%1,%%xmm0\n\tmovq %%xmm0,%0"
               : "=m"(output) : "m"(*(uint64_t *)(pages + pagesize - 8))
               : "xmm0", "memory");
  if (output != UINT64_C(0x4000000000000000)) return 7;
  *(uint32_t *)(pages + pagesize - 4) = 0x3fc00000;
  asm volatile("roundss $8,%1,%%xmm0\n\tmovd %%xmm0,%0"
               : "=m"(status) : "m"(*(uint32_t *)(pages + pagesize - 4))
               : "xmm0", "memory");
  if (status != 0x40000000) return 8;
  munmap(pages, pagesize * 2);
  asm volatile("ldmxcsr %0" : : "m"(saved));
  return 0;
}
