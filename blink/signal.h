#ifndef BLINK_SIGNAL_H_
#define BLINK_SIGNAL_H_
#include "blink/machine.h"

void OpRestore(struct Machine *);
int ConsumeSignal(struct Machine *);
void EnqueueSignal(struct Machine *, int);
void TerminateSignal(struct Machine *, int);
void DeliverSignal(struct Machine *, int, int);

#endif /* BLINK_SIGNAL_H_ */
