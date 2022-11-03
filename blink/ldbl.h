#ifndef BLINK_LDBL_H_
#define BLINK_LDBL_H_
#include <stdint.h>

double DeserializeLdbl(const uint8_t[10]);
uint8_t *SerializeLdbl(uint8_t[10], double);

#endif /* BLINK_LDBL_H_ */
