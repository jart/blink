/* Reading a directory doesn't require opening it with O_DIRECTORY. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(void) {
  char data[4096];
  int fd = open("/tmp", O_RDONLY);
  if (fd == -1) return 1;
  if (syscall(SYS_getdents64, fd, data, sizeof(data)) <= 0) return 2;
  close(fd);
  fd = open("/dev/null", O_RDONLY);
  if (fd == -1) return 3;
  errno = 0;
  if (syscall(SYS_getdents64, fd, data, sizeof(data)) != -1 || errno != ENOTDIR)
    return 4;
  close(fd);
  return 0;
}
