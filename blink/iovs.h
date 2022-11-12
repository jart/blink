#ifndef BLINK_IOVS_H_
#define BLINK_IOVS_H_
#include <stddef.h>
#include "blink/types.h"
#include <sys/uio.h>

struct Iovs {
  unsigned i, n;
  struct iovec *p;
  struct iovec init[2];
};

int AppendIovs(struct Iovs *, void *, size_t);
void FreeIovs(struct Iovs *);
void InitIovs(struct Iovs *);

#endif /* BLINK_IOVS_H_ */
