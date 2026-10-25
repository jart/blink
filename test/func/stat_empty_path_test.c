#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
  int fd;
  struct stat st;
  if (stat("", &st) != -1 || errno != ENOENT) return 1;
  if (lstat("", &st) != -1 || errno != ENOENT) return 2;
  if ((fd = open("/", O_RDONLY | O_DIRECTORY)) == -1) return 3;
  if (fstatat(fd, "", &st, 0) != -1 || errno != ENOENT) return 4;
  if (fstat(fd, &st) || !S_ISDIR(st.st_mode)) return 5;
  if (fstatat(fd, "", &st, AT_EMPTY_PATH) || !S_ISDIR(st.st_mode)) return 6;
  close(fd);
  return 0;
}
