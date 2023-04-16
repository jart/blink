#ifndef BLINK_DEFBIOS_H_
#define BLINK_DEFBIOS_H_

#include "blink/machine.h"

#define kBiosBase  0x000F0000        // nominal base address of BIOS image
#define kBiosSeg   (kBiosBase >> 4)  // nominal base segment of BIOS image
#define kBiosEnd   0x00100000        // address immediately after BIOS image
#define kBiosEntry 0x000FFFF0        // entry point

void LoadDefaultBios(struct Machine *);
void SetDefaultBiosIntVectors(struct Machine *);
void SetDefaultBiosDataArea(struct Machine *);

#endif /* BLINK_DEFBIOS_H_ */
