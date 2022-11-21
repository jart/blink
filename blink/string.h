#ifndef BLINK_STRING_H_
#define BLINK_STRING_H_
#include "blink/machine.h"

#define STRING_CMPS 0
#define STRING_MOVS 1
#define STRING_STOS 2
#define STRING_LODS 3
#define STRING_SCAS 4
#define STRING_OUTS 5
#define STRING_INS  6

void OpMovsb(struct Machine *, DISPATCH_PARAMETERS);
void OpStosb(struct Machine *, DISPATCH_PARAMETERS);
void OpMovs(struct Machine *, DISPATCH_PARAMETERS);
void OpCmps(struct Machine *, DISPATCH_PARAMETERS);
void OpStos(struct Machine *, DISPATCH_PARAMETERS);
void OpLods(struct Machine *, DISPATCH_PARAMETERS);
void OpScas(struct Machine *, DISPATCH_PARAMETERS);
void OpIns(struct Machine *, DISPATCH_PARAMETERS);
void OpOuts(struct Machine *, DISPATCH_PARAMETERS);

#endif /* BLINK_STRING_H_ */
