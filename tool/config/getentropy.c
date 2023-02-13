// checks for getentropy() with bsd style include
#include <unistd.h>

/*
 * assuming the system is working correctly there's a 1 in
 * 235716896562095165800448 chance this check should flake
 */
#define BYTES 9
#define TRIES 16

int main(int argc, char *argv[]) {
  int rc;
  size_t i, j;
  int haszero, fails = 0;
  for (i = 0; i < TRIES; ++i) {
    unsigned char x[BYTES] = {0};
    rc = getentropy(x, sizeof(x));
    if (rc == -1) return 1;
    if (rc) return 2;
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
