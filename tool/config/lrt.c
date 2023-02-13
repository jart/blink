// checks that -lrt won't break the build
#include <time.h>

int main(int argc, char *argv[]) {
  struct timespec ts;
  if (clock_getres(CLOCK_REALTIME, &ts)) return 1;
  if (clock_gettime(CLOCK_REALTIME, &ts)) return 1;
  return 0;
}
