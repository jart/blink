#ifndef BLINK_MODRM_H_
#define BLINK_MODRM_H_
#include "blink/machine.h"

#define Rex(x)      ((x & 000000000020) >> 004)
#define Osz(x)      ((x & 000000000040) >> 005)
#define Asz(x)      ((x & 000010000000) >> 025)
#define Rep(x)      ((x & 030000000000) >> 036)
#define Rexr(x)     ((x & 000000000010) >> 003)
#define Rexw(x)     ((x & 000000000100) >> 006)
#define Rexx(x)     ((x & 000000400000) >> 021)
#define Rexb(x)     ((x & 000000002000) >> 012)
#define Sego(x)     ((x & 000007000000) >> 022)
#define Mode(x)     ((x & 001400000000) >> 032)
#define Eamode(x)   ((x & 000300000000) >> 030)
#define RexbRm(x)   ((x & 000000003600) >> 007)
#define RexrReg(x)  ((x & 000000000017) >> 000)
#define RegLog2(x)  ((x & 006000000000) >> 034)
#define ModrmRm(x)  ((x & 000000001600) >> 007)
#define ModrmReg(x) ((x & 000000000007) >> 000)
#define ModrmSrm(x) ((x & 000000070000) >> 014)
#define ModrmMod(x) ((x & 000060000000) >> 026)
#define Modrm(x)    (ModrmMod(x) << 6 | ModrmReg(x) << 3 | ModrmRm(x))

#define RexbBase(m, x)     (Rexb(x) << 3 | SibBase(m->xedd))
#define AddrByteReg(m, k)  ((uint8_t *)m->reg + kByteReg[k])
#define ByteRexrReg(m, x)  AddrByteReg(m, (x & 00000000037) >> 0)
#define ByteRexbRm(m, x)   AddrByteReg(m, (x & 00000007600) >> 7)
#define ByteRexbSrm(m, x)  AddrByteReg(m, (x & 00000370000) >> 12)
#define RegSrm(m, x)       m->reg[(x & 00000070000) >> 12]
#define RegRexbRm(m, x)    m->reg[RexbRm(x)]
#define RegRexbSrm(m, x)   m->reg[(x & 00000170000) >> 12]
#define RegRexrReg(m, x)   m->reg[RexrReg(x)]
#define RegRexbBase(m, x)  m->reg[RexbBase(m, x)]
#define RegRexxIndex(m, x) m->reg[Rexx(x) << 3 | SibIndex(m->xedd)]
#define MmRm(m, x)         m->xmm[(x & 00000001600) >> 7]
#define MmReg(m, x)        m->xmm[(x & 00000000007) >> 0]
#define XmmRexbRm(m, x)    m->xmm[RexbRm(x)]
#define XmmRexrReg(m, x)   m->xmm[RexrReg(x)]

#define SibBase(m)          (m->op.sib & 0007)
#define SibIndex(m)         ((m->op.sib & 0070) >> 3)
#define SibScale(m)         ((m->op.sib & 0300) >> 6)
#define SibExists(x)        (ModrmRm(x) == 4)
#define IsModrmRegister(x)  (ModrmMod(x) == 3)
#define SibHasIndex(x)      (SibIndex(x) != 4 || Rexx(x->op.rde))
#define SibHasBase(x, r)    (SibBase(x) != 5 || ModrmMod(r))
#define SibIsAbsolute(x, r) (!SibHasBase(x, r) && !SibHasIndex(x))
#define IsRipRelative(x)    (Eamode(x) && ModrmRm(x) == 5 && !ModrmMod(x))

struct AddrSeg {
  int64_t addr;
  const uint8_t *seg;
};

extern const uint8_t kByteReg[32];

int64_t ComputeAddress(struct Machine *, uint32_t);
struct AddrSeg LoadEffectiveAddress(const struct Machine *, uint32_t);
void *ComputeReserveAddressRead(struct Machine *, uint32_t, size_t);
void *ComputeReserveAddressRead1(struct Machine *, uint32_t);
void *ComputeReserveAddressRead4(struct Machine *, uint32_t);
void *ComputeReserveAddressRead8(struct Machine *, uint32_t);
void *ComputeReserveAddressWrite(struct Machine *, uint32_t, size_t);
void *ComputeReserveAddressWrite1(struct Machine *, uint32_t);
void *ComputeReserveAddressWrite4(struct Machine *, uint32_t);
void *ComputeReserveAddressWrite8(struct Machine *, uint32_t);
uint8_t *GetModrmRegisterBytePointerRead(struct Machine *, uint32_t);
uint8_t *GetModrmRegisterBytePointerWrite(struct Machine *, uint32_t);
uint8_t *GetModrmRegisterMmPointerRead(struct Machine *, uint32_t, size_t);
uint8_t *GetModrmRegisterMmPointerRead8(struct Machine *, uint32_t);
uint8_t *GetModrmRegisterMmPointerWrite(struct Machine *, uint32_t, size_t);
uint8_t *GetModrmRegisterMmPointerWrite8(struct Machine *, uint32_t);
uint8_t *GetModrmRegisterWordPointerRead(struct Machine *, uint32_t, size_t);
uint8_t *GetModrmRegisterWordPointerRead2(struct Machine *, uint32_t);
uint8_t *GetModrmRegisterWordPointerRead4(struct Machine *, uint32_t);
uint8_t *GetModrmRegisterWordPointerRead8(struct Machine *, uint32_t);
uint8_t *GetModrmRegisterWordPointerReadOsz(struct Machine *, uint32_t);
uint8_t *GetModrmRegisterWordPointerReadOszRexw(struct Machine *, uint32_t);
uint8_t *GetModrmRegisterWordPointerWrite(struct Machine *, uint32_t, size_t);
uint8_t *GetModrmRegisterWordPointerWrite2(struct Machine *, uint32_t);
uint8_t *GetModrmRegisterWordPointerWrite4(struct Machine *, uint32_t);
uint8_t *GetModrmRegisterWordPointerWrite8(struct Machine *, uint32_t);
uint8_t *GetModrmRegisterWordPointerWriteOsz(struct Machine *, uint32_t);
uint8_t *GetModrmRegisterWordPointerWriteOszRexw(struct Machine *, uint32_t);
uint8_t *GetModrmRegisterXmmPointerRead(struct Machine *, uint32_t, size_t);
uint8_t *GetModrmRegisterXmmPointerRead16(struct Machine *, uint32_t);
uint8_t *GetModrmRegisterXmmPointerRead4(struct Machine *, uint32_t);
uint8_t *GetModrmRegisterXmmPointerRead8(struct Machine *, uint32_t);
uint8_t *GetModrmRegisterXmmPointerWrite(struct Machine *, uint32_t, size_t);
uint8_t *GetModrmRegisterXmmPointerWrite16(struct Machine *, uint32_t);
uint8_t *GetModrmRegisterXmmPointerWrite4(struct Machine *, uint32_t);
uint8_t *GetModrmRegisterXmmPointerWrite8(struct Machine *, uint32_t);

#endif /* BLINK_MODRM_H_ */
