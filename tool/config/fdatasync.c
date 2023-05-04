// checks for fdatasync() system call
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  char path[128];
  int fd, rc;
  snprintf(path, 128, "/tmp/blink.config.%d", getpid());
  fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0600);
  if (fd == -1) return 1;
  rc = fdatasync(fd);
  if (rc == -1) return 2;
  if (rc) return 3;
  close(fd);
  if (unlink(path)) return 4;
  return 0;
}
