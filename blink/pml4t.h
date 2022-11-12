#ifndef BLINK_PML4T_H_
#define BLINK_PML4T_H_
#include "blink/machine.h"

#define IsValidPage(x)    ((x)&1)
#define MaskPageAddr(x)   ((x)&0x00007ffffffff000)
#define UnmaskPageAddr(x) SignExtendAddr(MaskPageAddr(x))
#define SignExtendAddr(x) ((i64)((u64)(x) << 16) >> 16)

struct ContiguousMemoryRange {
  i64 a;
  i64 b;
};

struct ContiguousMemoryRanges {
  size_t i;
  struct ContiguousMemoryRange *p;
};

void FindContiguousMemoryRanges(struct Machine *,
                                struct ContiguousMemoryRanges *);

#endif /* BLINK_PML4T_H_ */
