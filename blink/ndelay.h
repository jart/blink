#ifndef BLINK_NDELAY_H_
#define BLINK_NDELAY_H_
#include <fcntl.h>

#if !defined(O_NDELAY) && defined(O_NONBLOCK)
#define O_NDELAY O_NONBLOCK
#endif

#endif /* BLINK_NDELAY_H_ */
