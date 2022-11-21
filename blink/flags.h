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

bool GetParity(u8);
u64 ExportFlags(u64);
void ImportFlags(struct Machine *, u64);

static inline bool GetFlag(u32 f, int b) {
  switch (b) {
    case FLAGS_PF:
      return GetLazyParityBool(f);
    default:
      return (f >> b) & 1;
  }
}

static inline u32 SetFlag(u32 f, int b, bool v) {
  switch (b) {
    case FLAGS_PF:
      return SetLazyParityByte(f, !v);
    default:
      return (f & ~(1u << b)) | v << b;
  }
}

static inline bool IsParity(struct Machine *m) {
  return GetFlag(m->flags, FLAGS_PF);
}

static inline bool IsBelowOrEqual(struct Machine *m) {  // CF || ZF
  return ((m->flags >> 6) | m->flags) & 1;
}

static inline bool IsAbove(struct Machine *m) {  // !CF && ~ZF
  return ((~m->flags >> 6) & ~m->flags) & 1;
}

static inline bool IsLess(struct Machine *m) {  // SF != OF
  return (i8)(m->flags ^ (m->flags >> 4)) < 0;
}

static inline bool IsGreaterOrEqual(struct Machine *m) {  // SF == OF
  return (i8)(~(m->flags ^ (m->flags >> 4))) < 0;
}

static inline bool IsLessOrEqual(struct Machine *m) {  // ZF || SF != OF
  return (i8)((m->flags ^ (m->flags >> 4)) | (m->flags << 1)) < 0;
}

static inline bool IsGreater(struct Machine *m) {  // !ZF && SF == OF
  return (i8)(~(m->flags ^ (m->flags >> 4)) & ~(m->flags << 1)) < 0;
}

#endif /* BLINK_FLAGS_H_ */
