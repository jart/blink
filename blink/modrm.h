#ifndef BLINK_MODRM_H_
#define BLINK_MODRM_H_
#include "blink/builtin.h"
#include "blink/machine.h"

#define kRexbRmMask 000000003600
#define RexbRm(x)   ((x & kRexbRmMask) >> 007)

#define kRexrRegMask 000000000017
#define RexrReg(x)   ((x & kRexrRegMask) >> 000)

#define kOplengthMask 00007400000000000000000
#define Oplength(x)   ((x & kOplengthMask) >> 065)

#define kRegRexbSrmMask  00000170000
#define RexbSrm(x)       ((x & kRegRexbSrmMask) >> 014)
#define RegRexbSrm(m, x) m->weg[RexbSrm(x)]

#define Rex(x)      ((x & 000000000020) >> 004)
#define Osz(x)      ((x & 000000000040) >> 005)
#define Asz(x)      ((x & 000010000000) >> 025)
#define Srm(x)      ((x & 000000070000) >> 014)
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
#define ByteRexr(x) ((x & 000000000037) >> 000)
#define ByteRexb(x) ((x & 000000007600) >> 007)
#define Modrm(x)    (ModrmMod(x) << 6 | ModrmReg(x) << 3 | ModrmRm(x))

#define SibBase(x)  ((x & 00000000000340000000000) >> 040)
#define SibIndex(x) ((x & 00000000003400000000000) >> 043)
#define SibScale(x) ((x & 00000000014000000000000) >> 046)
#define Opcode(x)   ((x & 00000007760000000000000) >> 050)
#define Opmap(x)    ((x & 00000070000000000000000) >> 060)
#define Mopcode(x)  ((x & 00000077760000000000000) >> 050)
#define Rep(x)      ((x & 00000300000000000000000) >> 063)
#define WordLog2(x) ((x & 00030000000000000000000) >> 071)

#define Bite(x)            (~ModrmSrm(x) & 1)
#define RexbBase(x)        (Rexb(x) << 3 | SibBase(x))
#define AddrByteReg(m, k)  (m->beg + kByteReg[k])
#define ByteRexrReg(m, x)  AddrByteReg(m, ByteRexr(x))
#define ByteRexbRm(m, x)   AddrByteReg(m, ByteRexb(x))
#define ByteRexbSrm(m, x)  AddrByteReg(m, (x & 00000370000) >> 014)
#define RegSrm(m, x)       m->weg[Srm(x)]
#define RegRexbRm(m, x)    m->weg[RexbRm(x)]
#define RegRexrReg(m, x)   m->weg[RexrReg(x)]
#define RegRexbBase(m, x)  m->weg[RexbBase(x)]
#define RegRexxIndex(m, x) m->weg[Rexx(x) << 3 | SibIndex(x)]
#define MmRm(m, x)         m->xmm[(x & 00000001600) >> 7]
#define MmReg(m, x)        m->xmm[(x & 00000000007) >> 0]
#define XmmRexbRm(m, x)    m->xmm[RexbRm(x)]
#define XmmRexrReg(m, x)   m->xmm[RexrReg(x)]

#define IsByteOp(x)        (~Srm(rde) & 1)
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

i64 ComputeAddress(P);
struct AddrSeg LoadEffectiveAddress(const P);
u8 *ComputeReserveAddressRead(P, size_t);
u8 *ComputeReserveAddressRead1(P);
u8 *ComputeReserveAddressRead4(P);
u8 *ComputeReserveAddressRead8(P);
u8 *ComputeReserveAddressWrite(P, size_t);
u8 *ComputeReserveAddressWrite1(P);
u8 *ComputeReserveAddressWrite4(P);
u8 *ComputeReserveAddressWrite8(P);
u8 *GetModrmRegisterBytePointerRead1(P);
u8 *GetModrmRegisterBytePointerWrite1(P);
u8 *GetModrmRegisterMmPointerRead(P, size_t);
u8 *GetModrmRegisterMmPointerRead8(P);
u8 *GetModrmRegisterMmPointerWrite(P, size_t);
u8 *GetModrmRegisterMmPointerWrite8(P);
u8 *GetModrmRegisterWordPointerRead(P, size_t);
u8 *GetModrmRegisterWordPointerRead2(P);
u8 *GetModrmRegisterWordPointerRead4(P);
u8 *GetModrmRegisterWordPointerRead8(P);
u8 *GetModrmRegisterWordPointerReadOsz(P);
u8 *GetModrmRegisterWordPointerReadOszRexw(P);
u8 *GetModrmRegisterWordPointerWrite(P, size_t);
u8 *GetModrmRegisterWordPointerWrite2(P);
u8 *GetModrmRegisterWordPointerWrite4(P);
u8 *GetModrmRegisterWordPointerWrite8(P);
u8 *GetModrmRegisterWordPointerWriteOsz(P);
u8 *GetModrmRegisterWordPointerWriteOszRexw(P);
u8 *GetModrmRegisterXmmPointerRead16(P);
u8 *GetModrmRegisterXmmPointerRead4(P);
u8 *GetModrmRegisterXmmPointerRead8(P);
u8 *GetModrmRegisterXmmPointerWrite16(P);
u8 *GetModrmRegisterXmmPointerWrite4(P);
u8 *GetModrmRegisterXmmPointerWrite8(P);
u8 *GetXmmAddress(P) returnsaligned((16));
u8 *GetMmxAddress(P) returnsaligned((8));

#endif /* BLINK_MODRM_H_ */
