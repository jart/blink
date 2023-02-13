// checks for memccpy() xsi extension
#include <string.h>

int main(int argc, char *argv[]) {
  char buf[16];
  if (memccpy(buf, "hi", '\0', sizeof(buf)) != buf + 3) return 1;
  if (strcmp(buf, "hi")) return 2;
  return 0;
}
