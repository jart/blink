// tests for non-buggy sysctl(KERN_ARND)
// author: justine tunney <jtunney@gmail.com>
#include <sys/types.h>
// order sometimes matters
#include <string.h>
#include <sys/param.h>
#include <sys/sysctl.h>

/*
 * "On FreeBSD old implementations returned longs, newer versions
 * support variable sizes up to 256 byte. The code below would not work
 * properly when the sysctl returns long and we want to request
 * something not a multiple of longs, which should never be the case."
 * -Quoth the OpenSSL source code
 *
 * "On NetBSD before 4.0 KERN_ARND was an alias for KERN_URND, and only
 * filled in an int, leaving the rest uninitialized. Since NetBSD 4.0 it
 * returns a variable number of bytes with the current version
 * supporting up to 256 bytes." -Quoth the OpenSSL source code.
 */

/*
 * assuming the system is working correctly there's a 1 in
 * 235716896562095165800448 chance this check should flake
 */
#define BYTES 9
#define TRIES 16

int main(int argc, char *argv[]) {
  int haszero;
  int fails = 0;
  size_t i, j, m;
  unsigned char x[BYTES];
  int mib[2] = {CTL_KERN, KERN_ARND};
  for (i = 0; i < TRIES; ++i) {
    memset(x, 0, sizeof(x));
    m = sizeof(x);
    if (sysctl(mib, 2, &x, &m, 0, 0) == -1) return 1;
    if (m != sizeof(x)) return 2;
    for (haszero = j = 0; j < BYTES; ++j) {
      if (!x[j]) haszero = 1;
    }
    if (haszero) ++fails;
  }
  if (fails < TRIES) {
    return 0;
  } else {
    return 3;
  }
}
