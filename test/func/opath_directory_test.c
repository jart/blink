#define _GNU_SOURCE
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
  char path[] = "/tmp/blink-opath-dir-XXXXXX";
  struct stat st;
  int fd, child;
  if (!mkdtemp(path)) return 1;
  if ((fd = open(path, O_PATH | O_NOFOLLOW | O_CLOEXEC)) < 0) return 2;
  if (fstat(fd, &st) || !S_ISDIR(st.st_mode)) return 3;
  if (!(fcntl(fd, F_GETFL) & O_PATH) || !(fcntl(fd, F_GETFD) & FD_CLOEXEC)) return 4;
  if ((child = openat(fd, "child", O_CREAT | O_WRONLY, 0600)) < 0) return 5;
  close(child);
  if (fstatat(fd, "child", &st, 0) || !S_ISREG(st.st_mode)) return 6;
  if (unlinkat(fd, "child", 0)) return 7;
  close(fd);
  return rmdir(path) ? 8 : 0;
}
