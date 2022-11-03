#ifndef BLINK_FLAGS_H_
#define BLINK_FLAGS_H_
#include "blink/machine.h"

#define FLAGS_CF   0
#define FLAGS_VF   1
#define FLAGS_PF   2
#define FLAGS_F1   3
#define FLAGS_AF   4
#define FLAGS_KF   5
#define FLAGS_ZF   6
#define FLAGS_SF   7
#define FLAGS_TF   8
#define FLAGS_IF   9
#define FLAGS_DF   10
#define FLAGS_OF   11
#define FLAGS_IOPL 12
#define FLAGS_NT   14
#define FLAGS_F0   15
#define FLAGS_RF   16
#define FLAGS_VM   17
#define FLAGS_AC   18
#define FLAGS_VIF  19
#define FLAGS_VIP  20
#define FLAGS_ID   21

#define GetLazyParityBool(f)    GetParity((0xff000000 & (f)) >> 24)
#define SetLazyParityByte(f, x) ((0x00ffffff & (f)) | (255 & (x)) << 24)

bool GetParity(uint8_t);
uint64_t ExportFlags(uint64_t);
void ImportFlags(struct Machine *, uint64_t);

static inline bool GetFlag(uint32_t f, int b) {
  switch (b) {
    case FLAGS_PF:
      return GetLazyParityBool(f);
    default:
      return (f >> b) & 1;
  }
}

static inline uint32_t SetFlag(uint32_t f, int b, bool v) {
  switch (b) {
    case FLAGS_PF:
      return SetLazyParityByte(f, !v);
    default:
      return (f & ~(1u << b)) | v << b;
  }
}

#endif /* BLINK_FLAGS_H_ */
