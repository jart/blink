#ifndef BLINK_PREADV_H_
#define BLINK_PREADV_H_
#include <sys/uio.h>

#include "blink/builtin.h"

#ifdef POLYFILL_PREADV
ssize_t preadv(int, const struct iovec *, int, off_t);
ssize_t pwritev(int, const struct iovec *, int, off_t);
#endif

#endif /* BLINK_PREADV_H_ */
