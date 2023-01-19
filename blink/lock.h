#ifndef BLINK_LOCK_H_
#define BLINK_LOCK_H_
#include <pthread.h>

#include "blink/assert.h"

#define LOCK(x)   unassert(!pthread_mutex_lock(x))
#define UNLOCK(x) unassert(!pthread_mutex_unlock(x))

#ifdef __HAIKU__
#include <OS.h>
#undef UNLOCK
#define UNLOCK(x)                       \
  do {                                  \
    (x)->owner = find_thread(NULL);     \
    unassert(!pthread_mutex_unlock(x)); \
  } while (0)
#endif

#endif /* BLINK_LOCK_H_ */
