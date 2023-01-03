#ifndef BLINK_SPIN_H_
#define BLINK_SPIN_H_
#include <stdatomic.h>

#include "blink/assert.h"

static inline void SpinLock(atomic_int *lock) {
  int x;
  for (;;) {
    x = atomic_exchange_explicit(lock, 1, memory_order_acquire);
    if (!x) break;
    unassert(x == 1);
  }
}

static inline void SpinUnlock(atomic_int *lock) {
  unassert(atomic_load_explicit(lock, memory_order_relaxed) == 1);
  atomic_store_explicit(lock, 0, memory_order_release);
}

#endif /* BLINK_SPIN_H_ */
