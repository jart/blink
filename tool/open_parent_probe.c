#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static int same(int a, int b) {
  struct stat x, y;
  return !fstat(a, &x) && !fstat(b, &y) &&
         x.st_dev == y.st_dev && x.st_ino == y.st_ino;
}

int main(void) {
  char path[] = "/tmp/open-parent-XXXXXX";
  int root, a, b, up, top, dot, dotup;
  if (!mkdtemp(path)) return 1;
  root = open(path, O_RDONLY | O_DIRECTORY);
  if (root < 0 || mkdirat(root, "a", 0700)) return 2;
  a = openat(root, "a", O_RDONLY | O_DIRECTORY);
  if (a < 0 || mkdirat(a, "b", 0700)) return 3;
  b = openat(a, "b", O_RDONLY | O_DIRECTORY);
  up = openat(b, "..", O_RDONLY | O_DIRECTORY);
  if (up < 0 || !same(up, a)) return 4;
  if (unlinkat(a, "b", AT_REMOVEDIR)) return 5;
  top = openat(up, "..", O_RDONLY | O_DIRECTORY);
  if (top < 0 || !same(top, root)) return 6;
  dot = openat(a, ".", O_RDONLY | O_DIRECTORY);
  dotup = openat(dot, "..", O_RDONLY | O_DIRECTORY);
  if (dot < 0 || dotup < 0 || !same(dotup, root)) return 7;
  close(dotup);
  close(dot);
  close(top);
  close(up);
  close(b);
  close(a);
  if (unlinkat(root, "a", AT_REMOVEDIR)) return 8;
  close(root);
  if (rmdir(path)) return 9;
  puts("directory parent traversal passed");
  return 0;
}
