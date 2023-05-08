#ifndef BLINK_BIOSROM_H_
#define BLINK_BIOSROM_H_

#define kBiosBase    0x000F0000        // nominal base address of BIOS image
#define kBiosSeg     (kBiosBase >> 4)  // nominal base segment of BIOS image
#define kBiosEnd     0x00100000        // address immediately after BIOS image
#define kBiosEntry   0x000FFFF0        // entry point
#define kBiosOptBase 0x000C0000        // lowest possible base address of
                                       // BIOS image, including option ROMs

#if !(__ASSEMBLER__ + __LINKER__ + 0)
#include "blink/builtin.h"
#include "blink/endian.h"
#include "blink/machine.h"

MICRO_OP_SAFE bool IsRomAddress(struct Machine *m, u8 *r) {
#ifndef DISABLE_ROM
  if (m->metal) {
    u8 *real = m->system->real;
#if CAN_ABUSE_POINTERS
    ptrdiff_t d = r - real;
    // gcc optimizes this conjunction into subtraction followed by comparison
    // FIXME: find better way to figure out if we can abuse ptrdiff_t like so
    if (kBiosOptBase <= d && d < kBiosEnd) return true;
#else
    if (&real[kBiosOptBase] <= r && r < &real[kBiosEnd]) return true;
#endif
  }
#endif
  return false;
}

void LoadBios(struct Machine *, const char *);
void SetDefaultBiosIntVectors(struct Machine *);
void SetDefaultBiosDataArea(struct Machine *);
u32 GetDefaultBiosDisketteParamTable(void);
#endif /*  !(__ASSEMBLER__ + __LINKER__ + 0) */

#endif /* BLINK_BIOSROM_H_ */
