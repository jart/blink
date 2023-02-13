// checks for sched_yield() support
#include <sched.h>

int main(int argc, char *argv[]) {
  if (sched_yield()) return 1;
  return 0;
}
