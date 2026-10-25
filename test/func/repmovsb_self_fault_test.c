#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>

static void fault(int sig) { _exit(sig == SIGSEGV ? 0 : 1); }

int main(void) {
  struct sigaction sa = {0};
  long n = sysconf(_SC_PAGESIZE);
  void *p = mmap(0, n, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  void *src = p, *dst = p;
  unsigned long count = 0;
  if (p == MAP_FAILED) return 2;
  // A zero-count operation must not touch even an inaccessible page.
  __asm__ volatile("cld; rep movsb" : "+S"(src), "+D"(dst), "+c"(count) : : "memory");
  sa.sa_handler = fault;
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGSEGV, &sa, 0)) return 3;
  count = 1;
  __asm__ volatile("rep movsb" : "+S"(src), "+D"(dst), "+c"(count) : : "memory");
  return 4;
}
