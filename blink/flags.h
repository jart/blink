#ifndef BLINK_FLAGS_H_
#define BLINK_FLAGS_H_
#include "blink/machine.h"

#define FLAGS_CF   0   // carry flag
#define FLAGS_VF   1   // always 1
#define FLAGS_PF   2   // parity flag
#define FLAGS_F1   3   // always 0
#define FLAGS_AF   4   // auxiliary carry flag
#define FLAGS_KF   5   // always 0
#define FLAGS_ZF   6   // zero flag
#define FLAGS_SF   7   // sign flag
#define FLAGS_TF   8   // trap flag
#define FLAGS_IF   9   // interrupt enable flag
#define FLAGS_DF   10  // direction flag
#define FLAGS_OF   11  // overflow flag
#define FLAGS_IOPL 12  // [12,13] i/o privilege level
#define FLAGS_NT   14  // nested task
#define FLAGS_F0   15  // always 0
#define FLAGS_RF   16  // resume flag
#define FLAGS_VM   17  // virtual-8086 mode
#define FLAGS_AC   18  // access control / alignment check
#define FLAGS_VIF  19  // virtual interrupt flag
#define FLAGS_VIP  20  // virtual interrupt pending
#define FLAGS_ID   21  // id flag
#define FLAGS_LP   24  // [24,31] low bits of last alu result (supposed to be 0)

#define GetLazyParityBool(f)    GetParity((0xff000000 & (f)) >> FLAGS_LP)
#define SetLazyParityByte(f, x) ((0x00ffffff & (f)) | (255 & (x)) << FLAGS_LP)

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
