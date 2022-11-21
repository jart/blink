#ifndef BLINK_LOADER_H_
#define BLINK_LOADER_H_
#include "blink/elf.h"
#include "blink/machine.h"

void LoadProgram(struct Machine *, char *, char **, char **);
void LoadDebugSymbols(struct Elf *);

#endif /* BLINK_LOADER_H_ */
