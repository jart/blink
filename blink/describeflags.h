#ifndef BLINK_DESCRIBEFLAGS_H_
#define BLINK_DESCRIBEFLAGS_H_
#include <stddef.h>

#include "blink/builtin.h"

struct DescribeFlagz {
  unsigned flag;
  const char *name;
};

const char *DescribeFlagz(char *, size_t, const struct DescribeFlagz *, size_t,
                          const char *, unsigned);

#endif /* BLINK_DESCRIBEFLAGS_H_ */
