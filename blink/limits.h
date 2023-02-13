#ifndef BLINK_LIMITS_H_
#define BLINK_LIMITS_H_
#include <limits.h>
#include <stdint.h>
#include <unistd.h>

#define NUMERIC_MAX(t)         \
  (((t) ~(t)0) > 1 ? (t) ~(t)0 \
                   : (t)((((uintmax_t)1) << (sizeof(t) * CHAR_BIT - 1)) - 1))

static inline long GetIovMax(void) {
#ifdef IOV_MAX
  return IOV_MAX;
#elif defined(_SC_IOV_MAX)
  return sysconf(_SC_IOV_MAX);
#elif defined(_XOPEN_IOV_MAX)
  return _XOPEN_IOV_MAX;
#else
  return 16;
#endif
}

static inline long GetSymloopMax(void) {
#ifdef SYMLOOP_MAX
  return SYMLOOP_MAX;
#elif defined(_SC_SYMLOOP_MAX)
  return sysconf(_SC_SYMLOOP_MAX);
#elif defined(_POSIX_SYMLOOP_MAX)
  return _POSIX_SYMLOOP_MAX;
#else
  return 8;
#endif
}

#endif /* BLINK_LIMITS_H_ */
