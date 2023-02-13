// checks for MAP_ANONYMOUS feature
#include <sys/mman.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  void *p;
  p = mmap(0, sysconf(_SC_PAGESIZE), PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (p == MAP_FAILED) return 1;
  return 0;
}
