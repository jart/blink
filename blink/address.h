#ifndef BLINK_ADDRESS_H_
#define BLINK_ADDRESS_H_
#include "blink/endian.h"
#include "blink/machine.h"
#include "blink/modrm.h"

uint64_t AddressOb(struct Machine *, uint32_t);
uint64_t AddressDi(struct Machine *, uint32_t);
uint64_t AddressSi(struct Machine *, uint32_t);
uint8_t *GetSegment(struct Machine *, uint32_t, int);
uint64_t DataSegment(struct Machine *, uint32_t, uint64_t);

static inline uint64_t MaskAddress(uint32_t mode, uint64_t x) {
  if (mode != XED_MODE_LONG) {
    if (mode == XED_MODE_REAL) {
      x &= 0xffff;
    } else {
      x &= 0xffffffff;
    }
  }
  return x;
}

static inline uint64_t AddSegment(struct Machine *m, uint32_t rde, uint64_t i,
                                  const uint8_t s[8]) {
  if (!Sego(rde)) {
    return i + Read64(s);
  } else {
    return i + Read64(GetSegment(m, rde, Sego(rde) - 1));
  }
}

#endif /* BLINK_ADDRESS_H_ */
