#ifndef BLINK_TIME_H_
#define BLINK_TIME_H_
#include "blink/machine.h"

void OpPause(struct Machine *, DISPATCH_PARAMETERS);
void OpRdtsc(struct Machine *, DISPATCH_PARAMETERS);
void OpRdtscp(struct Machine *, DISPATCH_PARAMETERS);
void OpRdpid(struct Machine *, DISPATCH_PARAMETERS);

#endif /* BLINK_TIME_H_ */
