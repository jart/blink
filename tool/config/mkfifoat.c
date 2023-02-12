// tests for mkfifoat() support
// not present on MacOS until 13+ c. 2022
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  char path[] = "/tmp/blXXXXXX";
  if (!mkstemp(path)) return 1;
  if (mkfifoat(AT_FDCWD, path, 0644)) return 2;
  if (unlink(path)) return 3;
  return 0;
}
