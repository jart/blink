#ifndef COSMOPOLITAN_BLINK_ANCILLARY_H_
#define COSMOPOLITAN_BLINK_ANCILLARY_H_
#include <sys/socket.h>

#include "blink/machine.h"

int SendAncillary(struct Machine *, struct msghdr *,
                  const struct msghdr_linux *);
int ReceiveAncillary(struct Machine *, struct msghdr_linux *, struct msghdr *,
                     int);

#endif /* COSMOPOLITAN_BLINK_ANCILLARY_H_ */
