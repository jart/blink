#ifndef BLINK_TIMEVAL_H_
#define BLINK_TIMEVAL_H_
#include <sys/time.h>

static inline long timeval_tomicros(struct timeval x) {
  return x.tv_sec * 1000000 + x.tv_usec;
}

static inline struct timeval timeval_sub(struct timeval a, struct timeval b) {
  a.tv_sec -= b.tv_sec;
  if (a.tv_usec < b.tv_usec) {
    a.tv_usec += 1000000;
    a.tv_sec--;
  }
  a.tv_usec -= b.tv_usec;
  return a;
}

#endif /* BLINK_TIMEVAL_H_ */
