#ifndef BLINK_LOCK_H_
#define BLINK_LOCK_H_
#include "blink/builtin.h"
#ifndef DISABLE_THREADS
#define HAVE_THREADS
#include <pthread.h>

#include "blink/assert.h"
#include "blink/log.h"

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

#define PTHREAD_ONCE_INIT_         PTHREAD_ONCE_INIT
#define PTHREAD_MUTEX_INITIALIZER_ PTHREAD_MUTEX_INITIALIZER

#define pthread_once_        pthread_once
#define pthread_once_t_      pthread_once_t
#define pthread_cond_t_      pthread_cond_t
#define pthread_mutex_t_     pthread_mutex_t
#define pthread_condattr_t_  pthread_condattr_t
#define pthread_mutexattr_t_ pthread_mutexattr_t

#else /* HAVE_THREADS */
#include <signal.h>

#ifdef _Thread_local
#undef _Thread_local
#endif
#define _Thread_local

#define LOCK(x)   (void)0
#define UNLOCK(x) (void)0

#define PTHREAD_ONCE_INIT_         0
#define PTHREAD_MUTEX_INITIALIZER_ 0

#define pthread_once_t_      char
#define pthread_cond_t_      char
#define pthread_mutex_t_     char
#define pthread_condattr_t_  char
#define pthread_mutexattr_t_ char

#define pthread_self()                     0
#define pthread_sigmask(x, y, z)           sigprocmask(x, y, z)
#define pthread_setcancelstate(x, y)       ((void)(y), 0)
#define pthread_mutex_init(x, y)           ((void)(y), 0)
#define pthread_mutex_destroy(x)           0
#define pthread_cond_init(x, y)            ((void)(y), 0)
#define pthread_cond_wait(x, y)            0
#define pthread_cond_signal(x)             0
#define pthread_cond_broadcast(x)          0
#define pthread_cond_timedwait(x, y, z)    ((void)(z), 0)
#define pthread_cond_destroy(x)            0
#define pthread_condattr_init(x)           0
#define pthread_condattr_setpshared(x, y)  0
#define pthread_condattr_destroy(x)        0
#define pthread_mutexattr_init(x)          0
#define pthread_mutexattr_setpshared(x, y) 0
#define pthread_mutexattr_destroy(x)       0
#define pthread_mutexattr_settype(x, y)    0
#define pthread_atfork(a, b, c)            0
#define pthread_create(a, b, c, d)         0
#define pthread_join(a, b)                 0

static inline int pthread_once_(pthread_once_t_ *once_control,
                                void (*init_routine)(void)) {
  if (!*once_control) {
    init_routine();
    *once_control = 1;
  }
  return 0;
}

#endif /* DISABLE_THREADS */
#endif /* BLINK_LOCK_H_ */
