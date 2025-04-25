#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include "../blink/flock.h"

int main(int argc, char *argv[]) {
  if (argc != 3) return 1;
  if (strcmp(argv[1], "-x")) return 2;
  if (flock(atoi(argv[2]), LOCK_EX)) return 3;
  return 0;
}
