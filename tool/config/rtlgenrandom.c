// tests for RtlGenRandom()
#include <stdbool.h>
#include <stdlib.h> // Required for __MINGW64_VERSION_MAJOR, not using __MINGW32__ since old MinGW32 also defines it. See https://github.com/cpredef/predef/blob/master/Compilers.md#mingw-and-mingw-w64.
#ifndef __MINGW64_VERSION_MAJOR
#include <w32api/_mingw.h>
#endif

bool __stdcall SystemFunction036(void *, __LONG32);

int main(int argc, char *argv[]) {
  long x = 0;
  if (!SystemFunction036(&x, sizeof(x))) return 1;
  if (!x) return 2;
  return 0;
}
