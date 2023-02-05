#ifndef BLINK_SPIN_H_
#define BLINK_SPIN_H_
#include <stdatomic.h>

static inline void SpinLock(atomic_int *lock) {
  int x;
  for (;;) {
    x = atomic_exchange_explicit(lock, 1, memory_order_acquire);
    if (!x) break;
  }
}

static inline void SpinUnlock(atomic_int *lock) {
  atomic_store_explicit(lock, 0, memory_order_release);
}

#endif /* BLINK_SPIN_H_ */
