// check for sync() xsi extension
#include <unistd.h>

void DontCallMeBro(void) {
  sync();
}

int main(int argc, char *argv[]) {
  return 0;
}
