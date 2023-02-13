// checks that setreuid() won't break the build
#include <unistd.h>

int main(int argc, char *argv[]) {
  int rc = setreuid(-1, -1);  // noop by definition
  if (rc == -1) return 1;     // failed
  if (rc) return 2;           // broken
  return 0;
}
