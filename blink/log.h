#ifndef BLINK_LOG_H_
#define BLINK_LOG_H_
#include <stdio.h>

extern FILE *g_log;

void Log(const char *, int, const char *, ...);

#define LOGF(...)                           \
  do {                                      \
    if (g_log) {                            \
      Log(__FILE__, __LINE__, __VA_ARGS__); \
    }                                       \
  } while (0)

#endif /* BLINK_LOG_H_ */
