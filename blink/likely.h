#ifndef BLINK_LIKELY_H_
#define BLINK_LIKELY_H_
#include "blink/builtin.h"

#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#endif /* BLINK_LIKELY_H_ */
