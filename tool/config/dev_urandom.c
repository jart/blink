// tests for non-buggy sysctl(KERN_ARND)
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
#error posix /dev does not exist on windows.
#endif

/*
 * assuming the system is working correctly there's a 1 in
 * 235716896562095165800448 chance this check should flake
 */
#define BYTES 9
#define TRIES 16

static ssize_t GetRandom(unsigned char *p, size_t n) {
  int fd;
  ssize_t rc;
  if ((fd = open("/dev/urandom", O_RDONLY)) == -1) return -1;
  rc = read(fd, p, n);
  close(fd);
  return rc;
}

int main(int argc, char *argv[]) {
  size_t i, j;
  int haszero, fails = 0;
  unsigned char x[BYTES];
  for (i = 0; i < TRIES; ++i) {
    memset(x, 0, sizeof(x));
    if (GetRandom(x, sizeof(x)) != sizeof(x)) return 1;
    for (haszero = j = 0; j < BYTES; ++j) {
      if (!x[j]) haszero = 1;
    }
    if (haszero) ++fails;
  }
  if (fails < TRIES) {
    return 0;
  } else {
    return 2;
  }
}
