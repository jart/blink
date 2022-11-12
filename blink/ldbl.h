#ifndef BLINK_LDBL_H_
#define BLINK_LDBL_H_
#include "blink/types.h"

double DeserializeLdbl(const u8[10]);
u8 *SerializeLdbl(u8[10], double);

#endif /* BLINK_LDBL_H_ */
