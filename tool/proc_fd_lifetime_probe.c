#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Run in an isolated guest root. Paths must follow rename and deletion. */
int main(void) {
  char path[64], buf[1024];
  int fd;
  ssize_t n;
  if (mkdir("/before", 0700)) return 1;
  if ((fd = open("/before", O_PATH)) == -1) return 2;
  snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
  if (rename("/before", "/after")) return 3;
  n = readlink(path, buf, sizeof(buf));
  if (n != 6 || memcmp(buf, "/after", 6)) return 4;
  printf("rename: %.*s\n", (int)n, buf);
  if (rmdir("/after")) return 5;
  n = readlink(path, buf, sizeof(buf));
  if (n != 16 || memcmp(buf, "/after (deleted)", 16)) return 6;
  printf("unlink: %.*s\n", (int)n, buf);
  close(fd);
  return 0;
}
