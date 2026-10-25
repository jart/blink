// Opt-in Linux guest probe for Blink's VFS /proc/stat CPU accounting.
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
  FILE *fp;
  char line[1024], name[32], prefix[4];
  unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
  unsigned cpu, count = 0;
  int fd;
  ssize_t n;
  if (!(fp = fopen("/proc/stat", "r"))) return 1;
  if (!fgets(line, sizeof(line), fp) || strncmp(line, "cpu ", 4)) return 2;
  while (fgets(line, sizeof(line), fp)) {
    if (strncmp(line, "cpu", 3)) break;
    if (sscanf(line, "%31s %llu %llu %llu %llu %llu %llu %llu %llu",
               name, &user, &nice, &system, &idle, &iowait, &irq, &softirq,
               &steal) != 9) return 3;
    if (sscanf(name, "cpu%u", &cpu) != 1 || cpu != count++) return 4;
  }
  if (ferror(fp) || !count) return 5;
  fclose(fp);
  if ((fd = open("/proc/stat", O_RDONLY)) == -1) return 6;
  // Small reads and pread must preserve the sequential file position.
  if (read(fd, prefix, 2) != 2 || memcmp(prefix, "cp", 2)) return 7;
  if (pread(fd, prefix, 4, 0) != 4 || memcmp(prefix, "cpu ", 4)) return 8;
  if (read(fd, prefix, 2) != 2 || memcmp(prefix, "u ", 2)) return 9;
  while ((n = read(fd, line, sizeof(line))) > 0) {}
  if (n < 0) return 13;
  if (read(fd, line, sizeof(line)) != 0) return 10;
  if (lseek(fd, 0, SEEK_SET) != 0) return 11;
  if (read(fd, prefix, 4) != 4 || memcmp(prefix, "cpu ", 4)) return 12;
  close(fd);
  printf("proc stat: %u CPUs; partial reads, pread, EOF and rewind passed\n", count);
  return 0;
}
