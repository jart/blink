#ifndef BLINK_STATS_H_
#define BLINK_STATS_H_
#include <stdbool.h>

#include "blink/builtin.h"
#include "blink/tsan.h"

#ifndef NDEBUG
// we don't care about the accuracy of statistics across threads. some
// hardware architectures don't even seem to have atomic addition ops.
#define STATISTIC(x)      \
  do {                    \
    IGNORE_RACES_START(); \
    x;                    \
    IGNORE_RACES_END();   \
  } while (0)
#else
#define STATISTIC(x) (void)0
#endif

#define AVERAGE(S, x) S.a += ((x)-S.a) / ++S.i

#ifndef NDEBUG
#ifdef __GNUC__
#define GET_COUNTER(S)    \
  __extension__({         \
    IGNORE_RACES_START(); \
    long S_ = S;          \
    IGNORE_RACES_END();   \
    S_;                   \
  })
#else
#define GET_COUNTER(S) (S)
#endif
#else
#define GET_COUNTER(S) 0L
#endif

#define DEFINE_COUNTER(S) extern long S;
#define DEFINE_AVERAGE(S) extern struct Average S;
#include "blink/stats.inc"
#undef DEFINE_COUNTER
#undef DEFINE_AVERAGE

struct Average {
  double a;
  long i;
};

extern bool FLAG_statistics;

void PrintStats(void);

#endif /* BLINK_STATS_H_ */
