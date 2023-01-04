#ifndef BLINK_END_H_
#define BLINK_END_H_
#include "blink/jit.h"
#include "blink/mop.h"

#if defined(__APPLE__) || defined(__EMSCRIPTEN__)
#define IMAGE_END ((u8 *)&StartJit)
#else
extern u8 end[];
#define IMAGE_END end
#endif

#endif /* BLINK_END_H_ */
