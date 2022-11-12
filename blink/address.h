#ifndef BLINK_ADDRESS_H_
#define BLINK_ADDRESS_H_
#include "blink/endian.h"
#include "blink/machine.h"
#include "blink/modrm.h"

u64 AddressOb(struct Machine *, u32);
u64 AddressDi(struct Machine *, u32);
u64 AddressSi(struct Machine *, u32);
u8 *GetSegment(struct Machine *, u32, int);
u64 DataSegment(struct Machine *, u32, u64);

static inline u64 MaskAddress(u32 mode, u64 x) {
  if (mode != XED_MODE_LONG) {
    if (mode == XED_MODE_REAL) {
      x &= 0xffff;
    } else {
      x &= 0xffffffff;
    }
  }
  return x;
}

static inline u64 AddSegment(struct Machine *m, u32 rde, u64 i,
                             const u8 s[8]) {
  if (!Sego(rde)) {
    return i + Read64(s);
  } else {
    return i + Read64(GetSegment(m, rde, Sego(rde) - 1));
  }
}

#endif /* BLINK_ADDRESS_H_ */
