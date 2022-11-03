#ifndef BLINK_TIME_H_
#define BLINK_TIME_H_
#include "blink/machine.h"

void OpPause(struct Machine *, uint32_t);
void OpRdtsc(struct Machine *, uint32_t);
void OpRdtscp(struct Machine *, uint32_t);
void OpRdpid(struct Machine *, uint32_t);

#endif /* BLINK_TIME_H_ */
