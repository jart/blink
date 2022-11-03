#ifndef BLINK_BITSCAN_H_
#define BLINK_BITSCAN_H_
#include <stdint.h>
#ifndef __GNUC__

int bsr(uint64_t);
int bsf(uint64_t);

#else
#define bsf(x) __builtin_ctzll(x)
#define bsr(x) (__builtin_clzll(x) ^ 63)
#endif /* GNUC */
#endif /* BLINK_BITSCAN_H_ */
