#ifndef BLINK_ADDRESS_H_
#define BLINK_ADDRESS_H_
#include "blink/endian.h"
#include "blink/machine.h"
#include "blink/modrm.h"

u64 MaskAddress(u32, u64);
i64 GetIp(struct Machine *);
i64 GetPc(struct Machine *);
u64 AddressOb(struct Machine *, DISPATCH_PARAMETERS);
u64 AddressDi(struct Machine *, DISPATCH_PARAMETERS);
u64 AddressSi(struct Machine *, DISPATCH_PARAMETERS);
u64 *GetSegment(struct Machine *, DISPATCH_PARAMETERS, int);
u64 DataSegment(struct Machine *, DISPATCH_PARAMETERS, u64);
u64 AddSegment(struct Machine *, DISPATCH_PARAMETERS, u64, u64);

#endif /* BLINK_ADDRESS_H_ */
