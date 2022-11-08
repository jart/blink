#ifndef BLINK_LOG_H_
#define BLINK_LOG_H_
#include <stdio.h>

void OpenLog(const char *);
void Log(const char *, int, const char *, ...);

#ifndef NDEBUG
#define LOGF(...) Log(__FILE__, __LINE__, __VA_ARGS__)
#else
#define LOGF(...) (void)0
#endif

#endif /* BLINK_LOG_H_ */
