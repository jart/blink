#ifndef BLINK_STACK_H_
#define BLINK_STACK_H_
#include "blink/machine.h"

void Push(struct Machine *, uint32_t, uint64_t);
uint64_t Pop(struct Machine *, uint32_t, uint16_t);
void OpCallJvds(struct Machine *, uint32_t);
void OpRet(struct Machine *, uint32_t);
void OpRetf(struct Machine *, uint32_t);
void OpLeave(struct Machine *, uint32_t);
void OpCallEq(struct Machine *, uint32_t);
void OpPopEvq(struct Machine *, uint32_t);
void OpPopZvq(struct Machine *, uint32_t);
void OpPushZvq(struct Machine *, uint32_t);
void OpPushEvq(struct Machine *, uint32_t);
void PopVq(struct Machine *, uint32_t);
void PushVq(struct Machine *, uint32_t);
void OpJmpEq(struct Machine *, uint32_t);
void OpPusha(struct Machine *, uint32_t);
void OpPopa(struct Machine *, uint32_t);
void OpCallf(struct Machine *, uint32_t);

#endif /* BLINK_STACK_H_ */
