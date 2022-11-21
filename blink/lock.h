#ifndef BLINK_LOCK_H_
#define BLINK_LOCK_H_
#include <pthread.h>

#include "blink/assert.h"

#define LOCK(x)   unassert(!pthread_mutex_lock(x))
#define UNLOCK(x) unassert(!pthread_mutex_unlock(x))

#endif /* BLINK_LOCK_H_ */
