// checks for wcwidth() support
#include <wchar.h>

int main(int argc, char *argv[]) {
  if (wcwidth(0) != 0) return 1;
  if (wcwidth(32) != 1) return 2;
  if (wcwidth(0x672c) != 2) return 3;
  return 0;
}
