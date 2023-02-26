#ifndef BLINK_UTIL_H_
#define BLINK_UTIL_H_
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <wchar.h>

#include "blink/builtin.h"
#include "blink/types.h"

extern int optind_;
extern char *optarg_;
extern const short kCp437[256];
extern bool g_exitdontabort;

_Noreturn void Abort(void);
char *GetStartDir(void);
int GetOpt(int, char *const[], const char *);
u64 tpenc(wint_t);
const char *DescribeSignal(int);
const char *DescribeHostErrno(int);
bool Sub(i64, i64, i64 *) dontdiscard;
bool Add(i64, i64, i64 *) dontdiscard;
bool Mul(u64, u64, u64 *) dontdiscard;
bool endswith(const char *, const char *);
bool startswith(const char *, const char *);
const char *doublenul(const char *, unsigned);
ssize_t readansi(int, char *, size_t);
char *FormatInt64(char *, int64_t);
char *FormatUint64(char *, uint64_t);
char *FormatInt64Thousands(char *, int64_t);
char *FormatUint64Thousands(char *, uint64_t);
char *FormatSize(char *, uint64_t, uint64_t);
char *Commandv(const char *, char *, size_t);
char *Demangle(char *, const char *, size_t);
void *Deflate(const void *, unsigned, unsigned *);
void Inflate(void *, unsigned, const void *, unsigned);
ssize_t UninterruptibleWrite(int, const void *, size_t);
long IsProcessTainted(void);
long Magikarp(u8 *, long);
int GetCpuCount(void);
char *strchrnul_(const char *, int);
int vasprintf_(char **, const char *, va_list);
char *realpath_(const char *, char *);
void *memccpy_(void *, const void *, int, size_t);
int wcwidth_(wchar_t);
u64 Vigna(u64[1]);

#ifndef HAVE_STRCHRNUL
#ifdef strchrnul
#undef strchrnul
#endif
#define strchrnul strchrnul_
#endif

#ifndef HAVE_VASPRINTF
#ifdef vasprintf
#undef vasprintf
#endif
#define vasprintf vasprintf_
#endif

#ifndef HAVE_REALPATH
#ifdef realpath
#undef realpath
#endif
#define realpath realpath_
#endif

#if !defined(HAVE_MEMCCPY) || defined(TINY)
#ifdef memccpy
#undef memccpy
#endif
#define memccpy memccpy_
#endif

#ifndef HAVE_WCWIDTH
#ifdef wcwidth
#undef wcwidth
#endif
#define wcwidth wcwidth_
#endif

#endif /* BLINK_UTIL_H_ */
