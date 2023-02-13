// checks that -lm won't break the build
#include <math.h>

double three(void) {
  return 3;
}

double (*f)(void) = three;

int main(int argc, char *argv[]) {
  if (pow(f(), f()) != 27.) return 1;
  return 0;
}
