#ifndef BLINK_PATH_H_
#define BLINK_PATH_H_
#include "blink/machine.h"

extern void (*AddPath_StartOp_Hook)(P);

bool CreatePath(P);
bool AddPath(P);
void AddPath_EndOp(P);
void AddPath_StartOp(P);
void AbandonPath(struct Machine *);
void CommitPath(P, intptr_t);

#endif /* BLINK_PATH_H_ */
