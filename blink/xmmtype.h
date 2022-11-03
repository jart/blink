#ifndef BLINK_XMMTYPE_H_
#define BLINK_XMMTYPE_H_
#include "blink/machine.h"

#define kXmmIntegral 0
#define kXmmDouble   1
#define kXmmFloat    2

struct XmmType {
  uint8_t type[16];
  uint8_t size[16];
};

void UpdateXmmType(struct Machine *, struct XmmType *);

#endif /* BLINK_XMMTYPE_H_ */
