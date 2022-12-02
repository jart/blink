#ifndef BLINK_PATH_H_
#define BLINK_PATH_H_
#include "blink/machine.h"

bool CreatePath(struct Machine *);
bool AddPath(P);
void AddPath_EndOp(struct Machine *);
void AddPath_StartOp(struct Machine *, u64);
void AbandonPath(struct Machine *);
void CommitPath(struct Machine *, intptr_t);

#endif /* BLINK_PATH_H_ */
