#ifndef BLINK_IOPORTS_H_
#define BLINK_IOPORTS_H_
#include "blink/machine.h"

uint64_t OpIn(struct Machine *, uint16_t);
void OpOut(struct Machine *, uint16_t, uint32_t);

#endif /* BLINK_IOPORTS_H_ */
