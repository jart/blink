#ifndef BLINK_PREADV_H_
#define BLINK_PREADV_H_
#include <sys/types.h>
#include <sys/uio.h>

#include "blink/builtin.h"

ssize_t preadv_(int, const struct iovec *, int, off_t);
ssize_t pwritev_(int, const struct iovec *, int, off_t);

#ifndef HAVE_PREADV
#ifdef preadv
#undef preadv
#endif
#ifdef pwritev
#undef pwritev
#endif
#define preadv  preadv_
#define pwritev pwritev_
#endif

#endif /* BLINK_PREADV_H_ */
