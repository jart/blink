// tests for mkfifo() support
// e.g. not provided by cosmopolitan libc
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  char path[128];
  snprintf(path, 128, "/tmp/blink.config.%d", getpid());
  if (mkfifo(path, 0644)) return 2;
  if (unlink(path)) return 3;
  return 0;
}
