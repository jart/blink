// checks for strchrnul() support
#include <string.h>

int main(int argc, char *argv[]) {
  char s[] = "abc";
  if (strchrnul(s, 'a') != s) return 1;
  if (strchrnul(s, 'b') != s + 1) return 2;
  if (strchrnul(s, 'c') != s + 2) return 3;
  if (strchrnul(s, 'd') != s + 3) return 4;
  if (strchrnul(s, 'e') != s + 3) return 5;
  return 0;
}
