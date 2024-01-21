#ifndef BLINK_FLAGS_H_
#define BLINK_FLAGS_H_
#include "blink/builtin.h"
#include "blink/jit.h"
#include "blink/likely.h"
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
#define FLAGS_F0   15  // use internally for single stepping (supposed to be 0)
#define FLAGS_RF   16  // resume flag
#define FLAGS_VM   17  // virtual-8086 mode
#define FLAGS_AC   18  // access control / alignment check
#define FLAGS_VIF  19  // virtual interrupt flag
#define FLAGS_VIP  20  // virtual interrupt pending
#define FLAGS_ID   21  // id flag
#define FLAGS_LP   24  // [24,31] low bits of last alu result (supposed to be 0)

#define CF ((u32)1 << FLAGS_CF)
#define PF ((u32)1 << FLAGS_PF)
#define AF ((u32)1 << FLAGS_AF)
#define ZF ((u32)1 << FLAGS_ZF)
#define SF ((u32)1 << FLAGS_SF)
#define DF ((u32)1 << FLAGS_DF)
#define OF ((u32)1 << FLAGS_OF)
#define RF ((u32)1 << FLAGS_RF)
#define AC ((u32)1 << FLAGS_AC)
#define ID ((u32)1 << FLAGS_ID)

#define GetLazyParityBool(f)    GetParity((0xff000000 & (f)) >> FLAGS_LP)
#define SetLazyParityByte(f, x) ((0x00ffffff & (f)) | (255 & (x)) << FLAGS_LP)

u64 ExportFlags(u64);
bool GetParity(u8) pureconst;
void ImportFlags(struct Machine *, u64);
int GetFlagDeps(u64) pureconst;
int GetFlagClobbers(u64) pureconst;
int GetNeededFlags(struct Machine *, i64, int);

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

MICRO_OP_SAFE bool IsParity(struct Machine *m) {
  return GetFlag(m->flags, FLAGS_PF);
}

MICRO_OP_SAFE bool IsBelowOrEqual(struct Machine *m) {  // CF || ZF
  return ((m->flags >> 6) | m->flags) & 1;
}

MICRO_OP_SAFE bool IsAbove(struct Machine *m) {  // !CF && ~ZF
  return ((~m->flags >> 6) & ~m->flags) & 1;
}

MICRO_OP_SAFE bool IsLess(struct Machine *m) {  // SF != OF
  return (i8)(m->flags ^ (m->flags >> 4)) < 0;
}

MICRO_OP_SAFE bool IsGreaterOrEqual(struct Machine *m) {  // SF == OF
  return (i8)(~(m->flags ^ (m->flags >> 4))) < 0;
}

MICRO_OP_SAFE bool IsLessOrEqual(struct Machine *m) {  // ZF || SF != OF
  return (i8)((m->flags ^ (m->flags >> 4)) | (m->flags << 1)) < 0;
}

MICRO_OP_SAFE bool IsGreater(struct Machine *m) {  // !ZF && SF == OF
  return (i8)(~(m->flags ^ (m->flags >> 4)) & ~(m->flags << 1)) < 0;
}

MICRO_OP_SAFE void DonePendingSingleStep(struct Machine *m) {
  // this should only be called if FLAGS_F0 is known to be set!
  m->flags = SetFlag(m->flags, FLAGS_F0, false);
  UpvoteJit(&m->system->jit);
}

MICRO_OP_SAFE void CancelPendingSingleStep(struct Machine *m) {
  if (VERY_UNLIKELY(GetFlag(m->flags, FLAGS_F0))) {
    DonePendingSingleStep(m);
  }
}

#endif /* BLINK_FLAGS_H_ */
