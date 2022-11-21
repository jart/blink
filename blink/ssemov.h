#ifndef BLINK_SSEMOV_H_
#define BLINK_SSEMOV_H_
#include "blink/machine.h"

void OpLddquVdqMdq(struct Machine *, u64);
void OpMovntiMdqpGdqp(struct Machine *, u64);
void OpPmovmskbGdqpNqUdq(struct Machine *, u64);
void OpMaskMovDiXmmRegXmmRm(struct Machine *, u64);
void OpMovntdqaVdqMdq(struct Machine *, u64);
void OpMovWpsVps(struct Machine *, u64);
void OpMov0f28(struct Machine *, u64);
void OpMov0f6e(struct Machine *, u64);
void OpMov0f6f(struct Machine *, u64);
void OpMov0fE7(struct Machine *, u64);
void OpMov0f7e(struct Machine *, u64);
void OpMov0f7f(struct Machine *, u64);
void OpMov0f10(struct Machine *, u64);
void OpMov0f29(struct Machine *, u64);
void OpMov0f2b(struct Machine *, u64);
void OpMov0f12(struct Machine *, u64);
void OpMov0f13(struct Machine *, u64);
void OpMov0f16(struct Machine *, u64);
void OpMov0f17(struct Machine *, u64);
void OpMov0fD6(struct Machine *, u64);

#endif /* BLINK_SSEMOV_H_ */
