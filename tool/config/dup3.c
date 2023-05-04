// checks for dup3() system call
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  char path[128];
  snprintf(path, 128, "/tmp/blink.config.%d", getpid());
  if (open(path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0600) != 3) return 1;
  if (fcntl(3, F_GETFD) != FD_CLOEXEC) return 2;
  if (dup3(3, 4, 0) != 4) return 3;
  if (fcntl(4, F_GETFD) != 0) return 4;
  if (dup3(3, 4, O_CLOEXEC) != 4) return 5;
  if (fcntl(4, F_GETFD) != FD_CLOEXEC) return 6;
  if (unlink(path)) return 7;
  return 0;
}
