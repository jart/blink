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

void OpMovsb(struct Machine *, uint32_t);
void OpStosb(struct Machine *, uint32_t);
void OpMovs(struct Machine *, uint32_t);
void OpCmps(struct Machine *, uint32_t);
void OpStos(struct Machine *, uint32_t);
void OpLods(struct Machine *, uint32_t);
void OpScas(struct Machine *, uint32_t);
void OpIns(struct Machine *, uint32_t);
void OpOuts(struct Machine *, uint32_t);

#endif /* BLINK_STRING_H_ */
