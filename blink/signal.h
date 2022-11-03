#ifndef BLINK_SIGNAL_H_
#define BLINK_SIGNAL_H_
#include "blink/machine.h"

struct Signals {
  int i, n;
  struct Signal {
    int sig;
    int code;
  } p[64];
};

void OpRestore(struct Machine *);
void TerminateSignal(struct Machine *, int);
void DeliverSignal(struct Machine *, int, int);
int ConsumeSignal(struct Machine *, struct Signals *);
void EnqueueSignal(struct Machine *, struct Signals *, int, int);

#endif /* BLINK_SIGNAL_H_ */
