#ifndef BLINK_SYSCALL_H_
#define BLINK_SYSCALL_H_
#include "blink/fds.h"
#include "blink/machine.h"

extern const struct MachineFdCb kMachineFdCbHost;

void OpSyscall(struct Machine *, uint32_t);

#endif /* BLINK_SYSCALL_H_ */
