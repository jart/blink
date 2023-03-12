// checks for fexecve
#include <unistd.h>

int main(int argc, char *argv[]) {
  char *const args[] = {0};
  char *const envs[] = {0};
  fexecve(0, args, envs);
  return 0;
}
