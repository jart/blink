// tests for preadv() and pwritev() support
// missing on MacOS (before 2020), Cygwin, Haiku, etc.
#include <stdio.h>
#include <sys/uio.h>

int main(int argc, char *argv[]) {
  FILE *f;
  char x, y;
  struct iovec vx, vy;

  if (!(f = tmpfile())) return 1;

  x = 123;
  vx.iov_base = &x;
  vx.iov_len = 1;
  if (pwritev(fileno(f), &vx, 1, 5) != 1) return 2;

  vy.iov_base = &y;
  vy.iov_len = 1;
  if (preadv(fileno(f), &vy, 1, 5) != 1) return 3;

  if (y != 123) return 4;

  fclose(f);

  return 0;
}
