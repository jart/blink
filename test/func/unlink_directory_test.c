#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
int main(void) {
  char path[] = "/tmp/blink-unlink-dir-XXXXXX";
  struct stat st;
  if (!mkdtemp(path)) return 1;
  errno = 0;
  if (unlink(path) != -1 || errno != EISDIR) return 2;
  int d = open(path, O_RDONLY | O_DIRECTORY);
  if (d < 0) return 3;
  if (mkdirat(d, "child", 0700)) return 4;
  errno = 0;
  if (unlinkat(d, "child", 0) != -1 || errno != EISDIR) return 5;
  if (symlinkat("child", d, "link")) return 6;
  if (unlinkat(d, "link", 0)) return 7;
  if (fstatat(d, "child", &st, 0) || !S_ISDIR(st.st_mode)) return 8;
  if (unlinkat(d, "child", AT_REMOVEDIR)) return 9;
  int fd = openat(d, "file", O_CREAT | O_WRONLY, 0600);
  if (fd < 0) return 10;
  close(fd);
  if (unlinkat(d, "file", 0)) return 11;
  errno = 0;
  if (unlinkat(d, "file", 0) != -1 || errno != ENOENT) return 12;
  close(d);
  return rmdir(path) ? 13 : 0;
}
