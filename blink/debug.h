#ifndef BLINK_DEBUG_H_
#define BLINK_DEBUG_H_
#include <stddef.h>

#include "blink/fds.h"
#include "blink/types.h"

void DumpHex(u8 *, size_t);
void PrintFds(struct Fds *);
i64 ReadWordSafely(int, u8 *);
const char *DescribeProt(int);
const char *DescribeMopcode(int);
const char *DescribeCpuFlags(int);
const char *GetBlinkBacktrace(void);

#endif /* BLINK_DEBUG_H_ */
