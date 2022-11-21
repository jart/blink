#ifndef BLINK_SSEMOV_H_
#define BLINK_SSEMOV_H_
#include "blink/machine.h"

void OpLddquVdqMdq(struct Machine *, DISPATCH_PARAMETERS);
void OpMovntiMdqpGdqp(struct Machine *, DISPATCH_PARAMETERS);
void OpPmovmskbGdqpNqUdq(struct Machine *, DISPATCH_PARAMETERS);
void OpMaskMovDiXmmRegXmmRm(struct Machine *, DISPATCH_PARAMETERS);
void OpMovntdqaVdqMdq(struct Machine *, DISPATCH_PARAMETERS);
void OpMovWpsVps(struct Machine *, DISPATCH_PARAMETERS);
void OpMov0f28(struct Machine *, DISPATCH_PARAMETERS);
void OpMov0f6e(struct Machine *, DISPATCH_PARAMETERS);
void OpMov0f6f(struct Machine *, DISPATCH_PARAMETERS);
void OpMov0fE7(struct Machine *, DISPATCH_PARAMETERS);
void OpMov0f7e(struct Machine *, DISPATCH_PARAMETERS);
void OpMov0f7f(struct Machine *, DISPATCH_PARAMETERS);
void OpMov0f10(struct Machine *, DISPATCH_PARAMETERS);
void OpMov0f29(struct Machine *, DISPATCH_PARAMETERS);
void OpMov0f2b(struct Machine *, DISPATCH_PARAMETERS);
void OpMov0f12(struct Machine *, DISPATCH_PARAMETERS);
void OpMov0f13(struct Machine *, DISPATCH_PARAMETERS);
void OpMov0f16(struct Machine *, DISPATCH_PARAMETERS);
void OpMov0f17(struct Machine *, DISPATCH_PARAMETERS);
void OpMov0fD6(struct Machine *, DISPATCH_PARAMETERS);

#endif /* BLINK_SSEMOV_H_ */
