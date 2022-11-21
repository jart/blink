#ifndef BLINK_LOG_H_
#define BLINK_LOG_H_
#include <stdatomic.h>
#include <stdio.h>

#ifndef NDEBUG
#define LOGF(...) Log(__FILE__, __LINE__, __VA_ARGS__)
#else
#define LOGF(...) (void)0
#endif

#if LOG_ENABLED
#define LOG_ONCE(x)                                                   \
  do {                                                                \
    static _Atomic(int) once_;                                        \
    if (!atomic_exchange_explicit(&once_, 1, memory_order_relaxed)) { \
      x;                                                              \
    }                                                                 \
  } while (0)
#else
#define LOG_ONCE(x) (void)0
#endif

extern const char *g_progname;

void LogInit(const char *);
void Log(const char *, int, const char *, ...);
int WriteError(int, const char *, int);
void WriteErrorInit(void);
int WriteErrorString(const char *);

#endif /* BLINK_LOG_H_ */
