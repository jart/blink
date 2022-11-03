#ifndef BLINK_THOMPIKE_H_
#define BLINK_THOMPIKE_H_
#include "blink/bitscan.h"

#define ThomPikeCont(x)     (((x)&0300) == 0200)
#define ThomPikeByte(x)     ((x) & (((1 << ThomPikeMsb(x)) - 1) | 3))
#define ThomPikeLen(x)      (7 - ThomPikeMsb(x))
#define ThomPikeMsb(x)      (((x)&0xff) < 252 ? bsr(~(x)&0xff) : 1)
#define ThomPikeMerge(x, y) ((x) << 6 | ((y)&077))

#endif /* BLINK_THOMPIKE_H_ */
