#ifndef BLINK_ADDRESS_H_
#define BLINK_ADDRESS_H_
#include "blink/endian.h"
#include "blink/machine.h"
#include "blink/modrm.h"

u64 AddressOb(struct Machine *, u64);
u64 AddressDi(struct Machine *, u64);
u64 AddressSi(struct Machine *, u64);
u64 *GetSegment(struct Machine *, u64, int);
u64 DataSegment(struct Machine *, u64, u64);

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

static inline u64 AddSegment(struct Machine *m, u64 rde, u64 i, u64 s) {
  if (!Sego(rde)) {
    return i + s;
  } else {
    return i + *GetSegment(m, rde, Sego(rde) - 1);
  }
}

#endif /* BLINK_ADDRESS_H_ */
