#ifndef BLINK_PATH_H_
#define BLINK_PATH_H_
#include "blink/machine.h"

extern void (*AddPath_StartOp_Hook)(P);

bool AddPath(P);
bool CreatePath(P);
void CompletePath(P);
void AddPath_EndOp(P);
void AddPath_StartOp(P);
long GetPrologueSize(void);
void FinishPath(struct Machine *);
void AbandonPath(struct Machine *);
void AddIp(struct Machine *, long);

#endif /* BLINK_PATH_H_ */
