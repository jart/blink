#ifndef BLINK_LOADER_H_
#define BLINK_LOADER_H_
#include "blink/elf.h"
#include "blink/machine.h"

struct Elf {
  const char *prog;
  Elf64_Ehdr *ehdr;
  long size;
  i64 base;
  char *map;
  long mapsize;
};

void LoadProgram(struct Machine *, char *, char **, char **, struct Elf *);
void LoadDebugSymbols(struct Elf *);

#endif /* BLINK_LOADER_H_ */
