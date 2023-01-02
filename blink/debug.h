#ifndef BLINK_DEBUG_H_
#define BLINK_DEBUG_H_
#include "blink/assert.h"
#include "blink/builtin.h"
#include "blink/fds.h"
#include "blink/machine.h"
#include "blink/types.h"

#if !defined(NDEBUG) && defined(__GNUC__)
#define IB(x)                      \
  __extension__({                  \
    __typeof__(x) x_ = (x);        \
    unassert((intptr_t)x_ > 4096); \
    x_;                            \
  })
#else
#define IB(x) (x)
#endif

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
