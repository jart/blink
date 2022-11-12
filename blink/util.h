#ifndef BLINK_UTIL_H_
#define BLINK_UTIL_H_
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include "blink/types.h"
#include <sys/types.h>

bool mulo(u64, u64, u64 *);
bool startswith(const char *, const char *);
const char *doublenul(const char *, unsigned);
int popcount(u64);
ssize_t readansi(int, char *, size_t);

#endif /* BLINK_UTIL_H_ */
