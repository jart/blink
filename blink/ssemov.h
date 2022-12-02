#ifndef BLINK_SSEMOV_H_
#define BLINK_SSEMOV_H_
#include "blink/machine.h"

void OpLddquVdqMdq(P);
void OpMaskMovDiXmmRegXmmRm(P);
void OpMov0f10(P);
void OpMov0f12(P);
void OpMov0f13(P);
void OpMov0f16(P);
void OpMov0f17(P);
void OpMov0f28(P);
void OpMov0f29(P);
void OpMov0f2b(P);
void OpMov0f6e(P);
void OpMov0f6f(P);
void OpMov0f7e(P);
void OpMov0f7f(P);
void OpMov0fD6(P);
void OpMov0fE7(P);
void OpMovWpsVps(P);
void OpMovntdqaVdqMdq(P);
void OpMovntiMdqpGdqp(P);
void OpPmovmskbGdqpNqUdq(P);

#endif /* BLINK_SSEMOV_H_ */
