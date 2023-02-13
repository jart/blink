// tests for mkfifoat() support
// not present on MacOS until 13+ c. 2022
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  char path[128];
  snprintf(path, 128, "/tmp/blink.config.%d", getpid());
  if (mkfifoat(AT_FDCWD, path, 0644)) return 2;
  if (unlink(path)) return 3;
  return 0;
}
