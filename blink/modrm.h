#ifndef BLINK_MODRM_H_
#define BLINK_MODRM_H_
#include "blink/machine.h"

#define kRexbRmMask 000000003600
#define RexbRm(x)   ((x & kRexbRmMask) >> 007)

#define kRexrRegMask 000000000017
#define RexrReg(x)   ((x & kRexrRegMask) >> 000)

#define kOplengthMask 00007400000000000000000
#define Oplength(x)   ((x & kOplengthMask) >> 065)

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
#define RegLog2(x)  ((x & 006000000000) >> 034)
#define ModrmRm(x)  ((x & 000000001600) >> 007)
#define ModrmReg(x) ((x & 000000000007) >> 000)
#define ModrmSrm(x) ((x & 000000070000) >> 014)
#define ModrmMod(x) ((x & 000060000000) >> 026)
#define Modrm(x)    (ModrmMod(x) << 6 | ModrmReg(x) << 3 | ModrmRm(x))
#define SibBase(x)  ((x & 00000000000340000000000) >> 040)
#define SibIndex(x) ((x & 00000000003400000000000) >> 043)
#define SibScale(x) ((x & 00000000014000000000000) >> 046)
#define Opcode(x)   ((x & 00000007760000000000000) >> 050)
#define Opmap(x)    ((x & 00000070000000000000000) >> 060)
#define Mopcode(x)  ((x & 00000077760000000000000) >> 050)
#define Rep(x)      ((x & 00000300000000000000000) >> 063)

#define Bite(x)            (~ModrmSrm(x) & 1)
#define RexbBase(x)        (Rexb(x) << 3 | SibBase(x))
#define AddrByteReg(m, k)  ((u8 *)m->reg + kByteReg[k])
#define ByteRexrReg(m, x)  AddrByteReg(m, (x & 00000000037) >> 0)
#define ByteRexbRm(m, x)   AddrByteReg(m, (x & 00000007600) >> 7)
#define ByteRexbSrm(m, x)  AddrByteReg(m, (x & 00000370000) >> 12)
#define RegSrm(m, x)       m->reg[(x & 00000070000) >> 12]
#define RegRexbRm(m, x)    m->reg[RexbRm(x)]
#define RegRexbSrm(m, x)   m->reg[(x & 00000170000) >> 12]
#define RegRexrReg(m, x)   m->reg[RexrReg(x)]
#define RegRexbBase(m, x)  m->reg[RexbBase(x)]
#define RegRexxIndex(m, x) m->reg[Rexx(x) << 3 | SibIndex(x)]
#define MmRm(m, x)         m->xmm[(x & 00000001600) >> 7]
#define MmReg(m, x)        m->xmm[(x & 00000000007) >> 0]
#define XmmRexbRm(m, x)    m->xmm[RexbRm(x)]
#define XmmRexrReg(m, x)   m->xmm[RexrReg(x)]

#define SibExists(x)       (ModrmRm(x) == 4)
#define IsModrmRegister(x) (ModrmMod(x) == 3)
#define SibHasIndex(x)     (SibIndex(x) != 4 || Rexx(x))
#define SibHasBase(x)      (SibBase(x) != 5 || ModrmMod(x))
#define SibIsAbsolute(x)   (!SibHasBase(x) && !SibHasIndex(x))
#define IsRipRelative(x)   (Eamode(x) && ModrmRm(x) == 5 && !ModrmMod(x))

struct AddrSeg {
  i64 addr;
  u64 seg;
};

extern const u8 kByteReg[32];

i64 ComputeAddress(struct Machine *, DISPATCH_PARAMETERS);
struct AddrSeg LoadEffectiveAddress(const struct Machine *, DISPATCH_PARAMETERS);
u8 *ComputeReserveAddressRead(struct Machine *, DISPATCH_PARAMETERS, size_t);
u8 *ComputeReserveAddressRead1(struct Machine *, DISPATCH_PARAMETERS);
u8 *ComputeReserveAddressRead4(struct Machine *, DISPATCH_PARAMETERS);
u8 *ComputeReserveAddressRead8(struct Machine *, DISPATCH_PARAMETERS);
u8 *ComputeReserveAddressWrite(struct Machine *, DISPATCH_PARAMETERS, size_t);
u8 *ComputeReserveAddressWrite1(struct Machine *, DISPATCH_PARAMETERS);
u8 *ComputeReserveAddressWrite4(struct Machine *, DISPATCH_PARAMETERS);
u8 *ComputeReserveAddressWrite8(struct Machine *, DISPATCH_PARAMETERS);
u8 *GetModrmRegisterBytePointerRead(struct Machine *, DISPATCH_PARAMETERS);
u8 *GetModrmRegisterBytePointerWrite(struct Machine *, DISPATCH_PARAMETERS);
u8 *GetModrmRegisterMmPointerRead(struct Machine *, DISPATCH_PARAMETERS, size_t);
u8 *GetModrmRegisterMmPointerRead8(struct Machine *, DISPATCH_PARAMETERS);
u8 *GetModrmRegisterMmPointerWrite(struct Machine *, DISPATCH_PARAMETERS, size_t);
u8 *GetModrmRegisterMmPointerWrite8(struct Machine *, DISPATCH_PARAMETERS);
u8 *GetModrmRegisterWordPointerRead(struct Machine *, DISPATCH_PARAMETERS, size_t);
u8 *GetModrmRegisterWordPointerRead2(struct Machine *, DISPATCH_PARAMETERS);
u8 *GetModrmRegisterWordPointerRead4(struct Machine *, DISPATCH_PARAMETERS);
u8 *GetModrmRegisterWordPointerRead8(struct Machine *, DISPATCH_PARAMETERS);
u8 *GetModrmRegisterWordPointerReadOsz(struct Machine *, DISPATCH_PARAMETERS);
u8 *GetModrmRegisterWordPointerReadOszRexw(struct Machine *, DISPATCH_PARAMETERS);
u8 *GetModrmRegisterWordPointerWrite(struct Machine *, DISPATCH_PARAMETERS, size_t);
u8 *GetModrmRegisterWordPointerWrite2(struct Machine *, DISPATCH_PARAMETERS);
u8 *GetModrmRegisterWordPointerWrite4(struct Machine *, DISPATCH_PARAMETERS);
u8 *GetModrmRegisterWordPointerWrite8(struct Machine *, DISPATCH_PARAMETERS);
u8 *GetModrmRegisterWordPointerWriteOsz(struct Machine *, DISPATCH_PARAMETERS);
u8 *GetModrmRegisterWordPointerWriteOszRexw(struct Machine *, DISPATCH_PARAMETERS);
u8 *GetModrmRegisterXmmPointerRead(struct Machine *, DISPATCH_PARAMETERS, size_t);
u8 *GetModrmRegisterXmmPointerRead16(struct Machine *, DISPATCH_PARAMETERS);
u8 *GetModrmRegisterXmmPointerRead4(struct Machine *, DISPATCH_PARAMETERS);
u8 *GetModrmRegisterXmmPointerRead8(struct Machine *, DISPATCH_PARAMETERS);
u8 *GetModrmRegisterXmmPointerWrite(struct Machine *, DISPATCH_PARAMETERS, size_t);
u8 *GetModrmRegisterXmmPointerWrite16(struct Machine *, DISPATCH_PARAMETERS);
u8 *GetModrmRegisterXmmPointerWrite4(struct Machine *, DISPATCH_PARAMETERS);
u8 *GetModrmRegisterXmmPointerWrite8(struct Machine *, DISPATCH_PARAMETERS);

#endif /* BLINK_MODRM_H_ */
