#ifndef BLINK_LOCK_H_
#define BLINK_LOCK_H_
#include <pthread.h>

#include "blink/assert.h"
#include "blink/builtin.h"

#ifndef DISABLE_THREADS

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

#else /* DISABLE_THREADS */
#define LOCK(x)                            (void)0
#define UNLOCK(x)                          (void)0
#define pthread_setcancelstate(x, y)       ((void)(y), 0)
#define pthread_mutex_init(x, y)           ((void)(y), 0)
#define pthread_mutex_destroy(x)           0
#define pthread_cond_init(x, y)            ((void)(y), 0)
#define pthread_cond_wait(x, y)            0
#define pthread_cond_signal(x)             0
#define pthread_cond_timedwait(x, y, z)    ((void)(z), 0)
#define pthread_cond_destroy(x)            0
#define pthread_condattr_init(x)           0
#define pthread_condattr_setpshared(x, y)  0
#define pthread_condattr_destroy(x)        0
#define pthread_mutexattr_init(x)          0
#define pthread_mutexattr_setpshared(x, y) 0
#define pthread_mutexattr_destroy(x)       0
#endif /* DISABLE_THREADS */
#endif /* BLINK_LOCK_H_ */
