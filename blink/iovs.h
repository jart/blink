#ifndef BLINK_IOVS_H_
#define BLINK_IOVS_H_
#include <limits.h>
#include <stddef.h>
#include <sys/uio.h>

#include "blink/machine.h"
#include "blink/types.h"

struct Iovs {
  unsigned i, n;
  struct iovec *p;
  struct iovec init[8];
};

void FreeIovs(struct Iovs *);
void InitIovs(struct Iovs *);
int AppendIovsReal(struct Machine *, struct Iovs *, i64, u64, int);
int AppendIovsGuest(struct Machine *, struct Iovs *, i64, int, int);

#endif /* BLINK_IOVS_H_ */
