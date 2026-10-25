#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
  char dir[] = "/tmp/blink-utimens-XXXXXX", file[128], link[128];
  struct timespec first[2] = {{1234567890, 0}, {1234567891, 0}};
  struct timespec second[2] = {{1234567892, 0}, {1234567893, 0}};
  struct stat st;
  int fd, rc = 0;
  if (!mkdtemp(dir)) return 1;
  snprintf(file, sizeof(file), "%s/file", dir);
  snprintf(link, sizeof(link), "%s/link", dir);
  if ((fd = open(file, O_CREAT | O_WRONLY, 0600)) == -1) { rc = 2; goto done; }
  close(fd);
  if (utimensat(AT_FDCWD, file, first, AT_SYMLINK_NOFOLLOW)) { rc = 3; goto done; }
  if (stat(file, &st) || st.st_mtime != first[1].tv_sec) { rc = 4; goto done; }
  if (symlink("file", link)) { rc = 5; goto done; }
  if (utimensat(AT_FDCWD, link, second, AT_SYMLINK_NOFOLLOW)) { rc = 6; goto done; }
  if (lstat(link, &st) || st.st_mtime != second[1].tv_sec) { rc = 7; goto done; }
  if (stat(file, &st) || st.st_mtime != first[1].tv_sec) { rc = 8; goto done; }
done:
  unlink(link);
  unlink(file);
  rmdir(dir);
  return rc;
}
