// tests for syscall(SYS_getrandom)
#include <sys/syscall.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  long x = 0;
  if (syscall(SYS_getrandom, &x, sizeof(x), 0) != sizeof(x)) return 1;
  if (!x) return 2;
  return 0;
}
