#ifndef BLINK_ALU_H_
#define BLINK_ALU_H_
#include <stdbool.h>

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

typedef i64 (*aluop_f)(u64, u64, u32 *);

extern const aluop_f kAlu[12][4];
extern const aluop_f kBsu[8][4];

i64 Xor8(u64, u64, u32 *);
i64 Xor16(u64, u64, u32 *);
i64 Xor32(u64, u64, u32 *);
i64 Xor64(u64, u64, u32 *);
i64 Or8(u64, u64, u32 *);
i64 Or16(u64, u64, u32 *);
i64 Or32(u64, u64, u32 *);
i64 Or64(u64, u64, u32 *);
i64 And8(u64, u64, u32 *);
i64 And16(u64, u64, u32 *);
i64 And32(u64, u64, u32 *);
i64 And64(u64, u64, u32 *);
i64 Sub8(u64, u64, u32 *);
i64 Sbb8(u64, u64, u32 *);
i64 Sub16(u64, u64, u32 *);
i64 Sbb16(u64, u64, u32 *);
i64 Sub32(u64, u64, u32 *);
i64 Sbb32(u64, u64, u32 *);
i64 Sub64(u64, u64, u32 *);
i64 Sbb64(u64, u64, u32 *);
i64 Add8(u64, u64, u32 *);
i64 Adc8(u64, u64, u32 *);
i64 Add16(u64, u64, u32 *);
i64 Adc16(u64, u64, u32 *);
i64 Add32(u64, u64, u32 *);
i64 Adc32(u64, u64, u32 *);
i64 Add64(u64, u64, u32 *);
i64 Adc64(u64, u64, u32 *);
i64 Not8(u64, u64, u32 *);
i64 Not16(u64, u64, u32 *);
i64 Not32(u64, u64, u32 *);
i64 Not64(u64, u64, u32 *);
i64 Neg8(u64, u64, u32 *);
i64 Neg16(u64, u64, u32 *);
i64 Neg32(u64, u64, u32 *);
i64 Neg64(u64, u64, u32 *);
i64 Inc8(u64, u64, u32 *);
i64 Inc16(u64, u64, u32 *);
i64 Inc32(u64, u64, u32 *);
i64 Inc64(u64, u64, u32 *);
i64 Dec8(u64, u64, u32 *);
i64 Dec16(u64, u64, u32 *);
i64 Dec32(u64, u64, u32 *);
i64 Dec64(u64, u64, u32 *);

i64 Shr8(u64, u64, u32 *);
i64 Shr16(u64, u64, u32 *);
i64 Shr32(u64, u64, u32 *);
i64 Shr64(u64, u64, u32 *);
i64 Shl8(u64, u64, u32 *);
i64 Shl16(u64, u64, u32 *);
i64 Shl32(u64, u64, u32 *);
i64 Shl64(u64, u64, u32 *);
i64 Sar8(u64, u64, u32 *);
i64 Sar16(u64, u64, u32 *);
i64 Sar32(u64, u64, u32 *);
i64 Sar64(u64, u64, u32 *);
i64 Rol8(u64, u64, u32 *);
i64 Rol16(u64, u64, u32 *);
i64 Rol32(u64, u64, u32 *);
i64 Rol64(u64, u64, u32 *);
i64 Ror8(u64, u64, u32 *);
i64 Ror16(u64, u64, u32 *);
i64 Ror32(u64, u64, u32 *);
i64 Ror64(u64, u64, u32 *);
i64 Rcr8(u64, u64, u32 *);
i64 Rcr16(u64, u64, u32 *);
i64 Rcr32(u64, u64, u32 *);
i64 Rcr64(u64, u64, u32 *);
i64 Rcl8(u64, u64, u32 *);
i64 Rcl16(u64, u64, u32 *);
i64 Rcl32(u64, u64, u32 *);
i64 Rcl64(u64, u64, u32 *);

u64 BsuDoubleShift(int, u64, u64, u8, bool, u32 *);

#endif /* BLINK_ALU_H_ */
