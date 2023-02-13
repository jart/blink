// checks if struct timezone is specified
#include <sys/time.h>

int main(int argc, char *argv[]) {
  struct timezone tz;
  tz.tz_minuteswest = 1;
  tz.tz_dsttime = 1;
  (void)tz;
  return 0;
}
