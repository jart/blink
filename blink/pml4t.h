#ifndef BLINK_PML4T_H_
#define BLINK_PML4T_H_
#include "blink/machine.h"

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
