#ifndef BLINK_TIMESPEC_H_
#define BLINK_TIMESPEC_H_
#include <errno.h>
#include <time.h>

#include "blink/assert.h"
#include "blink/limits.h"

static inline struct timespec GetTime(void) {
  struct timespec ts;
  unassert(!clock_gettime(CLOCK_REALTIME, &ts));
  return ts;
}

static inline struct timespec GetMonotonic(void) {
  struct timespec ts;
  unassert(!clock_gettime(CLOCK_MONOTONIC, &ts));
  return ts;
}

static inline struct timespec GetMaxTime(void) {
  struct timespec ts;
  ts.tv_sec = NUMERIC_MAX(time_t);
  ts.tv_nsec = 999999999;
  return ts;
}

static inline struct timespec GetZeroTime(void) {
  struct timespec ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 0;
  return ts;
}

static inline struct timespec FromSeconds(time_t x) {
  struct timespec ts;
  ts.tv_sec = x;
  ts.tv_nsec = 0;
  return ts;
}

static inline struct timespec FromMilliseconds(time_t x) {
  struct timespec ts;
  ts.tv_sec = x / 1000;
  ts.tv_nsec = x % 1000 * 1000000;
  return ts;
}

static inline struct timespec FromMicroseconds(time_t x) {
  struct timespec ts;
  ts.tv_sec = x / 1000000;
  ts.tv_nsec = x % 1000000 * 1000;
  return ts;
}

static inline struct timespec FromNanoseconds(time_t x) {
  struct timespec ts;
  ts.tv_sec = x / 1000000000;
  ts.tv_nsec = x % 1000000000;
  return ts;
}

static inline struct timespec SleepTime(struct timespec dur) {
  struct timespec unslept;
  if (!nanosleep(&dur, &unslept)) {
    unslept.tv_sec = 0;
    unslept.tv_nsec = 0;
  } else {
    unassert(errno == EINTR);
  }
  return unslept;
}

static inline time_t ToSeconds(struct timespec ts) {
  unassert(ts.tv_nsec < 1000000000);
  if (ts.tv_sec < NUMERIC_MAX(time_t)) {
    if (ts.tv_nsec) {
      ts.tv_sec += 1;
    }
    return ts.tv_sec;
  } else {
    return NUMERIC_MAX(time_t);
  }
}

static inline time_t ToMilliseconds(struct timespec ts) {
  unassert(ts.tv_nsec < 1000000000);
  if (ts.tv_sec < NUMERIC_MAX(time_t) / 1000 - 999) {
    if (ts.tv_nsec <= 999000000) {
      ts.tv_nsec = (ts.tv_nsec + 999999) / 1000000;
    } else {
      ts.tv_sec += 1;
      ts.tv_nsec = 0;
    }
    return ts.tv_sec * 1000 + ts.tv_nsec;
  } else {
    return NUMERIC_MAX(time_t);
  }
}

static inline time_t ToMicroseconds(struct timespec ts) {
  unassert(ts.tv_nsec < 1000000000);
  if (ts.tv_sec < NUMERIC_MAX(time_t) / 1000000 - 999999) {
    if (ts.tv_nsec <= 999999000) {
      ts.tv_nsec = (ts.tv_nsec + 999) / 1000;
    } else {
      ts.tv_sec += 1;
      ts.tv_nsec = 0;
    }
    return ts.tv_sec * 1000000 + ts.tv_nsec;
  } else {
    return NUMERIC_MAX(time_t);
  }
}

static inline time_t ToNanoseconds(struct timespec ts) {
  unassert(ts.tv_nsec < 1000000000);
  if (ts.tv_sec < NUMERIC_MAX(time_t) / 1000000000 - 999999999) {
    return ts.tv_sec * 1000000000 + ts.tv_nsec;
  } else {
    return NUMERIC_MAX(time_t);
  }
}

int CompareTime(struct timespec, struct timespec);
struct timespec AddTime(struct timespec, struct timespec);
struct timespec SubtractTime(struct timespec, struct timespec);

#endif /* BLINK_TIMESPEC_H_ */
