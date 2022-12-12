#ifndef BLINK_LOG_H_
#define BLINK_LOG_H_
#include <stdatomic.h>
#include <stdio.h>

#ifndef NDEBUG
#define LOG_ENABLED 1
#else
#define LOG_ENABLED 0
#endif

#define LOG_SYS 0
#define LOG_SIG 0
#define LOG_ASM 0
#define LOG_JIT 0
#define LOG_MEM 0
#define LOG_THR 0

#if LOG_ENABLED
#define LOGF(...) Log(__FILE__, __LINE__, __VA_ARGS__)
#else
#define LOGF(...) (void)0
#endif

#if LOG_SYS
#define SYS_LOGF(...) Log(__FILE__, __LINE__, "(sys) " __VA_ARGS__)
#else
#define SYS_LOGF(...) (void)0
#endif

#if LOG_SIG
#define SIG_LOGF(...) Log(__FILE__, __LINE__, "(sig) " __VA_ARGS__)
#else
#define SIG_LOGF(...) (void)0
#endif

#if LOG_ASM
#define ASM_LOGF(...) Log(__FILE__, __LINE__, "(asm) " __VA_ARGS__)
#else
#define ASM_LOGF(...) (void)0
#endif

#if LOG_JIT
#define JIT_LOGF(...) Log(__FILE__, __LINE__, "(jit) " __VA_ARGS__)
#else
#define JIT_LOGF(...) (void)0
#endif

#if LOG_MEM
#define MEM_LOGF(...) Log(__FILE__, __LINE__, "(mem) " __VA_ARGS__)
#else
#define MEM_LOGF(...) (void)0
#endif

#if LOG_THR
#define THR_LOGF(...) Log(__FILE__, __LINE__, "(thr) " __VA_ARGS__)
#else
#define THR_LOGF(...) (void)0
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
