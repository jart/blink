#ifndef BLINK_CYGWIN_H_
#define BLINK_CYGWIN_H_
#ifdef __CYGWIN__
#include <sys/uio.h>

ssize_t preadv(int, const struct iovec *, int, off_t);
ssize_t pwritev(int, const struct iovec *, int, off_t);

#endif /* __CYGWIN__ */
#endif /* BLINK_CYGWIN_H_ */
