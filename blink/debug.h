#ifndef BLINK_DEBUG_H_
#define BLINK_DEBUG_H_
#include <stddef.h>

#include "blink/fds.h"
#include "blink/types.h"

void PrintBacktrace(void);
void DumpHex(u8 *, size_t);
void PrintFds(struct Fds *);
const char *DescribeProt(int);
const char *DescribeMopcode(int);
const char *DescribeCpuFlags(int);

#endif /* BLINK_DEBUG_H_ */
