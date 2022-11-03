#ifndef BLINK_THROW_H_
#define BLINK_THROW_H_
#include "blink/machine.h"

_Noreturn void OpUd(struct Machine *, uint32_t);
_Noreturn void HaltMachine(struct Machine *, int);
_Noreturn void ThrowDivideError(struct Machine *);
_Noreturn void ThrowSegmentationFault(struct Machine *, int64_t);
_Noreturn void ThrowProtectionFault(struct Machine *);
_Noreturn void OpHlt(struct Machine *, uint32_t);

#endif /* BLINK_THROW_H_ */
