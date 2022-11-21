#ifndef BLINK_TIME_H_
#define BLINK_TIME_H_
#include "blink/machine.h"

void OpPause(struct Machine *, u64);
void OpRdtsc(struct Machine *, u64);
void OpRdtscp(struct Machine *, u64);
void OpRdpid(struct Machine *, u64);

#endif /* BLINK_TIME_H_ */
