#ifndef BLINK_ALU_H_
#define BLINK_ALU_H_
#include <stdbool.h>

#include "blink/machine.h"
#include "blink/types.h"

#define ALU_ADD 0
#define ALU_OR  1
#define ALU_ADC 2
#define ALU_SBB 3
#define ALU_AND 4
#define ALU_SUB 5
#define ALU_XOR 6
#define ALU_CMP 7
#define ALU_NOT 8
#define ALU_NEG 9
#define ALU_INC 10
#define ALU_DEC 11

#define BSU_ROL 0
#define BSU_ROR 1
#define BSU_RCL 2
#define BSU_RCR 3
#define BSU_SHL 4
#define BSU_SHR 5
#define BSU_SAL 6
#define BSU_SAR 7

#define ALU_INT8  0
#define ALU_INT16 1
#define ALU_INT32 2
#define ALU_INT64 3

typedef i64 (*aluop_f)(struct Machine *, u64, u64);

extern const aluop_f kAlu[12][4];
extern const aluop_f kBsu[8][4];
extern const aluop_f kJustAlu[8];
extern const aluop_f kJustBsu[8];
extern const aluop_f kFastDec[4];
extern const aluop_f kJustBsu32[8];
extern const aluop_f kAluFast[8][4];
extern const aluop_f kJustBsuCl32[8];
extern const aluop_f kJustBsuCl64[8];

i64 JustDec(u64);
i64 JustAdd(struct Machine *, u64, u64);
i64 JustOr(struct Machine *, u64, u64);
i64 JustAdc(struct Machine *, u64, u64);
i64 JustSbb(struct Machine *, u64, u64);
i64 JustAnd(struct Machine *, u64, u64);
i64 JustSub(struct Machine *, u64, u64);
i64 JustXor(struct Machine *, u64, u64);

i64 Xor8(struct Machine *, u64, u64);
i64 Xor16(struct Machine *, u64, u64);
i64 Xor32(struct Machine *, u64, u64);
i64 Xor64(struct Machine *, u64, u64);
i64 Or8(struct Machine *, u64, u64);
i64 Or16(struct Machine *, u64, u64);
i64 Or32(struct Machine *, u64, u64);
i64 Or64(struct Machine *, u64, u64);
i64 And8(struct Machine *, u64, u64);
i64 And16(struct Machine *, u64, u64);
i64 And32(struct Machine *, u64, u64);
i64 And64(struct Machine *, u64, u64);
i64 Sub8(struct Machine *, u64, u64);
i64 Sbb8(struct Machine *, u64, u64);
i64 Sub16(struct Machine *, u64, u64);
i64 Sbb16(struct Machine *, u64, u64);
i64 Sub32(struct Machine *, u64, u64);
i64 Sbb32(struct Machine *, u64, u64);
i64 Sub64(struct Machine *, u64, u64);
i64 Sbb64(struct Machine *, u64, u64);
i64 Add8(struct Machine *, u64, u64);
i64 Adc8(struct Machine *, u64, u64);
i64 Add16(struct Machine *, u64, u64);
i64 Adc16(struct Machine *, u64, u64);
i64 Add32(struct Machine *, u64, u64);
i64 Adc32(struct Machine *, u64, u64);
i64 Add64(struct Machine *, u64, u64);
i64 Adc64(struct Machine *, u64, u64);
i64 Not8(struct Machine *, u64, u64);
i64 Not16(struct Machine *, u64, u64);
i64 Not32(struct Machine *, u64, u64);
i64 Not64(struct Machine *, u64, u64);
i64 Neg8(struct Machine *, u64, u64);
i64 Neg16(struct Machine *, u64, u64);
i64 Neg32(struct Machine *, u64, u64);
i64 Neg64(struct Machine *, u64, u64);
i64 Inc8(struct Machine *, u64, u64);
i64 Inc16(struct Machine *, u64, u64);
i64 Inc32(struct Machine *, u64, u64);
i64 Inc64(struct Machine *, u64, u64);
i64 Dec8(struct Machine *, u64, u64);
i64 Dec16(struct Machine *, u64, u64);
i64 Dec32(struct Machine *, u64, u64);
i64 Dec64(struct Machine *, u64, u64);

i64 Shr8(struct Machine *, u64, u64);
i64 Shr16(struct Machine *, u64, u64);
i64 Shr32(struct Machine *, u64, u64);
i64 Shr64(struct Machine *, u64, u64);
i64 Shl8(struct Machine *, u64, u64);
i64 Shl16(struct Machine *, u64, u64);
i64 Shl32(struct Machine *, u64, u64);
i64 Shl64(struct Machine *, u64, u64);
i64 Sar8(struct Machine *, u64, u64);
i64 Sar16(struct Machine *, u64, u64);
i64 Sar32(struct Machine *, u64, u64);
i64 Sar64(struct Machine *, u64, u64);
i64 Rol8(struct Machine *, u64, u64);
i64 Rol16(struct Machine *, u64, u64);
i64 Rol32(struct Machine *, u64, u64);
i64 Rol64(struct Machine *, u64, u64);
i64 Ror8(struct Machine *, u64, u64);
i64 Ror16(struct Machine *, u64, u64);
i64 Ror32(struct Machine *, u64, u64);
i64 Ror64(struct Machine *, u64, u64);
i64 Rcr8(struct Machine *, u64, u64);
i64 Rcr16(struct Machine *, u64, u64);
i64 Rcr32(struct Machine *, u64, u64);
i64 Rcr64(struct Machine *, u64, u64);
i64 Rcl8(struct Machine *, u64, u64);
i64 Rcl16(struct Machine *, u64, u64);
i64 Rcl32(struct Machine *, u64, u64);
i64 Rcl64(struct Machine *, u64, u64);

u64 BsuDoubleShift(struct Machine *, int, u64, u64, u8, bool);

i64 Adcx32(u64, u64, struct Machine *);
i64 Adcx64(u64, u64, struct Machine *);
i64 Adox32(u64, u64, struct Machine *);
i64 Adox64(u64, u64, struct Machine *);

#endif /* BLINK_ALU_H_ */
