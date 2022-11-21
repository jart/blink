#ifndef BLINK_XMMTYPE_H_
#define BLINK_XMMTYPE_H_
#include "blink/machine.h"

#define kXmmIntegral 0
#define kXmmDouble   1
#define kXmmFloat    2

struct XmmType {
  u8 type[16];
  u8 size[16];
};

void UpdateXmmType(u64, struct XmmType *);

#endif /* BLINK_XMMTYPE_H_ */
