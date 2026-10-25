#define _GNU_SOURCE
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
int main(void) {
  char name[80], buf[128];
  int fd, other;
  if (mkdir("/fd-probe", 0700)) return 1;
  if ((fd = open("/fd-probe", O_PATH | O_CLOEXEC)) == -1) return 2;
  snprintf(name, sizeof(name), "/proc/self/fd/%d", fd);
  int n = readlink(name, buf, sizeof(buf));
  if (n != 9 || memcmp(buf, "/fd-probe", 9)) return 3;
  if (readlink(name, buf, 3) != 3 || memcmp(buf, "/fd", 3)) return 4;
  if ((other = dup(fd)) == -1) return 5;
  if (close(fd)) return 6;
  if (readlink(name, buf, sizeof(buf)) != -1 || errno != ENOENT) return 7;
  snprintf(name, sizeof(name), "/proc/self/fd/%d", other);
  if (readlink(name, buf, sizeof(buf)) != 9) return 8;
  strcat(name, "junk");
  if (readlink(name, buf, sizeof(buf)) != -1 || errno != ENOENT) return 9;
  DIR *dir = opendir("/proc/self/fd");
  struct dirent *entry;
  int found = 0;
  char number[32];
  snprintf(number, sizeof(number), "%d", other);
  if (!dir) return 10;
  errno = 0;
  while ((entry = readdir(dir))) {
    if (!strcmp(entry->d_name, number)) found = 1;
  }
  if (errno || !found) return 11;
  rewinddir(dir);
  if (!readdir(dir)) return 12;
  closedir(dir);
  close(other);
  rmdir("/fd-probe");
  return 0;
}
