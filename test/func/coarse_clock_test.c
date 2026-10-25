#include <time.h>

static double Seconds(struct timespec t) {
  return t.tv_sec + t.tv_nsec / 1000000000.;
}

static int Check(clockid_t coarse, clockid_t regular) {
  struct timespec before, value, after, resolution, previous;
  int i;
  if (clock_getres(coarse, &resolution)) return 1;
  if (resolution.tv_sec < 0 || resolution.tv_nsec < 0 ||
      resolution.tv_nsec >= 1000000000 ||
      (!resolution.tv_sec && !resolution.tv_nsec)) return 2;
  if (clock_gettime(regular, &before) || clock_gettime(coarse, &value) ||
      clock_gettime(regular, &after)) return 3;
  if (value.tv_nsec < 0 || value.tv_nsec >= 1000000000) return 4;
  // Native coarse clocks may lag the high-resolution clock by a tick.
  if (Seconds(value) < Seconds(before) - 1 ||
      Seconds(value) > Seconds(after) + 1) return 5;
  if (regular == CLOCK_MONOTONIC) {
    previous = value;
    for (i = 0; i < 100; ++i) {
      if (clock_gettime(coarse, &value)) return 6;
      if (value.tv_sec < previous.tv_sec ||
          (value.tv_sec == previous.tv_sec && value.tv_nsec < previous.tv_nsec))
        return 7;
      previous = value;
    }
  }
  return 0;
}

int main(void) {
  int result;
  result = Check(5, CLOCK_REALTIME);
  if (result) return result;
  result = Check(6, CLOCK_MONOTONIC);
  return result ? result + 10 : 0;
}
