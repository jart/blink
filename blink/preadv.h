#ifndef BLINK_PREADV_H_
#define BLINK_PREADV_H_
#if defined(__CYGWIN__) || defined(__HAIKU__) || defined(__APPLE__)
#include <sys/uio.h>

ssize_t preadv(int, const struct iovec *, int, off_t);
ssize_t pwritev(int, const struct iovec *, int, off_t);

#endif /* __PREADV__ */
#endif /* BLINK_PREADV_H_ */
