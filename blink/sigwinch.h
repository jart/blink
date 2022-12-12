#ifndef BLINK_SIGWINCH_H_
#define BLINK_SIGWINCH_H_
#include <signal.h>

#ifndef SIGWINCH
// SIGWINCH has the same magic number on all platforms. Yet for some
// reason, platforms are stubborn about defining it in their headers
#define SIGWINCH 28
#endif

#endif /* BLINK_SIGWINCH_H_ */
