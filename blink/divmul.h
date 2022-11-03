#ifndef BLINK_DIVMUL_H_
#define BLINK_DIVMUL_H_
#include "blink/machine.h"

void OpDivAlAhAxEbSigned(struct Machine *, uint32_t);
void OpDivAlAhAxEbUnsigned(struct Machine *, uint32_t);
void OpDivRdxRaxEvqpSigned(struct Machine *, uint32_t);
void OpDivRdxRaxEvqpUnsigned(struct Machine *, uint32_t);
void OpImulGvqpEvqp(struct Machine *, uint32_t);
void OpImulGvqpEvqpImm(struct Machine *, uint32_t);
void OpMulAxAlEbSigned(struct Machine *, uint32_t);
void OpMulAxAlEbUnsigned(struct Machine *, uint32_t);
void OpMulRdxRaxEvqpSigned(struct Machine *, uint32_t);
void OpMulRdxRaxEvqpUnsigned(struct Machine *, uint32_t);

#endif /* BLINK_DIVMUL_H_ */
