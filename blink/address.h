#ifndef BLINK_ADDRESS_H_
#define BLINK_ADDRESS_H_
#include "blink/endian.h"
#include "blink/machine.h"
#include "blink/modrm.h"

u64 MaskAddress(u32, u64);
i64 GetIp(struct Machine *);
i64 GetPc(struct Machine *);
u64 AddressOb(P);
u64 AddressDi(P);
u64 AddressSi(P);
u64 *GetSegment(P, int);
u64 DataSegment(P, u64);
u64 AddSegment(P, u64, u64);

#endif /* BLINK_ADDRESS_H_ */
