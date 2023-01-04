#ifndef BLINK_DEBUG_H_
#define BLINK_DEBUG_H_
#include "blink/assert.h"
#include "blink/builtin.h"
#include "blink/fds.h"
#include "blink/machine.h"
#include "blink/types.h"

void DumpHex(u8 *, size_t);
void PrintFds(struct Fds *);
void LogCpu(struct Machine *);
const char *DescribeFlags(int);
const char *DescribeSignal(int);
const char *DescribeMopcode(int);
const char *GetBacktrace(struct Machine *);
const char *DescribeOp(struct Machine *, i64);
int GetInstruction(struct Machine *, i64, struct XedDecodedInst *);

#endif /* BLINK_DEBUG_H_ */
