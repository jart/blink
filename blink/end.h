#ifndef BLINK_END_H_
#define BLINK_END_H_
#include "blink/flag.h"

// many platforms complain about `end` / `_end` so we'll just pick some
// arbitarry variable in the .bss section, which should be close enough
#define IMAGE_END ((u8 *)&FLAG_noconnect)

#endif /* BLINK_END_H_ */
