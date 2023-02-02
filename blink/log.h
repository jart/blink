#ifndef BLINK_LOG_H_
#define BLINK_LOG_H_
#include <stdatomic.h>
#include <stdio.h>

#include "blink/builtin.h"

#ifndef NDEBUG
#define LOG_ENABLED 1
#else
#define LOG_ENABLED 0
#endif

#define LOG_SYS 0  // log system calls run by guest
#define LOG_SIG 0  // log signal handling behaviors
#define LOG_ASM 0  // log executed assembly opcodes
#define LOG_JIT 0  // jit path construction logging
#define LOG_JIX 0  // verbose jit execution logging
#define LOG_MEM 0  // system memory mapping logging
#define LOG_THR 0  // multi-threaded operation logs
#define LOG_ELF 0  // elf executable loader logging
#define LOG_SPX 0  // speculative execution logging
#define LOG_CPU 0  // produce txt file of registers
#define LOG_COD 0  // produce asm file of jit codes

#if LOG_ENABLED
#define ERRF(...) LogErr(__FILE__, __LINE__, __VA_ARGS__)
#define LOGF(...) LogInfo(__FILE__, __LINE__, __VA_ARGS__)
#else
#define ERRF(...) (void)0
#define LOGF(...) (void)0
#endif

#if LOG_SYS
#define SYS_LOGF(...) LogInfo(__FILE__, __LINE__, "(sys) " __VA_ARGS__)
#else
#define SYS_LOGF(...) (void)0
#endif

#if LOG_SIG
#define SIG_LOGF(...) LogInfo(__FILE__, __LINE__, "(sig) " __VA_ARGS__)
#else
#define SIG_LOGF(...) (void)0
#endif

#if LOG_ASM
#define ASM_LOGF(...) LogInfo(__FILE__, __LINE__, "(asm) " __VA_ARGS__)
#else
#define ASM_LOGF(...) (void)0
#endif

#if LOG_JIT
#define JIT_LOGF(...) LogInfo(__FILE__, __LINE__, "(jit) " __VA_ARGS__)
#else
#define JIT_LOGF(...) (void)0
#endif

#if LOG_JIX
#define JIX_LOGF(...) LogInfo(__FILE__, __LINE__, "(jix) " __VA_ARGS__)
#else
#define JIX_LOGF(...) (void)0
#endif

#if LOG_MEM
#define MEM_LOGF(...) LogInfo(__FILE__, __LINE__, "(mem) " __VA_ARGS__)
#else
#define MEM_LOGF(...) (void)0
#endif

#if LOG_THR
#define THR_LOGF(...) LogInfo(__FILE__, __LINE__, "(thr) " __VA_ARGS__)
#else
#define THR_LOGF(...) (void)0
#endif

#if LOG_ELF
#define ELF_LOGF(...) LogInfo(__FILE__, __LINE__, "(elf) " __VA_ARGS__)
#else
#define ELF_LOGF(...) (void)0
#endif

#if LOG_SPX
#define SPX_LOGF(...) LogInfo(__FILE__, __LINE__, "(spx) " __VA_ARGS__)
#else
#define SPX_LOGF(...) (void)0
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

extern char *g_progname;

void LogInit(const char *);
void LogErr(const char *, int, const char *, ...) printfesque(3);
void LogInfo(const char *, int, const char *, ...) printfesque(3);
int WriteError(int, const char *, int);
void WriteErrorInit(void);
int WriteErrorString(const char *);

#endif /* BLINK_LOG_H_ */
