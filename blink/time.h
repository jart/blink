#ifndef BLINK_TIME_H_
#define BLINK_TIME_H_
#include "blink/machine.h"

void OpPause(struct Machine *, u32);
void OpRdtsc(struct Machine *, u32);
void OpRdtscp(struct Machine *, u32);
void OpRdpid(struct Machine *, u32);

#endif /* BLINK_TIME_H_ */
