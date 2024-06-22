#ifndef BLINK_DESCRIBEFLAGS_H_
#define BLINK_DESCRIBEFLAGS_H_
#include <stddef.h>

#include "blink/builtin.h"

struct DescribeFlags {
  unsigned flag;
  const char *name;
};

const char *DescribeFlags(char *, size_t, const struct DescribeFlags *, size_t,
                          const char *, unsigned);

#endif /* BLINK_DESCRIBEFLAGS_H_ */
