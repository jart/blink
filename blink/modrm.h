#ifndef BLINK_MODRM_H_
#define BLINK_MODRM_H_
#include "blink/machine.h"

#define Rex(x)      ((x & 000000000020) >> 004)
#define Osz(x)      ((x & 000000000040) >> 005)
#define Asz(x)      ((x & 000010000000) >> 025)
#define Lock(x)     ((x & 020000000000) >> 037)
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
#define AddrByteReg(m, k)  ((u8 *)m->reg + kByteReg[k])
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
  i64 addr;
  u64 seg;
};

extern const u8 kByteReg[32];

i64 ComputeAddress(struct Machine *, u32);
struct AddrSeg LoadEffectiveAddress(const struct Machine *, u32);
u8 *ComputeReserveAddressRead(struct Machine *, u32, size_t);
u8 *ComputeReserveAddressRead1(struct Machine *, u32);
u8 *ComputeReserveAddressRead4(struct Machine *, u32);
u8 *ComputeReserveAddressRead8(struct Machine *, u32);
u8 *ComputeReserveAddressWrite(struct Machine *, u32, size_t);
u8 *ComputeReserveAddressWrite1(struct Machine *, u32);
u8 *ComputeReserveAddressWrite4(struct Machine *, u32);
u8 *ComputeReserveAddressWrite8(struct Machine *, u32);
u8 *GetModrmRegisterBytePointerRead(struct Machine *, u32);
u8 *GetModrmRegisterBytePointerWrite(struct Machine *, u32);
u8 *GetModrmRegisterMmPointerRead(struct Machine *, u32, size_t);
u8 *GetModrmRegisterMmPointerRead8(struct Machine *, u32);
u8 *GetModrmRegisterMmPointerWrite(struct Machine *, u32, size_t);
u8 *GetModrmRegisterMmPointerWrite8(struct Machine *, u32);
u8 *GetModrmRegisterWordPointerRead(struct Machine *, u32, size_t);
u8 *GetModrmRegisterWordPointerRead2(struct Machine *, u32);
u8 *GetModrmRegisterWordPointerRead4(struct Machine *, u32);
u8 *GetModrmRegisterWordPointerRead8(struct Machine *, u32);
u8 *GetModrmRegisterWordPointerReadOsz(struct Machine *, u32);
u8 *GetModrmRegisterWordPointerReadOszRexw(struct Machine *, u32);
u8 *GetModrmRegisterWordPointerWrite(struct Machine *, u32, size_t);
u8 *GetModrmRegisterWordPointerWrite2(struct Machine *, u32);
u8 *GetModrmRegisterWordPointerWrite4(struct Machine *, u32);
u8 *GetModrmRegisterWordPointerWrite8(struct Machine *, u32);
u8 *GetModrmRegisterWordPointerWriteOsz(struct Machine *, u32);
u8 *GetModrmRegisterWordPointerWriteOszRexw(struct Machine *, u32);
u8 *GetModrmRegisterXmmPointerRead(struct Machine *, u32, size_t);
u8 *GetModrmRegisterXmmPointerRead16(struct Machine *, u32);
u8 *GetModrmRegisterXmmPointerRead4(struct Machine *, u32);
u8 *GetModrmRegisterXmmPointerRead8(struct Machine *, u32);
u8 *GetModrmRegisterXmmPointerWrite(struct Machine *, u32, size_t);
u8 *GetModrmRegisterXmmPointerWrite16(struct Machine *, u32);
u8 *GetModrmRegisterXmmPointerWrite4(struct Machine *, u32);
u8 *GetModrmRegisterXmmPointerWrite8(struct Machine *, u32);

#endif /* BLINK_MODRM_H_ */
