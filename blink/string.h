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

void OpMovsb(struct Machine *, u32);
void OpStosb(struct Machine *, u32);
void OpMovs(struct Machine *, u32);
void OpCmps(struct Machine *, u32);
void OpStos(struct Machine *, u32);
void OpLods(struct Machine *, u32);
void OpScas(struct Machine *, u32);
void OpIns(struct Machine *, u32);
void OpOuts(struct Machine *, u32);

#endif /* BLINK_STRING_H_ */
