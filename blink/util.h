#ifndef BLINK_UTIL_H_
#define BLINK_UTIL_H_
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "blink/types.h"

extern int optind_;
extern char *optarg_;
int getopt_(int, char *const[], const char *);

bool mulo(u64, u64, u64 *);
bool endswith(const char *, const char *);
bool startswith(const char *, const char *);
const char *doublenul(const char *, unsigned);
int popcount(u64);
ssize_t readansi(int, char *, size_t);
int vasprintf_(char **, const char *, va_list);
char *FormatInt64(char *, int64_t);
char *FormatUint64(char *, uint64_t);
char *FormatInt64Thousands(char *, int64_t);
char *FormatUint64Thousands(char *, uint64_t);
char *FormatSize(char *, uint64_t, uint64_t);
char *Commandv(const char *, char *, size_t);
char *Demangle(char *, const char *, size_t);
long Magikarp(u8 *, long);

#endif /* BLINK_UTIL_H_ */
