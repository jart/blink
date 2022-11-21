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

void OpMovsb(struct Machine *, u64);
void OpStosb(struct Machine *, u64);
void OpMovs(struct Machine *, u64);
void OpCmps(struct Machine *, u64);
void OpStos(struct Machine *, u64);
void OpLods(struct Machine *, u64);
void OpScas(struct Machine *, u64);
void OpIns(struct Machine *, u64);
void OpOuts(struct Machine *, u64);

#endif /* BLINK_STRING_H_ */
