// test kernel sys_select(timeout) is always modified
// the linux kernel even appears to modify it on errors like einval!
#include <sys/select.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int rc;
  int pfds[2];
  fd_set rfds;
  struct timeval tv = {0, 10 * 1000};
  if (pipe(pfds)) return 1;
  FD_ZERO(&rfds);
  FD_SET(pfds[0], &rfds);
  if (syscall(SYS_select, pfds[0] + 1, &rfds, 0, 0, &tv)) return 2;
  if (tv.tv_sec) return 3;
  if (tv.tv_usec) return 4;
  return 0;
}
