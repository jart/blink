// tests for RtlGenRandom()
#include <stdbool.h>
#include <w32api/_mingw.h>

bool __stdcall SystemFunction036(void *, __LONG32);

int main(int argc, char *argv[]) {
  long x = 0;
  if (!SystemFunction036(&x, sizeof(x))) return 1;
  if (!x) return 2;
  return 0;
}
