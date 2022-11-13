#ifndef BLINK_LOG_H_
#define BLINK_LOG_H_
#include <stdio.h>

#ifndef NDEBUG
#define LOGF(...) Log(__FILE__, __LINE__, __VA_ARGS__)
#else
#define LOGF(...) (void)0
#endif

void LogInit(const char *);
void Log(const char *, int, const char *, ...);
int WriteError(int, const char *, int);
void WriteErrorInit(void);
int WriteErrorString(const char *);

#endif /* BLINK_LOG_H_ */
