// checks that realpath() won't break build
#include <limits.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  char path[PATH_MAX];
  if (!realpath("/tmp", path)) return 1;
  return 0;
}
