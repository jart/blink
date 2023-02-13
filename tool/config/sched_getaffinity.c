// checks for sched_getaffinity()
#include <sched.h>

int main(int argc, char *argv[]) {
  int rc;
  cpu_set_t cs;
  CPU_ZERO(&cs);
  rc = sched_getaffinity(0, sizeof(cs), &cs);
  if (rc == -1) return 1;         // system call failed
  if (rc) return 2;               // libc wrapper broken
  if (!CPU_COUNT(&cs)) return 3;  // system call broken
  return 0;
}
