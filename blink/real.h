#ifndef BLINK_REAL_H_
#define BLINK_REAL_H_
#include "blink/machine.h"
#include "blink/tsan.h"

NO_THREAD_SAFETY_ANALYSIS
static inline long GetRealMemorySize(struct System *s) {
  long n;
  // assertions use this variable
  // testing n is safe because n is monotonic
  IGNORE_RACES_START();
  __asm__ volatile("" ::: "memory");
  n = s->real.n;
  __asm__ volatile("" ::: "memory");
  IGNORE_RACES_END();
  return n;
}

#endif /* BLINK_REAL_H_ */
