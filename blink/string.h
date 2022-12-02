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

void OpCmps(P);
void OpIns(P);
void OpLods(P);
void OpMovs(P);
void OpMovsb(P);
void OpOuts(P);
void OpScas(P);
void OpStos(P);
void OpStosb(P);

#endif /* BLINK_STRING_H_ */
