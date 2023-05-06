#ifndef BLINK_BIOSROM_H_
#define BLINK_BIOSROM_H_

#define kBiosBase    0x000F0000        // nominal base address of BIOS image
#define kBiosSeg     (kBiosBase >> 4)  // nominal base segment of BIOS image
#define kBiosEnd     0x00100000        // address immediately after BIOS image
#define kBiosEntry   0x000FFFF0        // entry point
#define kBiosOptBase 0x000C0000        // lowest possible base address of
                                       // BIOS image, including option ROMs

#if !(__ASSEMBLER__ + __LINKER__ + 0)
#include "blink/machine.h"

void LoadBios(struct Machine *, const char *);
void SetDefaultBiosIntVectors(struct Machine *);
void SetDefaultBiosDataArea(struct Machine *);
u32 GetDefaultBiosDisketteParamTable(void);
#endif /*  !(__ASSEMBLER__ + __LINKER__ + 0) */

#endif /* BLINK_BIOSROM_H_ */
