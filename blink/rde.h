#ifndef BLINK_RDE_H_
#define BLINK_RDE_H_

#define kRexbRmMask 000000003600
#define RexbRm(x)   ((x & kRexbRmMask) >> 007)

#define kRexrRegMask 000000000017
#define RexrReg(x)   ((x & kRexrRegMask) >> 000)

#define kOplengthMask 00007400000000000000000
#define Oplength(x)   ((x & kOplengthMask) >> 065)

#define kRegRexbSrmMask 00000170000
#define RexbSrm(x)      ((x & kRegRexbSrmMask) >> 014)

#define Rex(x)        ((x & 000000000020) >> 004)
#define Osz(x)        ((x & 000000000040) >> 005)
#define Asz(x)        ((x & 000010000000) >> 025)
#define Srm(x)        ((x & 000000070000) >> 014)
#define Rexr(x)       ((x & 000000000010) >> 003)
#define Rexw(x)       ((x & 000000000100) >> 006)
#define Rexx(x)       ((x & 000000400000) >> 021)
#define Rexb(x)       ((x & 000000002000) >> 012)
#define Sego(x)       ((x & 000007000000) >> 022)
#define Ymm(x)        ((x & 010000000000) >> 036)
#define RegLog2(x)    ((x & 006000000000) >> 034)
#define ModrmRm(x)    ((x & 000000001600) >> 007)
#define ModrmReg(x)   ((x & 000000000007) >> 000)
#define ModrmSrm(x)   ((x & 000000070000) >> 014)
#define ModrmMod(x)   ((x & 000060000000) >> 026)
#define RexRexr(x)    ((x & 000000000037) >> 000)
#define RexRexb(x)    ((x & 000000007600) >> 007)
#define RexRexbSrm(x) ((x & 000000370000) >> 014)
#define Modrm(x)      (ModrmMod(x) << 6 | ModrmReg(x) << 3 | ModrmRm(x))

#define SibBase(x)  ((x & 00000000000340000000000) >> 040)
#define SibIndex(x) ((x & 00000000003400000000000) >> 043)
#define SibScale(x) ((x & 00000000014000000000000) >> 046)
#define Opcode(x)   ((x & 00000007760000000000000) >> 050)
#define Opmap(x)    ((x & 00000070000000000000000) >> 060)
#define Mopcode(x)  ((x & 00000077760000000000000) >> 050)
#define Rep(x)      ((x & 00000300000000000000000) >> 063)
#define WordLog2(x) ((x & 00030000000000000000000) >> 071)
#define Vreg(x)     ((x & 01700000000000000000000) >> 074)

#define Bite(x)     (~ModrmSrm(x) & 1)
#define RexbBase(x) (Rexb(x) << 3 | SibBase(x))

#define IsByteOp(x)        (~Srm(rde) & 1)
#define SibExists(x)       (ModrmRm(x) == 4)
#define IsModrmRegister(x) (ModrmMod(x) == 3)
#define SibHasIndex(x)     (SibIndex(x) != 4 || Rexx(x))
#define SibHasBase(x)      (SibBase(x) != 5 || ModrmMod(x))
#define SibIsAbsolute(x)   (!SibHasBase(x) && !SibHasIndex(x))
#define IsRipRelative(x)   (Eamode(x) && ModrmRm(x) == 5 && !ModrmMod(x))

#endif /* BLINK_RDE_H_ */
