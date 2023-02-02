#ifndef BLINK_LIMITS_H_
#define BLINK_LIMITS_H_
#include <limits.h>
#include <stdint.h>

#define NUMERIC_MAX(t)         \
  (((t) ~(t)0) > 1 ? (t) ~(t)0 \
                   : (t)((((uintmax_t)1) << (sizeof(t) * CHAR_BIT - 1)) - 1))

#endif /* BLINK_LIMITS_H_ */
