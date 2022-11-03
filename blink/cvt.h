#ifndef BLINK_CVT_H_
#define BLINK_CVT_H_
#include "blink/machine.h"

void OpCvt0f2a(struct Machine *, uint32_t);
void OpCvtt0f2c(struct Machine *, uint32_t);
void OpCvt0f2d(struct Machine *, uint32_t);
void OpCvt0f5a(struct Machine *, uint32_t);
void OpCvt0f5b(struct Machine *, uint32_t);
void OpCvt0fE6(struct Machine *, uint32_t);

#endif /* BLINK_CVT_H_ */
