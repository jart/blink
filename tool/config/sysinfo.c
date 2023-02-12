// tests for linux sysinfo() system call
#include <sys/sysinfo.h>

int main(int argc, char *argv[]) {
  struct sysinfo si;
  if (sysinfo(&si)) return 1;
  return 0;
}
