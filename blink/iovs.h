#ifndef BLINK_IOVS_H_
#define BLINK_IOVS_H_
#include <stddef.h>
#include <sys/uio.h>

#include "blink/machine.h"
#include "blink/types.h"

struct Iovs {
  unsigned i, n;
  struct iovec *p;
  struct iovec init[2];
};

int AppendIovs(struct Iovs *, void *, size_t);
void FreeIovs(struct Iovs *);
void InitIovs(struct Iovs *);
int AppendIovsReal(struct Machine *, struct Iovs *, i64, u64);
int AppendIovsGuest(struct Machine *, struct Iovs *, i64, i64);

#endif /* BLINK_IOVS_H_ */
