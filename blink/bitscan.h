#ifndef BLINK_BITSCAN_H_
#define BLINK_BITSCAN_H_
#include "blink/builtin.h"
#include "blink/types.h"
#ifndef __GNUC__

int bsr(u64);
int bsf(u64);
int popcount(u64);

#else
#define bsf(x)      __builtin_ctzll(x)
#define bsr(x)      (__builtin_clzll(x) ^ 63)
#define popcount(x) __builtin_popcountll(x)
#endif /* GNUC */
#endif /* BLINK_BITSCAN_H_ */
