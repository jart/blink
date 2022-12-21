#ifndef BLINK_SIGNAL_H_
#define BLINK_SIGNAL_H_
#include "blink/machine.h"

void SigRestore(struct Machine *);
bool IsSignalIgnoredByDefault(int);
int ConsumeSignal(struct Machine *);
void EnqueueSignal(struct Machine *, int);
void TerminateSignal(struct Machine *, int);
void DeliverSignal(struct Machine *, int, int);
void DeliverSignalToUser(struct Machine *, int);

#endif /* BLINK_SIGNAL_H_ */
