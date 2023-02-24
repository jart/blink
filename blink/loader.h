#ifndef BLINK_LOADER_H_
#define BLINK_LOADER_H_
#include "blink/elf.h"
#include "blink/machine.h"

bool CanEmulateExecutable(struct Machine *, char **, char ***);
void LoadProgram(struct Machine *, char *, char *, char **, char **);
void LoadDebugSymbols(struct System *);
void LoadFileSymbols(struct System *, const char *, i64);
bool IsSupportedExecutable(const char *, void *, size_t);

#endif /* BLINK_LOADER_H_ */
